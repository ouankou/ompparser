/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "OpenMPParser.h"

#include "OpenMPIR.h"
#include "OpenMPParserInternal.h"
#include "OpenMPSchema.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <iterator>
#include <numeric>

namespace {

thread_local std::vector<ompparser::Diagnostic> CurrentDiagnostics;

using NestedDirectiveVisitor = std::function<void(const OpenMPDirective &)>;

void visitImmediateNestedDirectives(const OpenMPDirective &directive,
                                    const NestedDirectiveVisitor &visitor) {
  if (const auto *end_directive =
          dynamic_cast<const OpenMPEndDirective *>(&directive)) {
    if (end_directive->getPairedDirectiveRole() ==
        OpenMPPairedDirectiveRole::Complete) {
      if (const OpenMPDirective *paired = end_directive->getPairedDirective()) {
        visitor(*paired);
      }
    }
  }

  for (const OpenMPClause *clause : directive.getClausesInOriginalOrder()) {
    if (clause == nullptr) {
      continue;
    }
    if (const auto *variant =
            dynamic_cast<const OpenMPVariantClause *>(clause)) {
      for (const auto &set : variant->getTraitSets()) {
        for (const auto &selector : set.selectors) {
          if (selector.construct_directive != nullptr) {
            visitor(*selector.construct_directive);
          }
        }
      }
    }

    const OpenMPDirective *variant_directive = nullptr;
    if (const auto *when = dynamic_cast<const OpenMPWhenClause *>(clause)) {
      variant_directive = when->getVariantDirective();
    } else if (const auto *otherwise =
                   dynamic_cast<const OpenMPOtherwiseClause *>(clause)) {
      variant_directive = otherwise->getVariantDirective();
    } else if (const auto *default_clause =
                   dynamic_cast<const OpenMPDefaultClause *>(clause)) {
      variant_directive = default_clause->getVariantDirective();
    }
    // Missing when and otherwise payloads represent their optional implicit
    // nothing variants in OpenMP 6.0 Sections 9.4.1 and 9.4.2.
    if (variant_directive != nullptr) {
      visitor(*variant_directive);
    }
  }
}

void applyHostLanguageHooks(OpenMPDirective &directive,
                            const ompparser::HostLanguageHooks &hooks,
                            std::vector<ompparser::Diagnostic> &diagnostics) {
  directive.visitHostFragments([&](ompparser::HostFragment &fragment) {
    fragment.semantic = hooks.parse(fragment, diagnostics);
  });
}

void validateExtensionPolicyImpl(
    const OpenMPDirective &directive,
    std::vector<ompparser::Diagnostic> &diagnostics,
    std::vector<const OpenMPDirective *> &visited) {
  if (std::find(visited.begin(), visited.end(), &directive) != visited.end()) {
    return;
  }
  visited.push_back(&directive);

  auto report = [&diagnostics](const std::string &message) {
    ompparser::Diagnostic diagnostic;
    diagnostic.code = ompparser::DiagnosticCode::UnsupportedExtension;
    diagnostic.severity = ompparser::DiagnosticSeverity::Error;
    diagnostic.message = message;
    diagnostics.push_back(std::move(diagnostic));
  };

  if (directive.getKind() == OMPD_ompx) {
    report("OMPX directives require ExtensionPolicy::AllowRegistered");
  }
  for (const OpenMPClause *clause : directive.getClausesInOriginalOrder()) {
    if (!clause) {
      continue;
    }
    if (clause->getKind() == OMPC_ext_implementation_defined_requirement) {
      report("implementation-defined OpenMP clauses require the registered "
             "extension policy");
    }
    const auto *variant_clause =
        dynamic_cast<const OpenMPVariantClause *>(clause);
    if (variant_clause != nullptr) {
      for (const auto &set : variant_clause->getTraitSets()) {
        for (const auto &selector : set.selectors) {
          for (const auto &property : selector.properties) {
            if (property.requirement != nullptr &&
                property.requirement->OpenMPClause::getKind() ==
                    OMPC_ext_implementation_defined_requirement) {
              report("implementation-defined OpenMP clauses require the "
                     "registered extension policy");
            }
          }
        }
      }
    }
    const auto *map_clause = dynamic_cast<const OpenMPMapClause *>(clause);
    if (map_clause) {
      for (const auto &policies : map_clause->getDistDataPolicies()) {
        if (!policies.empty()) {
          report("dist_data requires ExtensionPolicy::AllowRegistered");
          break;
        }
      }
    }
  }

  visitImmediateNestedDirectives(directive, [&](const OpenMPDirective &nested) {
    validateExtensionPolicyImpl(nested, diagnostics, visited);
  });
}

void validateExtensionPolicy(const OpenMPDirective &directive,
                             ompparser::ExtensionPolicy policy,
                             std::vector<ompparser::Diagnostic> &diagnostics) {
  if (policy == ompparser::ExtensionPolicy::AllowRegistered) {
    return;
  }
  std::vector<const OpenMPDirective *> visited;
  validateExtensionPolicyImpl(directive, diagnostics, visited);
}

void addStructureDiagnostic(std::vector<ompparser::Diagnostic> &diagnostics,
                            ompparser::DiagnosticCode code,
                            const std::string &message) {
  ompparser::Diagnostic diagnostic;
  diagnostic.code = code;
  diagnostic.severity = ompparser::DiagnosticSeverity::Error;
  diagnostic.message = message;
  diagnostics.push_back(std::move(diagnostic));
}

bool isOpenMPIdentifier(const std::string &spelling) {
  if (spelling.empty() ||
      (!std::isalpha(static_cast<unsigned char>(spelling.front())) &&
       spelling.front() != '_')) {
    return false;
  }
  return std::all_of(spelling.begin() + 1, spelling.end(),
                     [](unsigned char character) {
                       return std::isalnum(character) || character == '_';
                     });
}

bool isFortranDefinedOperator(const std::string &spelling) {
  return spelling.size() >= 3 && spelling.front() == '.' &&
         spelling.back() == '.' &&
         std::all_of(
             spelling.begin() + 1, spelling.end() - 1,
             [](unsigned char character) { return std::isalpha(character); });
}

bool isCxxReductionIdExpression(const std::string &spelling) {
  if (spelling.empty()) {
    return false;
  }
  std::size_t offset = spelling.compare(0, 2, "::") == 0 ? 2 : 0;
  if (offset == spelling.size()) {
    return false;
  }
  while (offset < spelling.size()) {
    const std::size_t separator = spelling.find("::", offset);
    const std::string component = spelling.substr(
        offset, separator == std::string::npos ? std::string::npos
                                               : separator - offset);
    if (!isOpenMPIdentifier(component)) {
      return false;
    }
    if (separator == std::string::npos) {
      return true;
    }
    offset = separator + 2;
    if (offset == spelling.size()) {
      return false;
    }
  }
  return false;
}

void validateReductionIdentifier(
    OpenMPBaseLang language, bool c_only, bool fortran_only, bool is_user,
    const ompparser::HostFragment &user_identifier,
    std::vector<ompparser::Diagnostic> &diagnostics) {
  if (language == Lang_unknown) {
    addStructureDiagnostic(diagnostics, ompparser::DiagnosticCode::InvalidAst,
                           "reduction identifier has no exact base language");
  }
  if (c_only && language == Lang_Fortran) {
    addStructureDiagnostic(diagnostics,
                           ompparser::DiagnosticCode::InvalidClause,
                           "C/C++ reduction identifier is invalid in Fortran");
  }
  if (fortran_only && language != Lang_Fortran) {
    addStructureDiagnostic(diagnostics,
                           ompparser::DiagnosticCode::InvalidClause,
                           "Fortran reduction identifier is invalid in C/C++");
  }
  if (is_user) {
    const bool valid_spelling =
        language == Lang_Cplusplus
            ? isCxxReductionIdExpression(user_identifier.spelling)
        : language == Lang_Fortran
            ? isOpenMPIdentifier(user_identifier.spelling) ||
                  isFortranDefinedOperator(user_identifier.spelling)
            : isOpenMPIdentifier(user_identifier.spelling);
    if (!valid_spelling ||
        user_identifier.role != ompparser::HostFragmentRole::Declarator ||
        user_identifier.parse_mode != OMP_EXPR_PARSE_openmp_syntax) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidClause,
          "user-defined reduction identifier '" + user_identifier.spelling +
              "' is not a typed OpenMP name (mode " +
              std::to_string(user_identifier.parse_mode) + ")");
    }
  } else if (!user_identifier.spelling.empty()) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "predefined reduction identifier has a user-defined name payload");
  }
}

