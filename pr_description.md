# OpenMP 6.0 Support and AST Refactoring

## Summary
This PR implements significant improvements to `ompparser`, including support for OpenMP 6.0 constructs, a major refactoring of the AST to use Enums instead of strings for better type safety and performance, and resolving several parsing bugs and regressions.

## Key Changes

### OpenMP 6.0 Implementation
- Added support for numerous OpenMP 6.0 clauses and directives, including:
  - `absent`, `contains`, `holds`, `no_openmp`, `no_openmp_constructs`, `no_openmp_routines`, `no_parallelism`
  - `transparent`, `replayable`, `graph_reset`, `graph_id`
  - `memscope`, `init_complete`, `safesync`, `device_safesync`
  - `indirect`, `init`, `interop` (refactored)
- Updated `OpenMPKinds.h` with new enums for 6.0 clauses and directives.

### AST Refactoring
- Refactored `OpenMPIR` and `ompparser.yy` to use `OpenMPClauseKind` and `OpenMPDirectiveKind` enums for clause and directive identification instead of string comparisons.
- This improves code maintainability, reduces string overhead, and prevents typo-related bugs.

### Bug Fixes
- **`no_openmp_constructs` Unparsing:** Fixed a regression where the optional expression argument was not being correctly serialized in `toString()`.
- **Lexer `if` Clause Handling:** Resolved a critical bug in `omplexer.ll` regarding nested parentheses within `if` clauses. The lexer now correctly tracks parenthesis depth to distinguish between the clause's delimiter and expressions within the clause.
- **Reduce/Reduce Conflicts:** Refactored grammar rules in `ompparser.yy` (specifically for optional clause sequences) to eliminate reduce/reduce conflicts, ensuring deterministic parsing.

### Code Formatting
- Completely reformatted `src/OpenMPKinds.h` to ensure consistent 2-space indentation and alignment of enum values, significantly improving readability.

## Verification
- **Test Suite:** Passed 100% of the `ctest` suite (1524 tests), including new tests for OpenMP 6.0 constructs and regression tests for bug fixes.
- **Compilation:** Clean build with no errors.
