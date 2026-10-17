# 设计规范：Cobalt 现代化线性 Pike VM 正则表达式引擎

**文档标识**：`docs/superpowers/specs/2026-10-17-regex-engine-design.md`  
**创建时间**：2026-10-17  
**作者**：Cobalt Architecture Team & Pair Programmer  
**目标版本**：Cobalt v2.7.0  
**状态**：APPROVED (待用户最终审阅)

---

## 1. 背景与设计目标

### 1.1 背景
Cobalt 作为一个现代 C11、零外部依赖的底层框架，在经历 Phase 1（排序引擎）、Phase 2（线程池与调度）、Phase 3（EventLoop 与异步流缓冲）、Phase 4（并行算法与 TaskGraph DAG）的演进后，已具备强大的数据处理与并发调度底座。然而，目前框架仅在 `include/cobalt/utils/string.h` 中提供基础的前后缀与子串判定函数（`cobalt_starts_with`、`cobalt_contains`），缺乏通用的正则表达式（Regular Expression）模式匹配与子串提取能力。

传统的 C 正则库存在明显的两极分化：
1. **PCRE / PCRE2**：功能完备，但源码达十万行以上，库体积数兆字节，且基于回溯（Backtracking）机制，极易遭受 **ReDoS（灾难性回溯拒绝服务）** 攻击（指数级时间复杂度 $O(2^N)$）；
2. **轻量嵌入式库**：多数缺乏捕获组提取能力或语法支持过于贫瘠。

### 1.2 设计目标
本规范设计并实现一套面向高性能系统编程、嵌入式与网络协议解析的 **现代化 Pike VM（Thompson NFA 字节码模拟）正则表达式引擎**：
1. **架构定位**：归属 Cobalt **Layer 3（算法层）**，头文件为 `include/cobalt/algorithm/regex.h`，实现为 `src/algorithm/regex.c`；
2. **零外部依赖与纯 C11**：无需系统 `regex.h` 或外部 PCRE，代码小巧纯粹（约 1,000 行），无平台绑定；
3. **线性时间与 ReDoS 免疫**：除显式使用反向引用外的所有标准模式，均严格在 $O(N \times M)$ 线性时间内完成匹配，杜绝 CPU 挂死；
4. **零拷贝切片捕获**：匹配结果通过紧凑的结构体数组 `cobalt_regex_capture_t` 输出字节偏移区间 `[start, end)`，匹配热路径**零堆内存分配**；
5. **语法完备性**：
   - 基础元字符：`.`, `^`, `$`, `*`, `+`, `?`
   - 非贪婪量词：`*?`, `+?`, `??`, `{m,n}?`
   - 范围量词：`{m,n}`, `{m,}`, `{m}`
   - 字符集与取反：`[a-zA-Z0-9_]`, `[^0-9]`
   - 常用字符类简写：`\d`, `\D`, `\w`, `\W`, `\s`, `\S`
   - 单词边界断言：`\b`, `\B`
   - 分支选择：`a|b|c`
   - 子捕获组与非捕获组：`(abc)`, `(?:abc)`
   - 标志位：忽略大小写 `COBALT_REGEX_CASE_INSENSITIVE`、点号通配所有 `COBALT_REGEX_DOTALL`、多行模式 `COBALT_REGEX_MULTILINE`
   - 高级扩展：支持基于线程局部槽位记忆的**反向引用（Backreference，如 `(\w+)\s+\1`）**。
6. **分配器注入**：完整支持 Cobalt 统一内存分配契约 `cobalt_allocator_t`。

---

## 2. 架构设计与公共 API

### 2.1 模块分层与依赖
```
+-------------------------------------------------------------+
| L1 / L2 Modules: EventLoop, JSON, HTTP Routing, TaskGraph   |
+-------------------------------------------------------------+
                              | (单向自底向上依赖)
                              v
+-------------------------------------------------------------+
| L3 Algorithm: Regex Engine (include/cobalt/algorithm/regex.h) |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
| L7 Memory: Allocator API (cobalt_allocator_t)               |
| L8 Platform: Platform types, stdatomic, stdbool             |
+-------------------------------------------------------------+
```

