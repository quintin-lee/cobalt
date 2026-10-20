# Maintainability Split Assessment

Date: 2026-10-21
Scope: eventloop.c (1865 LOC), regex.c (1635 LOC), threadsafewrapper.c (1214 LOC)
Approach: Only split if clean boundaries exist; preserve APIs and behavior.

## Assessment
### threadsafewrapper.c

### threadsafewrapper.c
- Contains multiple independent wrapper types: tsvector, tshashmap, tslist, tsdeque, tsqueue (and related) each with their own struct + API
- Boundaries exist between different wrapper types (separate structs and function families), but they're heavily interleaved in one file
- Coupling between types appears low; each type has its own lock/state
- **Recommendation:** POTENTIALLY SPLITTABLE - could split by type into separate files (e.g., threadsafewrapper_vector.c, _hashmap.c, etc.), but need to ensure internal symbols stay accessible. Must preserve API in public header.

### eventloop.c
- Mixes signal handling, timer management, polling/event dispatch in one file
- Some logical separation exists (signals at top, timers in middle) but tightly coupled through eventloop struct
- **Recommendation:** DEFER - boundaries less clean, high coupling risk

### regex.c
- Mixes lexer/parser/AST/codegen/Pike VM in one file
- Phases somewhat identifiable but interdependent
- **Recommendation:** DEFER - complex interdependencies, risk outweighs benefit

## Decision
- **threadsafewrapper.c**: candidate for minimal split (type-based) if done carefully
- **eventloop.c/regex.c**: defer - no clean boundary with acceptable coupling
- Proceed with targeted split of threadsafewrapper.c only if it can be done without API changes and tests pass.
