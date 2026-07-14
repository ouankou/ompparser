/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <OpenMPIR.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Record {
  OpenMPDirectiveKind directive;
  OpenMPClauseKind clause;
  OpenMPExprParseMode mode;
  std::string expression;
};

struct Capture {
  std::vector<Record> records;
};

void *captureExpression(OpenMPDirectiveKind directive, OpenMPClauseKind clause,
                        OpenMPExprParseMode mode, const char *expression,
                        void *user_data) {
  auto *capture = static_cast<Capture *>(user_data);
  if (capture == nullptr || expression == nullptr) {
    std::cerr << "callback received a null capture or expression\n";
    std::abort();
  }
  capture->records.push_back({directive, clause, mode, expression});
  return capture;
}

bool checkRecords(const char *input, OpenMPDirectiveKind directive_kind,
                  const std::vector<std::string> &expressions,
                  OpenMPExprParseMode mode) {
  Capture capture;
  std::unique_ptr<OpenMPDirective> directive(
      parseOpenMP(input, captureExpression, &capture));
  if (directive == nullptr || directive->getKind() != directive_kind) {
    std::cerr << "failed to parse callback test: " << input << "\n";
    return false;
  }

  std::vector<std::string> actual;
  for (const Record &record : capture.records) {
    if (record.directive == directive_kind && record.clause == OMPC_unknown) {
      if (record.mode != mode) {
        std::cerr << "directive callback mode mismatch for: " << input << "\n";
        return false;
      }
      actual.push_back(record.expression);
    }
  }
  if (actual != expressions) {
    std::cerr << "directive callback expression mismatch for: " << input
              << "\n";
    return false;
  }

  if (auto *variant =
          dynamic_cast<OpenMPDeclareVariantDirective *>(directive.get())) {
    const bool has_base = !variant->getBaseFuncID().empty();
    return variant->getVariantFuncNode() == &capture &&
           (has_base ? variant->getBaseFuncNode() == &capture
                     : variant->getBaseFuncNode() == nullptr);
  }
  if (auto *allocate =
          dynamic_cast<OpenMPAllocateDirective *>(directive.get())) {
    return allocate->getAllocateNodes() ==
           std::vector<const void *>(expressions.size(), &capture);
  }
  if (auto *threadprivate =
          dynamic_cast<OpenMPThreadprivateDirective *>(directive.get())) {
    return threadprivate->getThreadprivateNodes() ==
           std::vector<const void *>(expressions.size(), &capture);
  }
  if (auto *groupprivate =
          dynamic_cast<OpenMPGroupprivateDirective *>(directive.get())) {
    return groupprivate->getGroupprivateNodes() ==
           std::vector<const void *>(expressions.size(), &capture);
  }
  if (auto *declare_target =
          dynamic_cast<OpenMPDeclareTargetDirective *>(directive.get())) {
    return declare_target->getExtendedListNodes() ==
           std::vector<const void *>(expressions.size(), &capture);
  }
  if (auto *declare_simd =
          dynamic_cast<OpenMPDeclareSimdDirective *>(directive.get())) {
    return expressions.size() == 1 &&
           declare_simd->getProcNameNode() == &capture;
  }
  if (auto *flush = dynamic_cast<OpenMPFlushDirective *>(directive.get())) {
    return flush->getFlushNodes() ==
           std::vector<const void *>(expressions.size(), &capture);
  }
  return false;
}

} // namespace

int main() {
  setLang(Lang_Cplusplus);
  bool ok = true;
  ok = checkRecords(
           "#pragma omp declare variant(foo) match(construct={parallel})",
           OMPD_declare_variant, {"foo"}, OMP_EXPR_PARSE_expression) &&
       ok;
  setLang(Lang_Fortran);
  ok = checkRecords(
           "!$omp declare variant(base:impl) match(construct={parallel})",
           OMPD_declare_variant, {"base", "impl"}, OMP_EXPR_PARSE_expression) &&
       ok;
  setLang(Lang_Cplusplus);
  ok = checkRecords("#pragma omp allocate(a,b)", OMPD_allocate, {"a", "b"},
                    OMP_EXPR_PARSE_variable_list) &&
       ok;
  ok = checkRecords("#pragma omp threadprivate(a,b)", OMPD_threadprivate,
                    {"a", "b"}, OMP_EXPR_PARSE_variable_list) &&
       ok;
  ok = checkRecords("#pragma omp groupprivate(a,b)", OMPD_groupprivate,
                    {"a", "b"}, OMP_EXPR_PARSE_variable_list) &&
       ok;
  ok = checkRecords("#pragma omp declare target(a,b[0:4])", OMPD_declare_target,
                    {"a", "b[0:4]"}, OMP_EXPR_PARSE_array_section) &&
       ok;
  ok = checkRecords("#pragma omp flush(a,b)", OMPD_flush, {"a", "b"},
                    OMP_EXPR_PARSE_variable_list) &&
       ok;
  setLang(Lang_Fortran);
  ok = checkRecords("!$omp declare simd(foo)", OMPD_declare_simd, {"foo"},
                    OMP_EXPR_PARSE_expression) &&
       ok;
  return ok ? 0 : 1;
}