### 2.2 公共数据结构与类型定义
```c
#ifndef COBALT_ALGORITHM_REGEX_H
#define COBALT_ALGORITHM_REGEX_H

#include "cobalt/memory/allocator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 最大支持捕获组上限 (含 group 0 完整匹配) */
#define COBALT_REGEX_MAX_CAPTURES 16

/**
 * @brief 正则编译控制标志位 (按位组合)
 */
typedef enum {
    COBALT_REGEX_DEFAULT          = 0,      /**< 默认模式 (区分大小写，单行模式) */
    COBALT_REGEX_CASE_INSENSITIVE = 1 << 0, /**< 忽略大小写匹配 (i) */
    COBALT_REGEX_DOTALL           = 1 << 1, /**< '.' 可匹配包含 '\n' 在内的所有字符 (s) */
    COBALT_REGEX_MULTILINE        = 1 << 2  /**< '^' 和 '$' 匹配每行首尾 (m) */
} cobalt_regex_flags_t;

/**
 * @brief 捕获组切片结果 (零内存拷贝)
 */
typedef struct {
    size_t start;   /**< 匹配起始字节偏移量 (从 0 开始) */
    size_t end;     /**< 匹配结束字节偏移量 (开区间 [start, end)) */
    size_t length;  /**< 子串长度 (end - start) */
} cobalt_regex_capture_t;

/**
 * @brief 编译后的正则表达式不透明句柄
 */
typedef struct cobalt_regex cobalt_regex_t;
```

### 2.3 公共 API 接口签名

```c
/* -------------------------------------------------------------------------- */
/* 生命周期管理                                                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief 使用系统分配器编译正则表达式
 * @param pattern 正则表达式模式字符串 (以 '\0' 结尾)
 * @param flags 编译标志位 (如 COBALT_REGEX_CASE_INSENSITIVE)
 * @return 编译成功的正则对象指针；语法错误或 OOM 时返回 NULL 并设置错误码
 */
cobalt_regex_t *cobalt_regex_compile(const char *pattern, int flags);

/**
 * @brief 使用自定义内存分配器编译正则表达式
 * @param pattern 正则表达式模式字符串
 * @param flags 编译标志位
 * @param alloc 自定义内存分配器
 * @return 编译成功的正则对象指针，失败返回 NULL
 */
cobalt_regex_t *cobalt_regex_compile_with_alloc(const char         *pattern,
                                                int                 flags,
                                                cobalt_allocator_t *alloc);

/**
 * @brief 销毁正则对象并释放全部内部字节码与元数据
 * @param re 正则对象指针，传入 NULL 安全无操作
 */
void cobalt_regex_destroy(cobalt_regex_t *re);

/* -------------------------------------------------------------------------- */
/* 匹配检索与执行                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief 全文搜索匹配 (搜索 text 中首个满足 pattern 的子串)
 * @param re 编译后的正则对象
 * @param text 待检索的目标字符串
 * @param captures 接收捕获组切片的数组 (captures[0] 为整个匹配，captures[1..N] 为子捕获组)
 * @param max_captures captures 数组最大容量
 * @return 匹配成功返回 1；未匹配返回 0；参数非法或运行时错误返回 -1
 */
int cobalt_regex_search(const cobalt_regex_t   *re,
                        const char             *text,
                        cobalt_regex_capture_t *captures,
                        size_t                  max_captures);

/**
 * @brief 完全匹配检测 (text 必须从头到尾完全契合 pattern)
 * @param re 编译后的正则对象
 * @param text 待检索的目标字符串
 * @param captures 接收捕获组切片的数组 (可选，传入 NULL 忽略)
 * @param max_captures captures 数组最大容量
 * @return 完全匹配返回 1；不完全匹配返回 0；错误返回 -1
 */
int cobalt_regex_match_exact(const cobalt_regex_t   *re,
                             const char             *text,
                             cobalt_regex_capture_t *captures,
                             size_t                  max_captures);

/**
 * @brief 单行快捷布尔匹配检测 (内部无需显式管理对象生命周期)
 * @param pattern 正则表达式模式
 * @param text 待检索的目标字符串
 * @return 匹配返回 1，不匹配返回 0，语法错误返回 -1
 */
int cobalt_regex_test(const char *pattern, const char *text);

/**
 * @brief 查询正则模式中定义的捕获组总数 (不含第 0 组完整匹配)
 * @param re 编译后的正则对象
 * @return 捕获组数量
 */
size_t cobalt_regex_capture_count(const cobalt_regex_t *re);

#ifdef __cplusplus
}
#endif

#endif /* COBALT_ALGORITHM_REGEX_H */
```

