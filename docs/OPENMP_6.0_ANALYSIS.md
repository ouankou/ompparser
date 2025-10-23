# OpenMP 6.0 Support Analysis

## Overview

This document analyzes the OpenMP 6.0 specification support across three implementations:
- **OpenMP 6.0 Specification** (November 2024 release)
- **roup** (Rust-based parser claiming full OpenMP 6.0 support)
- **ompparser** (Current C++ implementation)

## Executive Summary

**OpenMP 6.0 Status:**
- Released: November 2024
- Full specification: ~550+ pages
- Focus: Heterogeneous computing, AI/ML integration, task graphs, better device support

**roup Implementation:**
- **Directives**: 263 variants (including all combinations and Fortran variants)
- **Clauses**: 166 clause entries (including modifiers)
- **OpenMP 6.0 Coverage**: Claims full support to OpenMP 6.0

**ompparser Current Status:**
- **Directives**: 88 directives
- **Clauses**: 91 clauses
- **OpenMP Version**: Primarily OpenMP 5.0 with some 5.1/5.2 features
- **OpenMP 6.0 Coverage**: Partial (missing major 6.0 additions)

---

## OpenMP 6.0 Major New Features

### 1. Task Programming Enhancements
- **taskgraph**: Record and replay task graphs for efficient execution
- **task_iteration**: Subsidiary directive for task iteration control
- **graph_id** / **graph_reset** clauses: Task graph management
- **transparent** clause: Enable dependencies between transparent tasks
- **replayable** clause: Mark tasks as replayable

### 2. New Loop Transformation Directives
- **fuse**: Loop fusion optimization
- **interchange**: Loop interchange with permutation control
- **reverse**: Reverse loop iteration direction
- **split**: Split loops at specified points
- **stripe**: Stripe/block distribution
- **workdistribute**: Work distribution for Fortran array notation

### 3. Enhanced Device/Heterogeneous Computing
- **dispatch**: Function dispatch with interoperability support
- **interop**: Interoperability with CUDA, HIP, SYCL, etc.
- **local** clause: Local device variables
- **indirect** clause: Indirect device function pointers
- **device_safesync** clause: Safe device synchronization
- **safesync** clause: Safe synchronization

### 4. Scope and Data Environment
- **scope**: Worksharing scope construct (5.1)
- **groupprivate**: Group-private variables (6.0)
- **local** clause: Local data on target devices
- **looprange** clause: Loop range specification

### 5. Execution Control
- **masked**: Replacement for deprecated `master` (5.1)
- **masked taskloop** / **masked taskloop simd**: Combined masked constructs
- **error**: Compile-time and runtime error directive (5.1)
- **nothing**: No-op directive for metadirectives (5.1)
- **filter** clause: Control masked execution

### 6. Metaprogramming and Traits
- **assume** / **assumes**: Optimization assumptions (5.2)
- **begin assumes** / **end assumes**: Delimited assumption regions
- **absent** / **contains** / **holds** clauses: Trait specifications
- **no_openmp** / **no_openmp_constructs** / **no_openmp_routines** clauses
- **no_parallelism** clause: Declare no parallelism

### 7. Synchronization and Dependencies
- **doacross** clause: Replacement for `ordered(depend)` (5.2)
- **init** / **init_complete** clauses: Initialization control
- **memscope** clause: Memory scope specification
- **compare** / **fail** / **weak** clauses: Atomic compare-exchange

### 8. Advanced Reduction and Induction
- **declare_induction**: User-defined induction operations
- **induction** clause: Specify induction variables
- **inductor** / **collector** / **combiner** clauses: Induction components

### 9. Function Variants and Dispatch
- **adjust_args** / **append_args** clauses: Argument manipulation
- **apply** clause: Apply trait selectors
- **nocontext** / **novariants** clauses: Control dispatch behavior

### 10. Error Handling and Diagnostics
- **error** directive: User-defined errors
- **at** clause: Specify error timing (compile/execution)
- **severity** clause: Error severity (fatal/warning)
- **message** clause: Custom error messages

### 11. Memory Management
- **allocators**: Allocator construct (5.0/5.1)
- **uses_allocators** clause enhancements

---

## Missing Features in ompparser

### Critical Missing Directives (OpenMP 6.0)

