/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <OpenMPIR.h>
#include <OpenMPParser.h>

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

class TestSemanticNode final : public ompparser::HostSemanticNode {};

class RecordingHooks final : public ompparser::HostLanguageHooks {
public:
  mutable std::vector<ompparser::HostFragment> fragments;
  mutable bool parsed_recursively = false;

  std::shared_ptr<const ompparser::HostSemanticNode>
  parse(const ompparser::HostFragment &fragment,
        std::vector<ompparser::Diagnostic> &) const override {
    fragments.push_back(fragment);
    if (!parsed_recursively) {
      parsed_recursively = true;
      ompparser::ParseResult nested =
          ompparser::parseDirective("#pragma omp parallel private(nested)");
      if (!nested.success()) {
        return nullptr;
      }
    }
    return std::make_shared<TestSemanticNode>();
  }

  void validate(const OpenMPDirective &,
                std::vector<ompparser::Diagnostic> &) const override {}
};

class RejectingHooks final : public ompparser::HostLanguageHooks {
public:
  std::shared_ptr<const ompparser::HostSemanticNode>
  parse(const ompparser::HostFragment &,
        std::vector<ompparser::Diagnostic> &) const override {
    return std::make_shared<TestSemanticNode>();
  }

  void
  validate(const OpenMPDirective &,
           std::vector<ompparser::Diagnostic> &diagnostics) const override {
    ompparser::Diagnostic diagnostic;
    diagnostic.code = ompparser::DiagnosticCode::HostLanguageError;
    diagnostic.severity = ompparser::DiagnosticSeverity::Error;
    diagnostic.message = "host-language contextual rejection";
    diagnostics.push_back(std::move(diagnostic));
  }
};

template <typename ResultT>
bool hasDiagnostic(const ResultT &result, ompparser::DiagnosticCode code) {
  for (const ompparser::Diagnostic &diagnostic : result.diagnostics) {
    if (diagnostic.code == code) {
      return true;
    }
  }
  return false;
}

ompparser::SourcePosition sourcePositionAt(const std::string &input,
                                           std::size_t offset) {
  ompparser::SourcePosition position;
  position.line = 1;
  position.column = 1;
  for (std::size_t index = 0; index < offset && index < input.size(); ++index) {
    ++position.offset;
    if (input[index] == '\n') {
      ++position.line;
      position.column = 1;
    } else {
      ++position.column;
    }
  }
  return position;
}

bool hasSourceFaithfulRange(const ompparser::HostFragment &fragment,
                            const std::string &input) {
  const std::size_t begin = fragment.range.begin.offset;
  const std::size_t end = fragment.range.end.offset;
  if (end < begin || end > input.size() ||
      input.substr(begin, end - begin) != fragment.spelling) {
    return false;
  }

  const ompparser::SourcePosition expected_begin =
      sourcePositionAt(input, begin);
  const ompparser::SourcePosition expected_end = sourcePositionAt(input, end);
  return fragment.range.begin.line == expected_begin.line &&
         fragment.range.begin.column == expected_begin.column &&
         fragment.range.end.line == expected_end.line &&
         fragment.range.end.column == expected_end.column;
}

bool expectText(const char *label, const std::string &input,
                const ompparser::ParseOptions &options,
                const std::string &expected) {
  ompparser::ParseResult parsed = ompparser::parseDirective(input, options);
  if (!parsed.success()) {
    std::cerr << label << ": parsing failed\n";
    return false;
  }
  ompparser::UnparseResult unparsed = ompparser::unparse(*parsed.directive);
  if (!unparsed.success() || unparsed.text != expected) {
    std::cerr << label << ": expected '" << expected << "', got '"
              << unparsed.text << "'\n";
    return false;
  }
  return true;
}

bool expectSuccess(const char *label, const std::string &input,
                   const ompparser::ParseOptions &options) {
  ompparser::ParseResult parsed = ompparser::parseDirective(input, options);
  if (parsed.success() && ompparser::unparse(*parsed.directive).success()) {
    return true;
  }
  std::cerr << label << ": parsing or unparsing failed\n";
  for (const ompparser::Diagnostic &diagnostic : parsed.diagnostics) {
    std::cerr << "  " << diagnostic.message << "\n";
  }
  return false;
}

} // namespace

