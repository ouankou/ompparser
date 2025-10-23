# Safety and Memory Management Refactor

## Overview
This refactor improves the safety of the OpenMP parser component without
altering its public interface. The focus is on eliminating unmanaged dynamic
allocations, adopting RAII patterns, and providing compatibility wrappers when
legacy pointer-based APIs must be preserved.

## Key Improvements
- **Automatic clause lifetime management** – `OpenMPDirective` now tracks
  allocated clause objects and clause vectors with `std::unique_ptr`, ensuring
  all AST nodes are reclaimed automatically.
- **Compatibility helpers** – `trackClauseVector` and `takeOwnership` wrap
  existing raw-pointer workflows so external code continues to operate with raw
  pointers while the implementation retains ownership safely.
- **Resource-safe preprocessing pipeline** – C preprocessing now returns a
  pointer backed by internal storage, avoiding leaks while keeping the legacy
  pointer-returning API.
- **Modern driver usage** – Front-end utilities (`ompparser.cpp`) use
  `std::unique_ptr` internally instead of manual `new` / `delete` pairs.
- **Allocator clause hygiene** – Nested allocator data in `OpenMPUsesAllocators`
  is now stored via RAII containers to prevent leaks.

## Testing
The complete CMake-based regression suite (`cmake --build build --target check`)
was executed after the refactor; all 136 existing tests pass without
modification.
