/**
 * @file regex.c
 * @brief High-performance linear Pike VM regular expression engine
 */

#include "cobalt/algorithm/regex.h"
#include "cobalt/memory/allocator.h"
#include "cobalt/runtime/error.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Memory Allocator Wrappers                                                  */
/* -------------------------------------------------------------------------- */

static inline void *re_alloc(cobalt_allocator_t *alloc, size_t size)
{
    return alloc ? alloc->alloc(alloc, size) : malloc(size);
}

static inline void re_free(cobalt_allocator_t *alloc, void *ptr)
{
    if (alloc) {
        alloc->free(alloc, ptr);
    } else {
        free(ptr);
    }
}

static inline void *re_realloc(cobalt_allocator_t *alloc, void *ptr, size_t new_size)
{
    return alloc ? alloc->realloc(alloc, ptr, new_size) : realloc(ptr, new_size);
}

/* -------------------------------------------------------------------------- */
/* Bytecode Instruction Set                                                   */
/* -------------------------------------------------------------------------- */

typedef enum {
    OP_CHAR,
    OP_ANY,
    OP_CLASS,
    OP_BOL,
    OP_EOL,
    OP_WORD_BOUNDARY,
    OP_JUMP,
    OP_SPLIT,
    OP_SAVE,
    OP_BACKREF,
    OP_MATCH
} cobalt_opcode_t;

typedef struct {
    uint8_t opcode;
    uint8_t sub_op; /* slot for OP_SAVE, group_idx for OP_BACKREF, negate for OP_CLASS /
                       OP_WORD_BOUNDARY */
    uint16_t flags; /* flags: bit 0: case-insensitive, bit 1: match_newline */
    union {
        char ch;
        struct {
            uint32_t bitmap[8];
        } cls;
        struct {
            int32_t target;
        } jump;
        struct {
            int32_t target1;
            int32_t target2;
        } split;
    } u;
} cobalt_regex_inst_t;

struct cobalt_regex {
    cobalt_regex_inst_t *insts;
    size_t               inst_count;
    size_t               inst_capacity;
    size_t               capture_count;
    int                  flags;
    cobalt_allocator_t  *alloc;
    uint32_t             exact_pc;
    uint32_t             search_pc;
};

/* -------------------------------------------------------------------------- */
/* 256-Bit Character Class Bitmap Operations                                  */
/* -------------------------------------------------------------------------- */

static inline void bitmap_set(uint32_t bitmap[8], unsigned char c)
{
    bitmap[c >> 5] |= (1U << (c & 31));
}

static inline void
bitmap_set_range(uint32_t bitmap[8], unsigned char start, unsigned char end, bool ci)
{
    for (int c = (int)start; c <= (int)end; c++) {
        bitmap_set(bitmap, (unsigned char)c);
        if (ci) {
            if (c >= 'a' && c <= 'z') {
                bitmap_set(bitmap, (unsigned char)(c - 32));
            } else if (c >= 'A' && c <= 'Z') {
                bitmap_set(bitmap, (unsigned char)(c + 32));
            }
        }
    }
}

static inline void bitmap_set_digit(uint32_t bitmap[8])
{
    bitmap_set_range(bitmap, '0', '9', false);
}

static inline void bitmap_set_word(uint32_t bitmap[8])
{
    bitmap_set_range(bitmap, 'a', 'z', false);
    bitmap_set_range(bitmap, 'A', 'Z', false);
    bitmap_set_range(bitmap, '0', '9', false);
    bitmap_set(bitmap, '_');
}

static inline void bitmap_set_space(uint32_t bitmap[8])
{
    bitmap_set(bitmap, ' ');
    bitmap_set(bitmap, '\t');
    bitmap_set(bitmap, '\r');
    bitmap_set(bitmap, '\n');
    bitmap_set(bitmap, '\v');
    bitmap_set(bitmap, '\f');
}

static inline void bitmap_set_not_digit(uint32_t bitmap[8])
{
    uint32_t tmp[8] = {0};
    bitmap_set_digit(tmp);
    for (int i = 0; i < 8; i++) {
        bitmap[i] |= ~tmp[i];
    }
}

static inline void bitmap_set_not_word(uint32_t bitmap[8])
{
    uint32_t tmp[8] = {0};
    bitmap_set_word(tmp);
    for (int i = 0; i < 8; i++) {
        bitmap[i] |= ~tmp[i];
    }
}