bool initIncludesInteropType(const OpenMPInitClause &init_clause,
                             OpenMPInitClauseKind interop_type) {
  const auto &modifiers = init_clause.getModifiers().getModifiers();
  return std::any_of(modifiers.begin(), modifiers.end(),
                     [interop_type](const OpenMPInitModifier &modifier) {
                       return modifier.category ==
                                  OpenMPInitModifierCategory::InteropType &&
                              modifier.interop_type == interop_type;
                     });
}

void validateInitModifiers(const OpenMPInitModifierList &modifier_list,
                           bool require_interop_type, bool require_depinfo,
                           std::vector<ompparser::Diagnostic> &diagnostics) {
  std::size_t prefer_type_count = 0;
  std::size_t depinfo_count = 0;
  std::size_t depobj_name_count = 0;
  std::size_t interop_name_count = 0;
  std::size_t target_count = 0;
  std::size_t targetsync_count = 0;
  bool invalid_typed_modifier = false;
  for (const OpenMPInitModifier &modifier : modifier_list.getModifiers()) {
    switch (modifier.category) {
    case OpenMPInitModifierCategory::InteropType:
      if (modifier.interop_type == OMPC_INIT_KIND_target) {
        ++target_count;
      } else if (modifier.interop_type == OMPC_INIT_KIND_targetsync) {
        ++targetsync_count;
      } else {
        invalid_typed_modifier = true;
      }
      break;
    case OpenMPInitModifierCategory::DirectiveName:
      if (modifier.directive_name == OMPD_depobj) {
        ++depobj_name_count;
      } else if (modifier.directive_name == OMPD_interop) {
        ++interop_name_count;
      } else {
        invalid_typed_modifier = true;
      }
      break;
    case OpenMPInitModifierCategory::PreferType:
      ++prefer_type_count;
      if (modifier.argument.spelling.empty() ||
          modifier.argument.role != ompparser::HostFragmentRole::Verbatim ||
          modifier.argument.parse_mode != OMP_EXPR_PARSE_openmp_source) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "prefer_type requires a typed source argument");
      }
      break;
    case OpenMPInitModifierCategory::Depinfo:
      ++depinfo_count;
      if (modifier.dependence_type != OMPC_DEPENDENCE_TYPE_in &&
          modifier.dependence_type != OMPC_DEPENDENCE_TYPE_out &&
          modifier.dependence_type != OMPC_DEPENDENCE_TYPE_inout &&
          modifier.dependence_type != OMPC_DEPENDENCE_TYPE_inoutset &&
          modifier.dependence_type != OMPC_DEPENDENCE_TYPE_mutexinoutset) {
        invalid_typed_modifier = true;
      }
      if (modifier.argument.spelling.empty() ||
          modifier.argument.role != ompparser::HostFragmentRole::Locator ||
          modifier.argument.parse_mode != OMP_EXPR_PARSE_array_section) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "depinfo requires a typed locator list item");
      }
      break;
    default:
      invalid_typed_modifier = true;
      break;
    }
  }

  if (invalid_typed_modifier) {
    addStructureDiagnostic(diagnostics,
                           ompparser::DiagnosticCode::InvalidClause,
                           "init modifier has an invalid typed kind");
  }

  if (prefer_type_count > 1 || depinfo_count > 1 ||
      depobj_name_count + interop_name_count > 1 || target_count > 1 ||
      targetsync_count > 1) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "init modifier names and interop-type keywords must be unique");
  }
  const std::size_t interop_type_count = target_count + targetsync_count;
  if (require_interop_type && interop_type_count == 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "interop initialization requires an interop-type modifier");
  }
  if (!require_interop_type && interop_type_count != 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "interop-type modifiers are only valid for interop initialization");
  }
  if (require_depinfo && depinfo_count == 0) {
    addStructureDiagnostic(diagnostics,
                           ompparser::DiagnosticCode::InvalidClause,
                           "depobj initialization requires a depinfo modifier");
  }
  if (!require_depinfo && depinfo_count != 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "depinfo modifiers are only valid for depobj initialization");
  }
  if (!require_interop_type && prefer_type_count != 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "prefer_type is only valid for interop initialization");
  }
  if (require_interop_type && depobj_name_count != 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "depobj directive-name modifier is invalid for interop");
  }
  if (!require_interop_type && interop_name_count != 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "interop directive-name modifier is invalid for depobj");
  }
}

