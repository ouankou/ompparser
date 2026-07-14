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
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

struct Record {
  OpenMPDirectiveKind directive;
  OpenMPClauseKind clause;
  OpenMPExprParseMode mode;
  std::string expression;

  bool operator==(const Record &other) const {
    return directive == other.directive && clause == other.clause &&
           mode == other.mode && expression == other.expression;
  }
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

bool checkRecords(const char *input, OpenMPBaseLang base_lang,
                  OpenMPDirectiveKind directive_kind,
                  const std::vector<std::string> &expressions,
                  OpenMPExprParseMode mode) {
  Capture capture;
  OpenMPParseOptions options;
  options.base_lang = base_lang;
  options.expression_callback = captureExpression;
  options.expression_callback_user_data = &capture;
  auto directive = parseOpenMP(input, options);
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

bool checkClauseRecords(const char *input, OpenMPBaseLang base_lang,
                        OpenMPDirectiveKind directive_kind,
                        const std::vector<Record> &expected,
                        const std::string &expected_output) {
  Capture capture;
  OpenMPParseOptions options;
  options.base_lang = base_lang;
  options.normalize_clauses = false;
  options.expression_callback = captureExpression;
  options.expression_callback_user_data = &capture;
  auto directive = parseOpenMP(input, options);
  if (directive == nullptr || directive->getKind() != directive_kind ||
      directive->generatePragmaString() != expected_output ||
      capture.records.size() != expected.size()) {
    std::cerr << "clause callback result mismatch for: " << input << "\n";
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const Record &actual = capture.records[index];
    const Record &wanted = expected[index];
    if (actual.directive != wanted.directive ||
        actual.clause != wanted.clause || actual.mode != wanted.mode ||
        actual.expression != wanted.expression) {
      std::cerr << "clause callback record mismatch for: " << input << "\n";
      return false;
    }
  }
  return true;
}

bool checkTypedVariantSelectorRecords() {
  Capture capture;
  OpenMPParseOptions options;
  options.base_lang = Lang_Cplusplus;
  options.normalize_clauses = false;
  options.expression_callback = captureExpression;
  options.expression_callback_user_data = &capture;
  auto directive =
      parseOpenMP("#pragma omp metadirective "
                  "when(device={arch(\"nvptx\",\"amdgcn\"),kind(cpu,gpu)}, "
                  "target_device={device_num(device_id),"
                  "isa(\"sm_90\",\"gfx942\"),uid(\"device-7\")}, "
                  "implementation={vendor(score(5): amd,llvm),"
                  "extension(score(2): ext_a,ext_b),"
                  "requires(score(3): unified_shared_memory,"
                  "atomic_default_mem_order(acquire)),"
                  "atomic_default_mem_order(score(4): release),"
                  "rex_fast(score(6): prop,nested(7))}, "
                  "user={condition(score(7): enabled)}: parallel)",
                  options);
  if (directive == nullptr || directive->getKind() != OMPD_metadirective) {
    std::cerr << "typed metadirective callback fixture did not parse\n";
    return false;
  }
  const std::vector<OpenMPClause *> *when_clauses =
      directive->findClauses(OMPC_when);
  if (when_clauses == nullptr || when_clauses->size() != 1) {
    std::cerr << "typed metadirective fixture has no unique when clause\n";
    return false;
  }
  auto *when_clause = dynamic_cast<OpenMPWhenClause *>(when_clauses->front());
  if (when_clause == nullptr) {
    std::cerr << "when clause does not use typed variant IR\n";
    return false;
  }
  const auto &sets = when_clause->getTraitSets();
  const std::vector<OpenMPContextSelectorSequenceKind> expected_set_kinds = {
      OMPC_SELECTOR_device, OMPC_SELECTOR_target_device,
      OMPC_SELECTOR_implementation, OMPC_SELECTOR_user};
  if (sets.size() != expected_set_kinds.size()) {
    std::cerr << "typed metadirective lost a selector set\n";
    return false;
  }
  for (std::size_t i = 0; i < sets.size(); ++i) {
    if (sets[i].kind != expected_set_kinds[i] || sets[i].selectors.empty()) {
      std::cerr << "typed metadirective changed selector-set order\n";
      return false;
    }
    for (const auto &selector : sets[i].selectors) {
      if ((!selector.score.empty() && selector.score_node != &capture)) {
        std::cerr << "typed selector does not own its exact callback nodes\n";
        return false;
      }
      for (const auto &property : selector.properties) {
        if (!property.spelling.empty() && property.node != &capture) {
          std::cerr << "typed property does not own its exact callback node\n";
          return false;
        }
      }
    }
  }
  if (sets[0].selectors.size() != 2 ||
      sets[0].selectors[0].kind != OMPC_TRAIT_arch ||
      sets[0].selectors[1].kind != OMPC_TRAIT_kind ||
      sets[1].selectors.size() != 3 ||
      sets[1].selectors[0].kind != OMPC_TRAIT_device_num ||
      sets[1].selectors[1].kind != OMPC_TRAIT_isa ||
      sets[1].selectors[2].kind != OMPC_TRAIT_uid ||
      sets[2].selectors.size() != 5 ||
      sets[2].selectors[0].kind != OMPC_TRAIT_vendor ||
      sets[2].selectors[1].kind != OMPC_TRAIT_extension ||
      sets[2].selectors[2].kind != OMPC_TRAIT_requires ||
      sets[2].selectors[3].kind != OMPC_TRAIT_atomic_default_mem_order ||
      sets[2].selectors[4].kind != OMPC_TRAIT_implementation_user ||
      sets[3].selectors.size() != 1 ||
      sets[3].selectors[0].kind != OMPC_TRAIT_condition) {
    std::cerr << "typed metadirective changed trait order or kinds\n";
    return false;
  }
  if (sets[0].selectors[0].properties.size() != 2 ||
      sets[0].selectors[1].properties.size() != 2 ||
      sets[0].selectors[1].properties[0].context_kind !=
          OMPC_CONTEXT_KIND_cpu ||
      sets[0].selectors[1].properties[1].context_kind !=
          OMPC_CONTEXT_KIND_gpu ||
      sets[2].selectors[0].properties.size() != 2 ||
      sets[2].selectors[0].properties[0].context_vendor !=
          OMPC_CONTEXT_VENDOR_amd ||
      sets[2].selectors[0].properties[1].context_vendor !=
          OMPC_CONTEXT_VENDOR_llvm ||
      sets[2].selectors[3].properties[0].atomic_default_mem_order !=
          OMPC_ATOMIC_DEFAULT_MEM_ORDER_release ||
      sets[2].selectors[4].implementation_defined_name != "rex_fast") {
    std::cerr << "typed metadirective lost enum selector payload\n";
    return false;
  }
  const std::vector<Record> expected = {
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "\"nvptx\""},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "\"amdgcn\""},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_expression, "device_id"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "\"sm_90\""},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "\"gfx942\""},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "\"device-7\""},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_constant_integer, "5"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_constant_integer, "2"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "ext_a"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "ext_b"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_constant_integer, "3"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim,
       "unified_shared_memory"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim,
       "atomic_default_mem_order(acquire)"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_constant_integer, "4"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_constant_integer, "6"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "prop"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_verbatim, "nested(7)"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_constant_integer, "7"},
      {OMPD_metadirective, OMPC_when, OMP_EXPR_PARSE_expression, "enabled"}};
  if (capture.records != expected) {
    std::cerr << "typed metadirective callback sequence is not exact\n";
    return false;
  }
  const std::string unparsed = directive->generatePragmaString();
  return unparsed.find(
             "device = {arch(\"nvptx\", \"amdgcn\"), kind(cpu, gpu)}") !=
             std::string::npos &&
         unparsed.find("target_device = {device_num(device_id), isa(\"sm_90\", "
                       "\"gfx942\"), uid(\"device-7\")}") !=
             std::string::npos &&
         unparsed.find("requires(score(3): unified_shared_memory, "
                       "atomic_default_mem_order(acquire))") !=
             std::string::npos &&
         unparsed.find("rex_fast(score(6): prop, nested(7))") !=
             std::string::npos;
}