static inline void bitmap_set_not_space(uint32_t bitmap[8])
{
    uint32_t tmp[8] = {0};
    bitmap_set_space(tmp);
    for (int i = 0; i < 8; i++) {
        bitmap[i] |= ~tmp[i];
    }
}

/* -------------------------------------------------------------------------- */
/* AST Data Structures & Node Pool                                            */
/* -------------------------------------------------------------------------- */

#define AST_NULL UINT32_MAX

typedef enum {
    AST_EMPTY,
    AST_CHAR,
    AST_ANY,
    AST_CLASS,
    AST_BOL,
    AST_EOL,
    AST_WORD_BOUNDARY,
    AST_CONCAT,
    AST_ALT,
    AST_QUANT,
    AST_GROUP,
    AST_BACKREF
} ast_type_t;

typedef struct {
    ast_type_t type;
    uint32_t   left;
    uint32_t   right;
    int32_t    min_reps;
    int32_t    max_reps; /* -1 for unbounded */
    bool       greedy;
    bool       negate;
    uint8_t    group_idx;
    char       ch;
    uint32_t   bitmap[8];
} ast_node_t;

typedef struct {
    ast_node_t         *nodes;
    size_t              count;
    size_t              capacity;
    cobalt_allocator_t *alloc;
    bool                oom;
} node_pool_t;

static bool node_pool_init(node_pool_t *pool, cobalt_allocator_t *alloc)
{
    pool->alloc    = alloc;
    pool->count    = 0;
    pool->capacity = 64;
    pool->oom      = false;
    pool->nodes    = (ast_node_t *)re_alloc(alloc, pool->capacity * sizeof(ast_node_t));
    if (!pool->nodes) {
        pool->oom = true;
        return false;
    }
    return true;
}

static void node_pool_destroy(node_pool_t *pool)
{
    if (pool->nodes) {
        re_free(pool->alloc, pool->nodes);
        pool->nodes = NULL;
    }
    pool->count    = 0;
    pool->capacity = 0;
}

static uint32_t node_alloc(node_pool_t *pool, ast_type_t type)
{
    if (pool->oom) {
        return AST_NULL;
    }
    if (pool->count >= pool->capacity) {
        size_t      new_cap = pool->capacity * 2;
        ast_node_t *new_nodes =
            (ast_node_t *)re_realloc(pool->alloc, pool->nodes, new_cap * sizeof(ast_node_t));
        if (!new_nodes) {
            pool->oom = true;
            return AST_NULL;
        }
        pool->nodes    = new_nodes;
        pool->capacity = new_cap;
    }
    uint32_t idx = (uint32_t)pool->count++;
    memset(&pool->nodes[idx], 0, sizeof(ast_node_t));
    pool->nodes[idx].type  = type;
    pool->nodes[idx].left  = AST_NULL;
    pool->nodes[idx].right = AST_NULL;
    return idx;
}

static uint32_t pool_add_empty(node_pool_t *pool)
{
    return node_alloc(pool, AST_EMPTY);
}

static uint32_t pool_add_char(node_pool_t *pool, char c)
{
    uint32_t idx = node_alloc(pool, AST_CHAR);
    if (idx != AST_NULL) {
        pool->nodes[idx].ch = c;
    }
    return idx;
}

static uint32_t pool_add_any(node_pool_t *pool)
{
    return node_alloc(pool, AST_ANY);
}

static uint32_t pool_add_class(node_pool_t *pool, const uint32_t bitmap[8], bool negate)
{
    uint32_t idx = node_alloc(pool, AST_CLASS);
    if (idx != AST_NULL) {
        memcpy(pool->nodes[idx].bitmap, bitmap, sizeof(pool->nodes[idx].bitmap));
        pool->nodes[idx].negate = negate;
    }
    return idx;
}

static uint32_t pool_add_bol(node_pool_t *pool)
{
    return node_alloc(pool, AST_BOL);
}

static uint32_t pool_add_eol(node_pool_t *pool)
{
    return node_alloc(pool, AST_EOL);
}

static uint32_t pool_add_boundary(node_pool_t *pool, bool negate)
{
    uint32_t idx = node_alloc(pool, AST_WORD_BOUNDARY);
    if (idx != AST_NULL) {
        pool->nodes[idx].negate = negate;
    }
    return idx;
}

static uint32_t pool_add_concat(node_pool_t *pool, uint32_t left, uint32_t right)
{
    uint32_t idx = node_alloc(pool, AST_CONCAT);
    if (idx != AST_NULL) {
        pool->nodes[idx].left  = left;
        pool->nodes[idx].right = right;
    }
    return idx;
}

