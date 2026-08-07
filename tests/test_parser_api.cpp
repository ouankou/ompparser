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
#include <stdexcept>
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

struct ExpectedHostFragment {
  const char *spelling;
  OpenMPClauseKind clause_kind;
  OpenMPExprParseMode parse_mode;
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
    for (const ompparser::Diagnostic &diagnostic : parsed.diagnostics) {
      std::cerr << "  " << diagnostic.message << "\n";
    }
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

bool expectHostFragments(const char *label, const std::string &input,
                         const ompparser::ParseOptions &base_options,
                         const std::vector<ExpectedHostFragment> &expected) {
  RecordingHooks hooks;
  ompparser::ParseOptions options = base_options;
  options.host_hooks = &hooks;
  ompparser::ParseResult parsed = ompparser::parseDirective(input, options);
  if (!parsed.success() || !parsed.context_checks_complete ||
      hooks.fragments.size() != expected.size()) {
    std::cerr << label << ": host-fragment count mismatch: got "
              << hooks.fragments.size() << ", expected " << expected.size()
              << "\n";
    for (const ompparser::Diagnostic &diagnostic : parsed.diagnostics) {
      std::cerr << "  diagnostic at " << diagnostic.range.begin.line << ":"
                << diagnostic.range.begin.column << ": " << diagnostic.message
                << "\n";
    }
    for (const ompparser::HostFragment &fragment : hooks.fragments) {
      std::cerr << "  fragment: ('" << fragment.spelling << "', "
                << static_cast<int>(fragment.clause_kind) << ", "
                << static_cast<int>(fragment.parse_mode) << ")\n";
    }
    return false;
  }
  for (std::size_t index = 0; index < expected.size(); ++index) {
    const ompparser::HostFragment &actual = hooks.fragments[index];
    const ExpectedHostFragment &wanted = expected[index];
    if (actual.spelling != wanted.spelling ||
        actual.clause_kind != wanted.clause_kind ||
        actual.parse_mode != wanted.parse_mode ||
        !hasSourceFaithfulRange(actual, input)) {
      std::cerr << label << ": host-fragment mismatch at " << index
                << ": got ('" << actual.spelling << "', "
                << static_cast<int>(actual.clause_kind) << ", "
                << static_cast<int>(actual.parse_mode) << "), expected ('"
                << wanted.spelling << "', "
                << static_cast<int>(wanted.clause_kind) << ", "
                << static_cast<int>(wanted.parse_mode) << ")\n";
      std::cerr << "  range: " << actual.range.begin.offset << ".."
                << actual.range.end.offset;
      if (actual.range.end.offset >= actual.range.begin.offset &&
          actual.range.end.offset <= input.size()) {
        std::cerr << " maps to '"
                  << input.substr(actual.range.begin.offset,
                                  actual.range.end.offset -
                                      actual.range.begin.offset)
                  << "'";
      }
      std::cerr << "\n";
      return false;
    }
  }
  ompparser::UnparseResult unparsed = ompparser::unparse(*parsed.directive);
  if (!unparsed.success()) {
    std::cerr << label << ": typed host-fragment AST did not unparse\n";
    return false;
  }
  return true;
}

} // namespace