bool hasEmptyApplyTree(const OpenMPApplyClause &apply) {
  if (apply.getTransformations().empty()) {
    return true;
  }
  return std::any_of(apply.getTransformations().begin(),
                     apply.getTransformations().end(),
                     [](const OpenMPApplyClause::ApplyTransform &transform) {
                       return transform.nested_apply != nullptr &&
                              hasEmptyApplyTree(*transform.nested_apply);
                     });
}

void validateOpenMPStructure(const OpenMPDirective &directive,
                             std::vector<ompparser::Diagnostic> &diagnostics) {
  for (const auto &entry : directive.getAllClauses()) {
    if (ompparser::getClauseCardinality(entry.first) ==
            ompparser::ClauseCardinality::Unique &&
        entry.second.size() > 1) {
      addStructureDiagnostic(diagnostics,
                             ompparser::DiagnosticCode::DuplicateClause,
                             std::string("duplicate unique clause '") +
                                 ompparser::getClauseName(entry.first) + "'");
    }
  }

  for (const OpenMPClause *clause : directive.getClausesInOriginalOrder()) {
    if (clause == nullptr) {
      continue;
    }
    if (clause->getBaseLang() != directive.getBaseLang()) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidAst,
          "clause base language disagrees with its owning directive");
    }
    if (directive.getKind() != OMPD_end &&
        !ompparser::isClauseAllowedOnDirective(directive.getKind(),
                                               clause->getKind())) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidClause,
          std::string("clause '") +
              ompparser::getClauseName(clause->getKind()) +
              "' is not allowed on directive '" +
              ompparser::getDirectiveName(directive.getKind()) + "'");
    }
    if (ompparser::clauseRequiresExpressionList(clause->getKind())) {
      const auto &expressions = clause->getExpressionItems();
      if (expressions.empty() ||
          std::any_of(expressions.begin(), expressions.end(),
                      [](const OpenMPExpressionItem &expression) {
                        return expression.fragment.spelling.empty();
                      })) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            std::string("clause '") +
                ompparser::getClauseName(clause->getKind()) +
                "' requires a non-empty expression or locator list");
      }
    }
    const auto *absent = dynamic_cast<const OpenMPAbsentClause *>(clause);
    const auto *contains = dynamic_cast<const OpenMPContainsClause *>(clause);
    if ((absent != nullptr && absent->getDirectives().empty()) ||
        (contains != nullptr && contains->getDirectives().empty())) {
      addStructureDiagnostic(diagnostics,
                             ompparser::DiagnosticCode::InvalidClause,
                             std::string("clause '") +
                                 ompparser::getClauseName(clause->getKind()) +
                                 "' requires a non-empty directive-name list");
    }
    if (const auto *adjust_args =
            dynamic_cast<const OpenMPAdjustArgsClause *>(clause)) {
      if (adjust_args->getModifier() == OMPC_ADJUST_ARGS_unknown ||
          adjust_args->getArguments().empty()) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "adjust_args requires a typed adjust-op and a non-empty "
            "parameter list");
      }
    }
    if (const auto *allocate =
            dynamic_cast<const OpenMPAllocateClause *>(clause)) {
      const bool has_user_allocator =
          !allocate->getUserDefinedAllocator().empty();
      const bool typed_user_allocator =
          allocate->getAllocator() == OMPC_ALLOCATE_ALLOCATOR_user;
      if (has_user_allocator != typed_user_allocator ||
          (allocate->usesAllocatorModifierSyntax() && !typed_user_allocator)) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "allocate allocator kind, modifier syntax, and typed payload "
            "must agree");
      }
    }
    if (const auto *init = dynamic_cast<const OpenMPInitClause *>(clause)) {
      if (init->getOperand().empty()) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "init requires an init-var argument");
      }
      const bool on_interop = directive.getKind() == OMPD_interop;
      const bool on_depobj = directive.getKind() == OMPD_depobj;
      if (!on_interop && !on_depobj) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "init is only valid on interop and depobj directives");
      } else {
        validateInitModifiers(init->getModifiers(), on_interop, on_depobj,
                              diagnostics);
      }
    }
    if (clause->getKind() == OMPC_destroy &&
        directive.getKind() == OMPD_interop) {
      const auto &expressions = clause->getExpressionItems();
      if (expressions.empty() ||
          std::any_of(expressions.begin(), expressions.end(),
                      [](const OpenMPExpressionItem &expression) {
                        return expression.fragment.spelling.empty();
                      })) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "destroy on interop requires a destroy-var argument");
      }
    }
    if (const auto *append_args =
            dynamic_cast<const OpenMPAppendArgsClause *>(clause)) {
      if (append_args->getOperations().empty()) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "append_args requires a non-empty operation list");
      }
      for (const OpenMPAppendArgsClause::Operation &operation :
           append_args->getOperations()) {
        if (operation.kind != OMPC_APPEND_ARGS_interop) {
          addStructureDiagnostic(diagnostics,
                                 ompparser::DiagnosticCode::InvalidClause,
                                 "append_args contains an unknown operation");
          continue;
        }
        validateInitModifiers(operation.modifiers, true, false, diagnostics);
      }
    }
    if (const auto *apply = dynamic_cast<const OpenMPApplyClause *>(clause)) {
      if (hasEmptyApplyTree(*apply)) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "apply and nested apply clauses require non-empty "
            "applied-directives lists");
      }
    }
    if (const auto *variant =
            dynamic_cast<const OpenMPVariantClause *>(clause)) {
      std::vector<std::string> selector_errors;
      variant->validateSelectorInvariants(selector_errors);
      for (const std::string &message : selector_errors) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause, message);
      }
    }
    if (const auto *default_clause =
            dynamic_cast<const OpenMPDefaultClause *>(clause)) {
      if (default_clause->getDefaultClauseKind() == OMPC_DEFAULT_variant &&
          default_clause->getVariantDirective() == nullptr) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidAst,
            "default variant clause requires a variant directive");
      }
    }
    if (const auto *uses_allocators =
            dynamic_cast<const OpenMPUsesAllocatorsClause *>(clause)) {
      if (uses_allocators->getUsesAllocatorsAllocatorSequence().empty()) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "uses_allocators requires a non-empty allocator list");
      }
    }
    if (const auto *map_clause =
            dynamic_cast<const OpenMPMapClause *>(clause)) {
      const ompparser::HostFragment &mapper =
          map_clause->getMapperIdentifierFragment();
      if (!mapper.spelling.empty() &&
          (mapper.role != ompparser::HostFragmentRole::Declarator ||
           mapper.parse_mode != OMP_EXPR_PARSE_verbatim ||
           !isOpenMPIdentifier(mapper.spelling))) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "map mapper identifier '" + mapper.spelling +
                                   "' is not a typed OpenMP name (mode " +
                                   std::to_string(mapper.parse_mode) + ")");
      }
      const auto &all_policies = map_clause->getDistDataPolicies();
      const bool has_invalid_policy = std::any_of(
          all_policies.begin(), all_policies.end(), [](const auto &policies) {
            return std::any_of(
                policies.begin(), policies.end(),
                [](const OpenMPMapClause::DistDataPolicy &policy) {
                  return policy.kind == OpenMPMapClause::DIST_DATA_unknown;
                });
          });
      if (has_invalid_policy) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "dist_data contains an unrecognized or malformed policy");
      }
    }
    if (const auto *to_clause = dynamic_cast<const OpenMPToClause *>(clause)) {
      const ompparser::HostFragment &mapper =
          to_clause->getMapperIdentifierFragment();
      if (!mapper.spelling.empty() &&
          (mapper.role != ompparser::HostFragmentRole::Declarator ||
           mapper.parse_mode != OMP_EXPR_PARSE_verbatim ||
           !isOpenMPIdentifier(mapper.spelling))) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "to mapper identifier is not a typed OpenMP name");
      }
    }
    if (const auto *from_clause =
            dynamic_cast<const OpenMPFromClause *>(clause)) {
      const ompparser::HostFragment &mapper =
          from_clause->getMapperIdentifierFragment();
      if (!mapper.spelling.empty() &&
          (mapper.role != ompparser::HostFragmentRole::Declarator ||
           mapper.parse_mode != OMP_EXPR_PARSE_verbatim ||
           !isOpenMPIdentifier(mapper.spelling))) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            "from mapper identifier is not a typed OpenMP name");
      }
    }
    if (const auto *depend = dynamic_cast<const OpenMPDependClause *>(clause)) {
      if (depend->getType() == OMPC_DEPENDENCE_TYPE_sink) {
        if (depend->getDependenceVector().empty() &&
            depend->getExpressionItems().empty()) {
          addStructureDiagnostic(diagnostics,
                                 ompparser::DiagnosticCode::InvalidClause,
                                 "depend(sink) requires a dependence vector");
        }
      } else if (depend->getType() != OMPC_DEPENDENCE_TYPE_source &&
                 depend->getExpressionItems().empty()) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "depend requires a non-empty locator list");
      }
    }
    if (const auto *reduction =
            dynamic_cast<const OpenMPReductionClause *>(clause)) {
      switch (reduction->getModifier()) {
      case OMPC_REDUCTION_MODIFIER_unspecified:
      case OMPC_REDUCTION_MODIFIER_default:
      case OMPC_REDUCTION_MODIFIER_inscan:
      case OMPC_REDUCTION_MODIFIER_task:
      case OMPC_REDUCTION_MODIFIER_original_private:
        break;
      default:
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "reduction has an unknown modifier");
        break;
      }
      const OpenMPReductionClauseIdentifier identifier =
          reduction->getIdentifier();
      const bool c_only = identifier == OMPC_REDUCTION_IDENTIFIER_bitand ||
                          identifier == OMPC_REDUCTION_IDENTIFIER_bitor ||
                          identifier == OMPC_REDUCTION_IDENTIFIER_bitxor;
      const bool fortran_only = identifier == OMPC_REDUCTION_IDENTIFIER_eqv ||
                                identifier == OMPC_REDUCTION_IDENTIFIER_neqv;
      if (identifier == OMPC_REDUCTION_IDENTIFIER_unknown) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "reduction has an unknown identifier");
      }
      validateReductionIdentifier(directive.getBaseLang(), c_only, fortran_only,
                                  identifier == OMPC_REDUCTION_IDENTIFIER_user,
                                  reduction->getUserDefinedIdentifierFragment(),
                                  diagnostics);
    }
    if (const auto *reduction =
            dynamic_cast<const OpenMPInReductionClause *>(clause)) {
      const OpenMPInReductionClauseIdentifier identifier =
          reduction->getIdentifier();
      const bool c_only = identifier == OMPC_IN_REDUCTION_IDENTIFIER_bitand ||
                          identifier == OMPC_IN_REDUCTION_IDENTIFIER_bitor ||
                          identifier == OMPC_IN_REDUCTION_IDENTIFIER_bitxor;
      const bool fortran_only =
          identifier == OMPC_IN_REDUCTION_IDENTIFIER_eqv ||
          identifier == OMPC_IN_REDUCTION_IDENTIFIER_neqv;
      if (identifier == OMPC_IN_REDUCTION_IDENTIFIER_unknown) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "in_reduction has an unknown identifier");
      }
      validateReductionIdentifier(
          directive.getBaseLang(), c_only, fortran_only,
          identifier == OMPC_IN_REDUCTION_IDENTIFIER_user,
          reduction->getUserDefinedIdentifierFragment(), diagnostics);
    }
    if (const auto *reduction =
            dynamic_cast<const OpenMPTaskReductionClause *>(clause)) {
      const OpenMPTaskReductionClauseIdentifier identifier =
          reduction->getIdentifier();
      const bool c_only = identifier == OMPC_TASK_REDUCTION_IDENTIFIER_bitand ||
                          identifier == OMPC_TASK_REDUCTION_IDENTIFIER_bitor ||
                          identifier == OMPC_TASK_REDUCTION_IDENTIFIER_bitxor;
      const bool fortran_only =
          identifier == OMPC_TASK_REDUCTION_IDENTIFIER_eqv ||
          identifier == OMPC_TASK_REDUCTION_IDENTIFIER_neqv;
      if (identifier == OMPC_TASK_REDUCTION_IDENTIFIER_unknown) {
        addStructureDiagnostic(diagnostics,
                               ompparser::DiagnosticCode::InvalidClause,
                               "task_reduction has an unknown identifier");
      }
      validateReductionIdentifier(
          directive.getBaseLang(), c_only, fortran_only,
          identifier == OMPC_TASK_REDUCTION_IDENTIFIER_user,
          reduction->getUserDefinedIdentifierFragment(), diagnostics);
    }
  }

  auto clause_count = [&directive](OpenMPClauseKind kind) {
    const auto *clauses = directive.findClauses(kind);
    return clauses == nullptr ? std::size_t{0} : clauses->size();
  };
  auto require_any_clause = [&diagnostics, &clause_count](
                                std::initializer_list<OpenMPClauseKind> kinds,
                                const char *message) {
    const std::size_t count = std::accumulate(
        kinds.begin(), kinds.end(), std::size_t{0},
        [&clause_count](std::size_t sum, OpenMPClauseKind kind) {
          return sum + clause_count(kind);
        });
    if (count == 0) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidDirective, message);
    }
    return count;
  };
  auto reject_exclusive_clauses =
      [&diagnostics, &clause_count](
          std::initializer_list<OpenMPClauseKind> kinds, const char *message) {
        const std::size_t count = std::accumulate(
            kinds.begin(), kinds.end(), std::size_t{0},
            [&clause_count](std::size_t sum, OpenMPClauseKind kind) {
              return sum + clause_count(kind);
            });
        if (count > 1) {
          addStructureDiagnostic(
              diagnostics, ompparser::DiagnosticCode::InvalidClause, message);
        }
      };

  switch (directive.getKind()) {
  case OMPD_cancel:
  case OMPD_cancellation_point:
    if (require_any_clause(
            {OMPC_parallel, OMPC_sections, OMPC_for, OMPC_do, OMPC_taskgroup},
            "cancellation directive requires a construct "
            "type") > 1) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidClause,
          "cancellation directive permits exactly one construct type");
    }
    break;
  case OMPD_declare_mapper:
    require_any_clause({OMPC_map},
                       "declare mapper requires at least one map clause");
    break;
  case OMPD_target_data:
  case OMPD_target_data_composite:
    require_any_clause({OMPC_map, OMPC_use_device_addr, OMPC_use_device_ptr},
                       "target_data requires a data-environment clause");
    break;
  case OMPD_target_enter_data:
  case OMPD_target_exit_data:
    require_any_clause({OMPC_map},
                       "target data-motion construct requires a map clause");
    break;
  case OMPD_target_update:
    require_any_clause({OMPC_to, OMPC_from},
                       "target_update requires a to or from clause");
    break;
  case OMPD_depobj:
    if (require_any_clause(
            {OMPC_depend, OMPC_destroy, OMPC_depobj_update, OMPC_init},
            "depobj requires an action clause") > 1) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidClause,
          "depobj permits exactly one depend, destroy, update, or init "
          "clause");
    }
    break;
  case OMPD_interop: {
    require_any_clause({OMPC_destroy, OMPC_init, OMPC_use},
                       "interop requires an action clause");
    if (clause_count(OMPC_depend) == 0) {
      break;
    }

    const auto *init_clauses = directive.findClauses(OMPC_init);
    const bool has_targetsync_init =
        init_clauses != nullptr &&
        std::any_of(
            init_clauses->begin(), init_clauses->end(),
            [](const OpenMPClause *clause) {
              const auto *init = dynamic_cast<const OpenMPInitClause *>(clause);
              return init != nullptr &&
                     initIncludesInteropType(*init, OMPC_INIT_KIND_targetsync);
            });
    // A use or destroy clause inherits its interop type from the object's
    // prior initialization, which requires host-language contextual checks.
    const bool has_inferred_type_action =
        clause_count(OMPC_use) != 0 || clause_count(OMPC_destroy) != 0;
    if (!has_targetsync_init && !has_inferred_type_action) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidClause,
          "depend on interop requires a targetsync interop type");
    }
    break;
  }
  case OMPD_tile:
  case OMPD_stripe:
    require_any_clause({OMPC_sizes}, "tile and stripe require a sizes clause");
    break;
  case OMPD_split:
    require_any_clause({OMPC_counts}, "split requires a counts clause");
    break;
  case OMPD_declare_induction:
    require_any_clause({OMPC_inductor},
                       "declare_induction requires an inductor clause");
    require_any_clause({OMPC_collector},
                       "declare_induction requires a collector clause");
    break;
  default:
    break;
  }

  reject_exclusive_clauses(
      {OMPC_inbranch, OMPC_notinbranch},
      "inbranch and notinbranch clauses are mutually exclusive");
  reject_exclusive_clauses(
      {OMPC_grainsize, OMPC_num_tasks},
      "grainsize and num_tasks clauses are mutually exclusive");
  reject_exclusive_clauses({OMPC_full, OMPC_partial},
                           "full and partial clauses are mutually exclusive");
  if (clause_count(OMPC_copyprivate) != 0 && clause_count(OMPC_nowait) != 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "copyprivate and nowait clauses are mutually exclusive");
  }
  if (clause_count(OMPC_nogroup) != 0 && clause_count(OMPC_reduction) != 0) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "nogroup and reduction clauses are mutually exclusive");
  }
  if (directive.getKind() == OMPD_unroll && clause_count(OMPC_apply) != 0 &&
      clause_count(OMPC_partial) == 0) {
    addStructureDiagnostic(diagnostics,
                           ompparser::DiagnosticCode::InvalidClause,
                           "unroll apply clauses require a partial clause");
  }

  const auto *if_clauses = directive.findClauses(OMPC_if);
  if (if_clauses != nullptr) {
    std::vector<std::pair<OpenMPIfClauseModifier, std::string>> seen;
    for (const OpenMPClause *clause : *if_clauses) {
      const auto *if_clause = dynamic_cast<const OpenMPIfClause *>(clause);
      if (if_clause == nullptr) {
        continue;
      }
      const auto key =
          std::make_pair(if_clause->getModifier(),
                         if_clause->getModifier() == OMPC_IF_MODIFIER_user
                             ? if_clause->getUserDefinedModifier()
                             : std::string());
      if (std::find(seen.begin(), seen.end(), key) != seen.end()) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::DuplicateClause,
            "at most one if clause may apply to each constituent directive");
      } else {
        seen.push_back(key);
      }
    }
  }

  auto validate_categories =
      [&diagnostics](const std::vector<OpenMPClause *> *clauses,
                     OpenMPClauseKind clause_kind) {
        if (clauses == nullptr) {
          return;
        }
        std::vector<OpenMPDefaultmapClauseCategory> seen;
        for (const OpenMPClause *clause : *clauses) {
          OpenMPDefaultmapClauseCategory category =
              OMPC_DEFAULTMAP_CATEGORY_unknown;
          if (clause_kind == OMPC_default) {
            const auto *default_clause =
                dynamic_cast<const OpenMPDefaultClause *>(clause);
            if (default_clause != nullptr) {
              category = default_clause->getCategory();
            }
          } else {
            const auto *defaultmap_clause =
                dynamic_cast<const OpenMPDefaultmapClause *>(clause);
            if (defaultmap_clause != nullptr) {
              category = defaultmap_clause->getCategory();
            }
          }
          if (category == OMPC_DEFAULTMAP_CATEGORY_unspecified) {
            category = OMPC_DEFAULTMAP_CATEGORY_all;
          }
          if (category == OMPC_DEFAULTMAP_CATEGORY_unknown) {
            continue;
          }
          if (std::find(seen.begin(), seen.end(), category) != seen.end()) {
            addStructureDiagnostic(
                diagnostics, ompparser::DiagnosticCode::DuplicateClause,
                std::string("duplicate variable-category on '") +
                    ompparser::getClauseName(clause_kind) + "' clause");
          }
          seen.push_back(category);
        }
        if (std::find(seen.begin(), seen.end(), OMPC_DEFAULTMAP_CATEGORY_all) !=
                seen.end() &&
            seen.size() > 1) {
          addStructureDiagnostic(diagnostics,
                                 ompparser::DiagnosticCode::InvalidClause,
                                 std::string("the all variable-category on '") +
                                     ompparser::getClauseName(clause_kind) +
                                     "' excludes all other occurrences");
        }
      };
  validate_categories(directive.findClauses(OMPC_default), OMPC_default);
  validate_categories(directive.findClauses(OMPC_defaultmap), OMPC_defaultmap);

  if (directive.getKind() == OMPD_end) {
    const auto &end_directive =
        static_cast<const OpenMPEndDirective &>(directive);
    const OpenMPDirective *paired = end_directive.getPairedDirective();
    const OpenMPDirectiveKind paired_kind =
        paired != nullptr ? paired->getKind() : OMPD_unknown;
    if (paired == nullptr) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidDirective,
          "end directive requires an owned paired-directive payload");
    }
    for (const OpenMPClause *clause : directive.getClausesInOriginalOrder()) {
      if (clause == nullptr) {
        continue;
      }
      bool allowed = false;
      if (clause->getKind() == OMPC_copyprivate) {
        allowed = paired_kind == OMPD_single;
      } else if (clause->getKind() == OMPC_nowait) {
        allowed =
            ompparser::isClauseAllowedOnDirective(paired_kind, OMPC_nowait);
      }
      if (!allowed) {
        addStructureDiagnostic(
            diagnostics, ompparser::DiagnosticCode::InvalidClause,
            std::string("clause '") +
                ompparser::getClauseName(clause->getKind()) +
                "' does not have the end-clause property for this directive");
      }
    }
  }

  const OpenMPClauseKind memory_orders[] = {
      OMPC_seq_cst, OMPC_acq_rel, OMPC_release, OMPC_acquire, OMPC_relaxed};
  std::size_t memory_order_count = 0;
  for (OpenMPClauseKind kind : memory_orders) {
    const auto *clauses = directive.findClauses(kind);
    memory_order_count += clauses ? clauses->size() : 0;
  }
  if (memory_order_count > 1) {
    addStructureDiagnostic(
        diagnostics, ompparser::DiagnosticCode::InvalidClause,
        "mutually exclusive memory-order clauses cannot be combined");
  }

  if (directive.getKind() == OMPD_scan) {
    const auto *inclusive = directive.findClauses(OMPC_inclusive);
    const auto *exclusive = directive.findClauses(OMPC_exclusive);
    const auto *init_complete = directive.findClauses(OMPC_init_complete);
    const std::size_t scan_kind_count =
        (inclusive ? inclusive->size() : 0) +
        (exclusive ? exclusive->size() : 0) +
        (init_complete ? init_complete->size() : 0);
    if (scan_kind_count != 1) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidDirective,
          "scan requires exactly one inclusive, exclusive, or init_complete "
          "clause");
    }
  }

  if (const auto *declaration =
          dynamic_cast<const OpenMPDeclareReductionDirective *>(&directive)) {
    const auto *combiner_clauses = directive.findClauses(OMPC_combiner);
    const bool has_combiner_clause =
        combiner_clauses && !combiner_clauses->empty();
    if (declaration->getIdentifier().empty() ||
        declaration->getTypenameList().empty() ||
        (declaration->getCombiner().empty() && !has_combiner_clause)) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidDirective,
          "declare reduction requires an identifier, type list, and combiner");
    }
  }

  if (const auto *threadprivate =
          dynamic_cast<const OpenMPThreadprivateDirective *>(&directive)) {
    if (threadprivate->getThreadprivateList().empty()) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidDirective,
          "threadprivate requires a non-empty variable list");
    }
  }

  if (const auto *groupprivate =
          dynamic_cast<const OpenMPGroupprivateDirective *>(&directive)) {
    if (groupprivate->getGroupprivateList().empty()) {
      addStructureDiagnostic(diagnostics,
                             ompparser::DiagnosticCode::InvalidDirective,
                             "groupprivate requires a non-empty variable list");
    }
  }

  if (const auto *allocate =
          dynamic_cast<const OpenMPAllocateDirective *>(&directive)) {
    if (allocate->getAllocateList().empty()) {
      addStructureDiagnostic(diagnostics,
                             ompparser::DiagnosticCode::InvalidDirective,
                             "allocate requires a non-empty variable list");
    }
  }

  if (const auto *mapper =
          dynamic_cast<const OpenMPDeclareMapperDirective *>(&directive)) {
    if (mapper->getDeclareMapperType().empty() ||
        mapper->getDeclareMapperVar().empty()) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidDirective,
          "declare mapper requires a mapped type and variable");
    }
  }

  if (const auto *depobj =
          dynamic_cast<const OpenMPDepobjDirective *>(&directive)) {
    if (depobj->getDepobj().empty()) {
      addStructureDiagnostic(diagnostics,
                             ompparser::DiagnosticCode::InvalidDirective,
                             "depobj requires a depobj argument");
    }
  }

  if (const auto *declare_variant =
          dynamic_cast<const OpenMPDeclareVariantDirective *>(&directive)) {
    if (declare_variant->getVariantFuncID().empty()) {
      addStructureDiagnostic(
          diagnostics, ompparser::DiagnosticCode::InvalidDirective,
          "declare variant requires a variant function identifier");
    }
  }

  if (const auto *critical =
          dynamic_cast<const OpenMPCriticalDirective *>(&directive)) {
    const ompparser::HostFragment &name = critical->getCriticalNameFragment();
    if (!name.spelling.empty() &&
        (!isOpenMPIdentifier(name.spelling) ||
         name.role != ompparser::HostFragmentRole::Declarator ||
         name.parse_mode != OMP_EXPR_PARSE_openmp_syntax)) {
      addStructureDiagnostic(diagnostics,
                             ompparser::DiagnosticCode::InvalidDirective,
                             "critical name is not a typed OpenMP name");
    }
  }
}

