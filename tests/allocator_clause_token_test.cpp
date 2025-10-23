/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <OpenMPIR.h>
#include <iostream>
#include <memory>
#include <string>

extern OpenMPDirective *parseOpenMP(const char *, void *(_exprParse)(const char *));
extern void setLang(OpenMPBaseLang);

namespace {
OpenMPDirective *parseDirective(const char *directive) {
  setLang(Lang_C);
  return parseOpenMP(directive, nullptr);
}

int verifyAllocatorKind(OpenMPDirective *directive,
                        OpenMPAllocatorClauseAllocator expected_kind,
                        const std::string &expected_user_value) {
  if (directive == nullptr) {
    std::cerr << "Failed to parse directive" << std::endl;
    return 1;
  }

  auto *clauses = directive->getClauses(OMPC_allocator);
  if (clauses == nullptr || clauses->empty()) {
    std::cerr << "Allocator clause not found" << std::endl;
    return 2;
  }

  auto *allocator_clause =
      dynamic_cast<OpenMPAllocatorClause *>(clauses->front());
  if (allocator_clause == nullptr) {
    std::cerr << "Allocator clause has unexpected type" << std::endl;
    return 3;
  }

  if (allocator_clause->getAllocator() != expected_kind) {
    std::cerr << "Allocator kind mismatch" << std::endl;
    return 4;
  }

  if (allocator_clause->getUserDefinedAllocator() != expected_user_value) {
    std::cerr << "Allocator user-defined payload mismatch" << std::endl;
    return 5;
  }

  return 0;
}
} // namespace

int main() {
  std::unique_ptr<OpenMPDirective> parsed(
      parseDirective("#pragma omp allocate(x) allocator(omp_default_mem_alloc)"));
  if (int rc = verifyAllocatorKind(parsed.get(),
                                   OMPC_ALLOCATOR_ALLOCATOR_default, "")) {
    return rc;
  }

  parsed.reset(parseDirective(
      "#pragma omp allocate(y) allocator(user_defined_allocator)"));
  if (int rc = verifyAllocatorKind(parsed.get(), OMPC_ALLOCATOR_ALLOCATOR_user,
                                   "user_defined_allocator")) {
    return rc;
  }

  return 0;
}