static uint32_t pool_add_alt(node_pool_t *pool, uint32_t left, uint32_t right)
{
    uint32_t idx = node_alloc(pool, AST_ALT);
    if (idx != AST_NULL) {
        pool->nodes[idx].left  = left;
        pool->nodes[idx].right = right;
    }
    return idx;
}

static uint32_t
pool_add_quant(node_pool_t *pool, uint32_t child, int32_t min_reps, int32_t max_reps, bool greedy)
{
    uint32_t idx = node_alloc(pool, AST_QUANT);
    if (idx != AST_NULL) {
        pool->nodes[idx].left     = child;
        pool->nodes[idx].min_reps = min_reps;
        pool->nodes[idx].max_reps = max_reps;
        pool->nodes[idx].greedy   = greedy;
    }
    return idx;
}

static uint32_t pool_add_group(node_pool_t *pool, uint32_t child, uint8_t group_idx)
{
    uint32_t idx = node_alloc(pool, AST_GROUP);
    if (idx != AST_NULL) {
        pool->nodes[idx].left      = child;
        pool->nodes[idx].group_idx = group_idx;
    }
    return idx;
}

static uint32_t pool_add_backref(node_pool_t *pool, uint8_t group_idx)
{
    uint32_t idx = node_alloc(pool, AST_BACKREF);
    if (idx != AST_NULL) {
        pool->nodes[idx].group_idx = group_idx;
    }
    return idx;
}

/* -------------------------------------------------------------------------- */
/* Recursive-Descent Lexer & Parser                                           */
/* -------------------------------------------------------------------------- */

typedef struct {
    const char         *pattern;
    size_t              len;
    size_t              pos;
    int                 flags;
    size_t              group_count;
    cobalt_allocator_t *alloc;
    cobalt_error_t      last_error;
} parser_t;

static inline char peek(const parser_t *p)
{
    return (p->pos < p->len) ? p->pattern[p->pos] : '\0';
}

static inline char peek2(const parser_t *p)
{
    return (p->pos + 1 < p->len) ? p->pattern[p->pos + 1] : '\0';
}

static inline char consume(parser_t *p)
{
    return (p->pos < p->len) ? p->pattern[p->pos++] : '\0';
}

static uint32_t parse_expr(parser_t *p, node_pool_t *pool);

static uint32_t parse_class(parser_t *p, node_pool_t *pool)
{
    /* '[' is already consumed */
    bool negate = false;
    if (peek(p) == '^') {
        negate = true;
        consume(p);
    }

    uint32_t bitmap[8] = {0};
    bool     is_ci     = (p->flags & COBALT_REGEX_CASE_INSENSITIVE) != 0;

    if (peek(p) == ']') {
        /* Leading ']' treated as literal ']' */
        char c = consume(p);
        bitmap_set_range(bitmap, (unsigned char)c, (unsigned char)c, is_ci);
    }

    while (peek(p) != ']' && peek(p) != '\0') {
        char c = consume(p);
        if (c == '\\') {
            if (peek(p) == '\0') {
                p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
                return AST_NULL;
            }
            char esc = consume(p);
            if (esc == 'd') {
                bitmap_set_digit(bitmap);
            } else if (esc == 'D') {
                bitmap_set_not_digit(bitmap);
            } else if (esc == 'w') {
                bitmap_set_word(bitmap);
            } else if (esc == 'W') {
                bitmap_set_not_word(bitmap);
            } else if (esc == 's') {
                bitmap_set_space(bitmap);
            } else if (esc == 'S') {
                bitmap_set_not_space(bitmap);
            } else if (esc == 'n') {
                bitmap_set_range(bitmap, '\n', '\n', is_ci);
            } else if (esc == 'r') {
                bitmap_set_range(bitmap, '\r', '\r', is_ci);
            } else if (esc == 't') {
                bitmap_set_range(bitmap, '\t', '\t', is_ci);
            } else if (esc == 'v') {
                bitmap_set_range(bitmap, '\v', '\v', is_ci);
            } else if (esc == 'f') {
                bitmap_set_range(bitmap, '\f', '\f', is_ci);
            } else if (esc == '0') {
                bitmap_set_range(bitmap, '\0', '\0', is_ci);
            } else {
                bitmap_set_range(bitmap, (unsigned char)esc, (unsigned char)esc, is_ci);
            }
        } else {
            /* Check for range: c - c2 */
            if (peek(p) == '-' && peek2(p) != ']' && peek2(p) != '\0') {
                consume(p); /* consume '-' */
                char c2 = consume(p);
                if (c2 == '\\') {
                    if (peek(p) == '\0') {
                        p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
                        return AST_NULL;
                    }
                    c2 = consume(p);
                }
                if ((unsigned char)c > (unsigned char)c2) {
                    /* Invalid range, e.g. [z-a] */
                    p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
                    return AST_NULL;
                }
                bitmap_set_range(bitmap, (unsigned char)c, (unsigned char)c2, is_ci);
            } else {
                bitmap_set_range(bitmap, (unsigned char)c, (unsigned char)c, is_ci);
            }
        }
    }

    if (peek(p) != ']') {
        /* Unclosed bracket */
        p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
        return AST_NULL;
    }
    consume(p); /* consume ']' */

    return pool_add_class(pool, bitmap, negate);
}