bool checkNowaitRejectsSecondExpression() {
  const pid_t child = fork();
  if (child < 0) {
    std::cerr << "failed to fork nowait cardinality death test\n";
    return false;
  }
  if (child == 0) {
    OpenMPNowaitClause clause;
    clause.addLangExpr("first");
    clause.addLangExpr("second");
    _exit(0);
  }
  int status = 0;
  if (waitpid(child, &status, 0) != child) {
    std::cerr << "failed to wait for nowait cardinality death test\n";
    return false;
  }
  return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

bool checkOptionalMetadirectiveVariants() {
  OpenMPParseOptions options;
  options.base_lang = Lang_Cplusplus;
  auto directive = parseOpenMP(
      "#pragma omp metadirective when(device={kind(cpu)}:) otherwise()",
      options);
  if (directive == nullptr || directive->getKind() != OMPD_metadirective) {
    std::cerr << "optional metadirective variants did not parse\n";
    return false;
  }
  const std::vector<OpenMPClause *> *when_clauses =
      directive->findClauses(OMPC_when);
  const std::vector<OpenMPClause *> *otherwise_clauses =
      directive->findClauses(OMPC_otherwise);
  if (when_clauses == nullptr || when_clauses->size() != 1 ||
      otherwise_clauses == nullptr || otherwise_clauses->size() != 1) {
    std::cerr << "optional metadirective lost when/otherwise clauses\n";
    return false;
  }
  auto *when_clause = dynamic_cast<OpenMPWhenClause *>(when_clauses->front());
  auto *otherwise_clause =
      dynamic_cast<OpenMPOtherwiseClause *>(otherwise_clauses->front());
  if (when_clause == nullptr || otherwise_clause == nullptr ||
      when_clause->getVariantDirective() != nullptr ||
      otherwise_clause->getVariantDirective() != nullptr) {
    std::cerr << "optional metadirective invented an explicit directive\n";
    return false;
  }
  return directive->generatePragmaString() ==
         "#pragma omp metadirective when (device = {kind(cpu)} :) otherwise ()";
}

} // namespace