int main() {
  bool ok = true;

  ompparser::ParseOptions c_options;
  c_options.language = ompparser::BaseLanguage::C;
  ompparser::ParseOptions cxx_options;
  cxx_options.language = ompparser::BaseLanguage::CXX;
  ompparser::ParseOptions registered_fragment_options = c_options;
  registered_fragment_options.extensions =
      ompparser::ExtensionPolicy::AllowRegistered;
  ok = expectHostFragments(
           "allocate_host_fragment_modes", "#pragma omp allocate(a,b)",
           c_options,
           {{"a", OMPC_unknown, OMP_EXPR_PARSE_variable_list},
            {"b", OMPC_unknown, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectHostFragments("device_wildcard_host_fragment_mode",
                           "#pragma omp target device(*)", c_options,
                           {{"*", OMPC_device, OMP_EXPR_PARSE_verbatim}}) &&
       ok;
  ok = expectHostFragments("spaced_device_wildcard_host_fragment_mode",
                           "#pragma omp target device(* )", c_options,
                           {{"* ", OMPC_device, OMP_EXPR_PARSE_verbatim}}) &&
       ok;
  ok = expectHostFragments("device_modifier_scalar_expression_mode",
                           "#pragma omp target device(ancestor:1)", c_options,
                           {{"1", OMPC_device, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "critical_name_openmp_syntax_mode", "#pragma omp critical(rex_lock)",
           c_options,
           {{"rex_lock", OMPC_unknown, OMP_EXPR_PARSE_openmp_syntax}}) &&
       ok;
  ok = expectHostFragments("requires_extension_is_grammar_owned",
                           "#pragma omp requires ext_vendor_runtime",
                           registered_fragment_options, {}) &&
       ok;
  ok = expectHostFragments(
           "uses_allocators_user_and_traits_are_separate",
           "#pragma omp target uses_allocators(rex_allocator(traits))",
           c_options,
           {{"traits", OMPC_uses_allocators, OMP_EXPR_PARSE_expression},
            {"rex_allocator", OMPC_uses_allocators,
             OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok =
      expectHostFragments(
          "uses_allocators_cxx_qualified_allocator_is_one_fragment",
          "#pragma omp target uses_allocators(ns::pool)", cxx_options,
          {{"ns::pool", OMPC_uses_allocators, OMP_EXPR_PARSE_variable_list}}) &&
      ok;
  ok = expectHostFragments(
           "from_iterator_has_typed_fragment_ownership",
           "#pragma omp target update "
           "from(iterator(unsigned long i = 0:n:step) : values[i])",
           c_options,
           {{"unsigned long", OMPC_from, OMP_EXPR_PARSE_openmp_iterator_type},
            {"i", OMPC_from, OMP_EXPR_PARSE_openmp_iterator_name},
            {"0", OMPC_from, OMP_EXPR_PARSE_expression},
            {"n", OMPC_from, OMP_EXPR_PARSE_expression},
            {"step", OMPC_from, OMP_EXPR_PARSE_expression},
            {"values[i]", OMPC_from, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "doacross_sink_has_one_typed_owner",
           "#pragma omp ordered doacross(sink:i-1, j+1)", c_options,
           {{"i-1", OMPC_doacross, OMP_EXPR_PARSE_variable_list},
            {"j+1", OMPC_doacross, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectHostFragments(
           "looprange_operands_are_expressions",
           "#pragma omp fuse looprange(first + 1, count)", c_options,
           {{"first + 1", OMPC_looprange, OMP_EXPR_PARSE_expression},
            {"count", OMPC_looprange, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "scalar_simd_length_expression_modes",
           "#pragma omp simd safelen(width) simdlen(width-1)", c_options,
           {{"width", OMPC_safelen, OMP_EXPR_PARSE_expression},
            {"width-1", OMPC_simdlen, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "partial_cast_expression_mode",
           "#pragma omp unroll partial((int)(2u + 2u))", c_options,
           {{"(int)(2u + 2u)", OMPC_partial, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "partial_keyword_identifier_expression_mode",
           "#pragma omp unroll partial(unroll_factor)", cxx_options,
           {{"unroll_factor", OMPC_partial, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectText("parameterless_partial_before_apply",
                  "#pragma omp unroll partial apply(reverse)", c_options,
                  "#pragma omp unroll partial apply(reverse)") &&
       ok;
  ok = expectHostFragments(
           "apply_partial_keyword_identifier_expression_mode",
           "#pragma omp tile sizes(4) apply(unroll partial(unroll_factor))",
           cxx_options,
           {{"4", OMPC_sizes, OMP_EXPR_PARSE_expression},
            {"unroll_factor", OMPC_apply, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "sizes_scalar_expression_modes",
           "#pragma omp tile sizes((int)(1u << 1), width + 2)", c_options,
           {{"(int)(1u << 1)", OMPC_sizes, OMP_EXPR_PARSE_expression},
            {"width + 2", OMPC_sizes, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "target_update_mapper_name_mode",
           "#pragma omp target update to(mapper(custom):value)", c_options,
           {{"custom", OMPC_to, OMP_EXPR_PARSE_verbatim},
            {"value", OMPC_to, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "cxx_scope_operator_expression",
           "#pragma omp parallel if(rex::choose())", cxx_options,
           {{"rex::choose()", OMPC_if, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "typed_iterator_host_fragment_modes",
           "#pragma omp target map(iterator(unsigned long i = "
           "f<std::pair<int, long>>(0, 1):n + 1:step(2)), to: a[i])",
           c_options,
           {{"unsigned long", OMPC_map, OMP_EXPR_PARSE_openmp_iterator_type},
            {"i", OMPC_map, OMP_EXPR_PARSE_openmp_iterator_name},
            {"f<std::pair<int, long>>(0, 1)", OMPC_map,
             OMP_EXPR_PARSE_expression},
            {"n + 1", OMPC_map, OMP_EXPR_PARSE_expression},
            {"step(2)", OMPC_map, OMP_EXPR_PARSE_expression},
            {"a[i]", OMPC_map, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "declare_mapper_host_fragment_modes",
           "#pragma omp declare mapper(custom : const MapperRecord value) "
           "map(to: value)",
           c_options,
           {{"custom", OMPC_unknown,
             OMP_EXPR_PARSE_openmp_declare_mapper_identifier},
            {"const MapperRecord", OMPC_unknown,
             OMP_EXPR_PARSE_openmp_declare_mapper_type},
            {"value", OMPC_unknown,
             OMP_EXPR_PARSE_openmp_declare_mapper_variable},
            {"value", OMPC_map, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "allocate_clause_host_fragment_modes",
           "#pragma omp parallel allocate(allocator(select_allocator()), "
           "align(compute_align()): x)",
           c_options,
           {{"select_allocator()", OMPC_allocate, OMP_EXPR_PARSE_expression},
            {"compute_align()", OMPC_allocate, OMP_EXPR_PARSE_expression},
            {"x", OMPC_allocate, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectHostFragments(
           "prefer_type_source_fragment_mode",
           "#pragma omp interop "
           "init(prefer_type(rex_vendor_type), target: object)",
           c_options,
           {{"rex_vendor_type", OMPC_init, OMP_EXPR_PARSE_openmp_source},
            {"object", OMPC_init, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ompparser::ParseResult typed_allocate = ompparser::parseDirective(
      "#pragma omp parallel allocate(allocator(pool), align(64): value)",
      c_options);
  const std::vector<OpenMPClause *> *typed_allocate_clauses =
      typed_allocate.success()
          ? typed_allocate.directive->findClauses(OMPC_allocate)
          : nullptr;
  const auto *typed_allocate_clause =
      typed_allocate_clauses != nullptr && typed_allocate_clauses->size() == 1
          ? dynamic_cast<const OpenMPAllocateClause *>(
                typed_allocate_clauses->front())
          : nullptr;
  if (typed_allocate_clause == nullptr ||
      typed_allocate_clause->getAllocator() != OMPC_ALLOCATE_ALLOCATOR_user ||
      !typed_allocate_clause->usesAllocatorModifierSyntax() ||
      typed_allocate_clause->getUserDefinedAllocator() != "pool" ||
      typed_allocate_clause->getAlignment() != "64") {
    std::cerr << "allocate modifier syntax did not produce one coherent typed "
                 "allocator payload\n";
    ok = false;
  }
  ok =
      expectHostFragments(
          "allocate_clause_raw_string_host_fragment",
          R"omp(#pragma omp parallel allocate(allocator(lookup(R"(foo")")): value))omp",
          cxx_options,
          {{R"omp(lookup(R"(foo")"))omp", OMPC_allocate,
            OMP_EXPR_PARSE_expression},
           {"value", OMPC_allocate, OMP_EXPR_PARSE_variable_list}}) &&
      ok;
  ok = expectHostFragments(
           "reduction_identifier_clause_ownership",
           "#pragma omp parallel reduction(custom: value)", c_options,
           {{"custom", OMPC_reduction, OMP_EXPR_PARSE_openmp_syntax},
            {"value", OMPC_reduction, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ompparser::ParseResult original_private_reduction = ompparser::parseDirective(
      "#pragma omp for reduction(original(private),+: sum_v)", cxx_options);
  const std::vector<OpenMPClause *> *original_private_clauses =
      original_private_reduction.success()
          ? original_private_reduction.directive->findClauses(OMPC_reduction)
          : nullptr;
  const auto *original_private_clause =
      original_private_clauses != nullptr &&
              original_private_clauses->size() == 1
          ? dynamic_cast<const OpenMPReductionClause *>(
                original_private_clauses->front())
          : nullptr;
  if (original_private_clause == nullptr ||
      original_private_clause->getModifier() !=
          OMPC_REDUCTION_MODIFIER_original_private ||
      original_private_clause->getIdentifier() !=
          OMPC_REDUCTION_IDENTIFIER_plus ||
      !original_private_clause->getUserDefinedIdentifier().empty()) {
    std::cerr << "original(private) reduction did not produce one coherent "
                 "typed modifier and operator\n";
    ok = false;
  }
  ompparser::ParseOptions extension_options = c_options;
  extension_options.extensions = ompparser::ExtensionPolicy::AllowRegistered;
  ok = expectHostFragments(
           "dist_data_argument_clause_ownership",
           "#pragma omp target map(to: value dist_data(block(chunk)))",
           extension_options,
           {{"chunk", OMPC_map, OMP_EXPR_PARSE_expression},
            {"value", OMPC_map, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "typed_context_selector_property_modes",
           "#pragma omp metadirective "
           "when(device={arch(\"nvptx\",\"amdgcn\"),kind(cpu,gpu)}, "
           "target_device={device_num(device_id),"
           "isa(\"sm_90\",\"gfx942\"),uid(\"device-7\")}, "
           "implementation={vendor(score(5): amd,llvm),"
           "extension(score(2): ext_a,ext_b),"
           "requires(score(3): unified_shared_memory,"
           "dynamic_allocators(require_enabled),"
           "atomic_default_mem_order(acquire)),"
           "atomic_default_mem_order(score(4): release),"
           "rex_fast(score(6): prop,nested(7))}, "
           "user={condition(score(7): enabled)}: parallel)",
           cxx_options,
           {{"\"nvptx\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"\"amdgcn\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"device_id", OMPC_when, OMP_EXPR_PARSE_expression},
            {"\"sm_90\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"\"gfx942\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"\"device-7\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"5", OMPC_when, OMP_EXPR_PARSE_constant_integer},
            {"2", OMPC_when, OMP_EXPR_PARSE_constant_integer},
            {"ext_a", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"ext_b", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"3", OMPC_when, OMP_EXPR_PARSE_constant_integer},
            {"require_enabled", OMPC_dynamic_allocators,
             OMP_EXPR_PARSE_expression},
            {"4", OMPC_when, OMP_EXPR_PARSE_constant_integer},
            {"6", OMPC_when, OMP_EXPR_PARSE_constant_integer},
            {"prop", OMPC_when, OMP_EXPR_PARSE_verbatim},
            {"nested(7)", OMPC_when, OMP_EXPR_PARSE_verbatim},
            {"7", OMPC_when, OMP_EXPR_PARSE_constant_integer},
            {"enabled", OMPC_when, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "openmp_syntax_fragment_modes",
           "#pragma omp tile sizes(4) apply(grid: custom)", c_options,
           {{"4", OMPC_sizes, OMP_EXPR_PARSE_expression},
            {"grid", OMPC_apply, OMP_EXPR_PARSE_openmp_syntax},
            {"custom", OMPC_apply, OMP_EXPR_PARSE_openmp_syntax}}) &&
       ok;
  ok = expectHostFragments(
           "induction_syntax_fragment_modes",
           "#pragma omp parallel for induction(step(1), user_defined: value)",
           c_options,
           {{"1", OMPC_induction, OMP_EXPR_PARSE_expression},
            {"user_defined", OMPC_induction, OMP_EXPR_PARSE_openmp_syntax},
            {"value", OMPC_induction, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "induction_binding_before_step_modes",
           "#pragma omp parallel for induction(*: induction_value, "
           "step(step_value), induction_value)",
           c_options,
           {{"*", OMPC_induction, OMP_EXPR_PARSE_openmp_syntax},
            {"induction_value", OMPC_induction, OMP_EXPR_PARSE_expression},
            {"step_value", OMPC_induction, OMP_EXPR_PARSE_expression},
            {"induction_value", OMPC_induction, OMP_EXPR_PARSE_expression}}) &&
       ok;

  ompparser::ParseResult induction_sequence = ompparser::parseDirective(
      "#pragma omp parallel for induction(step(1), i: j, passthrough)",
      c_options);
  std::vector<std::string> induction_items;
  if (induction_sequence.success()) {
    const std::vector<OpenMPClause *> *induction_clauses =
        induction_sequence.directive->findClauses(OMPC_induction);
    const auto *induction =
        induction_clauses != nullptr && induction_clauses->size() == 1
            ? dynamic_cast<const OpenMPInductionClause *>(
                  induction_clauses->front())
            : nullptr;
    if (induction != nullptr) {
      induction->visitSpecificationItems(
          [&](OpenMPInductionClause::SpecificationItemKind kind,
              const ompparser::HostFragment *label,
              const ompparser::HostFragment &expression) {
            const char *kind_name = nullptr;
            switch (kind) {
            case OpenMPInductionClause::SpecificationItemKind::Step:
              kind_name = "step";
              break;
            case OpenMPInductionClause::SpecificationItemKind::Binding:
              kind_name = "binding";
              break;
            case OpenMPInductionClause::SpecificationItemKind::Expression:
              kind_name = "expression";
              break;
            }
            induction_items.push_back(
                std::string(kind_name) + ":" +
                (label == nullptr ? std::string() : label->spelling + ":") +
                expression.spelling);
          });
    }
  }
  if (induction_items != std::vector<std::string>{"step:1", "binding:i:j",
                                                  "expression:passthrough"}) {
    std::cerr << "typed induction specification order was not preserved\n";
    ok = false;
  }
  ok = expectHostFragments(
           "quoted_kind_vendor_property_modes",
           "#pragma omp metadirective "
           "when(device={kind(\"cpu\" )}, "
           "implementation={vendor(\"amd\" )}: parallel)",
           cxx_options,
           {{"\"cpu\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"\"amd\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name}}) &&
       ok;
  ok = expectText("escaped_cxx_kind_property_identity",
                  "#pragma omp metadirective "
                  "when(device={kind(\"c\\x70u\")}: parallel)",
                  cxx_options,
                  "#pragma omp metadirective "
                  "when (device = {kind(\"c\\x70u\")} : parallel)") &&
       ok;
  ok = expectSuccess("escaped_c_kind_property_identity",
                     "#pragma omp metadirective "
                     "when(device={kind(\"c\\x70u\")}: parallel)",
                     c_options) &&
       ok;
  const std::string invalid_cxx_context_name_literals[] = {
      "#pragma omp metadirective "
      "when(device={arch('x86')}: parallel)",
      "#pragma omp metadirective "
      "when(device={arch('x86')}: parallel)",
      "#pragma omp metadirective "
      "when(device={kind(cpu,\"c\\x70u\")}: parallel)",
      "#pragma omp metadirective "
      "when(device={arch(name,\"na\\155e\")}: parallel)"};
  const ompparser::ParseOptions *invalid_cxx_context_name_options[] = {
      &c_options, &cxx_options, &cxx_options, &cxx_options};
  for (std::size_t index = 0;
       index < sizeof(invalid_cxx_context_name_literals) /
                   sizeof(invalid_cxx_context_name_literals[0]);
       ++index) {
    if (ompparser::parseDirective(invalid_cxx_context_name_literals[index],
                                  *invalid_cxx_context_name_options[index])
            .success()) {
      std::cerr << "invalid or duplicate decoded C/C++ context name was "
                   "accepted: "
                << invalid_cxx_context_name_literals[index] << "\n";
      ok = false;
    }
  }
  ok = expectHostFragments(
           "prefixed_kind_vendor_property_modes",
           "#pragma omp metadirective "
           "when(device={kind(R\"(cpu)\")}, "
           "implementation={vendor(u8\"amd\")}: parallel)",
           cxx_options,
           {{"R\"(cpu)\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name},
            {"u8\"amd\"", OMPC_when, OMP_EXPR_PARSE_openmp_context_name}}) &&
       ok;
  ok =
      expectHostFragments(
          "requires_host_fragment_modes",
          "#pragma omp requires reverse_offload(can_reverse) "
          "unified_address(has_address) dynamic_allocators(can_allocate) "
          "self_maps(has_self_maps) device_safesync(can_sync)",
          c_options,
          {{"can_reverse", OMPC_reverse_offload, OMP_EXPR_PARSE_expression},
           {"has_address", OMPC_unified_address, OMP_EXPR_PARSE_expression},
           {"can_allocate", OMPC_dynamic_allocators, OMP_EXPR_PARSE_expression},
           {"has_self_maps", OMPC_self_maps, OMP_EXPR_PARSE_expression},
           {"can_sync", OMPC_device_safesync, OMP_EXPR_PARSE_expression}}) &&
      ok;
  ok = expectHostFragments(
           "directive_owned_host_fragment_modes",
           "#pragma omp declare variant(foo) match(construct={parallel})",
           cxx_options, {{"foo", OMPC_unknown, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "threadprivate_host_fragment_modes",
           "#pragma omp threadprivate(a,b)", cxx_options,
           {{"a", OMPC_unknown, OMP_EXPR_PARSE_variable_list},
            {"b", OMPC_unknown, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectHostFragments(
           "groupprivate_host_fragment_modes", "#pragma omp groupprivate(a,b)",
           cxx_options,
           {{"a", OMPC_unknown, OMP_EXPR_PARSE_variable_list},
            {"b", OMPC_unknown, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectHostFragments(
           "declare_target_host_fragment_modes",
           "#pragma omp declare target(a,b[0:4])", cxx_options,
           {{"a", OMPC_unknown, OMP_EXPR_PARSE_array_section},
            {"b[0:4]", OMPC_unknown, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "flush_host_fragment_modes", "#pragma omp flush(a,b)", cxx_options,
           {{"a", OMPC_unknown, OMP_EXPR_PARSE_variable_list},
            {"b", OMPC_unknown, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectHostFragments(
           "depobj_host_fragment_mode",
           "#pragma omp depobj(obj) depend(inout: value)", cxx_options,
           {{"obj", OMPC_unknown, OMP_EXPR_PARSE_expression},
            {"value", OMPC_depend, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "declare_simd_host_fragment_mode", "#pragma omp declare simd(foo)",
           cxx_options, {{"foo", OMPC_unknown, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "nowait_expression_host_fragment_mode",
           "#pragma omp target nowait(is_deferred)", cxx_options,
           {{"is_deferred", OMPC_nowait, OMP_EXPR_PARSE_expression}}) &&
       ok;
  ok = expectHostFragments(
           "to_iterator_host_fragment_modes",
           "#pragma omp target update "
           "to(iterator(T T = 0:n, int i = T:n): a[T][i])",
           cxx_options,
           {{"T", OMPC_to, OMP_EXPR_PARSE_openmp_iterator_type},
            {"T", OMPC_to, OMP_EXPR_PARSE_openmp_iterator_name},
            {"0", OMPC_to, OMP_EXPR_PARSE_expression},
            {"n", OMPC_to, OMP_EXPR_PARSE_expression},
            {"int", OMPC_to, OMP_EXPR_PARSE_openmp_iterator_type},
            {"i", OMPC_to, OMP_EXPR_PARSE_openmp_iterator_name},
            {"T", OMPC_to, OMP_EXPR_PARSE_expression},
            {"n", OMPC_to, OMP_EXPR_PARSE_expression},
            {"a[T][i]", OMPC_to, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "quoted_iterator_delimiter_host_fragment_modes",
           "#pragma omp target map(iterator(int k = "
           "(flag ? ')' : sizeof(R\"tag(\"),:)tag\")):n), to: a[k])",
           cxx_options,
           {{"int", OMPC_map, OMP_EXPR_PARSE_openmp_iterator_type},
            {"k", OMPC_map, OMP_EXPR_PARSE_openmp_iterator_name},
            {"(flag ? ')' : sizeof(R\"tag(\"),:)tag\"))", OMPC_map,
             OMP_EXPR_PARSE_expression},
            {"n", OMPC_map, OMP_EXPR_PARSE_expression},
            {"a[k]", OMPC_map, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "implicit_default_mapper_host_fragment_modes",
           "#pragma omp declare mapper(MapperRecord value) map(to: value)",
           cxx_options,
           {{"MapperRecord", OMPC_unknown,
             OMP_EXPR_PARSE_openmp_declare_mapper_type},
            {"value", OMPC_unknown,
             OMP_EXPR_PARSE_openmp_declare_mapper_variable},
            {"value", OMPC_map, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "complex_allocate_modifier_host_fragment_modes",
           "#pragma omp parallel "
           "allocate(allocator(select_allocator(\")\\\"\", "
           "pool[index + nested(1, 2)])), "
           "align(compute_align(sizeof(int[4]), \"),:\")): x)",
           cxx_options,
           {{"select_allocator(\")\\\"\", "
             "pool[index + nested(1, 2)])",
             OMPC_allocate, OMP_EXPR_PARSE_expression},
            {"compute_align(sizeof(int[4]), \"),:\")", OMPC_allocate,
             OMP_EXPR_PARSE_expression},
            {"x", OMPC_allocate, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
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
           "target_device_selector_call_spacing",
           "#pragma omp metadirective "
           "when(target_device={uid (\"device-7\"), device_num (device_id)}: "
           "parallel)",
           c_options,
           "#pragma omp metadirective "
           "when (target_device = {uid(\"device-7\"), "
           "device_num(device_id)} : parallel)") &&
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
  ok = expectText(
           "typed_requires_clause_payloads",
           "#pragma omp requires reverse_offload(can_reverse) "
           "unified_address(has_address) unified_shared_memory "
           "dynamic_allocators(can_allocate) self_maps(has_self_maps) "
           "device_safesync(can_sync) atomic_default_mem_order(acquire)",
           c_options,
           "#pragma omp requires reverse_offload(can_reverse) "
           "unified_address(has_address) unified_shared_memory "
           "dynamic_allocators(can_allocate) self_maps(has_self_maps) "
           "device_safesync(can_sync) atomic_default_mem_order(acquire)") &&
       ok;
  ok =
      expectText(
          "typed_context_requires_payloads",
          "#pragma omp metadirective "
          "when(implementation={requires(reverse_offload(can_reverse), "
          "unified_address, atomic_default_mem_order(release))}: parallel)",
          c_options,
          "#pragma omp metadirective "
          "when (implementation = {requires(reverse_offload(can_reverse), "
          "unified_address, atomic_default_mem_order(release))} : parallel)") &&
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
      "#pragma omp interop init(depobj, target: object)",
      "#pragma omp depobj(object) init(interop, in(value): object)",
      "#pragma omp declare variant(foo) match(construct={dispatch}) "
      "append_args(interop(depobj, target))",
      "#pragma omp metadirective "
      "when(device={arch(score(1): x86)}:)",
      "#pragma omp metadirective "
      "when(device={arch(x86), arch(arm)}:)",
      "#pragma omp metadirective "
      "when(target_device={kind(cpu), kind(gpu)}:)",
      "#pragma omp metadirective when(device={device_num(0)}:)",
      "#pragma omp metadirective when(device={kind(cpu,cpu)}:)",
      "#pragma omp metadirective when(device={kind(cpu,\"cpu\")}:)",
      "#pragma omp metadirective when(device={arch(x86,\"x86\")}:)",
      "#pragma omp metadirective when(device={kind(any),arch(x86)}:)",
      "#pragma omp metadirective "
      "when(implementation={vendor(amd,amd)}:)",
      "#pragma omp metadirective "
      "when(implementation={vendor(amd,\"amd\")}:)",
      "#pragma omp metadirective "
      "when(implementation={requires(reverse_offload, "
      "reverse_offload(enabled))}:)",
      "#pragma omp metadirective "
      "when(implementation={requires(vendor_requirement)}:)",
      "#pragma omp target "
      "map(iterator(unsigned long i 0:n), to: a[i])",
      "#pragma omp target "
      "map(iterator(unsigned long i = 0:), to: a[i])",
      "#pragma omp declare mapper(custom : value) map(to: value)",
      "#pragma omp target uses_allocators(rex_allocator::)",
      "#pragma omp target uses_allocators(rex_allocator])",
      "#pragma omp parallel allocate(allocator(a), allocator(b): x)",
      "#pragma omp parallel allocate(align(8), align(16): x)"};
  for (const std::string &input : malformed_typed_inputs) {
    if (ompparser::parseDirective(input, c_options).success()) {
      std::cerr << "malformed typed clause was accepted: " << input << "\n";
      ok = false;
    }
  }

  const std::string invalid_selector_tokens[] = {
      "#pragma omp metadirective "
      "when(implementation={-}: parallel)",
      "#pragma omp metadirective "
      "when(implementation={requires(-)}: parallel)"};
  for (const std::string &input : invalid_selector_tokens) {
    ompparser::ParseResult result = ompparser::parseDirective(input, c_options);
    if (result.success() ||
        !hasDiagnostic(result, ompparser::DiagnosticCode::SyntaxError)) {
      std::cerr << "invalid selector token did not produce a syntax error: "
                << input << "\n";
      ok = false;
    }
  }

  ompparser::ParseOptions registered_c_options = c_options;
  registered_c_options.extensions = ompparser::ExtensionPolicy::AllowRegistered;
  ompparser::ParseOptions registered_cxx_options = cxx_options;
  registered_cxx_options.extensions =
      ompparser::ExtensionPolicy::AllowRegistered;
  ok = expectText(
           "registered_context_requirement",
           "#pragma omp metadirective "
           "when(implementation={requires(ext_vendor_runtime)}: parallel)",
           registered_c_options,
           "#pragma omp metadirective "
           "when (implementation = {requires(ext_vendor_runtime)} : "
           "parallel)") &&
       ok;
  ompparser::ParseResult rejected_context_requirement =
      ompparser::parseDirective(
          "#pragma omp metadirective "
          "when(implementation={requires(ext_vendor_runtime)}: parallel)",
          c_options);
  if (rejected_context_requirement.success() ||
      !hasDiagnostic(rejected_context_requirement,
                     ompparser::DiagnosticCode::UnsupportedExtension)) {
    std::cerr << "default extension policy accepted a context requirement\n";
    ok = false;
  }
  ok = expectText("registered_dist_data",
                  "#pragma omp target data map(to: a[0:N] "
                  "dist_data(duplicate, block(4), cyclic(2)))",
                  registered_c_options,
                  "#pragma omp target data map(to : a[0:N] "
                  "dist_data(duplicate, block(4), cyclic(2)))") &&
       ok;
  ok = expectHostFragments(
           "dist_data_quoted_delimiter",
           "#pragma omp target data map(to: a[idx(\")\")] "
           "dist_data(block(4)))",
           registered_cxx_options,
           {{"4", OMPC_map, OMP_EXPR_PARSE_expression},
            {"a[idx(\")\")]", OMPC_map, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ok = expectHostFragments(
           "dist_data_raw_string_delimiter",
           "#pragma omp target data map(to: a[idx(R\"tag(foo)bar)tag\")] "
           "dist_data(cyclic(2)))",
           registered_cxx_options,
           {{"2", OMPC_map, OMP_EXPR_PARSE_expression},
            {"a[idx(R\"tag(foo)bar)tag\")]", OMPC_map,
             OMP_EXPR_PARSE_array_section}}) &&
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
  ok =
      expectHostFragments(
          "uses_allocators_fortran_component_allocator_is_one_fragment",
          "!$omp target uses_allocators(obj%pool)", fortran_options,
          {{"obj%pool", OMPC_uses_allocators, OMP_EXPR_PARSE_variable_list}}) &&
      ok;
  ok = expectHostFragments(
           "cxx_qualified_reduction_identifier",
           "#pragma omp parallel reduction(ns::sum: value)", cxx_options,
           {{"ns::sum", OMPC_reduction, OMP_EXPR_PARSE_openmp_syntax},
            {"value", OMPC_reduction, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectHostFragments(
           "cxx_global_reduction_identifier",
           "#pragma omp parallel reduction(::sum: value)", cxx_options,
           {{"::sum", OMPC_reduction, OMP_EXPR_PARSE_openmp_syntax},
            {"value", OMPC_reduction, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectText("cxx_qualified_in_reduction_identifier",
                  "#pragma omp task in_reduction(ns::sum: value)", cxx_options,
                  "#pragma omp task in_reduction (ns::sum : value)") &&
       ok;
  ok = expectText("cxx_qualified_task_reduction_identifier",
                  "#pragma omp taskgroup task_reduction(ns::sum: value)",
                  cxx_options,
                  "#pragma omp taskgroup task_reduction (ns::sum : value)") &&
       ok;
  const char *non_cxx_qualified_reductions[] = {
      "#pragma omp parallel reduction(ns::sum: value)",
      "!$omp parallel do reduction(ns::sum: value)"};
  const ompparser::ParseOptions *non_cxx_qualified_options[] = {
      &c_options, &fortran_options};
  for (std::size_t index = 0;
       index < sizeof(non_cxx_qualified_reductions) /
                   sizeof(non_cxx_qualified_reductions[0]);
       ++index) {
    if (ompparser::parseDirective(non_cxx_qualified_reductions[index],
                                  *non_cxx_qualified_options[index])
            .success()) {
      std::cerr << "qualified C++ reduction identifier was accepted in "
                   "another base language: "
                << non_cxx_qualified_reductions[index] << "\n";
      ok = false;
    }
  }
  ok = expectSuccess("fortran_doubled_quote_context_name",
                     "!$omp metadirective "
                     "when(device={arch('foo''bar')}: parallel)",
                     fortran_options) &&
       ok;
  const std::string cross_language_context_literals[] = {
      "!$omp metadirective when(device={arch(R\"(cpu)\")}: parallel)",
      "#pragma omp metadirective when(device={arch(R\"(cpu)\")}: parallel)",
      "#pragma omp metadirective "
      "when(device={arch('foo''bar')}: parallel)",
      "!$omp metadirective "
      "when(device={arch('foo''bar',\"foo'bar\")}: parallel)"};
  const ompparser::ParseOptions *cross_language_context_options[] = {
      &fortran_options, &c_options, &cxx_options, &fortran_options};
  for (std::size_t index = 0;
       index < sizeof(cross_language_context_literals) /
                   sizeof(cross_language_context_literals[0]);
       ++index) {
    if (ompparser::parseDirective(cross_language_context_literals[index],
                                  *cross_language_context_options[index])
            .success()) {
      std::cerr << "context selector accepted another language's literal: "
                << cross_language_context_literals[index] << "\n";
      ok = false;
    }
  }
  ok = expectHostFragments("fortran_spaced_map_mapper_identifier",
                           "!$omp target map(mapper( left_id), tofrom: a)",
                           fortran_options,
                           {{"left_id", OMPC_map, OMP_EXPR_PARSE_verbatim},
                            {"a", OMPC_map, OMP_EXPR_PARSE_array_section}}) &&
       ok;
  ompparser::ParseResult fortran_default_mapper = ompparser::parseDirective(
      "!$omp declare mapper(DEFAULT : integer :: value) map(tofrom: value)",
      fortran_options);
  const auto *fortran_default_mapper_directive =
      fortran_default_mapper.success()
          ? dynamic_cast<const OpenMPDeclareMapperDirective *>(
                fortran_default_mapper.directive.get())
          : nullptr;
  if (fortran_default_mapper_directive == nullptr ||
      fortran_default_mapper_directive->getIdentifier() !=
          OMPD_DECLARE_MAPPER_IDENTIFIER_default ||
      !fortran_default_mapper_directive->hasExplicitIdentifier() ||
      !fortran_default_mapper_directive->getUserDefinedIdentifier().empty()) {
    std::cerr << "Fortran DEFAULT mapper identifier was not classified as "
                 "the predefined default identifier\n";
    ok = false;
  }
  ok = expectHostFragments(
           "fortran_logical_reduction_typed_operator",
           "!$omp parallel do reduction(.and.:value)", fortran_options,
           {{"value", OMPC_reduction, OMP_EXPR_PARSE_variable_list}}) &&
       ok;
  ok = expectText("fortran_logical_reduction_spelling",
                  "!$omp parallel do reduction(.and.:value)", fortran_options,
                  "!$omp parallel do reduction(.and. : value)") &&
       ok;
  ok = expectText("fortran_equivalence_reduction_spelling",
                  "!$omp parallel do reduction(.neqv.:value)", fortran_options,
                  "!$omp parallel do reduction(.neqv. : value)") &&
       ok;
  ok = expectText("fortran_in_reduction_spelling",
                  "!$omp task in_reduction(.or.:value)", fortran_options,
                  "!$omp task in_reduction (.or. : value)") &&
       ok;
  ok =
      expectText("fortran_task_reduction_spelling",
                 "!$omp taskgroup task_reduction(.eqv.:value)", fortran_options,
                 "!$omp taskgroup task_reduction (.eqv. : value)") &&
      ok;

  struct CrossLanguageLogicalReductionCase {
    const char *input;
    const ompparser::ParseOptions *options;
  };
  const CrossLanguageLogicalReductionCase cross_language_logical_reductions[] =
      {{"#pragma omp parallel reduction(.and.: value)", &c_options},
       {"#pragma omp parallel reduction(.or.: value)", &cxx_options},
       {"#pragma omp task in_reduction(.and.: value)", &c_options},
       {"#pragma omp taskgroup task_reduction(.or.: value)", &cxx_options},
       {"!$omp parallel do reduction(&&: value)", &fortran_options},
       {"!$omp task in_reduction(||: value)", &fortran_options},
       {"!$omp taskgroup task_reduction(&&: value)", &fortran_options},
       {"#pragma omp declare reduction(.and. : int) "
        "combiner(omp_out = omp_out && omp_in)",
        &c_options},
       {"!$omp declare reduction(&& : integer) "
        "combiner(omp_out = omp_out .and. omp_in)",
        &fortran_options}};
  for (const CrossLanguageLogicalReductionCase &test :
       cross_language_logical_reductions) {
    if (ompparser::parseDirective(test.input, *test.options).success()) {
      std::cerr << "cross-language logical reduction spelling was accepted: "
                << test.input << "\n";
      ok = false;
    }
  }

  ompparser::ParseResult c_fortran_reduction = ompparser::parseDirective(
      "#pragma omp parallel reduction(.eqv.:value)", c_options);
  if (c_fortran_reduction.success() ||
      !hasDiagnostic(c_fortran_reduction,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "Fortran-only reduction operator was accepted in C\n";
    ok = false;
  }
  ompparser::ParseResult fortran_c_reduction = ompparser::parseDirective(
      "!$omp parallel do reduction(^:value)", fortran_options);
  if (fortran_c_reduction.success() ||
      !hasDiagnostic(fortran_c_reduction,
                     ompparser::DiagnosticCode::InvalidClause)) {
    std::cerr << "C-only reduction operator was accepted in Fortran\n";
    ok = false;
  }
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
  bool malformed_clause_threw = false;
  try {
    malformed_ast.addOpenMPClause(OMPC_device);
  } catch (const std::invalid_argument &) {
    malformed_clause_threw = true;
  }
  if (!malformed_clause_threw) {
    std::cerr << "mistyped clause construction did not fail immediately\n";
    ok = false;
  }
  bool mistyped_clause_threw = false;
  try {
    malformed_ast.addOpenMPClause(OMPC_device, std::string("ancestor"));
  } catch (const std::invalid_argument &) {
    mistyped_clause_threw = true;
  }
  bool extra_clause_argument_threw = false;
  try {
    malformed_ast.addOpenMPClause(OMPC_nowait, 1);
  } catch (const std::invalid_argument &) {
    extra_clause_argument_threw = true;
  }
  if (!mistyped_clause_threw || !extra_clause_argument_threw) {
    std::cerr << "invalid clause argument shape did not fail immediately\n";
    ok = false;
  }

  OpenMPMatchClause invalid_atomic_property;
  invalid_atomic_property.beginTraitSet(OMPC_SELECTOR_implementation);
  invalid_atomic_property.beginTraitSelector(
      OMPC_TRAIT_atomic_default_mem_order, nullptr);
  bool unknown_atomic_property_threw = false;
  try {
    invalid_atomic_property.addAtomicDefaultMemOrderProperty(
        OMPC_ATOMIC_DEFAULT_MEM_ORDER_unknown);
  } catch (const std::invalid_argument &) {
    unknown_atomic_property_threw = true;
  }

  OpenMPMatchClause invalid_requires_atomic_property;
  invalid_requires_atomic_property.beginTraitSet(OMPC_SELECTOR_implementation);
  invalid_requires_atomic_property.beginTraitSelector(OMPC_TRAIT_requires,
                                                      nullptr);
  bool unknown_requires_atomic_property_threw = false;
  try {
    invalid_requires_atomic_property.addRequiresAtomicDefaultMemOrderProperty(
        OMPC_ATOMIC_DEFAULT_MEM_ORDER_unknown);
  } catch (const std::invalid_argument &) {
    unknown_requires_atomic_property_threw = true;
  }
  if (!unknown_atomic_property_threw ||
      !unknown_requires_atomic_property_threw) {
    std::cerr << "unknown context-selector atomic order was stored\n";
    ok = false;
  }

  OpenMPDirective transactional_clause_ast(OMPD_target);
  bool parameterized_extra_argument_threw = false;
  try {
    transactional_clause_ast.addOpenMPClause(
        OMPC_device, OMPC_DEVICE_MODIFIER_unspecified, 1);
  } catch (const std::invalid_argument &) {
    parameterized_extra_argument_threw = true;
  }
  std::vector<std::string> transactional_errors;
  if (!parameterized_extra_argument_threw ||
      !transactional_clause_ast.getAllClauses().empty() ||
      !transactional_clause_ast.getClausesInOriginalOrder()->empty() ||
      !transactional_clause_ast.validateInvariants(transactional_errors)) {
    std::cerr << "invalid parameterized clause mutated directive ownership\n";
    ok = false;
  }

  OpenMPDirective null_clause_ast(OMPD_parallel);
  bool null_clause_threw = false;
  try {
    null_clause_ast.registerClause(nullptr);
  } catch (const std::invalid_argument &) {
    null_clause_threw = true;
  }
  if (!null_clause_threw) {
    std::cerr << "null clause registration did not fail immediately\n";
    ok = false;
  }

  OpenMPDirective null_expression_ast(OMPD_parallel);
  OpenMPClause *null_expression_clause =
      null_expression_ast.addOpenMPClause(OMPC_private);
  if (null_expression_clause == nullptr) {
    std::cerr << "failed to construct null-expression AST test\n";
    ok = false;
  } else {
    bool null_expression_threw = false;
    try {
      null_expression_clause->addLangExpr(nullptr);
    } catch (const std::invalid_argument &) {
      null_expression_threw = true;
    }
    if (!null_expression_threw) {
      std::cerr << "null host expression did not fail immediately\n";
      ok = false;
    }
  }

  OpenMPDirective null_nested_apply_ast(OMPD_fuse);
  auto *null_nested_apply = dynamic_cast<OpenMPApplyClause *>(
      null_nested_apply_ast.addOpenMPClause(OMPC_apply));
  if (null_nested_apply == nullptr) {
    std::cerr << "failed to construct null nested-apply AST test\n";
    ok = false;
  } else {
    bool null_nested_apply_threw = false;
    try {
      null_nested_apply->addNestedApply(nullptr, OMPC_CLAUSE_SEP_comma);
    } catch (const std::invalid_argument &) {
      null_nested_apply_threw = true;
    }
    if (!null_nested_apply_threw) {
      std::cerr << "null nested apply did not fail immediately\n";
      ok = false;
    }
  }

  OpenMPClause expression_index_contract(OMPC_private);
  bool get_node_threw = false;
  bool set_node_threw = false;
  bool get_mode_threw = false;
  try {
    expression_index_contract.getExpressionNode(0);
  } catch (const std::out_of_range &) {
    get_node_threw = true;
  }
  try {
    expression_index_contract.setExpressionNode(0, nullptr);
  } catch (const std::out_of_range &) {
    set_node_threw = true;
  }
  try {
    expression_index_contract.getExpressionParseMode(0);
  } catch (const std::out_of_range &) {
    get_mode_threw = true;
  }
  if (!get_node_threw || !set_node_threw || !get_mode_threw) {
    std::cerr << "out-of-range expression access did not fail hard\n";
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

  OpenMPDirective legacy_allocator_align_ast(OMPD_parallel);
  auto *legacy_allocator_align = dynamic_cast<OpenMPAllocateClause *>(
      OpenMPAllocateClause::addAllocateClause(&legacy_allocator_align_ast,
                                              OMPC_ALLOCATE_ALLOCATOR_default,
                                              nullptr));
  if (legacy_allocator_align == nullptr) {
    std::cerr << "failed to construct legacy allocator alignment AST test\n";
    ok = false;
  } else {
    legacy_allocator_align->setAlignModifier("64");
    legacy_allocator_align->addLangExpr("value", OMPC_CLAUSE_SEP_space, 1, 1,
                                        OMP_EXPR_PARSE_variable_list);
    ompparser::ValidationResult legacy_allocator_align_validation =
        ompparser::validate(legacy_allocator_align_ast);
    ompparser::UnparseResult legacy_allocator_align_unparse =
        ompparser::unparse(legacy_allocator_align_ast);
    if (legacy_allocator_align_validation.success() ||
        !hasDiagnostic(legacy_allocator_align_validation,
                       ompparser::DiagnosticCode::InvalidAst) ||
        legacy_allocator_align_unparse.success() ||
        !hasDiagnostic(legacy_allocator_align_unparse,
                       ompparser::DiagnosticCode::InvalidAst)) {
      std::cerr << "exclusive legacy allocator syntax accepted an align "
                   "modifier\n";
      ok = false;
    }
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
  auto *misplaced_map = misplaced_clause_ast.addOpenMPClause(
      OMPC_map, OMPC_MAP_MODIFIER_unspecified, OMPC_MAP_MODIFIER_unspecified,
      OMPC_MAP_MODIFIER_unspecified, OMPC_MAP_TYPE_unspecified,
      OMPC_MAP_REF_MODIFIER_unspecified);
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