| Directive | OpenMP Version | Description | Priority |
|-----------|---------------|-------------|----------|
| `taskgraph` | 6.0 | Task graph recording and replay | HIGH |
| `task_iteration` | 6.0 | Task iteration control | HIGH |
| `dispatch` | 6.0 | Function dispatch with interop | HIGH |
| `interop` | 5.1/6.0 | Interoperability with other APIs | HIGH |
| `groupprivate` | 6.0 | Group-private variables | HIGH |
| `workdistribute` | 6.0 | Fortran array work distribution | HIGH |
| `scope` | 5.1 | Worksharing scope | HIGH |
| `masked` | 5.1 | Replacement for master | HIGH |
| `masked taskloop` | 5.1 | Combined masked+taskloop | MEDIUM |
| `masked taskloop simd` | 5.1 | Combined masked+taskloop+simd | MEDIUM |
| `parallel masked` | 5.1 | Combined parallel+masked | MEDIUM |
| `parallel masked taskloop` | 5.1 | Combined parallel+masked+taskloop | MEDIUM |
| `parallel masked taskloop simd` | 5.1 | Combined parallel+masked+taskloop+simd | MEDIUM |
| `error` | 5.1 | User error directive | MEDIUM |
| `nothing` | 5.1 | No-op for metadirectives | MEDIUM |
| `fuse` | 6.0 | Loop fusion | MEDIUM |
| `interchange` | 6.0 | Loop interchange | MEDIUM |
| `reverse` | 6.0 | Reverse loop iteration | MEDIUM |
| `split` | 6.0 | Loop splitting | MEDIUM |
| `stripe` | 6.0 | Loop striping | MEDIUM |
| `allocators` | 5.0/5.1 | Memory allocator construct | LOW |
| `assume` | 5.2 | Block assumptions | LOW |
| `assumes` | 5.2 | Unassociated assumptions | LOW |
| `begin assumes` | 5.2 | Delimited assumptions start | LOW |
| `end assumes` | 5.2 | Delimited assumptions end | LOW |
| `begin metadirective` | 6.0 | Delimited metadirective start | LOW |

### Missing Combined Directives with `loop`/`loop simd` Variants

| Directive | Status in ompparser |
|-----------|---------------------|
| `parallel loop` | ✅ Exists |
| `parallel loop simd` | ❌ Missing |
| `teams loop` | ✅ Exists |
| `teams loop simd` | ❌ Missing |
| `target loop` | ❌ Missing |
| `target loop simd` | ❌ Missing |
| `target parallel loop` | ✅ Exists |
| `target parallel loop simd` | ❌ Missing |
| `target teams loop` | ✅ Exists |
| `target teams loop simd` | ❌ Missing |
| `distribute parallel loop` | ❌ Missing |
| `distribute parallel loop simd` | ❌ Missing |
| `teams distribute parallel loop` | ❌ Missing |
| `teams distribute parallel loop simd` | ❌ Missing |
| `target teams distribute parallel loop` | ❌ Missing |
| `target teams distribute parallel loop simd` | ❌ Missing |

### Critical Missing Clauses (OpenMP 6.0)

| Clause | OpenMP Version | Description | Priority |
|--------|---------------|-------------|----------|
| `graph_id(id)` | 6.0 | Task graph identifier | HIGH |
| `graph_reset` | 6.0 | Reset task graph | HIGH |
| `transparent` | 6.0 | Transparent task dependency | HIGH |
| `doacross(type:vec)` | 5.2 | Doacross dependencies | HIGH |
| `filter(thread-num)` | 5.1 | Masked filter | HIGH |
| `local(list)` | 6.0 | Local device variables | HIGH |
| `indirect` | 6.0 | Indirect device function | HIGH |
| `interop(expr)` | 5.1/6.0 | Interop object | HIGH |
| `init(expr)` | 6.0 | Initialization | HIGH |
| `init_complete` | 6.0 | Init complete notification | MEDIUM |
| `at(compile\|execution)` | 5.1 | Error timing | MEDIUM |
| `severity(fatal\|warning)` | 5.1 | Error severity | MEDIUM |
| `message(string)` | 5.1 | Error message | MEDIUM |
| `otherwise` | 6.0 | Metadirective default | MEDIUM |
| `compare` | 5.1 | Atomic compare | MEDIUM |
| `fail(seq_cst\|...)` | 5.1 | Atomic failure order | MEDIUM |
| `weak` | 5.1 | Weak atomic | MEDIUM |
| `memscope(...)` | 6.0 | Memory scope | MEDIUM |
| `looprange(...)` | 6.0 | Loop range spec | MEDIUM |
| `threadset(...)` | 6.0 | Thread set | MEDIUM |
| `safesync` | 6.0 | Safe synchronization | MEDIUM |
| `device_safesync` | 6.0 | Device safe sync | MEDIUM |
| `replayable` | 6.0 | Replayable task | MEDIUM |
| `induction(...)` | 6.0 | Induction variable | MEDIUM |
| `inductor(...)` | 6.0 | Induction operator | LOW |
| `collector(...)` | 6.0 | Collection operator | LOW |
| `combiner(...)` | 6.0 | Combiner operator | LOW |
| `adjust_args(...)` | 6.0 | Adjust variant args | LOW |
| `append_args(...)` | 6.0 | Append variant args | LOW |
| `apply(...)` | 6.0 | Apply traits | LOW |
| `absent(...)` | 6.0 | Absent trait | LOW |
| `contains(...)` | 6.0 | Contains trait | LOW |
| `holds(...)` | 6.0 | Holds trait | LOW |
| `no_openmp` | 6.0 | No OpenMP trait | LOW |
| `no_openmp_constructs` | 6.0 | No OpenMP constructs | LOW |
| `no_openmp_routines` | 6.0 | No OpenMP routines | LOW |
| `no_parallelism` | 6.0 | No parallelism trait | LOW |
| `nocontext` | 6.0 | No context | LOW |
| `novariants` | 6.0 | No variants | LOW |
| `permutation(...)` | 6.0 | Interchange permutation | LOW |
| `counts(...)` | 6.0 | Split counts | LOW |

