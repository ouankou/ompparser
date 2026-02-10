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
#include <vector>

namespace {

struct ClauseExpectation {
  OpenMPClauseKind kind;
  int line;
  int column;
};

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
  setLang(lang);

  std::unique_ptr<OpenMPDirective> directive(
      parseOpenMP(input, nullptr, nullptr));
  if (!directive) {
    std::cerr << "[" << label << "] parse failed for input: " << input << "\n";
    return false;
  }

  bool ok = true;

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
    OpenMPClause *clause = findClause(directive.get(), expectation.kind);
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
  setLang(lang);

  std::unique_ptr<OpenMPDirective> directive(
      parseOpenMP(input, nullptr, nullptr));
  if (directive != nullptr) {
    std::cerr
        << "[" << label
        << "] expected parse failure but parseOpenMP returned a directive\n";
    return false;
  }

  return true;
}

bool checkMapClauseFirstExpressionContains(
    const std::string &label, const char *input, OpenMPBaseLang lang,
    const std::string &expected_fragment) {
  setLang(lang);

  std::unique_ptr<OpenMPDirective> directive(
      parseOpenMP(input, nullptr, nullptr));
  if (!directive) {
    std::cerr << "[" << label << "] parse failed for input: " << input << "\n";
    return false;
  }

  OpenMPClause *clause = findClause(directive.get(), OMPC_map);
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

  setLang(Lang_unknown);
  return ok ? 0 : 1;
}