static bool parse_quant_braces(parser_t *p, int32_t *out_min, int32_t *out_max)
{
    size_t start_pos = p->pos;
    consume(p); /* consume '{' */

    if (!isdigit((unsigned char)peek(p))) {
        p->pos = start_pos;
        return false;
    }

    long min_val = 0;
    while (isdigit((unsigned char)peek(p))) {
        min_val = min_val * 10 + (consume(p) - '0');
        if (min_val > 128) {
            return false;
        }
    }

    long max_val = min_val;
    if (peek(p) == ',') {
        consume(p); /* consume ',' */
        if (peek(p) == '}') {
            max_val = -1; /* unbounded {m,} */
        } else if (isdigit((unsigned char)peek(p))) {
            max_val = 0;
            while (isdigit((unsigned char)peek(p))) {
                max_val = max_val * 10 + (consume(p) - '0');
                if (max_val > 128) {
                    return false;
                }
            }
            if (max_val < min_val) {
                return false; /* e.g. {5,2} */
            }
        } else {
            return false;
        }
    }

    if (peek(p) != '}') {
        return false;
    }
    consume(p); /* consume '}' */

    *out_min = (int32_t)min_val;
    *out_max = (int32_t)max_val;
    return true;
}

static uint32_t parse_atom(parser_t *p, node_pool_t *pool)
{
    char c = peek(p);
    if (c == '\0') {
        return AST_NULL;
    }

    /* Dangling quantifiers */
    if (c == '*' || c == '+' || c == '?' || c == '{') {
        p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
        return AST_NULL;
    }

    if (c == '^') {
        consume(p);
        return pool_add_bol(pool);
    }
    if (c == '$') {
        consume(p);
        return pool_add_eol(pool);
    }
    if (c == '.') {
        consume(p);
        return pool_add_any(pool);
    }

    if (c == '(') {
        consume(p);
        if (peek(p) == '?' && peek2(p) == ':') {
            consume(p); /* '?' */
            consume(p); /* ':' */
            uint32_t body = parse_expr(p, pool);
            if (p->last_error != COBALT_SUCCESS || peek(p) != ')') {
                p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
                return AST_NULL;
            }
            consume(p); /* ')' */
            return body;
        }

        /* Capturing group */
        p->group_count++;
        if (p->group_count >= COBALT_REGEX_MAX_CAPTURES) {
            p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
            return AST_NULL;
        }
        uint8_t  gid  = (uint8_t)p->group_count;
        uint32_t body = parse_expr(p, pool);
        if (p->last_error != COBALT_SUCCESS || peek(p) != ')') {
            p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
            return AST_NULL;
        }
        consume(p); /* ')' */
        return pool_add_group(pool, body, gid);
    }

    if (c == ')') {
        /* Unmatched closing parenthesis */
        p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
        return AST_NULL;
    }

    if (c == '[') {
        consume(p);
        return parse_class(p, pool);
    }

    if (c == '\\') {
        consume(p); /* consume '\' */
        if (peek(p) == '\0') {
            p->last_error = COBALT_ERROR_INVALID_ARGUMENT; /* Trailing escape */
            return AST_NULL;
        }
        char esc = consume(p);
        if (esc >= '1' && esc <= '9') {
            uint8_t gid = (uint8_t)(esc - '0');
            if (isdigit((unsigned char)peek(p))) {
                uint8_t next_gid = (uint8_t)(gid * 10 + (peek(p) - '0'));
                if (next_gid <= p->group_count) {
                    consume(p);
                    gid = next_gid;
                }
            }
            if (gid > p->group_count) {
                /* Undefined backreference */
                p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
                return AST_NULL;
            }
            return pool_add_backref(pool, gid);
        }
        if (esc == 'b') {
            return pool_add_boundary(pool, false);
        }
        if (esc == 'B') {
            return pool_add_boundary(pool, true);
        }
        if (esc == 'd' || esc == 'D') {
            uint32_t bm[8] = {0};
            bitmap_set_digit(bm);
            return pool_add_class(pool, bm, esc == 'D');
        }
        if (esc == 'w' || esc == 'W') {
            uint32_t bm[8] = {0};
            bitmap_set_word(bm);
            return pool_add_class(pool, bm, esc == 'W');
        }
        if (esc == 's' || esc == 'S') {
            uint32_t bm[8] = {0};
            bitmap_set_space(bm);
            return pool_add_class(pool, bm, esc == 'S');
        }
        if (esc == 'n') {
            return pool_add_char(pool, '\n');
        }
        if (esc == 'r') {
            return pool_add_char(pool, '\r');
        }
        if (esc == 't') {
            return pool_add_char(pool, '\t');
        }
        if (esc == 'v') {
            return pool_add_char(pool, '\v');
        }
        if (esc == 'f') {
            return pool_add_char(pool, '\f');
        }
        if (esc == '0') {
            return pool_add_char(pool, '\0');
        }
        return pool_add_char(pool, esc);
    }

    consume(p);
    return pool_add_char(pool, c);
}