void validateDirectiveTree(const OpenMPDirective &directive,
                           std::vector<ompparser::Diagnostic> &diagnostics,
                           std::vector<const OpenMPDirective *> &active,
                           std::vector<const OpenMPDirective *> &validated) {
  if (std::find(active.begin(), active.end(), &directive) != active.end()) {
    addStructureDiagnostic(diagnostics, ompparser::DiagnosticCode::InvalidAst,
                           "directive tree contains a cycle");
    return;
  }
  if (std::find(validated.begin(), validated.end(), &directive) !=
      validated.end()) {
    return;
  }

  active.push_back(&directive);
  std::vector<std::string> errors;
  directive.validateInvariants(errors);
  for (std::string &message : errors) {
    addStructureDiagnostic(diagnostics, ompparser::DiagnosticCode::InvalidAst,
                           message);
  }
  validateOpenMPStructure(directive, diagnostics);
  visitImmediateNestedDirectives(directive, [&](const OpenMPDirective &nested) {
    validateDirectiveTree(nested, diagnostics, active, validated);
  });
  active.pop_back();
  validated.push_back(&directive);
}

OpenMPBaseLang convertLanguage(ompparser::BaseLanguage language) {
  switch (language) {
  case ompparser::BaseLanguage::C:
    return Lang_C;
  case ompparser::BaseLanguage::CXX:
    return Lang_Cplusplus;
  case ompparser::BaseLanguage::Fortran:
    return Lang_Fortran;
  }
  return Lang_unknown;
}

} // namespace

