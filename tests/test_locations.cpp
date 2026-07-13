/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <OpenMPIR.h>
#include <OpenMPParser.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct ClauseExpectation {
  OpenMPClauseKind kind;
  int line;
  int column;
};

ompparser::BaseLanguage convertLanguage(OpenMPBaseLang language) {
  switch (language) {
  case Lang_C:
    return ompparser::BaseLanguage::C;
  case Lang_Cplusplus:
    return ompparser::BaseLanguage::CXX;
  case Lang_Fortran:
    return ompparser::BaseLanguage::Fortran;
  case Lang_unknown:
    break;
  }
  return ompparser::BaseLanguage::C;
}

ompparser::ParseResult parse(const char *input, OpenMPBaseLang language) {
  ompparser::ParseOptions options;
  options.language = convertLanguage(language);
  return ompparser::parseDirective(input, options);
}

OpenMPClause *findClause(OpenMPDirective *directive, OpenMPClauseKind kind) {
  if (directive == nullptr) {
    return nullptr;
  }

  std::vector<OpenMPClause *> *clauses = directive->getClausesInOriginalOrder();
  if (clauses == nullptr) {
    return nullptr;
  }

  for (OpenMPClause *clause : *clauses) {
    if (clause != nullptr && clause->getKind() == kind) {
      return clause;
    }
  }

  return nullptr;
}

void emitLocationError(const std::string &label, const std::string &target,
                       int expected_line, int expected_column, int actual_line,
                       int actual_column) {
  std::cerr << "[" << label << "] " << target << " location mismatch: "
            << "expected (" << expected_line << ", " << expected_column
            << "), got (" << actual_line << ", " << actual_column << ")\n";
}

bool checkCase(const std::string &label, const char *input, OpenMPBaseLang lang,
               OpenMPDirectiveKind expected_directive_kind,
               int expected_directive_line, int expected_directive_column,
               const std::vector<ClauseExpectation> &clause_expectations) {
  ompparser::ParseResult result = parse(input, lang);
  if (!result.success()) {
    std::cerr << "[" << label << "] parse failed for input: " << input << "\n";
    return false;
  }

  bool ok = true;

  OpenMPDirective *directive = result.directive.get();
  if (directive->getKind() != expected_directive_kind) {
    std::cerr << "[" << label << "] directive kind mismatch: expected "
              << expected_directive_kind << ", got " << directive->getKind()
              << "\n";
    ok = false;
  }

  if (directive->getLine() != expected_directive_line ||
      directive->getColumn() != expected_directive_column) {
    emitLocationError(label, "directive", expected_directive_line,
                      expected_directive_column, directive->getLine(),
                      directive->getColumn());
    ok = false;
  }

  for (const ClauseExpectation &expectation : clause_expectations) {
    OpenMPClause *clause = findClause(directive, expectation.kind);
    if (clause == nullptr) {
      std::cerr << "[" << label << "] missing clause kind " << expectation.kind
                << "\n";
      ok = false;
      continue;
    }

    if (clause->getLine() != expectation.line ||
        clause->getColumn() != expectation.column) {
      emitLocationError(
          label, std::string("clause kind ") + std::to_string(expectation.kind),
          expectation.line, expectation.column, clause->getLine(),
          clause->getColumn());
      ok = false;
    }
  }

  return ok;
}

bool checkInvalidCase(const std::string &label, const char *input,
                      OpenMPBaseLang lang) {
  ompparser::ParseResult result = parse(input, lang);
  if (result.success()) {
    std::cerr << "[" << label
              << "] expected parse failure but parsing returned a directive\n";
    return false;
  }

  return true;
}

bool checkMapClauseFirstExpressionContains(
    const std::string &label, const char *input, OpenMPBaseLang lang,
    const std::string &expected_fragment) {
  ompparser::ParseResult result = parse(input, lang);
  if (!result.success()) {
    std::cerr << "[" << label << "] parse failed for input: " << input << "\n";
    return false;
  }

  OpenMPClause *clause = findClause(result.directive.get(), OMPC_map);
  if (clause == nullptr) {
    std::cerr << "[" << label << "] missing map clause\n";
    return false;
  }

  auto *map_clause = dynamic_cast<OpenMPMapClause *>(clause);
  if (map_clause == nullptr) {
    std::cerr << "[" << label << "] map clause has unexpected type\n";
    return false;
  }

  std::vector<const char *> *expressions = map_clause->getExpressions();
  if (expressions == nullptr || expressions->empty() ||
      (*expressions)[0] == nullptr) {
    std::cerr << "[" << label << "] map clause has no expressions\n";
    return false;
  }

  const std::string first_expression((*expressions)[0]);
  if (first_expression.find(expected_fragment) == std::string::npos) {
    std::cerr << "[" << label
              << "] map expression mismatch: expected to contain '"
              << expected_fragment << "', got '" << first_expression << "'\n";
    return false;
  }

  return true;
}