---

## 3. Pike 虚拟机指令集与执行模型

### 3.1 字节码指令集 (ISA)

每个编译后的正则模式表示为连续的指令数组 `cobalt_regex_inst_t[]`：

| 操作码 (Opcode) | 操作数字段 | 语义与执行行为 |
|---|---|---|
| `OP_CHAR` | `char c` | 若当前字符与 `c` 匹配（受大小写标志影响），该线程存活并推入下一轮队列，`pc++`；否则线程淘汰。 |
| `OP_ANY` | 无 | 匹配任意字符（除 `\n`，`DOTALL` 下匹配 `\n`），线程存活推进。 |
| `OP_CLASS` | `uint32_t bitmap[8]`, `bool negate` | 256 位位图，利用字符的 ASCII 码作为位偏移直接索引判定，单步 $O(1)$。支持 `[^...]` 取反。 |
| `OP_BOL` | 无 | 零宽行首断言：要求当前处于输入起点 `pos == 0`，或前一字符为 `\n`（多行模式）。 |
| `OP_EOL` | 无 | 零宽行尾断言：要求当前处于输入终点 `text[pos] == '\0'`，或当前字符为 `\n`（多行模式）。 |
| `OP_WORD_BOUNDARY` | `bool negate` | 零宽单词边界 `\b`（或非边界 `\B`）：判断前后字符是否一个为 `\w` 另一个非 `\w`。 |
| `OP_JUMP` | `int32_t target` | 瞬时 $\epsilon$-跳转至绝对指令偏移 `target`。 |
| `OP_SPLIT` | `int32_t target1, int32_t target2` | 瞬时 $\epsilon$-分流，同时向状态集中派生分支。`target1` 具有更高优先级（用于贪婪量词），非贪婪时两分支对调。 |
| `OP_SAVE` | `uint8_t slot` | 瞬时记录当前输入位置 `pos` 到线程槽位 `slots[slot]`，随后 `pc++`。 |
| `OP_BACKREF` | `uint8_t group_idx` | 从当前线程的 `slots[2*group_idx]` 和 `slots[2*group_idx+1]` 取出已匹配子串，与输入进行逐字节比对，全等则推进对应长度，否则淘汰。 |
| `OP_MATCH` | 无 | 成功终结符。由于线程按优先级排序推进，首个命中 `OP_MATCH` 的线程即为全局最优最长贪婪匹配。 |

### 3.2 Pike 虚拟机执行流图示