namespace ompparser::detail {

void beginDiagnostics() { CurrentDiagnostics.clear(); }

void reportDiagnostic(DiagnosticCode code, const std::string &message, int line,
                      int column) {
  Diagnostic diagnostic;
  diagnostic.code = code;
  diagnostic.severity = DiagnosticSeverity::Error;
  diagnostic.range.begin.line = line > 0 ? static_cast<uint32_t>(line) : 0;
  diagnostic.range.begin.column =
      column > 0 ? static_cast<uint32_t>(column) : 0;
  diagnostic.range.end = diagnostic.range.begin;
  diagnostic.message = message;
  CurrentDiagnostics.push_back(std::move(diagnostic));
}

bool hasErrorDiagnostics() {
  return std::any_of(CurrentDiagnostics.begin(), CurrentDiagnostics.end(),
                     [](const Diagnostic &diagnostic) {
                       return diagnostic.severity == DiagnosticSeverity::Error;
                     });
}

std::vector<Diagnostic> takeDiagnostics() {
  std::vector<Diagnostic> diagnostics = std::move(CurrentDiagnostics);
  CurrentDiagnostics.clear();
  return diagnostics;
}

} // namespace ompparser::detail

namespace ompparser {

ParseResult::ParseResult() = default;
ParseResult::~ParseResult() = default;
ParseResult::ParseResult(ParseResult &&) noexcept = default;
ParseResult &ParseResult::operator=(ParseResult &&) noexcept = default;

bool ParseResult::success() const {
  return directive != nullptr &&
         std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) {
                        return diagnostic.severity == DiagnosticSeverity::Error;
                      });
}