bool checkMapClauseFirstPolicyCount(const std::string &label, const char *input,
                                    OpenMPBaseLang lang,
                                    std::size_t expected_count) {
  ompparser::ParseResult result = parse(input, lang);
  if (!result.success()) {
    std::cerr << "[" << label << "] parse failed for input: " << input << "\n";
    return false;
  }

  OpenMPClause *clause = findClause(result.directive.get(), OMPC_map);
  if (clause == nullptr) {
    std::cerr << "[" << label << "] missing map clause\n";
    return false;
  }

  auto *map_clause = dynamic_cast<OpenMPMapClause *>(clause);
  if (map_clause == nullptr) {
    std::cerr << "[" << label << "] map clause has unexpected type\n";
    return false;
  }

  const auto &all_policies = map_clause->getDistDataPolicies();
  const std::size_t actual_count =
      all_policies.empty() ? 0 : all_policies.front().size();
  if (actual_count != expected_count) {
    std::cerr << "[" << label << "] dist_data policy count mismatch: expected "
              << expected_count << ", got " << actual_count << "\n";
    return false;
  }

  return true;
}

} // namespace

int main() {
  bool ok = true;

  ok = checkCase("c_parallel_for",
                 "#pragma omp parallel for private(i) collapse(2)", Lang_C,
                 OMPD_parallel_for, 1, 13,
                 {{OMPC_private, 1, 26}, {OMPC_collapse, 1, 37}}) &&
       ok;

  ok = checkCase("c_parallel_loop_empty_suffix",
                 "#pragma omp parallel loop private(i)", Lang_C,
                 OMPD_parallel_loop, 1, 13, {{OMPC_private, 1, 27}}) &&
       ok;

  ok = checkCase("fortran_parallel_do", "!$omp parallel do private(i)",
                 Lang_Fortran, OMPD_parallel_do, 1, 7,
                 {{OMPC_private, 1, 19}}) &&
       ok;

  ok = checkCase("fortran_compact_parallel_do", "!$omp paralleldo private(i)",
                 Lang_Fortran, OMPD_parallel_do, 1, 7,
                 {{OMPC_private, 1, 18}}) &&
       ok;

  ok = checkCase("c_declare_target_underscore",
                 "#pragma omp declare_target device_type(nohost)", Lang_C,
                 OMPD_declare_target, 1, 13, {{OMPC_device_type, 1, 40}}) &&
       ok;

  ok = checkCase("fortran_compact_end_do", "!$omp enddo nowait", Lang_Fortran,
                 OMPD_end, 1, 7, {{OMPC_nowait, 1, 13}}) &&
       ok;

  ok = checkCase("c_target_enter_data_underscore",
                 "#pragma omp target_enter_data map(to: EnterMap) nowait",
                 Lang_C, OMPD_target_enter_data, 1, 13,
                 {{OMPC_nowait, 1, 49}}) &&
       ok;

  ok =
      checkCase("c_target_exit_data_underscore",
                "#pragma omp target_exit_data map(from: ExitMap) nowait",
                Lang_C, OMPD_target_exit_data, 1, 13, {{OMPC_nowait, 1, 49}}) &&
      ok;

  ok = checkCase("metadirective_default",
                 "#pragma omp metadirective default(parallel)", Lang_C,
                 OMPD_metadirective, 1, 13, {{OMPC_default, 1, 34}}) &&
       ok;

  ok = checkInvalidCase("invalid_missing_paren",
                        "#pragma omp parallel for private(i", Lang_C) &&
       ok;

  ok = checkInvalidCase("invalid_unknown_directive",
                        "#pragma omp does_not_exist", Lang_C) &&
       ok;

  ok = checkCase("invalid_recovery_followup",
                 "#pragma omp parallel for private(i)", Lang_C,
                 OMPD_parallel_for, 1, 13, {{OMPC_private, 1, 26}}) &&
       ok;

  ok = checkCase("schedule_clause_location", "#pragma omp for schedule(static)",
                 Lang_C, OMPD_for, 1, 13, {{OMPC_schedule, 1, 17}}) &&
       ok;

  ok = checkInvalidCase("invalid_schedule_clause_missing_rparen",
                        "#pragma omp for schedule(static", Lang_C) &&
       ok;

  ok = checkCase("fortran_schedule_location_after_invalid_schedule",
                 "!$omp do schedule(static)", Lang_Fortran, OMPD_do, 1, 7,
                 {{OMPC_schedule, 1, 10}}) &&
       ok;

  ok = checkMapClauseFirstExpressionContains(
           "map_member_dist_data_not_suffix",
           "#pragma omp target map(to: obj.dist_data(x))", Lang_C,
           "dist_data(") &&
       ok;

  ok = checkMapClauseFirstExpressionContains(
           "map_nested_dist_data_call_not_suffix",
           "#pragma omp target map(to: foo(a, dist_data(b)))", Lang_C,
           "dist_data(") &&
       ok;

  ok = checkMapClauseFirstPolicyCount(
           "map_trailing_plus_dist_data_not_suffix",
           "#pragma omp target map(to: foo + dist_data(duplicate))", Lang_C,
           0) &&
       ok;

  return ok ? 0 : 1;
}