static uint32_t parse_factor(parser_t *p, node_pool_t *pool)
{
    uint32_t atom = parse_atom(p, pool);
    if (atom == AST_NULL) {
        return AST_NULL;
    }

    char c = peek(p);
    if (c == '*' || c == '+' || c == '?' || c == '{') {
        int32_t min_reps = 0;
        int32_t max_reps = 0;
        bool    greedy   = true;

        if (c == '*') {
            consume(p);
            min_reps = 0;
            max_reps = -1;
        } else if (c == '+') {
            consume(p);
            min_reps = 1;
            max_reps = -1;
        } else if (c == '?') {
            consume(p);
            min_reps = 0;
            max_reps = 1;
        } else if (c == '{') {
            if (!parse_quant_braces(p, &min_reps, &max_reps)) {
                p->last_error = COBALT_ERROR_INVALID_ARGUMENT;
                return AST_NULL;
            }
        }

        if (peek(p) == '?') {
            consume(p);
            greedy = false;
        }

        return pool_add_quant(pool, atom, min_reps, max_reps, greedy);
    }

    return atom;
}

static uint32_t parse_term(parser_t *p, node_pool_t *pool)
{
    uint32_t root = AST_NULL;

    while (peek(p) != '\0' && peek(p) != '|' && peek(p) != ')') {
        uint32_t factor = parse_factor(p, pool);
        if (factor == AST_NULL) {
            if (p->last_error != COBALT_SUCCESS) {
                return AST_NULL;
            }
            break;
        }
        if (root == AST_NULL) {
            root = factor;
        } else {
            root = pool_add_concat(pool, root, factor);
        }
    }

    if (root == AST_NULL) {
        root = pool_add_empty(pool);
    }
    return root;
}

static uint32_t parse_expr(parser_t *p, node_pool_t *pool)
{
    uint32_t left = parse_term(p, pool);
    if (left == AST_NULL && p->last_error != COBALT_SUCCESS) {
        return AST_NULL;
    }

    while (peek(p) == '|') {
        consume(p);
        uint32_t right = parse_term(p, pool);
        if (right == AST_NULL && p->last_error != COBALT_SUCCESS) {
            return AST_NULL;
        }
        left = pool_add_alt(pool, left, right);
    }

    return left;
}

/* -------------------------------------------------------------------------- */
/* Bytecode Instruction Generator                                             */
/* -------------------------------------------------------------------------- */

typedef struct {
    cobalt_regex_inst_t *insts;
    size_t               count;
    size_t               capacity;
    cobalt_allocator_t  *alloc;
    bool                 oom;
} codegen_t;

