# Safety and Memory Management Refactor Notes

## Overview
This document describes the internal refactoring applied to improve
memory-safety guarantees throughout the OpenMP parser component while
keeping the legacy public interfaces intact for downstream consumers.

## Clause Lifetime Management
* Clause objects are now owned by each `OpenMPDirective` instance through
  a `std::vector<std::unique_ptr<OpenMPClause>>`. This guarantees
  automatic reclamation of clause objects and prevents the many leaks that
  previously existed due to raw `new` allocations.
* Clause vectors stored in directive maps now use
  `std::unique_ptr<std::vector<OpenMPClause *>>`, ensuring the container
  lifetime is tied to the directive and eliminating manual `new`/`delete`
  bookkeeping.
* A public helper `OpenMPDirective::registerClause` centralises clause
  creation. Wrapper utilities now invoke this helper when constructing
  derived clause types so that every clause is tracked in the owning
  storage automatically.
* Existing accessors such as `getClauses` and `getClausesInOriginalOrder`
  still return raw pointers for compatibility, but the underlying storage
  is now safely managed.

## String Ownership in Clauses
* `OpenMPClause::addLangExpr` previously stored raw `const char *`
  pointers without ownership, creating dangling pointer risks. The method
  now copies incoming strings into managed buffers (via
  `std::unique_ptr<char[]>`) before exposing them, preserving pointer
  validity for clients that rely on C-style strings.

## Atomic Directive Storage
* Atomic directive clause collections now mirror the safety improvements
  by using smart pointers for the clause vectors. Helper accessors lazily
  instantiate vectors when first needed, providing the same behaviour
  without manual allocation.

## Front-End Utilities
* The preprocessing pipeline gained a safe entry point
  (`preProcessCManaged`) that returns a `std::unique_ptr`-wrapped vector.
  The original `preProcessC` function remains as a thin compatibility
  wrapper, so existing integrations continue to compile while the rest of
  the project now uses RAII.
* The command-line driver (`ompparser.cpp`) switched to `std::unique_ptr`
  for owning directive and string collections. This removes a set of
  leaks and aligns the executable with the new safe APIs.

## Testing
All regression tests in the CMake suite (`cmake --build build --target
check`) pass, confirming that the refactor preserves parser behaviour
while improving memory safety.

## Migration Guidance
Downstream users are encouraged to migrate to the new managed helper APIs
(e.g., `preProcessCManaged` and `OpenMPDirective::registerClause`) when
possible. The legacy entry points remain functional but should be
considered deprecated in favour of the safer alternatives.