### Missing Clause Modifiers/Extensions

| Clause Extension | Description |
|-----------------|-------------|
| `nowait` as bare clause | Currently Flexible, should support bare |
| `full` as bare clause | Currently exists but may need refinement |
| `partial` as flexible clause | Currently exists |
| `ordered` as flexible clause | Support both bare and parenthesized |

---

## roup Gaps vs OpenMP 6.0 Specification

Based on analysis of roup's `keywords_analysis.json`:

```json
"missing_clauses": [],
"missing_directives": []
```

**roup claims NO missing features from OpenMP 6.0 spec!**

However, they list some "extra" clauses not documented in their OpenMP 6.0 reference:
- `device_resident` (OpenACC spillover?)
- `label` (internal use?)
- `public` (internal use?)
- `reproducible` (possible extension?)
- `reverse` (listed as both clause and directive)
- `tile` (listed as both clause and directive)
- `unroll` (listed as both clause and directive)

### Notes on roup Implementation
- Total 263 directive variants (including Fortran `do` equivalents)
- Total 166 clauses with full OpenMP 6.0 spec section references
- Comprehensive test suite with openmp_vv validation
- Claims 620+ automated tests

---

## Implementation Plan for ompparser

### Phase 1: Critical OpenMP 5.1/5.2 Features (Baseline)
**Goal**: Achieve OpenMP 5.2 compliance

1. **Directives**:
   - [ ] `scope` - worksharing scope
   - [ ] `masked` + combined forms
   - [ ] `error` - error directive
   - [ ] `nothing` - no-op
   - [ ] `assume` / `assumes` / `begin assumes` / `end assumes`
   - [ ] `begin metadirective`

2. **Clauses**:
   - [ ] `filter` - masked filtering
   - [ ] `doacross` - dependency specification
   - [ ] `at` / `severity` / `message` - error control
   - [ ] `compare` / `fail` / `weak` - atomic extensions
   - [ ] `otherwise` - metadirective default

3. **Loop variants**:
   - [ ] All missing `*_loop_simd` combinations

### Phase 2: OpenMP 6.0 Task Graph Features
**Goal**: Full task graph support

1. **Directives**:
   - [ ] `taskgraph`
   - [ ] `task_iteration`

2. **Clauses**:
   - [ ] `graph_id`
   - [ ] `graph_reset`
   - [ ] `transparent`
   - [ ] `replayable`
   - [ ] `threadset`

### Phase 3: OpenMP 6.0 Loop Transformations
**Goal**: Complete loop transformation support

1. **Directives**:
   - [ ] `fuse`
   - [ ] `interchange`
   - [ ] `reverse`
   - [ ] `split`
   - [ ] `stripe`
   - [ ] `workdistribute`

2. **Clauses**:
   - [ ] `permutation`
   - [ ] `counts`
   - [ ] `looprange`

### Phase 4: OpenMP 6.0 Device/Heterogeneous Features
**Goal**: Full device interop and advanced device support