```
+------------------------------------------------------------------------+
|                              Pike VM                                   |
|                                                                        |
|   输入字符 c_i                                                         |
|         │                                                              |
|         ▼                                                              |
|   clist (当前活跃线程集, 容量 M)                                       |
|   ┌──────────────────────────────────────────────────────────────┐     |
|   │ Thread 0: pc=3, slots=[0, 4, 1, 3]                           │     |
|   │ Thread 1: pc=7, slots=[0, 4, 0, 0]                           │     |
|   └──────────────────────────────────────────────────────────────┘     |
|         │                                                              |
|         │ 字符匹配 (OP_CHAR / OP_ANY / OP_CLASS / OP_BACKREF)          |
|         ▼                                                              |
|   nlist (下一轮候选线程集)                                             |
|         │                                                              |
|         │ 即时展开 ε-闭包 (OP_SPLIT, OP_JUMP, OP_SAVE, OP_BOL/EOL)     |
|         │ 依据 visited_gen[pc] 去重 (保留最高优先级分支)               |
|         ▼                                                              |
|   若命中 OP_MATCH -> 更新最佳匹配结果 (Best Candidate)                 |
|   clist = nlist, 清空 nlist, 读取下一个字符 c_{i+1}                    |
+------------------------------------------------------------------------+
```

---

## 4. 编译器前端与代码生成

### 4.1 语法解析与优先级
递归下降单遍解析器按标准算符优先级递归：
1. **分支选择（最低优先级）**：`|`
2. **串联（Concatenation）**：隐式序列连接
3. **量词重复（Quantifier）**：`*`, `+`, `?`, `{m,n}`, 及非贪婪后缀 `?`
4. **原子元素（最高优先级）**：字符、转义字符、字符集 `[...]`、括号子表达式 `(...)`

### 4.2 模版转换代码生成规则

1. **分支 `A | B`**：
   ```
   offset_0: OP_SPLIT (offset_0 + 1), L_B
             [编译 A]
             OP_JUMP L_END
   L_B:      [编译 B]
   L_END:    ...
   ```
2. **贪婪闭包 `A*`**：
   ```
   L_START:  OP_SPLIT L_BODY, L_END
   L_BODY:   [编译 A]
             OP_JUMP L_START
   L_END:    ...
   ```
3. **非贪婪闭包 `A*?`**：
   ```
   L_START:  OP_SPLIT L_END, L_BODY    /* 仅对调分支参数 */
   L_BODY:   [编译 A]
             OP_JUMP L_START
   L_END:    ...
   ```
4. **正闭包 `A+` 与 `A+?`**：
   ```
   L_BODY:   [编译 A]
             OP_SPLIT L_BODY, L_END    /* 非贪婪为 OP_SPLIT L_END, L_BODY */
   L_END:    ...
   ```
5. **可选匹配 `A?` 与 `A??`**：
   ```
   L_START:  OP_SPLIT L_BODY, L_END    /* 非贪婪为 OP_SPLIT L_END, L_BODY */
   L_BODY:   [编译 A]
   L_END:    ...
   ```
6. **范围量词 `A{m,n}`**：
   展开为 $m$ 次顺序 `[编译 A]`，后接 $n - m$ 次可选分支。严格约束 $n \le 128$，若超限则返回语法错误，防止恶意正则表达式导致字节码膨胀。
7. **捕获组 `(A)` 与非捕获组 `(?:A)`**：
   - 捕获组：分配槽位 $k \in [1, 15]$，在 `A` 前后分别插入 `OP_SAVE 2*k` 与 `OP_SAVE 2*k + 1`；
   - 非捕获组：直接编译 `A`，不分配任何槽位与 `OP_SAVE`。
8. **反向引用 `\k`**：
   直接生成 `OP_BACKREF k`（$1 \le k \le 15$）。若引用的分组 $k$ 尚未在前面定义，编译器直接报错中止。

---

## 5. 内存管理与执行优化

### 5.1 零堆分配栈上快径 (Stack Fast-Path)
对于绝大部分日常正则表达式，编译生成的指令数 $M \le 64$。
Pike VM 在执行搜索时，直接利用调用栈局部数组作为线程容器：
```c
#define COBALT_REGEX_STACK_THRESHOLD 64

pike_thread_t stack_threads[COBALT_REGEX_STACK_THRESHOLD * 2];
uint32_t      stack_gen[COBALT_REGEX_STACK_THRESHOLD];
```
- **$M \le 64$**：完全在栈上分配并运行，**零堆分配、零内存碎片、缓存局部性极致**。
- **$M > 64$**：在搜索入口处按 $2M$ 大小单次申请堆缓冲区，函数返回前干净释放。