bool UnparseResult::success() const {
  return !text.empty() &&
         std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) {
                        return diagnostic.severity == DiagnosticSeverity::Error;
                      });
}

bool DotResult::success() const {
  return !text.empty() &&
         std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) {
                        return diagnostic.severity == DiagnosticSeverity::Error;
                      });
}

bool ValidationResult::success() const {
  return std::none_of(diagnostics.begin(), diagnostics.end(),
                      [](const Diagnostic &diagnostic) {
                        return diagnostic.severity == DiagnosticSeverity::Error;
                      });
}

ValidationResult validate(const OpenMPDirective &directive) {
  ValidationResult result;
  std::vector<const OpenMPDirective *> active;
  std::vector<const OpenMPDirective *> validated;
  validateDirectiveTree(directive, result.diagnostics, active, validated);
  return result;
}

ParseResult parseDirective(std::string_view input,
                           const ParseOptions &options) {
  ParseResult result;
  if (input.data() == nullptr) {
    detail::beginDiagnostics();
    detail::reportDiagnostic(DiagnosticCode::NullInput,
                             "OpenMP directive input is null");
    result.diagnostics = detail::takeDiagnostics();
    return result;
  }

  std::string owned_input(input);
  detail::beginDiagnostics();
  setLang(convertLanguage(options.language));
  result.directive.reset(parseOpenMP(owned_input.c_str()));
  result.diagnostics = detail::takeDiagnostics();
  if (result.directive && options.host_hooks) {
    applyHostLanguageHooks(*result.directive, *options.host_hooks,
                           result.diagnostics);
    options.host_hooks->validate(*result.directive, result.diagnostics);
    result.context_checks_complete = true;
  }

  if (result.directive) {
    ValidationResult validation = validate(*result.directive);
    result.diagnostics.insert(
        result.diagnostics.end(),
        std::make_move_iterator(validation.diagnostics.begin()),
        std::make_move_iterator(validation.diagnostics.end()));
    validateExtensionPolicy(*result.directive, options.extensions,
                            result.diagnostics);
  }

  if (std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                  [](const Diagnostic &diagnostic) {
                    return diagnostic.severity == DiagnosticSeverity::Error;
                  })) {
    result.directive.reset();
  }
  return result;
}