int main() {
  bool ok = true;

  ompparser::ParseOptions c_options;
  c_options.language = ompparser::BaseLanguage::C;
  ok = expectText("c_literal_fidelity",
                  "#pragma omp parallel if(NAME == \"AbC\")", c_options,
                  "#pragma omp parallel if (NAME == \"AbC\")") &&
       ok;
  ok = expectText("quoted_comma", "#pragma omp error message(\"A,B:C\")",
                  c_options, "#pragma omp error message(\"A,B:C\")") &&
       ok;
  ok = expectText("c_literal_fortran_sentinel",
                  "#pragma omp error message(\"!$omp\")", c_options,
                  "#pragma omp error message(\"!$omp\")") &&
       ok;
  ok = expectText("cxx_raw_literal",
                  "#pragma omp error message(R\"tag(A\",B:C)tag\")", c_options,
                  "#pragma omp error message(R\"tag(A\",B:C)tag\")") &&
       ok;
  ok = expectText("omitted_when_variant",
                  "#pragma omp metadirective "
                  "when(user={condition(enabled)}:)",
                  c_options,
                  "#pragma omp metadirective "
                  "when (user = {condition(enabled)} : )") &&
       ok;
  ok = expectText("omitted_otherwise_variant",
                  "#pragma omp metadirective otherwise", c_options,
                  "#pragma omp metadirective otherwise") &&
       ok;
  ok = expectText("independent_device_selector_sets",
                  "#pragma omp metadirective "
                  "when(device={kind(cpu), arch(x86), isa(sse)}, "
                  "target_device={kind(gpu), arch(nvptx), isa(sm_90), "
                  "device_num(1)}: parallel)",
                  c_options,
                  "#pragma omp metadirective "
                  "when (device = {kind(cpu), arch(x86), isa(sse)}, "
                  "target_device = {kind(gpu), arch(nvptx), isa(sm_90), "
                  "device_num(1)} : parallel)") &&
       ok;
  ok = expectText(
           "typed_adjust_args",
           "#pragma omp declare variant(foo) match(construct={dispatch}) "
           "adjust_args(need_device_addr: a, b) adjust_args(nothing: c)",
           c_options,
           "#pragma omp declare variant (foo) match (construct = {dispatch}) "
           "adjust_args(need_device_addr: a, b) adjust_args(nothing: c)") &&
       ok;
  ok = expectText(
           "typed_append_args",
           "#pragma omp declare variant(foo) match(construct={dispatch}) "
           "append_args(interop(target, targetsync), "
           "interop(prefer_type({fr(\"cuda\")}), target))",
           c_options,
           "#pragma omp declare variant (foo) match (construct = {dispatch}) "
           "append_args(interop(target, targetsync), "
           "interop(prefer_type({fr(\"cuda\")}), target))") &&
       ok;
  ok = expectText("num_threads_list",
                  "#pragma omp parallel num_threads(strict: a, b, c, d)",
                  c_options,
                  "#pragma omp parallel num_threads(strict:a, b, c, d)") &&
       ok;
  ok = expectText("post_modified_default_categories",
                  "#pragma omp parallel default(private: scalar) "
                  "default(shared: aggregate)",
                  c_options,
                  "#pragma omp parallel default (private: scalar) "
                  "default (shared: aggregate)") &&
       ok;
  ok = expectText("optional_logical_clauses",
                  "#pragma omp task untied(flag) mergeable(other) "
                  "replayable(again)",
                  c_options,
                  "#pragma omp task untied(flag) mergeable(other) "
                  "replayable(again)") &&
       ok;
  ok = expectText("bare_optional_logical_clauses",
                  "#pragma omp task untied mergeable replayable", c_options,
                  "#pragma omp task untied mergeable replayable") &&
       ok;
  ok = expectText("init_complete_expression",
                  "#pragma omp scan init_complete(make_phase)", c_options,
                  "#pragma omp scan init_complete(make_phase)") &&
       ok;
  ok = expectText("bare_init_complete", "#pragma omp scan init_complete",
                  c_options, "#pragma omp scan init_complete") &&
       ok;
  ok = expectText("nested_allocator_call",
                  "#pragma omp parallel allocate(allocator(pool): value)",
                  c_options,
                  "#pragma omp parallel allocate (allocator(pool): value)") &&
       ok;
  ok = expectText("array_shaping_locator",
                  "#pragma omp target update from((([N][M]) a)[0:N][1])",
                  c_options,
                  "#pragma omp target update from ((([N][M]) a)[0:N][1])") &&
       ok;

  const std::pair<const char *, const char *> openmp_60_valid_inputs[] = {
      {"task_iteration_clauses",
       "#pragma omp task_iteration affinity(a) depend(in: b) if(flag)"},
      {"split_clauses", "#pragma omp split counts(4, omp_fill)"},
      {"stripe_clauses", "#pragma omp stripe sizes(4)"},
      {"target_data_clauses",
       "#pragma omp target_data map(tofrom: x) private(tmp) nowait"},
      {"target_data_alternate_spelling",
       "#pragma omp target data map(tofrom: x) private(tmp) nowait"},
      // OpenMP 6.0 Section 15.7 permits task clauses on target_data.
      {"target_data_openmp_60_task_clauses",
       "#pragma omp target data map(to: a) affinity(a) allocate(a) "
       "default(shared)"},
      {"parallel_compound_openmp_60_clauses",
       "#pragma omp parallel for induction(step(1), i: j) "
       "severity(warning) message(\"loop\") safesync(flag)"},
      {"target_compound_openmp_60_clauses",
       "#pragma omp target parallel device_type(any) priority(2) replayable"},
      {"target_compound_nowait",
       "#pragma omp target teams distribute parallel for nowait"},
      {"taskloop_openmp_60_clauses",
       "#pragma omp taskloop induction(step(1), i: j) replayable "
       "threadset(omp_team) transparent"}};
  for (const auto &entry : openmp_60_valid_inputs) {
    ok = expectSuccess(entry.first, entry.second, c_options) && ok;
  }

  const std::string invalid_applicability_inputs[] = {
      "#pragma omp task_iteration private(x)",
      "#pragma omp taskgraph replayable",
      "#pragma omp allocators uses_allocators(omp_default_mem_alloc)",
      "#pragma omp workdistribute private(x)",
      "#pragma omp fuse counts(1)",
      "#pragma omp reverse looprange(1, 1)",
      "#pragma omp split looprange(1, 1)",
      "#pragma omp stripe counts(4)",
      "#pragma omp target shared(x)",
      "#pragma omp target_data private(x)",
      "#pragma omp target update if(flag)",
      "#pragma omp distribute simd num_threads(2)",
      "#pragma omp distribute simd default(none)",
      "#pragma omp distribute simd copyin(value)",
      "#pragma omp distribute simd proc_bind(close)",
      "#pragma omp distribute simd schedule(static)",
      "#pragma omp distribute simd severity(warning)",
      "#pragma omp distribute simd message(\"not allowed\")",
      "#pragma omp parallel for nowait",
      "#pragma omp distribute parallel for nowait",
      "#pragma omp distribute parallel for simd nowait",
      "#pragma omp teams distribute parallel for nowait",
      "#pragma omp teams distribute parallel for simd nowait",
      "#pragma omp distribute parallel loop nowait",
      "#pragma omp distribute parallel loop simd nowait",
      "#pragma omp teams distribute parallel loop nowait",
      "#pragma omp teams distribute parallel loop simd nowait",
      "#pragma omp declare mapper(default: struct S value)"};
  for (const std::string &input : invalid_applicability_inputs) {
    if (ompparser::parseDirective(input, c_options).success()) {
      std::cerr << "invalid directive/clause combination was accepted: "
                << input << "\n";
      ok = false;
    }
  }

  const std::string malformed_typed_inputs[] = {
      "#pragma omp declare variant(foo) match(construct={dispatch}) "
      "adjust_args(vendor_op: a)",
      "#pragma omp declare variant(foo) match(construct={dispatch}) "
      "adjust_args(a)",
      "#pragma omp declare variant(foo) match(construct={dispatch}) "
      "append_args(raw_operation)",
      "#pragma omp interop init(vendor_type: object)",
      "#pragma omp interop init(target, target: object)",
      "#pragma omp metadirective "
      "when(device={arch(score(1): x86)}:)",
      "#pragma omp metadirective "
      "when(device={arch(x86), arch(arm)}:)",
      "#pragma omp metadirective "
      "when(target_device={kind(cpu), kind(gpu)}:)",
      "#pragma omp metadirective when(device={device_num(0)}:)"};
  for (const std::string &input : malformed_typed_inputs) {
    if (ompparser::parseDirective(input, c_options).success()) {
      std::cerr << "malformed typed clause was accepted: " << input << "\n";
      ok = false;
    }
  }

  ompparser::ParseOptions registered_c_options = c_options;
  registered_c_options.extensions = ompparser::ExtensionPolicy::AllowRegistered;
  ok = expectText("registered_dist_data",
                  "#pragma omp target data map(to: a[0:N] "
                  "dist_data(duplicate, block(4), cyclic(2)))",
                  registered_c_options,
                  "#pragma omp target data map(to : a[0:N] "
                  "dist_data(duplicate, block(4), cyclic(2)))") &&
       ok;

  const std::string malformed_dist_data_inputs[] = {
      "#pragma omp target data map(to: a[0:N] dist_data(vendor(1)))",
      "#pragma omp target data map(to: a[0:N] dist_data())",
      "#pragma omp target data map(to: a[0:N] dist_data(duplicate,))",
      "#pragma omp target data map(to: a[0:N] "
      "dist_data(duplicate(1) trailing))"};
  for (const std::string &input : malformed_dist_data_inputs) {
    ompparser::ParseResult result =
        ompparser::parseDirective(input, registered_c_options);
    if (result.success() ||
        !hasDiagnostic(result, ompparser::DiagnosticCode::InvalidClause)) {
      std::cerr << "malformed dist_data policy was accepted: " << input << "\n";
      ok = false;
    }
  }

  ompparser::ParseResult rejected_dist_data = ompparser::parseDirective(
      "#pragma omp target data map(to: a dist_data(duplicate))", c_options);
  if (rejected_dist_data.success() ||
      !hasDiagnostic(rejected_dist_data,
                     ompparser::DiagnosticCode::UnsupportedExtension)) {
    std::cerr << "default extension policy accepted dist_data\n";
    ok = false;
  }

  ompparser::ParseOptions fortran_options;
  fortran_options.language = ompparser::BaseLanguage::Fortran;
  struct TargetHasDeviceAddrCase {
    const char *input;
    const ompparser::ParseOptions *options;
  };
  const TargetHasDeviceAddrCase target_has_device_addr_cases[] = {
      {"#pragma omp target parallel has_device_addr(p)", &c_options},
      {"#pragma omp target parallel for has_device_addr(p)", &c_options},
      {"!$omp target parallel do has_device_addr(p)", &fortran_options},
      {"#pragma omp target parallel for simd has_device_addr(p)", &c_options},
      {"!$omp target parallel do simd has_device_addr(p)", &fortran_options},
      {"#pragma omp target parallel loop has_device_addr(p)", &c_options},
      {"#pragma omp target parallel loop simd has_device_addr(p)", &c_options},
      {"#pragma omp target loop has_device_addr(p)", &c_options},
      {"#pragma omp target loop simd has_device_addr(p)", &c_options},
      {"#pragma omp target simd has_device_addr(p)", &c_options},
      {"#pragma omp target teams has_device_addr(p)", &c_options},
      {"#pragma omp target teams workdistribute has_device_addr(p)",
       &c_options},
      {"#pragma omp target teams distribute has_device_addr(p)", &c_options},
      {"#pragma omp target teams distribute simd has_device_addr(p)",
       &c_options},
      {"#pragma omp target teams loop has_device_addr(p)", &c_options},
      {"#pragma omp target teams loop simd has_device_addr(p)", &c_options},
      {"#pragma omp target teams distribute parallel for has_device_addr(p)",
       &c_options},
      {"!$omp target teams distribute parallel do has_device_addr(p)",
       &fortran_options},
      {"#pragma omp target teams distribute parallel loop "
       "has_device_addr(p)",
       &c_options},
      {"#pragma omp target teams distribute parallel for simd "
       "has_device_addr(p)",
       &c_options},
      {"!$omp target teams distribute parallel do simd has_device_addr(p)",
       &fortran_options},
      {"#pragma omp target teams distribute parallel loop simd "
       "has_device_addr(p)",
       &c_options}};
  for (const TargetHasDeviceAddrCase &entry : target_has_device_addr_cases) {
    ok =
        expectText(entry.input, entry.input, *entry.options, entry.input) && ok;
  }
  ok = expectText("dispatch_payloads",
                  "#pragma omp dispatch has_device_addr(p) interop(obj)",
                  c_options,
                  "#pragma omp dispatch has_device_addr(p) interop(obj)") &&
       ok;
  ok = expectText("fortran_literal_fidelity",
                  "!$OMP PARALLEL IF(NAME == \"AbC\")", fortran_options,
                  "!$omp parallel if (NAME == \"AbC\")") &&
       ok;
  ok = expectText("fortran_leading_whitespace", "  !$omp parallel",
                  fortran_options, "!$omp parallel") &&
       ok;
  ok = expectText("fortran_doubled_quote", "!$omp error message('A,B:''C')",
                  fortran_options, "!$omp error message('A,B:''C')") &&
       ok;
  ok = expectText("end_critical_name", "!$omp end critical(Guard)",
                  fortran_options, "!$omp end critical (Guard)") &&
       ok;
  ok = expectText("end_single_copyprivate",
                  "!$omp end single copyprivate(value)", fortran_options,
                  "!$omp end single copyprivate(value)") &&
       ok;
  ok = expectText("end_single_nowait", "!$omp end single nowait",
                  fortran_options, "!$omp end single nowait") &&
       ok;
  ok = expectText("compound_end_nowait", "!$omp end do simd nowait",
                  fortran_options, "!$omp end do simd nowait") &&
       ok;
  ok = expectText("kind_only_end_target_data", "!$omp end target data",
                  fortran_options, "!$omp end target data") &&
       ok;

  ompparser::ParseResult incompatible_end_single_clauses =
      ompparser::parseDirective("!$omp end single copyprivate(value) nowait",
                                fortran_options);
  if (incompatible_end_single_clauses.success() ||
      !hasDiagnostic(incompatible_end_single_clauses,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr
        << "copyprivate and nowait were accepted together on end single\n";
    ok = false;
  }

  ompparser::ParseResult incompatible_single_clauses =
      ompparser::parseDirective("#pragma omp single copyprivate(value) nowait",
                                c_options);
  if (incompatible_single_clauses.success() ||
      !hasDiagnostic(incompatible_single_clauses,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "copyprivate and nowait were accepted together on single\n";
    ok = false;
  }

  const std::string invalid_fortran_applicability_inputs[] = {
      "!$omp parallel do nowait",
      "!$omp parallel do simd nowait",
      "!$omp distribute parallel do nowait",
      "!$omp distribute parallel do simd nowait",
      "!$omp teams distribute parallel do nowait",
      "!$omp teams distribute parallel do simd nowait",
      "!$omp end teams distribute parallel do nowait",
      "!$omp end teams distribute parallel do simd nowait"};
  for (const std::string &input : invalid_fortran_applicability_inputs) {
    if (ompparser::parseDirective(input, fortran_options).success()) {
      std::cerr << "invalid Fortran directive/clause combination was accepted: "
                << input << "\n";
      ok = false;
    }
  }

  ompparser::ParseResult duplicate_threads = ompparser::parseDirective(
      "#pragma omp parallel num_threads(2) num_threads(3)", c_options);
  if (duplicate_threads.success() ||
      !hasDiagnostic(duplicate_threads,
                     ompparser::DiagnosticCode::DuplicateClause)) {
    std::cerr << "duplicate num_threads was not rejected\n";
    ok = false;
  }

  const std::string duplicate_nested_directive_inputs[] = {
      "#pragma omp metadirective "
      "when(user={condition(1)}: parallel num_threads(2) num_threads(4))",
      "#pragma omp metadirective "
      "otherwise(parallel num_threads(2) num_threads(4))",
      "#pragma omp metadirective "
      "default(parallel num_threads(2) num_threads(4))",
      "#pragma omp declare variant(foo) "
      "match(construct={parallel(num_threads(2) num_threads(4))})"};
  for (const std::string &input : duplicate_nested_directive_inputs) {
    ompparser::ParseResult result = ompparser::parseDirective(input, c_options);
    if (result.success() ||
        !hasDiagnostic(result, ompparser::DiagnosticCode::DuplicateClause)) {
      std::cerr << "duplicate clause on nested directive was accepted: "
                << input << "\n";
      ok = false;
    }
  }

  ompparser::ParseResult duplicate_paired_target = ompparser::parseDirective(
      "#pragma omp target nowait nowait end", c_options);
  if (duplicate_paired_target.success() ||
      !hasDiagnostic(duplicate_paired_target,
                     ompparser::DiagnosticCode::DuplicateClause)) {
    std::cerr << "duplicate clause on a paired target directive was accepted\n";
    ok = false;
  }

  ompparser::ParseResult invalid_paired_target = ompparser::parseDirective(
      "#pragma omp target defaultmap(to: all) defaultmap(from: scalar) end",
      c_options);
  if (invalid_paired_target.success() ||
      !hasDiagnostic(invalid_paired_target,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "invalid clause on a paired target directive was accepted\n";
    ok = false;
  }

  ompparser::ParseResult nested_extension = ompparser::parseDirective(
      "#pragma omp metadirective "
      "when(user={condition(1)}: target map(to: a dist_data(duplicate)))",
      c_options);
  if (nested_extension.success() ||
      !hasDiagnostic(nested_extension,
                     ompparser::DiagnosticCode::UnsupportedExtension)) {
    std::cerr << "nested registered extension bypassed extension policy\n";
    ok = false;
  }
  ok = expectSuccess("nested_registered_extension",
                     "#pragma omp metadirective "
                     "when(user={condition(1)}: target "
                     "map(to: a dist_data(duplicate)))",
                     registered_c_options) &&
       ok;

  OpenMPDirective duplicate_ast(OMPD_parallel);
  auto *first_num_threads = duplicate_ast.addOpenMPClause(OMPC_num_threads);
  auto *second_num_threads = duplicate_ast.addOpenMPClause(OMPC_num_threads);
  if (first_num_threads != nullptr) {
    first_num_threads->addLangExpr("2", OMPC_CLAUSE_SEP_space, 1, 1,
                                   OMP_EXPR_PARSE_expression);
  }
  if (second_num_threads != nullptr) {
    second_num_threads->addLangExpr("3", OMPC_CLAUSE_SEP_space, 1, 1,
                                    OMP_EXPR_PARSE_expression);
  }
  const auto *num_thread_occurrences =
      duplicate_ast.findClauses(OMPC_num_threads);
  if (first_num_threads == second_num_threads ||
      num_thread_occurrences == nullptr ||
      num_thread_occurrences->size() != 2 ||
      duplicate_ast.getClausesInOriginalOrder()->size() != 2 ||
      ompparser::validate(duplicate_ast).success()) {
    std::cerr << "duplicate unique occurrences were merged or hidden\n";
    ok = false;
  }

  OpenMPDirective cyclic_variant_ast(OMPD_metadirective);
  auto *cyclic_when = dynamic_cast<OpenMPWhenClause *>(
      cyclic_variant_ast.addOpenMPClause(OMPC_when));
  if (cyclic_when == nullptr) {
    std::cerr << "failed to construct cyclic variant AST test\n";
    ok = false;
  } else {
    cyclic_when->setVariantDirective(&cyclic_variant_ast);
    if (ompparser::validate(cyclic_variant_ast).success() ||
        ompparser::unparse(cyclic_variant_ast).success()) {
      std::cerr << "cyclic variant AST passed validation\n";
      ok = false;
    }
  }

  OpenMPDirective missing_default_variant_ast(OMPD_metadirective);
  auto *missing_default_variant = dynamic_cast<OpenMPDefaultClause *>(
      missing_default_variant_ast.addOpenMPClause(OMPC_default,
                                                  OMPC_DEFAULT_variant));
  if (missing_default_variant == nullptr) {
    std::cerr << "failed to construct missing default variant AST test\n";
    ok = false;
  } else {
    ompparser::ValidationResult validation =
        ompparser::validate(missing_default_variant_ast);
    ompparser::UnparseResult unparsed =
        ompparser::unparse(missing_default_variant_ast);
    if (validation.success() ||
        !hasDiagnostic(validation, ompparser::DiagnosticCode::InvalidAst) ||
        unparsed.success() ||
        !hasDiagnostic(unparsed, ompparser::DiagnosticCode::InvalidAst)) {
      std::cerr << "missing default variant passed validation or unparse\n";
      ok = false;
    }
  }

  ompparser::ParseResult duplicate_schedule = ompparser::parseDirective(
      "#pragma omp for schedule(static) schedule(dynamic, 4)", c_options);
  if (duplicate_schedule.success() ||
      !hasDiagnostic(duplicate_schedule,
                     ompparser::DiagnosticCode::DuplicateClause)) {
    std::cerr << "duplicate schedule was not rejected\n";
    ok = false;
  }

  ompparser::ParseResult duplicate_defaultmap =
      ompparser::parseDirective("#pragma omp target defaultmap(to: scalar) "
                                "defaultmap(from: scalar)",
                                c_options);
  if (duplicate_defaultmap.success() ||
      !hasDiagnostic(duplicate_defaultmap,
                     ompparser::DiagnosticCode::DuplicateClause)) {
    std::cerr << "duplicate schema-unique defaultmap was not rejected\n";
    ok = false;
  }

  ompparser::ParseResult invalid_end_clause = ompparser::parseDirective(
      "!$omp end distribute private(value)", fortran_options);
  if (invalid_end_clause.success() ||
      !hasDiagnostic(invalid_end_clause,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "non-end clause was accepted on a paired end directive\n";
    ok = false;
  }

  ompparser::ParseResult actionless_interop =
      ompparser::parseDirective("#pragma omp interop nowait", c_options);
  if (actionless_interop.success() ||
      !hasDiagnostic(actionless_interop,
                     ompparser::DiagnosticCode::InvalidDirective)) {
    std::cerr << "interop without an action clause was accepted\n";
    ok = false;
  }

  ompparser::ParseResult interop_depend_without_targetsync =
      ompparser::parseDirective(
          "#pragma omp interop init(target: object) depend(in: value)",
          c_options);
  if (interop_depend_without_targetsync.success() ||
      !hasDiagnostic(interop_depend_without_targetsync,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "interop depend without targetsync was accepted\n";
    ok = false;
  }
  ok = expectSuccess(
           "interop_targetsync_depend",
           "#pragma omp interop init(targetsync: object) depend(in: value)",
           c_options) &&
       ok;
  ok = expectSuccess("interop_inferred_targetsync_depend",
                     "#pragma omp interop use(object) depend(in: value)",
                     c_options) &&
       ok;

  ompparser::ParseResult argumentless_interop_destroy =
      ompparser::parseDirective("#pragma omp interop destroy", c_options);
  if (argumentless_interop_destroy.success() ||
      !hasDiagnostic(argumentless_interop_destroy,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "interop destroy without an operand was accepted\n";
    ok = false;
  }
  ok = expectSuccess("interop_destroy_operand",
                     "#pragma omp interop destroy(object)", c_options) &&
       ok;
  ok = expectSuccess("depobj_implicit_destroy_operand",
                     "#pragma omp depobj(object) destroy", c_options) &&
       ok;

  OpenMPDirective malformed_ast(OMPD_target);
  malformed_ast.addOpenMPClause(OMPC_device);
  ompparser::ValidationResult malformed_validation =
      ompparser::validate(malformed_ast);
  if (malformed_validation.success() ||
      ompparser::unparse(malformed_ast).success()) {
    std::cerr << "mistyped clause construction did not poison the AST\n";
    ok = false;
  }

  OpenMPEndDirective unpaired_end_ast;
  if (ompparser::validate(unpaired_end_ast).success() ||
      ompparser::unparse(unpaired_end_ast).success()) {
    std::cerr << "unpaired end AST passed semantic validation\n";
    ok = false;
  }

  OpenMPEndDirective incompatible_end_single_ast;
  incompatible_end_single_ast.setPairedDirective(
      std::make_unique<OpenMPDirective>(OMPD_single));
  auto *copyprivate =
      incompatible_end_single_ast.addOpenMPClause(OMPC_copyprivate);
  if (copyprivate != nullptr) {
    copyprivate->addLangExpr("value", OMPC_CLAUSE_SEP_space, 1, 1,
                             OMP_EXPR_PARSE_variable_list);
  }
  incompatible_end_single_ast.addOpenMPClause(OMPC_nowait);
  ompparser::ValidationResult incompatible_end_single_validation =
      ompparser::validate(incompatible_end_single_ast);
  ompparser::UnparseResult incompatible_end_single_unparse =
      ompparser::unparse(incompatible_end_single_ast);
  if (incompatible_end_single_validation.success() ||
      !hasDiagnostic(incompatible_end_single_validation,
                     ompparser::DiagnosticCode::InvalidClause) ||
      incompatible_end_single_unparse.success() ||
      !hasDiagnostic(incompatible_end_single_unparse,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "copyprivate and nowait passed programmatic end single "
                 "validation or unparse\n";
    ok = false;
  }

  OpenMPEndDirective malformed_paired_target_ast;
  auto paired_target = std::make_unique<OpenMPDirective>(OMPD_target);
  paired_target->addOpenMPClause(OMPC_nowait);
  paired_target->addOpenMPClause(OMPC_nowait);
  auto *paired_shared = paired_target->addOpenMPClause(OMPC_shared);
  if (paired_shared != nullptr) {
    paired_shared->addLangExpr("value", OMPC_CLAUSE_SEP_space, 1, 1,
                               OMP_EXPR_PARSE_variable_list);
  }
  malformed_paired_target_ast.setPairedDirective(std::move(paired_target));
  ompparser::ValidationResult malformed_paired_target_validation =
      ompparser::validate(malformed_paired_target_ast);
  ompparser::UnparseResult malformed_paired_target_unparse =
      ompparser::unparse(malformed_paired_target_ast);
  if (malformed_paired_target_validation.success() ||
      !hasDiagnostic(malformed_paired_target_validation,
                     ompparser::DiagnosticCode::DuplicateClause) ||
      !hasDiagnostic(malformed_paired_target_validation,
                     ompparser::DiagnosticCode::InvalidClause) ||
      malformed_paired_target_unparse.success() ||
      !hasDiagnostic(malformed_paired_target_unparse,
                     ompparser::DiagnosticCode::DuplicateClause) ||
      !hasDiagnostic(malformed_paired_target_unparse,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "malformed paired target AST passed validation or unparse\n";
    ok = false;
  }

  OpenMPDirective empty_private_ast(OMPD_parallel);
  empty_private_ast.addOpenMPClause(OMPC_private);
  if (ompparser::validate(empty_private_ast).success() ||
      ompparser::unparse(empty_private_ast).success()) {
    std::cerr << "empty required clause payload passed validation\n";
    ok = false;
  }

  struct EmptyPayloadCase {
    const char *label;
    OpenMPDirectiveKind directive;
    OpenMPClauseKind clause;
  };
  const EmptyPayloadCase empty_payload_cases[] = {
      {"graph_id", OMPD_taskgraph, OMPC_graph_id},
      {"threadset", OMPD_task, OMPC_threadset},
      {"local", OMPD_declare_target, OMPC_local},
      {"holds", OMPD_assume, OMPC_holds},
      {"combiner", OMPD_declare_reduction, OMPC_combiner},
      {"link", OMPD_declare_target, OMPC_link},
      {"enter", OMPD_declare_target, OMPC_enter},
      {"use", OMPD_interop, OMPC_use},
      {"interop_destroy", OMPD_interop, OMPC_destroy},
      {"dispatch_interop", OMPD_dispatch, OMPC_interop},
      {"absent", OMPD_assume, OMPC_absent},
      {"contains", OMPD_assume, OMPC_contains}};
  for (const EmptyPayloadCase &entry : empty_payload_cases) {
    OpenMPDirective empty_payload_ast(entry.directive);
    empty_payload_ast.addOpenMPClause(entry.clause);
    if (ompparser::validate(empty_payload_ast).success() ||
        ompparser::unparse(empty_payload_ast).success()) {
      std::cerr << "empty " << entry.label << " payload passed validation\n";
      ok = false;
    }
  }

  OpenMPDirective forbidden_clause_ast(OMPD_barrier);
  auto *forbidden_private = forbidden_clause_ast.addOpenMPClause(OMPC_private);
  if (forbidden_private != nullptr) {
    forbidden_private->addLangExpr("value", OMPC_CLAUSE_SEP_space, 1, 1,
                                   OMP_EXPR_PARSE_variable_list);
  }
  if (ompparser::validate(forbidden_clause_ast).success() ||
      ompparser::unparse(forbidden_clause_ast).success()) {
    std::cerr << "clause on a clause-free directive passed validation\n";
    ok = false;
  }

  OpenMPDirective misplaced_clause_ast(OMPD_parallel);
  auto *misplaced_map = misplaced_clause_ast.addOpenMPClause(OMPC_map);
  if (misplaced_map != nullptr) {
    misplaced_map->addLangExpr("value", OMPC_CLAUSE_SEP_space, 1, 1,
                               OMP_EXPR_PARSE_variable_list);
  }
  if (ompparser::validate(misplaced_clause_ast).success() ||
      ompparser::unparse(misplaced_clause_ast).success()) {
    std::cerr << "misplaced clause on non-clause-free AST passed validation\n";
    ok = false;
  }

  OpenMPDirective invalid_distribute_simd_ast(OMPD_distribute_simd);
  auto *invalid_num_threads =
      invalid_distribute_simd_ast.addOpenMPClause(OMPC_num_threads);
  if (invalid_num_threads != nullptr) {
    invalid_num_threads->addLangExpr("2", OMPC_CLAUSE_SEP_space, 1, 1,
                                     OMP_EXPR_PARSE_expression);
  }
  ompparser::ValidationResult invalid_distribute_simd_validation =
      ompparser::validate(invalid_distribute_simd_ast);
  ompparser::UnparseResult invalid_distribute_simd_unparse =
      ompparser::unparse(invalid_distribute_simd_ast);
  if (invalid_distribute_simd_validation.success() ||
      !hasDiagnostic(invalid_distribute_simd_validation,
                     ompparser::DiagnosticCode::InvalidClause) ||
      invalid_distribute_simd_unparse.success() ||
      !hasDiagnostic(invalid_distribute_simd_unparse,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "parallel-only clause on distribute simd AST passed "
                 "validation or unparse\n";
    ok = false;
  }

  const OpenMPDirectiveKind invalid_nowait_directives[] = {
      OMPD_parallel_for,
      OMPD_distribute_parallel_for,
      OMPD_distribute_parallel_for_simd,
      OMPD_teams_distribute_parallel_for,
      OMPD_teams_distribute_parallel_for_simd,
      OMPD_teams_distribute_parallel_do,
      OMPD_teams_distribute_parallel_do_simd,
      OMPD_distribute_parallel_loop,
      OMPD_distribute_parallel_loop_simd,
      OMPD_teams_distribute_parallel_loop,
      OMPD_teams_distribute_parallel_loop_simd};
  for (OpenMPDirectiveKind kind : invalid_nowait_directives) {
    OpenMPDirective invalid_nowait_ast(kind);
    invalid_nowait_ast.addOpenMPClause(OMPC_nowait);
    ompparser::ValidationResult validation =
        ompparser::validate(invalid_nowait_ast);
    ompparser::UnparseResult unparsed = ompparser::unparse(invalid_nowait_ast);
    if (validation.success() ||
        !hasDiagnostic(validation, ompparser::DiagnosticCode::InvalidClause) ||
        unparsed.success() ||
        !hasDiagnostic(unparsed, ompparser::DiagnosticCode::InvalidClause)) {
      std::cerr << "nowait on an ineligible parallel-loop compound AST passed "
                   "validation or unparse\n";
      ok = false;
    }
  }

  OpenMPDirective missing_required_clause_ast(OMPD_split);
  if (ompparser::validate(missing_required_clause_ast).success()) {
    std::cerr << "missing required clause passed validation\n";
    ok = false;
  }

  OpenMPDirective exclusive_clause_ast(OMPD_unroll);
  exclusive_clause_ast.addOpenMPClause(OMPC_full);
  exclusive_clause_ast.addOpenMPClause(OMPC_partial);
  if (ompparser::validate(exclusive_clause_ast).success()) {
    std::cerr << "mutually exclusive clauses passed validation\n";
    ok = false;
  }

  OpenMPDirective empty_specialized_payload_ast(OMPD_target);
  empty_specialized_payload_ast.addOpenMPClause(OMPC_uses_allocators);
  if (ompparser::validate(empty_specialized_payload_ast).success()) {
    std::cerr << "empty typed specialized payload passed validation\n";
    ok = false;
  }

  ompparser::ParseResult occurrences = ompparser::parseDirective(
      "#pragma omp parallel private(a) private(b)", c_options);
  if (!occurrences.success()) {
    std::cerr << "repeatable private clauses failed to parse\n";
    ok = false;
  } else {
    const auto *private_clauses =
        occurrences.directive->findClauses(OMPC_private);
    if (!private_clauses || private_clauses->size() != 2 ||
        occurrences.directive->getClausesInOriginalOrder()->size() != 2) {
      std::cerr << "private occurrences were merged or hidden\n";
      ok = false;
    }
    const std::size_t kind_count =
        occurrences.directive->getAllClauses().size();
    const std::size_t order_count =
        occurrences.directive->getClausesInOriginalOrder()->size();
    ompparser::UnparseResult rendered =
        ompparser::unparse(*occurrences.directive);
    ompparser::DotResult first_dot = ompparser::toDot(*occurrences.directive);
    ompparser::DotResult second_dot = ompparser::toDot(*occurrences.directive);
    if (!rendered.success() || !first_dot.success() || !second_dot.success() ||
        first_dot.text != second_dot.text ||
        occurrences.directive->getAllClauses().size() != kind_count ||
        occurrences.directive->getClausesInOriginalOrder()->size() !=
            order_count) {
      std::cerr << "unparse mutated the AST\n";
      ok = false;
    }
  }

  ompparser::ParseResult invalid =
      ompparser::parseDirective("#pragma omp parallel private(", c_options);
  ompparser::ParseResult recovery = ompparser::parseDirective(
      "#pragma omp parallel private(valid)", c_options);
  if (invalid.success() || !recovery.success()) {
    std::cerr << "invalid-to-valid recovery failed\n";
    ok = false;
  }

  ompparser::ParseResult rejected_extension =
      ompparser::parseDirective("!$ompx vendor_payload", fortran_options);
  if (rejected_extension.success() ||
      !hasDiagnostic(rejected_extension,
                     ompparser::DiagnosticCode::UnsupportedExtension)) {
    std::cerr << "default extension policy accepted OMPX\n";
    ok = false;
  }
  fortran_options.extensions = ompparser::ExtensionPolicy::AllowRegistered;
  if (!ompparser::parseDirective("!$ompx vendor_payload", fortran_options)
           .success()) {
    std::cerr << "registered extension policy rejected OMPX\n";
    ok = false;
  }

  RecordingHooks hooks;
  c_options.host_hooks = &hooks;
  const std::string hooked_input =
      "#pragma omp parallel if(ExactCase) private(Value)";
  ompparser::ParseResult hooked =
      ompparser::parseDirective(hooked_input, c_options);
  if (!hooked.success() || !hooked.context_checks_complete ||
      !hooks.parsed_recursively || hooks.fragments.size() != 2 ||
      hooks.fragments[0].spelling != "ExactCase" ||
      hooks.fragments[1].spelling != "Value") {
    std::cerr << "host hook fidelity or recursive parsing failed\n";
    ok = false;
  }
  for (const ompparser::HostFragment &fragment : hooks.fragments) {
    if (!hasSourceFaithfulRange(fragment, hooked_input)) {
      std::cerr << "host fragment source range is not source-faithful\n";
      ok = false;
    }
  }

  RejectingHooks rejecting_hooks;
  c_options.host_hooks = &rejecting_hooks;
  ompparser::ParseResult context_rejected = ompparser::parseDirective(
      "#pragma omp parallel private(contextual)", c_options);
  if (context_rejected.success() || !context_rejected.context_checks_complete ||
      !hasDiagnostic(context_rejected,
                     ompparser::DiagnosticCode::HostLanguageError)) {
    std::cerr << "host contextual validation was not enforced\n";
    ok = false;
  }

  RecordingHooks iterator_hooks;
  c_options.host_hooks = &iterator_hooks;
  ompparser::ParseResult iterator_hooked = ompparser::parseDirective(
      "#pragma omp target map(iterator(i=0:N), tofrom: a[i])", c_options);
  const auto *map_clauses =
      iterator_hooked.success()
          ? iterator_hooked.directive->findClauses(OMPC_map)
          : nullptr;
  const auto *map_clause =
      map_clauses && !map_clauses->empty()
          ? dynamic_cast<const OpenMPMapClause *>(map_clauses->front())
          : nullptr;
  if (!map_clause || map_clause->getIterators().size() != 1 ||
      iterator_hooks.fragments.size() != 4 ||
      iterator_hooks.fragments[0].spelling != "i" ||
      iterator_hooks.fragments[1].spelling != "0" ||
      iterator_hooks.fragments[2].spelling != "N" ||
      iterator_hooks.fragments[3].spelling != "a[i]" ||
      !map_clause->getIterators()[0].variable.semantic ||
      !map_clause->getExpressionNode(0)) {
    std::cerr << "typed iterator host-fragment extraction failed\n";
    std::cerr << "  parse success: " << iterator_hooked.success() << "\n";
    for (const auto &diagnostic : iterator_hooked.diagnostics) {
      std::cerr << "  diagnostic: " << diagnostic.message << "\n";
    }
    for (const auto &fragment : iterator_hooks.fragments) {
      std::cerr << "  fragment: '" << fragment.spelling << "'\n";
    }
    ok = false;
  }

  RecordingHooks directive_hooks;
  c_options.host_hooks = &directive_hooks;
  const std::string directive_hooked_input =
      "#pragma omp threadprivate(  GlobalState  )";
  ompparser::ParseResult directive_hooked =
      ompparser::parseDirective(directive_hooked_input, c_options);
  if (!directive_hooked.success() || directive_hooks.fragments.size() != 1 ||
      directive_hooks.fragments.front().spelling != "GlobalState" ||
      directive_hooks.fragments.front().role !=
          ompparser::HostFragmentRole::Variable) {
    std::cerr << "directive-level host fragments were not visited\n";
    ok = false;
  } else {
    const ompparser::SourceRange &range =
        directive_hooks.fragments.front().range;
    if (range.end.offset < range.begin.offset ||
        range.end.offset > directive_hooked_input.size() ||
        directive_hooked_input.substr(range.begin.offset,
                                      range.end.offset - range.begin.offset) !=
            directive_hooks.fragments.front().spelling) {
      std::cerr << "trimmed directive-level fragment range is not "
                   "source-faithful\n";
      std::cerr << "  spelling: '" << directive_hooks.fragments.front().spelling
                << "'\n";
      std::cerr << "  offsets: " << range.begin.offset << ".."
                << range.end.offset << "\n";
      if (range.begin.offset <= directive_hooked_input.size() &&
          range.end.offset >= range.begin.offset &&
          range.end.offset <= directive_hooked_input.size()) {
        std::cerr << "  source: '"
                  << directive_hooked_input.substr(range.begin.offset,
                                                   range.end.offset -
                                                       range.begin.offset)
                  << "'\n";
      }
      ok = false;
    }
  }

  RecordingHooks selector_hooks;
  c_options.host_hooks = &selector_hooks;
  ompparser::ParseResult selector_hooked =
      ompparser::parseDirective("#pragma omp metadirective "
                                "when(user={condition(score(20): Enabled)}:)",
                                c_options);
  if (!selector_hooked.success() || selector_hooks.fragments.size() != 2 ||
      selector_hooks.fragments[0].spelling != "20" ||
      selector_hooks.fragments[1].spelling != "Enabled") {
    std::cerr << "context-selector host fragments were not typed or visited\n";
    ok = false;
  }

  struct VariantHookCase {
    const char *label;
    const char *input;
    OpenMPClauseKind clause_kind;
    const char *selector_fragment;
    const char *variant_fragment;
  };
  const VariantHookCase variant_hook_cases[] = {
      {"when",
       "#pragma omp metadirective "
       "when(user={condition(WhenCondition)}: parallel "
       "private(WhenValue))",
       OMPC_when, "WhenCondition", "WhenValue"},
      {"otherwise",
       "#pragma omp metadirective "
       "otherwise(parallel private(OtherwiseValue))",
       OMPC_otherwise, nullptr, "OtherwiseValue"},
      {"default",
       "#pragma omp metadirective "
       "default(parallel private(DefaultValue))",
       OMPC_default, nullptr, "DefaultValue"}};
  for (const VariantHookCase &test : variant_hook_cases) {
    RecordingHooks variant_hooks;
    c_options.host_hooks = &variant_hooks;
    ompparser::ParseResult variant_hooked =
        ompparser::parseDirective(test.input, c_options);
    const auto *clauses =
        variant_hooked.success()
            ? variant_hooked.directive->findClauses(test.clause_kind)
            : nullptr;
    const OpenMPClause *variant_clause =
        clauses && !clauses->empty() ? clauses->front() : nullptr;
    const OpenMPDirective *variant_directive = nullptr;
    if (const auto *when_clause =
            dynamic_cast<const OpenMPWhenClause *>(variant_clause)) {
      variant_directive = when_clause->getVariantDirective();
    } else if (const auto *otherwise_clause =
                   dynamic_cast<const OpenMPOtherwiseClause *>(
                       variant_clause)) {
      variant_directive = otherwise_clause->getVariantDirective();
    } else if (const auto *default_clause =
                   dynamic_cast<const OpenMPDefaultClause *>(variant_clause)) {
      variant_directive = default_clause->getVariantDirective();
    }
    const auto *private_clauses =
        variant_directive ? variant_directive->findClauses(OMPC_private)
                          : nullptr;
    const OpenMPClause *private_clause =
        private_clauses && !private_clauses->empty() ? private_clauses->front()
                                                     : nullptr;
    const std::size_t expected_count = test.selector_fragment ? 2 : 1;
    const bool fragments_match =
        variant_hooks.fragments.size() == expected_count &&
        (!test.selector_fragment ||
         variant_hooks.fragments.front().spelling == test.selector_fragment) &&
        !variant_hooks.fragments.empty() &&
        variant_hooks.fragments.back().spelling == test.variant_fragment;
    if (!variant_hooked.success() || !fragments_match || !private_clause ||
        !private_clause->getExpressionNode(0)) {
      std::cerr << test.label
                << " variant directive host fragments were not visited\n";
      ok = false;
    }
  }

  struct SplitRangeCase {
    const char *label;
    const char *input;
    ompparser::BaseLanguage language;
    std::vector<std::string> expected_fragments;
    ompparser::ExtensionPolicy extensions =
        ompparser::ExtensionPolicy::RejectUnknown;
  };
  const SplitRangeCase split_range_cases[] = {
      {"map locator",
       "#pragma omp target map(to: MapValue[MapIndex])",
       ompparser::BaseLanguage::C,
       {"MapValue[MapIndex]"}},
      {"target update locators",
       "#pragma omp target update to(ToValue[ToIndex]) "
       "from(FromValue[FromIndex])",
       ompparser::BaseLanguage::C,
       {"ToValue[ToIndex]", "FromValue[FromIndex]"}},
      {"map locator with dist_data",
       "#pragma omp target map(to: DistValue[DistIndex] "
       "dist_data(block(ChunkSize)))",
       ompparser::BaseLanguage::C,
       {"DistValue[DistIndex]", "ChunkSize"},
       ompparser::ExtensionPolicy::AllowRegistered},
      {"compact parallel do",
       "!$omp paralleldo private(ParallelValue)",
       ompparser::BaseLanguage::Fortran,
       {"ParallelValue"}},
      {"declare target underscore",
       "#pragma omp declare_target(SymbolValue)",
       ompparser::BaseLanguage::C,
       {"SymbolValue"}},
      {"target enter data underscore",
       "#pragma omp target_enter_data map(to: EnterMap) if(EnterValue)",
       ompparser::BaseLanguage::C,
       {"EnterValue"}},
      {"target exit data underscore",
       "#pragma omp target_exit_data map(from: ExitMap) if(ExitValue)",
       ompparser::BaseLanguage::C,
       {"ExitValue"}},
      {"paired target suffix end",
       "#pragma omp target if(BoundaryCondition) end",
       ompparser::BaseLanguage::C,
       {"BoundaryCondition"}},
      {"init operand",
       "#pragma omp interop init(target: InitOperand)",
       ompparser::BaseLanguage::C,
       {"InitOperand"}},
      {"init depinfo locator",
       "#pragma omp depobj(Handle) init(in(Locator[Index]): InitVar)",
       ompparser::BaseLanguage::C,
       {"Locator[Index]", "InitVar"}},
      {"linear modifier delimiter",
       "#pragma omp for linear(val \t (LinearValue): 2)",
       ompparser::BaseLanguage::C,
       {"2", "LinearValue"}},
      {"compact unroll partial",
       "#pragma omp tile sizes(1) apply(unrollpartial(CompactFactor))",
       ompparser::BaseLanguage::C,
       {"1", "CompactFactor"}},
      {"compact unroll full",
       "#pragma omp tile sizes(1) apply(unrollfull, LoopLabel: reverse)",
       ompparser::BaseLanguage::C,
       {"1", "LoopLabel"}}};
  for (const SplitRangeCase &test : split_range_cases) {
    RecordingHooks range_hooks;
    ompparser::ParseOptions options;
    options.language = test.language;
    options.extensions = test.extensions;
    options.host_hooks = &range_hooks;
    const std::string input(test.input);
    ompparser::ParseResult range_hooked =
        ompparser::parseDirective(input, options);
    bool ranges_match = range_hooked.success();
    if (ranges_match) {
      for (const std::string &expected : test.expected_fragments) {
        bool found = false;
        for (const ompparser::HostFragment &fragment : range_hooks.fragments) {
          if (fragment.spelling == expected &&
              hasSourceFaithfulRange(fragment, input)) {
            found = true;
            break;
          }
        }
        if (!found) {
          ranges_match = false;
          break;
        }
      }
    }
    if (!ranges_match) {
      std::cerr << test.label
                << " did not preserve host-fragment source ranges\n";
      std::cerr << "  parse success: " << range_hooked.success() << "\n";
      for (const ompparser::Diagnostic &diagnostic : range_hooked.diagnostics) {
        std::cerr << "  diagnostic: " << diagnostic.message << " at "
                  << diagnostic.range.begin.offset << ".."
                  << diagnostic.range.end.offset << "\n";
      }
      for (const ompparser::HostFragment &fragment : range_hooks.fragments) {
        const std::size_t begin = fragment.range.begin.offset;
        const std::size_t end = fragment.range.end.offset;
        std::cerr << "  fragment: '" << fragment.spelling << "' at " << begin
                  << ".." << end;
        if (end >= begin && end <= input.size()) {
          std::cerr << " from '" << input.substr(begin, end - begin) << "'";
        }
        std::cerr << "\n";
      }
      ok = false;
    }
  }

  const std::string prefer_type_range_input =
      "#pragma omp interop init(prefer_type({fr(\"cuda\")}), target: "
      "InteropObject)";
  ompparser::ParseOptions prefer_type_range_options;
  prefer_type_range_options.language = ompparser::BaseLanguage::C;
  ompparser::ParseResult prefer_type_range = ompparser::parseDirective(
      prefer_type_range_input, prefer_type_range_options);
  const auto *prefer_type_init_clauses =
      prefer_type_range.success()
          ? prefer_type_range.directive->findClauses(OMPC_init)
          : nullptr;
  const auto *prefer_type_init =
      prefer_type_init_clauses && !prefer_type_init_clauses->empty()
          ? dynamic_cast<const OpenMPInitClause *>(
                prefer_type_init_clauses->front())
          : nullptr;
  bool prefer_type_range_matches = false;
  if (prefer_type_init != nullptr) {
    for (const OpenMPInitModifier &modifier :
         prefer_type_init->getModifiers().getModifiers()) {
      if (modifier.category == OpenMPInitModifierCategory::PreferType &&
          modifier.argument.spelling == "{fr(\"cuda\")}" &&
          hasSourceFaithfulRange(modifier.argument, prefer_type_range_input)) {
        prefer_type_range_matches = true;
        break;
      }
    }
  }
  if (!prefer_type_range_matches) {
    std::cerr << "init prefer_type did not preserve its source range\n";
    ok = false;
  }

  std::atomic<bool> threads_ok(true);
  std::vector<std::thread> threads;
  for (int thread_index = 0; thread_index < 8; ++thread_index) {
    threads.emplace_back([thread_index, &threads_ok]() {
      ompparser::ParseOptions options;
      options.language = (thread_index % 2 == 0)
                             ? ompparser::BaseLanguage::C
                             : ompparser::BaseLanguage::Fortran;
      const std::string input = (thread_index % 2 == 0)
                                    ? "#pragma omp parallel private(x)"
                                    : "!$omp parallel private(X)";
      for (int iteration = 0; iteration < 100; ++iteration) {
        if (!ompparser::parseDirective(input, options).success()) {
          threads_ok = false;
          return;
        }
      }
    });
  }
  for (std::thread &thread : threads) {
    thread.join();
  }
  if (!threads_ok) {
    std::cerr << "concurrent parser contexts failed\n";
    ok = false;
  }

  return ok ? 0 : 1;
}