static uint32_t emit_inst(codegen_t *cg, cobalt_opcode_t op)
{
    if (cg->oom) {
        return 0;
    }
    if (cg->count >= cg->capacity) {
        size_t               new_cap   = cg->capacity ? cg->capacity * 2 : 32;
        cobalt_regex_inst_t *new_insts = (cobalt_regex_inst_t *)re_realloc(
            cg->alloc, cg->insts, new_cap * sizeof(cobalt_regex_inst_t));
        if (!new_insts) {
            cg->oom = true;
            return 0;
        }
        cg->insts    = new_insts;
        cg->capacity = new_cap;
    }
    uint32_t idx = (uint32_t)cg->count++;
    memset(&cg->insts[idx], 0, sizeof(cobalt_regex_inst_t));
    cg->insts[idx].opcode = (uint8_t)op;
    return idx;
}

static void compile_node(codegen_t *cg, const node_pool_t *pool, uint32_t node_idx, int flags)
{
    if (node_idx == AST_NULL || cg->oom) {
        return;
    }
    const ast_node_t *node = &pool->nodes[node_idx];

    switch (node->type) {
    case AST_EMPTY:
        break;

    case AST_CHAR: {
        uint32_t pc        = emit_inst(cg, OP_CHAR);
        cg->insts[pc].u.ch = node->ch;
        if (flags & COBALT_REGEX_CASE_INSENSITIVE) {
            cg->insts[pc].flags |= 1U;
        }
        break;
    }

    case AST_ANY: {
        uint32_t pc = emit_inst(cg, OP_ANY);
        if (flags & COBALT_REGEX_DOTALL) {
            cg->insts[pc].flags |= 1U;
        }
        break;
    }

    case AST_CLASS: {
        uint32_t pc          = emit_inst(cg, OP_CLASS);
        cg->insts[pc].sub_op = node->negate ? 1 : 0;
        memcpy(cg->insts[pc].u.cls.bitmap, node->bitmap, sizeof(node->bitmap));
        break;
    }

    case AST_BOL:
        emit_inst(cg, OP_BOL);
        break;

    case AST_EOL:
        emit_inst(cg, OP_EOL);
        break;

    case AST_WORD_BOUNDARY: {
        uint32_t pc          = emit_inst(cg, OP_WORD_BOUNDARY);
        cg->insts[pc].sub_op = node->negate ? 1 : 0;
        break;
    }

    case AST_BACKREF: {
        uint32_t pc          = emit_inst(cg, OP_BACKREF);
        cg->insts[pc].sub_op = node->group_idx;
        if (flags & COBALT_REGEX_CASE_INSENSITIVE) {
            cg->insts[pc].flags |= 1U;
        }
        break;
    }

    case AST_GROUP: {
        uint32_t s1          = emit_inst(cg, OP_SAVE);
        cg->insts[s1].sub_op = (uint8_t)(2 * node->group_idx);

        compile_node(cg, pool, node->left, flags);

        uint32_t s2          = emit_inst(cg, OP_SAVE);
        cg->insts[s2].sub_op = (uint8_t)(2 * node->group_idx + 1);
        break;
    }

    case AST_CONCAT:
        compile_node(cg, pool, node->left, flags);
        compile_node(cg, pool, node->right, flags);
        break;

    case AST_ALT: {
        uint32_t split_pc = emit_inst(cg, OP_SPLIT);
        uint32_t body_a   = (uint32_t)cg->count;
        compile_node(cg, pool, node->left, flags);
        uint32_t jump_pc = emit_inst(cg, OP_JUMP);
        uint32_t body_b  = (uint32_t)cg->count;
        compile_node(cg, pool, node->right, flags);
        uint32_t end_pc = (uint32_t)cg->count;

        cg->insts[split_pc].u.split.target1 = (int32_t)body_a;
        cg->insts[split_pc].u.split.target2 = (int32_t)body_b;
        cg->insts[jump_pc].u.jump.target    = (int32_t)end_pc;
        break;
    }

    case AST_QUANT: {
        int32_t min_r  = node->min_reps;
        int32_t max_r  = node->max_reps;
        bool    greedy = node->greedy;

        if (min_r == 0 && max_r == 1) {
            uint32_t split_pc = emit_inst(cg, OP_SPLIT);
            uint32_t body_pc  = (uint32_t)cg->count;
            compile_node(cg, pool, node->left, flags);
            uint32_t end_pc = (uint32_t)cg->count;

            if (greedy) {
                cg->insts[split_pc].u.split.target1 = (int32_t)body_pc;
                cg->insts[split_pc].u.split.target2 = (int32_t)end_pc;
            } else {
                cg->insts[split_pc].u.split.target1 = (int32_t)end_pc;
                cg->insts[split_pc].u.split.target2 = (int32_t)body_pc;
            }
        } else if (min_r == 0 && max_r == -1) {
            uint32_t start_pc = (uint32_t)cg->count;
            uint32_t split_pc = emit_inst(cg, OP_SPLIT);
            uint32_t body_pc  = (uint32_t)cg->count;
            compile_node(cg, pool, node->left, flags);
            uint32_t jump_pc                 = emit_inst(cg, OP_JUMP);
            cg->insts[jump_pc].u.jump.target = (int32_t)start_pc;
            uint32_t end_pc                  = (uint32_t)cg->count;

            if (greedy) {
                cg->insts[split_pc].u.split.target1 = (int32_t)body_pc;
                cg->insts[split_pc].u.split.target2 = (int32_t)end_pc;
            } else {
                cg->insts[split_pc].u.split.target1 = (int32_t)end_pc;
                cg->insts[split_pc].u.split.target2 = (int32_t)body_pc;
            }
        } else if (min_r == 1 && max_r == -1) {
            uint32_t start_pc = (uint32_t)cg->count;
            compile_node(cg, pool, node->left, flags);
            uint32_t split_pc = emit_inst(cg, OP_SPLIT);
            uint32_t end_pc   = (uint32_t)cg->count;

            if (greedy) {
                cg->insts[split_pc].u.split.target1 = (int32_t)start_pc;
                cg->insts[split_pc].u.split.target2 = (int32_t)end_pc;
            } else {
                cg->insts[split_pc].u.split.target1 = (int32_t)end_pc;
                cg->insts[split_pc].u.split.target2 = (int32_t)start_pc;
            }
        } else {
            for (int32_t i = 0; i < min_r; i++) {
                compile_node(cg, pool, node->left, flags);
            }
            if (max_r == -1) {
                uint32_t start_pc = (uint32_t)cg->count;
                uint32_t split_pc = emit_inst(cg, OP_SPLIT);
                uint32_t body_pc  = (uint32_t)cg->count;
                compile_node(cg, pool, node->left, flags);
                uint32_t jump_pc                 = emit_inst(cg, OP_JUMP);
                cg->insts[jump_pc].u.jump.target = (int32_t)start_pc;
                uint32_t end_pc                  = (uint32_t)cg->count;

                if (greedy) {
                    cg->insts[split_pc].u.split.target1 = (int32_t)body_pc;
                    cg->insts[split_pc].u.split.target2 = (int32_t)end_pc;
                } else {
                    cg->insts[split_pc].u.split.target1 = (int32_t)end_pc;
                    cg->insts[split_pc].u.split.target2 = (int32_t)body_pc;
                }
            } else if (max_r > min_r) {
                int32_t  opt_count = max_r - min_r;
                uint32_t split_pcs[128];
                for (int32_t i = 0; i < opt_count; i++) {
                    split_pcs[i]     = emit_inst(cg, OP_SPLIT);
                    uint32_t body_pc = (uint32_t)cg->count;
                    compile_node(cg, pool, node->left, flags);
                    if (greedy) {
                        cg->insts[split_pcs[i]].u.split.target1 = (int32_t)body_pc;
                    } else {
                        cg->insts[split_pcs[i]].u.split.target2 = (int32_t)body_pc;
                    }
                }
                uint32_t end_pc = (uint32_t)cg->count;
                for (int32_t i = 0; i < opt_count; i++) {
                    if (greedy) {
                        cg->insts[split_pcs[i]].u.split.target2 = (int32_t)end_pc;
                    } else {
                        cg->insts[split_pcs[i]].u.split.target1 = (int32_t)end_pc;
                    }
                }
            }
        }
        break;
    }
    }
}