int main() {
  bool ok = true;
  ok = checkTypedVariantSelectorRecords() && ok;
  ok = checkNowaitRejectsSecondExpression() && ok;
  ok = checkOptionalMetadirectiveVariants() && ok;
  ok = checkRecords(
           "#pragma omp declare variant(foo) match(construct={parallel})",
           Lang_Cplusplus, OMPD_declare_variant, {"foo"},
           OMP_EXPR_PARSE_expression) &&
       ok;
  ok = checkRecords(
           "!$omp declare variant(base:impl) match(construct={parallel})",
           Lang_Fortran, OMPD_declare_variant, {"base", "impl"},
           OMP_EXPR_PARSE_expression) &&
       ok;
  ok = checkRecords("#pragma omp allocate(a,b)", Lang_Cplusplus, OMPD_allocate,
                    {"a", "b"}, OMP_EXPR_PARSE_variable_list) &&
       ok;
  ok = checkRecords("#pragma omp threadprivate(a,b)", Lang_Cplusplus,
                    OMPD_threadprivate, {"a", "b"},
                    OMP_EXPR_PARSE_variable_list) &&
       ok;
  ok = checkRecords("#pragma omp groupprivate(a,b)", Lang_Cplusplus,
                    OMPD_groupprivate, {"a", "b"},
                    OMP_EXPR_PARSE_variable_list) &&
       ok;
  ok = checkRecords("#pragma omp declare target(a,b[0:4])", Lang_Cplusplus,
                    OMPD_declare_target, {"a", "b[0:4]"},
                    OMP_EXPR_PARSE_array_section) &&
       ok;
  ok = checkRecords("#pragma omp flush(a,b)", Lang_Cplusplus, OMPD_flush,
                    {"a", "b"}, OMP_EXPR_PARSE_variable_list) &&
       ok;
  ok = checkRecords("!$omp declare simd(foo)", Lang_Fortran, OMPD_declare_simd,
                    {"foo"}, OMP_EXPR_PARSE_expression) &&
       ok;
  ok = checkClauseRecords("#pragma omp target nowait(is_deferred)",
                          Lang_Cplusplus, OMPD_target,
                          {{OMPD_target, OMPC_nowait, OMP_EXPR_PARSE_expression,
                            "is_deferred"}},
                          "#pragma omp target nowait(is_deferred)") &&
       ok;
  ok =
      checkClauseRecords(
          "!$omp allocators allocate(allocator(my_alloctr), align(64): a)",
          Lang_Fortran, OMPD_allocators,
          {{OMPD_allocators, OMPC_allocate, OMP_EXPR_PARSE_expression,
            "my_alloctr"},
           {OMPD_allocators, OMPC_allocate, OMP_EXPR_PARSE_expression, "64"},
           {OMPD_allocators, OMPC_allocate, OMP_EXPR_PARSE_variable_list, "a"}},
          "!$omp allocators allocate (allocator(my_alloctr), align(64): a)") &&
      ok;
  ok = checkClauseRecords(
           "#pragma omp parallel allocate(get_allocator(): x)", Lang_Cplusplus,
           OMPD_parallel,
           {{OMPD_parallel, OMPC_allocate, OMP_EXPR_PARSE_expression,
             "get_allocator()"},
            {OMPD_parallel, OMPC_allocate, OMP_EXPR_PARSE_variable_list, "x"}},
           "#pragma omp parallel allocate (get_allocator(): x)") &&
       ok;
  ok =
      checkClauseRecords(
          "#pragma omp parallel allocate(allocator(select_allocator(\")\\\"\", "
          "pool[index + nested(1, 2)])), "
          "align(compute_align(sizeof(int[4]), \"):,\")): x)",
          Lang_Cplusplus, OMPD_parallel,
          {{OMPD_parallel, OMPC_allocate, OMP_EXPR_PARSE_expression,
            "select_allocator(\")\\\"\", pool[index + nested(1, 2)])"},
           {OMPD_parallel, OMPC_allocate, OMP_EXPR_PARSE_expression,
            "compute_align(sizeof(int[4]), \"):,\")"},
           {OMPD_parallel, OMPC_allocate, OMP_EXPR_PARSE_variable_list, "x"}},
          "#pragma omp parallel allocate "
          "(allocator(select_allocator(\")\\\"\", "
          "pool[index + nested(1, 2)])), "
          "align(compute_align(sizeof(int[4]), \"):,\")): x)") &&
      ok;
  return ok ? 0 : 1;
}