1. **Directives**:
   - [ ] `dispatch`
   - [ ] `interop`
   - [ ] `groupprivate`
   - [ ] `allocators`

2. **Clauses**:
   - [ ] `interop`
   - [ ] `indirect`
   - [ ] `local`
   - [ ] `safesync` / `device_safesync`
   - [ ] `memscope`
   - [ ] `init` / `init_complete`
   - [ ] `nocontext` / `novariants`

### Phase 5: OpenMP 6.0 Advanced Features
**Goal**: Complete OpenMP 6.0 support

1. **Induction**:
   - [ ] `declare_induction` directive
   - [ ] `induction` clause
   - [ ] `inductor` / `collector` / `combiner` clauses

2. **Variant Extensions**:
   - [ ] `adjust_args` / `append_args` clauses
   - [ ] `apply` clause

3. **Traits**:
   - [ ] `absent` / `contains` / `holds` clauses
   - [ ] `no_openmp*` trait clauses

---

## Testing Strategy

### 1. Create OpenMP_VV Test Harness
Based on roup's `test_openmp_vv.sh`:

```bash
#!/bin/bash
# Test ompparser against OpenMP Validation & Verification Suite
# 1. Clone OpenMP_VV if not present
# 2. Preprocess C/C++ files with clang
# 3. Extract pragmas
# 4. Parse with ompparser
# 5. Unparse and normalize
# 6. Compare with original
```

### 2. Unit Tests
- One test file per new directive
- Multiple test cases per clause
- Combined directive tests
- Error handling tests

### 3. Validation Metrics
- **Parse Success Rate**: % of pragmas that parse without errors
- **Round-Trip Accuracy**: % of pragmas that unparse identically (normalized)
- **Feature Coverage**: % of OpenMP 6.0 features implemented

### 4. Continuous Comparison with roup
- Track parity with roup implementation
- Document any features where roup fails but ompparser succeeds
- Document design differences

---

## API Stability Guarantees

### Public API (MUST NOT CHANGE)
- `OpenMPDirective` class hierarchy
- `OpenMPClause` class hierarchy
- `parseOpenMP()` function signature
- Enum value orderings (for binary compatibility)
- Virtual method signatures
- Public getter methods

### Internal Implementation (CAN CHANGE)
- Lexer states (flex)
- Parser rules (bison)
- Private member variables
- Helper functions
- DOT generation details
- String unparsing formatting

### Extension Points (SAFE TO ADD)
- New `OpenMPDirectiveKind` enum values at END
- New `OpenMPClauseKind` enum values at END
- New specialized directive/clause subclasses
- New virtual methods with default implementations
- New optional parameters with defaults

---

## Documentation Deliverables

### 1. `ROUP_GAPS.md`
Document instances where roup claims support but fails tests:
- Specific pragma failures
- Parser errors
- Incorrect unparsing
- Semantic errors

### 2. `OMPPARSER_OPENMP_6.0_SUPPORT.md`
Comprehensive documentation of ompparser's OpenMP 6.0 implementation:
- Complete feature matrix
- Implementation details for each construct
- Usage examples
- IR class mappings
- Unparsing examples
- DOT visualization samples

### 3. `MIGRATION_GUIDE.md`
Guide for upgrading from OpenMP 5.0/5.1 to 6.0 pragmas:
- Deprecated feature replacements (`master` → `masked`)
- New recommended patterns
- Performance considerations
- Compatibility notes

---

## Timeline Estimate

- **Phase 1**: 3-4 days (baseline 5.1/5.2 features)
- **Phase 2**: 2-3 days (task graphs)
- **Phase 3**: 2-3 days (loop transformations)
- **Phase 4**: 3-4 days (device/interop)
- **Phase 5**: 2-3 days (advanced features)
- **Testing & Documentation**: 3-4 days
- **Total**: ~15-20 days of focused development

---

## Success Criteria

1. ✅ All OpenMP 6.0 directives parseable
2. ✅ All OpenMP 6.0 clauses parseable
3. ✅ Round-trip unparsing works correctly
4. ✅ DOT generation for all new constructs
5. ✅ Public API remains stable
6. ✅ 95%+ pass rate on OpenMP_VV test suite
7. ✅ Comprehensive documentation
8. ✅ Zero regressions in existing tests
9. ✅ Build succeeds on all platforms
10. ✅ Memory-safe (no leaks, modern C++17)

---

**Document Version**: 1.0
**Last Updated**: 2025-10-23
**Prepared By**: Claude Code
**Status**: Analysis Complete, Implementation In Progress
