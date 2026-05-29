---
name: openmpvv-sync
description: Use inside a standalone ompparser repository clone when syncing extracted OpenMPVV pragma tests from upstream OpenMP_VV, validating parser support for new OpenMP directives or clauses, or regenerating tests/openmp_vv snapshots.
---

# OpenMPVV Sync For ompparser

Use this at the root of a standalone ompparser clone. Treat ompparser as the complete project and use only paths inside this repository.

OpenMPVV lives here as extracted parser test inputs under `tests/openmp_vv/`. Keep that extraction model unless the user explicitly asks for a different test-suite layout.

## Workflow

1. Read the repository-local `AGENTS.md` if present and preserve LLVM style. For parser feature gaps, consult the current OpenMP specification and fix parser/IR support cleanly rather than editing extracted tests to mask failures.
2. Check local state with `git status --short`; do not revert unrelated work.
3. Ensure LLVM 22 tools are available: `clang-22`, `clang++-22`, `flang-22`, `clang-format-22`, plus `flex` and `bison`.
4. Regenerate extracted OpenMPVV pragmas:
   `LLVM_VERSION=22 CC=clang-22 CXX=clang++-22 FC=flang-22 ./tests/extract_openmp_vv_pragmas.sh`
   This wipes and rewrites `tests/openmp_vv/`, updates the local upstream checkout in `build/openmp_vv`, and stamps the upstream commit/date plus last extraction time in the script header.
5. Verify extraction integrity before fixing parser failures:
   - The `tests/extract_openmp_vv_pragmas.sh` header must show the new upstream OpenMP_VV commit and extraction time.
   - The extractor's `EXPECTED_PRAGMAS` check must stay exact. If the expected count changes, update it only with evidence from the regenerated real test-source directives.
   - Do not delete, trim, or skip extracted tests to get green results.
6. Inspect the diff. New upstream files should appear as new extracted tests; broad fixture churn should be explainable by upstream changes or extractor-origin filtering.
7. Configure and run all ompparser tests:
   `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++-22`
   `cmake --build build --target check -j$(nproc)`
8. If tests fail, fix parser, IR, or round-trip behavior at the root cause. Avoid string-based round-trip hacks, per-file skips, and extraction changes that hide unsupported OpenMP syntax.
9. Re-run the failing tests, then the full `check` target. Confirm the new OpenMPVV tests are registered, especially new OpenMP 6.0 cases.

## Reporting

Report the upstream OpenMPVV commit, extraction timestamp, file and pragma counts, any parser fixes, and the full `check` result. Keep publication local unless the user explicitly asks to commit, push, or open a PR.