UnparseResult unparse(const OpenMPDirective &directive) {
  UnparseResult result;
  ValidationResult validation = validate(directive);
  if (!validation.success()) {
    result.diagnostics = std::move(validation.diagnostics);
    return result;
  }
  result.text = directive.generatePragmaString();
  if (result.text.empty()) {
    Diagnostic diagnostic;
    diagnostic.code = DiagnosticCode::InvalidAst;
    diagnostic.severity = DiagnosticSeverity::Error;
    diagnostic.message = "cannot unparse an invalid OpenMP AST";
    result.diagnostics.push_back(std::move(diagnostic));
  }
  return result;
}

DotResult toDot(const OpenMPDirective &directive) {
  DotResult result;
  ValidationResult validation = validate(directive);
  if (!validation.success()) {
    result.diagnostics = std::move(validation.diagnostics);
    return result;
  }
  result.text = directive.generateDOTString();
  if (result.text.empty()) {
    Diagnostic diagnostic;
    diagnostic.code = DiagnosticCode::InvalidAst;
    diagnostic.severity = DiagnosticSeverity::Error;
    diagnostic.message = "cannot render an invalid OpenMP AST as DOT";
    result.diagnostics.push_back(std::move(diagnostic));
  }
  return result;
}

} // namespace ompparser