static bool ast_is_anchored_start(const node_pool_t *pool, uint32_t node_idx)
{
    if (node_idx == AST_NULL) {
        return false;
    }
    const ast_node_t *node = &pool->nodes[node_idx];
    if (node->type == AST_BOL) {
        return true;
    }
    if (node->type == AST_CONCAT) {
        return ast_is_anchored_start(pool, node->left);
    }
    if (node->type == AST_GROUP) {
        return ast_is_anchored_start(pool, node->left);
    }
    return false;
}

/* -------------------------------------------------------------------------- */
/* Public API Implementation                                                  */
/* -------------------------------------------------------------------------- */

cobalt_regex_t *cobalt_regex_compile(const char *pattern, int flags)
{
    return cobalt_regex_compile_with_alloc(pattern, flags, NULL);
}

cobalt_regex_t *
cobalt_regex_compile_with_alloc(const char *pattern, int flags, cobalt_allocator_t *alloc)
{
    if (!pattern) {
        cobalt_error_set(NULL, COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }

    parser_t p;
    p.pattern     = pattern;
    p.len         = strlen(pattern);
    p.pos         = 0;
    p.flags       = flags;
    p.group_count = 0;
    p.alloc       = alloc;
    p.last_error  = COBALT_SUCCESS;

    node_pool_t pool;
    if (!node_pool_init(&pool, alloc)) {
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    uint32_t root = parse_expr(&p, &pool);

    if (p.last_error != COBALT_SUCCESS || p.pos < p.len || pool.oom) {
        node_pool_destroy(&pool);
        cobalt_error_set(NULL,
                         pool.oom ? COBALT_ERROR_OUT_OF_MEMORY : COBALT_ERROR_INVALID_ARGUMENT);
        return NULL;
    }

    codegen_t cg;
    cg.insts    = NULL;
    cg.count    = 0;
    cg.capacity = 0;
    cg.alloc    = alloc;
    cg.oom      = false;

    bool     anchored  = ast_is_anchored_start(&pool, root) && !(flags & COBALT_REGEX_MULTILINE);
    uint32_t search_pc = 0;
    uint32_t exact_pc  = 0;

    if (!anchored) {
        /* Unanchored search prefix:
         * 0: OP_SPLIT exact_pc, 1
         * 1: OP_ANY (flags |= 1 for match newline)
         * 2: OP_JUMP 0
         */
        uint32_t s_pc   = emit_inst(&cg, OP_SPLIT);
        uint32_t any_pc = emit_inst(&cg, OP_ANY);
        cg.insts[any_pc].flags |= 1U; /* match newline */
        uint32_t j_pc                  = emit_inst(&cg, OP_JUMP);
        cg.insts[j_pc].u.jump.target   = 0;
        exact_pc                       = (uint32_t)cg.count;
        cg.insts[s_pc].u.split.target1 = (int32_t)exact_pc;
        cg.insts[s_pc].u.split.target2 = (int32_t)any_pc;
        search_pc                      = 0;
    } else {
        search_pc = 0;
        exact_pc  = 0;
    }

    /* Emit save 0 (start of whole match) */
    uint32_t save0         = emit_inst(&cg, OP_SAVE);
    cg.insts[save0].sub_op = 0;

    /* Compile root AST */
    compile_node(&cg, &pool, root, flags);

    /* Emit save 1 (end of whole match) */
    uint32_t save1         = emit_inst(&cg, OP_SAVE);
    cg.insts[save1].sub_op = 1;

    /* Emit match terminator */
    emit_inst(&cg, OP_MATCH);

    node_pool_destroy(&pool);

    if (cg.oom) {
        if (cg.insts) {
            re_free(alloc, cg.insts);
        }
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    cobalt_regex_t *re = (cobalt_regex_t *)re_alloc(alloc, sizeof(cobalt_regex_t));
    if (!re) {
        if (cg.insts) {
            re_free(alloc, cg.insts);
        }
        cobalt_error_set(NULL, COBALT_ERROR_OUT_OF_MEMORY);
        return NULL;
    }

    re->insts         = cg.insts;
    re->inst_count    = cg.count;
    re->inst_capacity = cg.capacity;
    re->capture_count = p.group_count;
    re->flags         = flags;
    re->alloc         = alloc;
    re->exact_pc      = exact_pc;
    re->search_pc     = search_pc;

    return re;
}

void cobalt_regex_destroy(cobalt_regex_t *re)
{
    if (!re) {
        return;
    }
    if (re->insts) {
        re_free(re->alloc, re->insts);
        re->insts = NULL;
    }
    re_free(re->alloc, re);
}

size_t cobalt_regex_capture_count(const cobalt_regex_t *re)
{
    return re ? re->capture_count : 0;
}

int cobalt_regex_search(const cobalt_regex_t   *re,
                        const char             *text,
                        cobalt_regex_capture_t *captures,
                        size_t                  max_captures)
{
    (void)re;
    (void)text;
    (void)captures;
    (void)max_captures;
    return -1;
}

int cobalt_regex_match_exact(const cobalt_regex_t   *re,
                             const char             *text,
                             cobalt_regex_capture_t *captures,
                             size_t                  max_captures)
{
    (void)re;
    (void)text;
    (void)captures;
    (void)max_captures;
    return -1;
}

int cobalt_regex_test(const char *pattern, const char *text)
{
    (void)pattern;
    (void)text;
    return -1;
}