### 5.2 自定义分配器契约
- 编译句柄 `cobalt_regex_t` 结构体及内部指令数组通过 `cobalt_allocator_t`（若传入）或 `malloc` 申请；
- 析构时保证递归全量释放，在 AddressSanitizer 与 LeakSanitizer 检测下保持 0 泄漏。

---

## 6. 测试与基准计划

### 6.1 单元测试用例设计 (`tests/unit/test_regex.c`)
1. **基础匹配**：单字符、多字符字面量、通配符 `.`、空串、首尾断言 `^` 与 `$`；
2. **量词体系**：`*`, `+`, `?` 及对应的非贪婪版本 `*?`, `+?`, `??`；
3. **字符类与简写**：`[a-z]`, `[0-9]`, `[^a-z]`, `\d`, `\D`, `\w`, `\W`, `\s`, `\S`, `\b`；
4. **分支与量词范围**：`cat|dog|fox`, `a{2,4}`, `a{3}`, `a{2,}`；
5. **捕获组与零拷贝切片**：
   - 提取完整匹配 `captures[0]`；
   - 多层嵌套捕获：`((a)(b(c)))` 逐级校验 `start`、`end` 与 `length`；
   - 日期时间模式：`(\d{4})-(\d{2})-(\d{2})`；
6. **反向引用校验**：
   - `([a-z]+)\s+\1` 成功匹配 `"hello hello"`，拒绝 `"hello world"`；
   - 标签匹配：`<([a-z]+)>.*?</\1>`；
7. **编译标志位**：大小写不敏感 `COBALT_REGEX_CASE_INSENSITIVE`、点号全匹配 `COBALT_REGEX_DOTALL`、多行 `COBALT_REGEX_MULTILINE`；
8. **语法防御**：不闭合括号、反向字符区间 `[z-a]`、悬空量词、未知转义、超范围反向引用等必须安全返回错误；
9. **自定义分配器注入**：验证分配器内存跟踪计数平衡。

### 6.2 抗 ReDoS 性能基准与吞吐量压测 (`tests/benchmark/benchmark_regex.c`)
1. **ReDoS 免疫压测**：
   - 模式：`^(a+)+$` 匹配连续 30 个 `a` 后接 `X`（`"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaX"`）；
   - 验证：Pike VM 在 $< 0.1$ 毫秒内瞬间完成判定，而传统回溯引擎在此模式下产生 $2^{30}$ 次计算挂死；
2. **工业负载吞吐量**：
   - IPv4 匹配提取（10MB 日志流）；
   - Email 格式批量验证（100,000 次）；
   - URL 路由解析（100,000 次）；
   - 测量指标：MB/s 与 ops/sec。
3. **集成至统一基准运行器**：注册入 `tests/benchmark/bench_runner.c`（支持文本与 `--json` 格式）。

---

## 7. 质量门禁与验收标准 (Quality Gate)

1. **代码规范**：严格遵守 Cobalt LLVM 格式规则（4 空格缩进、K&R 风格、指针右对齐 `char *ptr`、snake_case 命名）；
2. **编译器严苛检查**：在 `-Wall -Wextra -Werror` 下保持 **0 warning**；
3. **静态分析**：通过 `clang-tidy` 静态扫描与 `clang-format --dry-run -Werror` 格式检查；
4. **内存安全**：全量测试套件在 AddressSanitizer (ASan) 和 LeakSanitizer (LSan) 检测下 **0 内存泄漏、0 越界访问**；
5. **测试覆盖**：全量 CTest 套件通过率保持 100%。
