/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "OpenMPIR.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <stdarg.h>
#include <utility>

extern bool clause_separator_comma;

namespace {

void tightenScopeOps(std::string &text);

std::string trimWhitespace(const std::string &text) {
  const char *whitespace = " \t\n\r\f\v";
  const size_t begin = text.find_first_not_of(whitespace);
  if (begin == std::string::npos) {
    return std::string();
  }
  const size_t end = text.find_last_not_of(whitespace);
  return text.substr(begin, end - begin + 1);
}

std::string normalizeRawExpression(const char *expr,
                                   bool strip_trailing_colon = false) {
  if (!expr) {
    return std::string();
  }
  std::string value(expr);
  value = trimWhitespace(value);
  if (strip_trailing_colon) {
    while (!value.empty() && value.back() == ':') {
      value.pop_back();
    }
    value = trimWhitespace(value);
  }

  // Add spaces around compound assignments like "+=" to keep output stable.
  size_t pos = 0;
  while ((pos = value.find("+=", pos)) != std::string::npos) {
    if (pos > 0 && value[pos - 1] != ' ') {
      value.insert(pos, " ");
      ++pos;
    }
    if (pos + 2 < value.size() && value[pos + 2] != ' ') {
      value.insert(pos + 2, " ");
    }
    pos += 3;
  }

  tightenScopeOps(value);

  return value;
}

} // namespace

namespace {

std::string formatArraySubscripts(const std::string &expr) {
  std::string formatted = expr;
  size_t pos = 0;
  int bracket_depth = 0; // Tracks [] for C/C++
  int paren_depth = 0;   // Tracks () for Fortran arrays

  while (pos < formatted.size()) {
    if (formatted[pos] == '[') {
      bracket_depth++;
      pos++;
    } else if (formatted[pos] == ']') {
      bracket_depth--;
      pos++;
    } else if (formatted[pos] == '(') {
      paren_depth++;
      pos++;
    } else if (formatted[pos] == ')') {
      paren_depth--;
      pos++;
    } else if (formatted[pos] == ':' &&
               (bracket_depth > 0 || paren_depth > 0)) {
      bool space_before = (pos > 0 && formatted[pos - 1] == ' ');
      bool space_after =
          (pos + 1 < formatted.size() && formatted[pos + 1] == ' ');
      if (!space_after && pos + 1 < formatted.size()) {
        formatted.insert(pos + 1, " ");
      }
      if (!space_before && pos > 0) {
        formatted.insert(pos, " ");
        pos++;
      }
      pos++;
    } else {
      pos++;
    }
  }
  return formatted;
}

void tightenScopeOps(std::string &text) {
  size_t p = 0;
  while ((p = text.find(":", p)) != std::string::npos) {
    size_t next = p + 1;
    while (next < text.size() && text[next] == ' ') {
      ++next;
    }
    if (next >= text.size() || text[next] != ':') {
      ++p;
      continue;
    }
    // Remove spaces between the two colons
    if (next > p + 1) {
      text.erase(p + 1, next - (p + 1));
    }
    // Remove spaces after the second colon
    size_t second = p + 1;
    size_t after = second + 1;
    while (after < text.size() && text[after] == ' ') {
      text.erase(after, 1);
    }
    // Remove spaces before the first colon
    while (p > 0 && text[p - 1] == ' ') {
      text.erase(p - 1, 1);
      --p;
    }
    ++p;
  }
}

std::string normalizeClauseExpression(OpenMPClauseKind kind,
                                      const char *expression) {
  std::string result = expression ? std::string(expression) : std::string();
  result = formatArraySubscripts(result);
  if (kind == OMPC_combiner) {
    tightenScopeOps(result);
  }
  return result;
}

} // namespace

void OpenMPApplyClause::addTransformation(OpenMPApplyTransformKind kind,
                                          const std::string &argument,
                                          OpenMPClauseSeparator sep) {
  ApplyTransform t;
  t.kind = kind;
  t.argument = trimWhitespace(argument);
  t.separator = sep;
  transforms.push_back(std::move(t));
}

void OpenMPApplyClause::addNestedApply(OpenMPApplyClause *nested,
                                       OpenMPClauseSeparator sep) {
  if (nested == nullptr) {
    return;
  }
  ApplyTransform t;
  t.kind = OMPC_APPLY_TRANSFORM_apply;
  t.nested_apply.reset(nested);
  t.separator = sep;
  transforms.push_back(std::move(t));
}

OpenMPClause *
OpenMPDirective::registerClause(std::unique_ptr<OpenMPClause> clause) {
  OpenMPClause *raw_ptr = clause.get();
  clause_storage.push_back(std::move(clause));
  return raw_ptr;
}

void OpenMPDeclareReductionDirective::setCombiner(const char *_combiner) {
  combiner = normalizeRawExpression(_combiner, /*strip_trailing_colon=*/true);
}

void OpenMPDeclareReductionDirective::addTypenameList(
    const char *_typename_list) {
  if (_typename_list == nullptr) {
    return;
  }
  std::string cleaned = normalizeRawExpression(_typename_list);
  auto owned_value = std::make_unique<char[]>(cleaned.size() + 1);
  std::memcpy(owned_value.get(), cleaned.c_str(), cleaned.size() + 1);
  const char *stored_expression = owned_value.get();
  typename_list.push_back(stored_expression);
  typename_storage.push_back(std::move(owned_value));
}

void OpenMPInitializerClause::setUserDefinedPriv(char *_priv) {
  user_defined_priv = normalizeRawExpression(_priv);
}

void OpenMPDeclareMapperDirective::setUserDefinedIdentifier(
    std::string _user_defined_identifier) {
  user_defined_identifier = trimWhitespace(_user_defined_identifier);
}

void OpenMPDeclareMapperDirective::setDeclareMapperType(
    const char *_declare_mapper_type) {
  type = normalizeRawExpression(_declare_mapper_type);
}

void OpenMPDeclareMapperDirective::setDeclareMapperVar(
    const char *_declare_mapper_variable) {
  var = normalizeRawExpression(_declare_mapper_variable);
}

void OpenMPClause::addLangExpr(const char *expression,
                               OpenMPClauseSeparator sep, int line, int col) {
  if (expression == nullptr) {
    return;
  }
  std::string normalized = normalizeClauseExpression(this->kind, expression);
  // Since the size of expression vector is supposed to be small, brute force is
  // used here.
  // Skip deduplication if duplicates are allowed (e.g., for sizes(4, 4))
  if (!allow_duplicates) {
    for (unsigned int i = 0; i < this->expressions.size(); i++) {
      if (!strcmp(expressions.at(i), normalized.c_str())) {
        return;
      };
    };
  }
  size_t length = normalized.size();
  auto owned_value = std::make_unique<char[]>(length + 1);
  std::memcpy(owned_value.get(), normalized.c_str(), length + 1);
  const char *stored_expression = owned_value.get();
  expressions.push_back(stored_expression);
  expression_separators.push_back(sep);
  owned_expressions.push_back(std::move(owned_value));
  locations.push_back(SourceLocation(line, col));
};

void OpenMPFirstprivateClause::addLangExpr(const char *expression,
                                           OpenMPClauseSeparator sep, int line,
                                           int col) {
  size_t old_size = expressions.size();
  OpenMPClause::addLangExpr(expression, sep, line, col);
  if (expressions.size() > old_size) {
    saved_statuses.push_back(current_saved_state);
  }
};

void OpenMPInductionClause::addStepExpression(const char *expression) {
  if (expression == nullptr) {
    return;
  }
  std::string cleaned = trimWhitespace(std::string(expression));
  std::string normalized =
      normalizeClauseExpression(OMPC_induction, cleaned.c_str());

  if (step_expression.empty()) {
    step_expression = std::move(normalized);
    sequence.push_back({ItemStep, 0});
    return;
  }

  passthrough_items.push_back(std::move(normalized));
  sequence.push_back({ItemPassthrough, passthrough_items.size() - 1});
}

void OpenMPInductionClause::addBinding(const char *label,
                                       const char *expression) {
  if (expression == nullptr) {
    return;
  }
  Binding binding;
  if (label != nullptr) {
    binding.label = trimWhitespace(std::string(label));
  }
  std::string cleaned = trimWhitespace(std::string(expression));
  binding.expression =
      normalizeClauseExpression(OMPC_induction, cleaned.c_str());
  bindings.push_back(std::move(binding));
  sequence.push_back({ItemBinding, bindings.size() - 1});
}

void OpenMPInductionClause::addPassthroughItem(const char *expression) {
  if (expression == nullptr) {
    return;
  }
  std::string cleaned = trimWhitespace(std::string(expression));
  passthrough_items.push_back(
      normalizeClauseExpression(OMPC_induction, cleaned.c_str()));
  sequence.push_back({ItemPassthrough, passthrough_items.size() - 1});
}

void OpenMPAdjustArgsClause::addArgument(const std::string &arg) {
  std::string cleaned =
      normalizeClauseExpression(OMPC_adjust_args, arg.c_str());
  arguments.push_back(cleaned);
}

std::string OpenMPAdjustArgsClause::toString() {
  std::string result = "adjust_args(";
  std::string modifier_string;
  switch (modifier) {
  case OMPC_ADJUST_ARGS_need_device_ptr:
    modifier_string = "need_device_ptr";
    break;
  case OMPC_ADJUST_ARGS_unknown:
  default:
    modifier_string = raw_modifier;
    break;
  }

  if (!modifier_string.empty()) {
    result += modifier_string;
    if (!arguments.empty()) {
      result += ": ";
    }
  }

  for (size_t i = 0; i < arguments.size(); ++i) {
    if (i > 0) {
      result += ", ";
    }
    result += arguments[i];
  }

  result += ")";
  result += " ";
  return result;
}

void OpenMPAppendArgsClause::addArgument(const std::string &arg) {
  std::string cleaned =
      normalizeClauseExpression(OMPC_append_args, arg.c_str());
  arguments.push_back(cleaned);
}

std::string OpenMPAppendArgsClause::toString() {
  std::string result = "append_args(";
  if (!label.empty()) {
    result += label;
    if (!arguments.empty()) {
      result += ": ";
    }
  }
  for (size_t i = 0; i < arguments.size(); ++i) {
    if (i > 0) {
      result += ", ";
    }
    result += arguments[i];
  }
  result += ")";
  result += " ";
  return result;
}

void OpenMPUsesAllocatorsClause::addUsesAllocatorsAllocatorSequence(
    OpenMPUsesAllocatorsClauseAllocator _allocator,
    std::string _allocator_traits_array, std::string _allocator_user) {
  std::string traits_cleaned = normalizeClauseExpression(
      OMPC_uses_allocators, _allocator_traits_array.c_str());
  std::string user_cleaned =
      normalizeClauseExpression(OMPC_uses_allocators, _allocator_user.c_str());
  auto usesAllocatorsAllocator = std::make_unique<usesAllocatorParameter>(
      _allocator, traits_cleaned, user_cleaned);
  usesAllocatorsAllocatorSequenceView.push_back(usesAllocatorsAllocator.get());
  usesAllocatorsAllocatorSequenceStorage.push_back(
      std::move(usesAllocatorsAllocator));
}

void OpenMPInitClause::addInteropType(OpenMPInitClauseKind value) {
  if (value == OMPC_INIT_KIND_unknown) {
    return;
  }
  for (OpenMPInitClauseKind existing : interop_types) {
    if (existing == value) {
      return;
    }
  }
  interop_types.push_back(value);
}

void OpenMPInitClause::addInteropType(const std::string &raw_type) {
  if (raw_type.empty()) {
    return;
  }
  for (const std::string &existing : raw_interop_types) {
    if (existing == raw_type) {
      return;
    }
  }
  raw_interop_types.push_back(raw_type);
}

std::string OpenMPInitClause::toString() {
  std::string result = "init(";
  bool emitted_modifier = false;

  auto appendModifier = [&](const std::string &text) {
    if (text.empty()) {
      return;
    }
    if (emitted_modifier) {
      result += ", ";
    }
    result += text;
    emitted_modifier = true;
  };

  if (has_directive_name_modifier) {
    switch (directive_name_modifier) {
    case OMPD_depobj:
      appendModifier("depobj");
      break;
    case OMPD_interop:
      appendModifier("interop");
      break;
    default:
      break;
    }
  }

  if (has_prefer_type) {
    appendModifier("prefer_type(" + prefer_type_spec + ")");
  }

  if (has_depinfo) {
    std::string depinfo_keyword;
    switch (depinfo_type) {
    case OMPC_DEPENDENCE_TYPE_in:
      depinfo_keyword = "in";
      break;
    case OMPC_DEPENDENCE_TYPE_out:
      depinfo_keyword = "out";
      break;
    case OMPC_DEPENDENCE_TYPE_inout:
      depinfo_keyword = "inout";
      break;
    case OMPC_DEPENDENCE_TYPE_inoutset:
      depinfo_keyword = "inoutset";
      break;
    case OMPC_DEPENDENCE_TYPE_mutexinoutset:
      depinfo_keyword = "mutexinoutset";
      break;
    default:
      break;
    }
    if (!depinfo_keyword.empty()) {
      appendModifier(depinfo_keyword + "(" + depinfo_locator + ")");
    }
  }

  for (OpenMPInitClauseKind kind : interop_types) {
    switch (kind) {
    case OMPC_INIT_KIND_target:
      appendModifier("target");
      break;
    case OMPC_INIT_KIND_targetsync:
      appendModifier("targetsync");
      break;
    case OMPC_INIT_KIND_unknown:
    default:
      break;
    }
  }

  // Emit raw/unknown interop types
  for (const std::string &raw_type : raw_interop_types) {
    appendModifier(raw_type);
  }

  if (emitted_modifier) {
    result += ": ";
  }
  result += operand;
  result += ")";
  result += " ";
  return result;
}

/**
 *
 * @param kind
 * @param ..., parameters for the clause, the number of max number of parameters
 * is determined by the kind since each clause expects a fixed set of
 * parameters.
 * @return
 */
OpenMPClause *OpenMPDirective::addOpenMPClause(int k, ...) {

  OpenMPClauseKind kind = (OpenMPClauseKind)k;
  std::vector<OpenMPClause *> *current_clauses = getClauses(kind);
  va_list args;
  va_start(args, k);
  OpenMPClause *new_clause = NULL;

  auto makeClause =
      [&](OpenMPClauseKind clause_kind) -> std::unique_ptr<OpenMPClause> {
    if (clause_kind == OMPC_inclusive || clause_kind == OMPC_exclusive) {
      return std::make_unique<OpenMPScanClause>(clause_kind);
    }
    if (clause_kind == OMPC_firstprivate) {
      return std::make_unique<OpenMPFirstprivateClause>();
    }
    if (clause_kind == OMPC_apply) {
      return std::make_unique<OpenMPApplyClause>();
    }
    if (clause_kind == OMPC_induction) {
      return std::make_unique<OpenMPInductionClause>();
    }
    if (clause_kind == OMPC_init) {
      return std::make_unique<OpenMPInitClause>();
    }
    if (clause_kind == OMPC_adjust_args) {
      return std::make_unique<OpenMPAdjustArgsClause>();
    }
    if (clause_kind == OMPC_append_args) {
      return std::make_unique<OpenMPAppendArgsClause>();
    }
    if (clause_kind == OMPC_absent) {
      return std::make_unique<OpenMPAbsentClause>();
    }
    if (clause_kind == OMPC_contains) {
      return std::make_unique<OpenMPContainsClause>();
    }
    if (clause_kind == OMPC_graph_id) {
      return std::make_unique<OpenMPGraphIdClause>();
    }
    if (clause_kind == OMPC_graph_reset) {
      return std::make_unique<OpenMPGraphResetClause>();
    }
    if (clause_kind == OMPC_transparent) {
      return std::make_unique<OpenMPTransparentClause>();
    }
    if (clause_kind == OMPC_replayable) {
      return std::make_unique<OpenMPReplayableClause>();
    }
    if (clause_kind == OMPC_threadset) {
      return std::make_unique<OpenMPThreadsetClause>();
    }
    if (clause_kind == OMPC_indirect) {
      return std::make_unique<OpenMPIndirectClause>();
    }
    if (clause_kind == OMPC_local) {
      return std::make_unique<OpenMPLocalClause>();
    }
    if (clause_kind == OMPC_init_complete) {
      return std::make_unique<OpenMPInitCompleteClause>();
    }
    if (clause_kind == OMPC_safesync) {
      return std::make_unique<OpenMPSafesyncClause>();
    }
    if (clause_kind == OMPC_device_safesync) {
      return std::make_unique<OpenMPDeviceSafesyncClause>();
    }
    if (clause_kind == OMPC_memscope) {
      return std::make_unique<OpenMPMemscopeClause>();
    }
    if (clause_kind == OMPC_looprange) {
      return std::make_unique<OpenMPLooprangeClause>();
    }
    if (clause_kind == OMPC_permutation) {
      return std::make_unique<OpenMPPermutationClause>();
    }
    if (clause_kind == OMPC_counts) {
      return std::make_unique<OpenMPCountsClause>();
    }
    if (clause_kind == OMPC_inductor) {
      return std::make_unique<OpenMPInductorClause>();
    }
    if (clause_kind == OMPC_collector) {
      return std::make_unique<OpenMPCollectorClause>();
    }
    if (clause_kind == OMPC_combiner) {
      return std::make_unique<OpenMPCombinerClause>();
    }
    if (clause_kind == OMPC_no_openmp) {
      return std::make_unique<OpenMPNoOpenmpClause>();
    }
    if (clause_kind == OMPC_no_openmp_constructs) {
      return std::make_unique<OpenMPNoOpenmpConstructsClause>();
    }
    if (clause_kind == OMPC_no_openmp_routines) {
      return std::make_unique<OpenMPNoOpenmpRoutinesClause>();
    }
    if (clause_kind == OMPC_no_parallelism) {
      return std::make_unique<OpenMPNoParallelismClause>();
    }
    if (clause_kind == OMPC_nocontext) {
      return std::make_unique<OpenMPNocontextClause>();
    }
    if (clause_kind == OMPC_novariants) {
      return std::make_unique<OpenMPNovariantsClause>();
    }
    if (clause_kind == OMPC_enter) {
      return std::make_unique<OpenMPEnterClause>();
    }
    if (clause_kind == OMPC_use) {
      return std::make_unique<OpenMPUseClause>();
    }
    if (clause_kind == OMPC_holds) {
      return std::make_unique<OpenMPHoldsClause>();
    }

    return std::make_unique<OpenMPClause>(clause_kind);
  };

  switch (kind) {
  case OMPC_private:
  case OMPC_firstprivate:
  case OMPC_shared:
  case OMPC_num_teams:
  case OMPC_thread_limit:
  case OMPC_copyin:
  case OMPC_align:
  case OMPC_collapse:
  case OMPC_ordered:
  case OMPC_partial:
  case OMPC_nowait:
  case OMPC_full:
  case OMPC_safelen:
  case OMPC_simdlen:
  case OMPC_nontemporal:
  case OMPC_uniform:
  case OMPC_inbranch:
  case OMPC_notinbranch:
  case OMPC_copyprivate:
  case OMPC_parallel:
  case OMPC_sections:
  case OMPC_for:
  case OMPC_do:
  case OMPC_taskgroup:
  case OMPC_inclusive:
  case OMPC_exclusive:
  case OMPC_use_device_ptr:
  case OMPC_use_device_addr:
  case OMPC_nogroup:
  case OMPC_final:
  case OMPC_untied:
  case OMPC_mergeable:
  case OMPC_priority:
  case OMPC_detach:
  case OMPC_reverse_offload:
  case OMPC_unified_address:
  case OMPC_unified_shared_memory:
  case OMPC_dynamic_allocators:
  case OMPC_self_maps:
  case OMPC_is_device_ptr:
  case OMPC_has_device_addr:
  case OMPC_link:
  case OMPC_enter:
  case OMPC_threads:
  case OMPC_simd:
  case OMPC_acq_rel:
  case OMPC_seq_cst:
  case OMPC_release:
  case OMPC_acquire:
  case OMPC_relaxed:
  case OMPC_read:
  case OMPC_write:
  case OMPC_update:
  case OMPC_capture:
  case OMPC_compare:
  case OMPC_weak:
  case OMPC_hint:
  case OMPC_destroy:
  case OMPC_sizes:
  case OMPC_filter:
  case OMPC_message:
  case OMPC_absent:
  case OMPC_contains:
  case OMPC_holds:
  case OMPC_looprange:
  case OMPC_permutation:
  case OMPC_counts:
  case OMPC_apply:
  case OMPC_induction:
  case OMPC_inductor:
  case OMPC_collector:
  case OMPC_combiner:
  case OMPC_adjust_args:
  case OMPC_append_args:
  case OMPC_nocontext:
  case OMPC_novariants:
  case OMPC_no_openmp:
  case OMPC_no_openmp_constructs:
  case OMPC_no_openmp_routines:
  case OMPC_no_parallelism:
  case OMPC_indirect:
  case OMPC_transparent:
  case OMPC_threadset:
  case OMPC_safesync:
  case OMPC_device_safesync:
  case OMPC_local:
  case OMPC_init:
  case OMPC_init_complete:
  case OMPC_use:
  case OMPC_interop:

  {
    if (current_clauses->size() == 0) {
      new_clause = registerClause(makeClause(kind));
      current_clauses->push_back(new_clause);
    } else {
      if (kind == OMPC_num_threads) {
        std::cerr << "Cannot have two num_threads clause for the directive "
                  << kind << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_simdlen) {
        std::cerr << "Cannot have two simdlen clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_safelen) {
        std::cerr << "Cannot have two safelen clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_seq_cst) {
        std::cerr << "Cannot have two seq_cst clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_acq_rel) {
        std::cerr << "Cannot have two acq_rel clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_release) {
        std::cerr << "Cannot have two release clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_acquire) {
        std::cerr << "Cannot have two acquire clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_relaxed) {
        std::cerr << "Cannot have two relaxed clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
      if (kind == OMPC_hint) {
        std::cerr << "Cannot have two hint clause for the directive " << kind
                  << ", ignored\n";
      } else if (this->getNormalizeClauses()) {
        /* we can have multiple clause and we merge them together now, thus we
         * return the object that is already created */
        new_clause = current_clauses->at(0);
      } else {
        /* normalization is disabled, create a new clause */
        new_clause = registerClause(makeClause(kind));
        current_clauses->push_back(new_clause);
      }
    }
    break;
  }
  case OMPC_fail: {
    OpenMPFailClauseMemoryOrder memory_order =
        (OpenMPFailClauseMemoryOrder)va_arg(args, int);
    if (current_clauses->size() == 0) {
      new_clause =
          registerClause(std::make_unique<OpenMPFailClause>(memory_order));
      current_clauses->push_back(new_clause);
    } else {
      std::cerr << "Cannot have two fail clauses for the directive, ignored\n";
    }
    break;
  }

  case OMPC_severity: {
    OpenMPSeverityClauseKind severity_kind =
        (OpenMPSeverityClauseKind)va_arg(args, int);
    if (current_clauses->size() == 0) {
      new_clause =
          registerClause(std::make_unique<OpenMPSeverityClause>(severity_kind));
      current_clauses->push_back(new_clause);
    } else {
      std::cerr
          << "Cannot have two severity clauses for the directive, ignored\n";
    }
    break;
  }

  case OMPC_at: {
    OpenMPAtClauseKind at_kind = (OpenMPAtClauseKind)va_arg(args, int);
    if (current_clauses->size() == 0) {
      new_clause = registerClause(std::make_unique<OpenMPAtClause>(at_kind));
      current_clauses->push_back(new_clause);
    } else {
      std::cerr << "Cannot have two at clauses for the directive, ignored\n";
    }
    break;
  }
  case OMPC_if: {
    OpenMPIfClauseModifier modifier = (OpenMPIfClauseModifier)va_arg(args, int);
    char *user_defined_modifier = NULL;
    if (modifier == OMPC_IF_MODIFIER_user)
      user_defined_modifier = va_arg(args, char *);
    new_clause =
        OpenMPIfClause::addIfClause(this, modifier, user_defined_modifier);
    break;
  }
  case OMPC_default: {
    OpenMPDefaultClauseKind default_kind =
        (OpenMPDefaultClauseKind)va_arg(args, int);
    new_clause = OpenMPDefaultClause::addDefaultClause(this, default_kind);
    break;
  }
  case OMPC_order: {
    OpenMPOrderClauseKind order_kind = (OpenMPOrderClauseKind)va_arg(args, int);
    new_clause = OpenMPOrderClause::addOrderClause(this, order_kind);
    break;
  }
  case OMPC_ext_implementation_defined_requirement: {
    new_clause = OpenMPExtImplementationDefinedRequirementClause::
        addExtImplementationDefinedRequirementClause(this);
    break;
  }
  case OMPC_match: {
    new_clause = OpenMPMatchClause::addMatchClause(this);
    break;
  }

  case OMPC_reduction: {
    OpenMPReductionClauseModifier modifier =
        (OpenMPReductionClauseModifier)va_arg(args, int);
    OpenMPReductionClauseIdentifier identifier =
        (OpenMPReductionClauseIdentifier)va_arg(args, int);
    char *user_defined_modifier = va_arg(args, char *);
    char *user_defined_identifier = va_arg(args, char *);
    new_clause = OpenMPReductionClause::addReductionClause(
        this, modifier, identifier, user_defined_modifier,
        user_defined_identifier);
    break;
  }
  case OMPC_proc_bind: {
    OpenMPProcBindClauseKind proc_bind_kind =
        (OpenMPProcBindClauseKind)va_arg(args, int);
    new_clause = OpenMPProcBindClause::addProcBindClause(this, proc_bind_kind);
    break;
  }
  case OMPC_uses_allocators: {
    new_clause = OpenMPUsesAllocatorsClause::addUsesAllocatorsClause(this);
    break;
  }
  case OMPC_bind: {
    OpenMPBindClauseBinding bind_binding =
        (OpenMPBindClauseBinding)va_arg(args, int);
    new_clause = OpenMPBindClause::addBindClause(this, bind_binding);
    break;
  }

  case OMPC_lastprivate: {
    OpenMPLastprivateClauseModifier modifier =
        (OpenMPLastprivateClauseModifier)va_arg(args, int);
    new_clause = OpenMPLastprivateClause::addLastprivateClause(this, modifier);
    break;
  }

  case OMPC_linear: {
    OpenMPLinearClauseModifier modifier =
        (OpenMPLinearClauseModifier)va_arg(args, int);
    new_clause = OpenMPLinearClause::addLinearClause(this, modifier);
    break;
  }
  case OMPC_aligned: {
    new_clause = OpenMPAlignedClause::addAlignedClause(this);
    break;
  }
  case OMPC_dist_schedule: {
    OpenMPDistScheduleClauseKind dist_schedule_kind =
        (OpenMPDistScheduleClauseKind)va_arg(args, int);
    new_clause = OpenMPDistScheduleClause::addDistScheduleClause(
        this, dist_schedule_kind);
    break;
  }
  case OMPC_schedule: {
    OpenMPScheduleClauseModifier modifier1 =
        (OpenMPScheduleClauseModifier)va_arg(args, int);
    OpenMPScheduleClauseModifier modifier2 =
        (OpenMPScheduleClauseModifier)va_arg(args, int);
    OpenMPScheduleClauseKind schedule_kind =
        (OpenMPScheduleClauseKind)va_arg(args, int);
    char *user_defined_kind = NULL;
    if (schedule_kind == OMPC_SCHEDULE_KIND_user)
      user_defined_kind = va_arg(args, char *);
    new_clause = OpenMPScheduleClause::addScheduleClause(
        this, modifier1, modifier2, schedule_kind, user_defined_kind);

    break;
  }
  case OMPC_device: {
    OpenMPDeviceClauseModifier modifier =
        (OpenMPDeviceClauseModifier)va_arg(args, int);
    new_clause = OpenMPDeviceClause::addDeviceClause(this, modifier);
    break;
  }

  case OMPC_initializer: {
    OpenMPInitializerClausePriv priv =
        (OpenMPInitializerClausePriv)va_arg(args, int);
    char *user_defined_priv = NULL;
    if (priv == OMPC_INITIALIZER_PRIV_user)
      user_defined_priv = va_arg(args, char *);
    new_clause = OpenMPInitializerClause::addInitializerClause(
        this, priv, user_defined_priv);
    break;
  }
  case OMPC_allocate: {
    OpenMPAllocateClauseAllocator allocator =
        (OpenMPAllocateClauseAllocator)va_arg(args, int);
    char *user_defined_allocator = NULL;
    if (allocator == OMPC_ALLOCATE_ALLOCATOR_user)
      user_defined_allocator = va_arg(args, char *);
    new_clause = OpenMPAllocateClause::addAllocateClause(
        this, allocator, user_defined_allocator);
    break;
  }
  case OMPC_allocator: {
    OpenMPAllocatorClauseAllocator allocator =
        (OpenMPAllocatorClauseAllocator)va_arg(args, int);
    char *user_defined_allocator = NULL;
    if (allocator == OMPC_ALLOCATOR_ALLOCATOR_user)
      user_defined_allocator = va_arg(args, char *);
    new_clause = OpenMPAllocatorClause::addAllocatorClause(
        this, allocator, user_defined_allocator);

    break;
  }
  case OMPC_atomic_default_mem_order: {
    OpenMPAtomicDefaultMemOrderClauseKind atomic_default_mem_order_kind =
        (OpenMPAtomicDefaultMemOrderClauseKind)va_arg(args, int);
    new_clause =
        OpenMPAtomicDefaultMemOrderClause::addAtomicDefaultMemOrderClause(
            this, atomic_default_mem_order_kind);

    break;
  }
  case OMPC_in_reduction: {
    OpenMPInReductionClauseIdentifier identifier =
        (OpenMPInReductionClauseIdentifier)va_arg(args, int);
    char *user_defined_identifier = NULL;
    if (identifier == OMPC_IN_REDUCTION_IDENTIFIER_user)
      user_defined_identifier = va_arg(args, char *);
    new_clause = OpenMPInReductionClause::addInReductionClause(
        this, identifier, user_defined_identifier);
    break;
  }
  case OMPC_depobj_update: {
    OpenMPDepobjUpdateClauseDependeceType type =
        (OpenMPDepobjUpdateClauseDependeceType)va_arg(args, int);
    new_clause = OpenMPDepobjUpdateClause::addDepobjUpdateClause(this, type);
    break;
  }
  case OMPC_depend: {
    OpenMPDependClauseModifier modifier =
        (OpenMPDependClauseModifier)va_arg(args, int);
    OpenMPDependClauseType type = (OpenMPDependClauseType)va_arg(args, int);
    new_clause = OpenMPDependClause::addDependClause(this, modifier, type);
    break;
  }
  case OMPC_doacross: {
    OpenMPDoacrossClauseType type = (OpenMPDoacrossClauseType)va_arg(args, int);
    std::vector<OpenMPClause *> *current_clauses = getClauses(OMPC_doacross);
    if (current_clauses->size() == 0 || !this->getNormalizeClauses()) {
      new_clause = registerClause(std::make_unique<OpenMPDoacrossClause>(type));
      current_clauses->push_back(new_clause);
    } else {
      new_clause = current_clauses->at(0);
    }
    break;
  }
  case OMPC_affinity: {
    OpenMPAffinityClauseModifier modifier =
        (OpenMPAffinityClauseModifier)va_arg(args, int);
    new_clause = OpenMPAffinityClause::addAffinityClause(this, modifier);
    break;
  }
  case OMPC_grainsize: {
    OpenMPGrainsizeClauseModifier modifier =
        (OpenMPGrainsizeClauseModifier)va_arg(args, int);
    std::vector<OpenMPClause *> *current_clauses = getClauses(OMPC_grainsize);
    if (current_clauses->size() == 0) {
      new_clause =
          registerClause(std::make_unique<OpenMPGrainsizeClause>(modifier));
      current_clauses->push_back(new_clause);
    } else {
      std::cerr << "Cannot have two grainsize clauses, ignored\n";
    }
    break;
  }
  case OMPC_num_tasks: {
    OpenMPNumTasksClauseModifier modifier =
        (OpenMPNumTasksClauseModifier)va_arg(args, int);
    std::vector<OpenMPClause *> *current_clauses = getClauses(OMPC_num_tasks);
    if (current_clauses->size() == 0) {
      new_clause =
          registerClause(std::make_unique<OpenMPNumTasksClause>(modifier));
      current_clauses->push_back(new_clause);
    } else {
      std::cerr << "Cannot have two num_tasks clauses, ignored\n";
    }
    break;
  }
  case OMPC_to: {
    OpenMPToClauseKind to_kind = (OpenMPToClauseKind)va_arg(args, int);
    new_clause = OpenMPToClause::addToClause(this, to_kind);
    break;
  }
  case OMPC_from: {
    OpenMPFromClauseKind from_kind = (OpenMPFromClauseKind)va_arg(args, int);
    new_clause = OpenMPFromClause::addFromClause(this, from_kind);
    break;
  }

  case OMPC_device_type: {
    OpenMPDeviceTypeClauseKind device_type_kind =
        (OpenMPDeviceTypeClauseKind)va_arg(args, int);
    new_clause =
        OpenMPDeviceTypeClause::addDeviceTypeClause(this, device_type_kind);
    break;
  }

  case OMPC_defaultmap: {
    OpenMPDefaultmapClauseBehavior behavior =
        (OpenMPDefaultmapClauseBehavior)va_arg(args, int);
    OpenMPDefaultmapClauseCategory category =
        (OpenMPDefaultmapClauseCategory)va_arg(args, int);
    new_clause =
        OpenMPDefaultmapClause::addDefaultmapClause(this, behavior, category);
    break;
  }
  case OMPC_task_reduction: {
    OpenMPTaskReductionClauseIdentifier identifier =
        (OpenMPTaskReductionClauseIdentifier)va_arg(args, int);
    char *user_defined_identifier = NULL;
    if (identifier == OMPC_TASK_REDUCTION_IDENTIFIER_user)
      user_defined_identifier = va_arg(args, char *);
    new_clause = OpenMPTaskReductionClause::addTaskReductionClause(
        this, identifier, user_defined_identifier);
    break;
  }
  case OMPC_map: {
    OpenMPMapClauseModifier modifier1 =
        (OpenMPMapClauseModifier)va_arg(args, int);
    OpenMPMapClauseModifier modifier2 =
        (OpenMPMapClauseModifier)va_arg(args, int);
    OpenMPMapClauseModifier modifier3 =
        (OpenMPMapClauseModifier)va_arg(args, int);
    OpenMPMapClauseType type = (OpenMPMapClauseType)va_arg(args, int);
    OpenMPMapClauseRefModifier ref_modifier =
        (OpenMPMapClauseRefModifier)va_arg(args, int);
    std::string mapper_identifier = (std::string)va_arg(args, char *);
    new_clause =
        OpenMPMapClause::addMapClause(this, modifier1, modifier2, modifier3,
                                      type, ref_modifier, mapper_identifier);
    break;
  }
  case OMPC_num_threads: {
    if (current_clauses->size() == 0) {
      new_clause = registerClause(std::make_unique<OpenMPNumThreadsClause>());
      current_clauses->push_back(new_clause);
    } else {
      new_clause = current_clauses->at(0);
    }
    break;
  }
  case OMPC_when: {
    new_clause = OpenMPWhenClause::addWhenClause(this);
    break;
  }
  case OMPC_otherwise: {
    new_clause = OpenMPOtherwiseClause::addOtherwiseClause(this);
    break;
  }
  default: {
    ;
  }
  };

  if (new_clause != NULL) {
    if (clause_separator_comma) {
      new_clause->setPrecedingSeparator(OMPC_CLAUSE_SEP_comma);
    } else {
      new_clause->setPrecedingSeparator(OMPC_CLAUSE_SEP_space);
    }
    clause_separator_comma = false;
  }

  va_end(args);
  if (new_clause != NULL && new_clause->getClausePosition() == -1) {
    this->getClausesInOriginalOrder()->push_back(new_clause);
    new_clause->setClausePosition(this->getClausesInOriginalOrder()->size() -
                                  1);
  };
  return new_clause;
};

OpenMPClause *OpenMPMapClause::addMapClause(
    OpenMPDirective *directive, OpenMPMapClauseModifier modifier1,
    OpenMPMapClauseModifier modifier2, OpenMPMapClauseModifier modifier3,
    OpenMPMapClauseType type, OpenMPMapClauseRefModifier ref_modifier,
    std::string mapper_identifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_map);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPMapClause>(modifier1, modifier2, modifier3, type,
                                          ref_modifier, mapper_identifier));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        if (((OpenMPMapClause *)(*it))->getModifier1() == modifier1 &&
            ((OpenMPMapClause *)(*it))->getModifier2() == modifier2 &&
            ((OpenMPMapClause *)(*it))->getModifier3() == modifier3 &&
            ((OpenMPMapClause *)(*it))->getType() == type &&
            ((OpenMPMapClause *)(*it))->getRefModifier() == ref_modifier) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPMapClause>(modifier1, modifier2, modifier3, type,
                                          ref_modifier, mapper_identifier));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPTaskReductionClause::addTaskReductionClause(
    OpenMPDirective *directive, OpenMPTaskReductionClauseIdentifier identifier,
    char *user_defined_identifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_task_reduction);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPTaskReductionClause>(identifier));
    if (identifier == OMPC_TASK_REDUCTION_IDENTIFIER_user &&
        user_defined_identifier) {
      ((OpenMPTaskReductionClause *)new_clause)
          ->setUserDefinedIdentifier(user_defined_identifier);
    };
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        std::string current_user_defined_identifier_expression;
        if (user_defined_identifier) {
          current_user_defined_identifier_expression =
              std::string(user_defined_identifier);
        };
        if (((OpenMPTaskReductionClause *)(*it))->getIdentifier() ==
                identifier &&
            current_user_defined_identifier_expression.compare(
                ((OpenMPTaskReductionClause *)(*it))
                    ->getUserDefinedIdentifier()) == 0) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPTaskReductionClause>(identifier));
    if (identifier == OMPC_TASK_REDUCTION_IDENTIFIER_user)
      ((OpenMPTaskReductionClause *)new_clause)
          ->setUserDefinedIdentifier(user_defined_identifier);
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPDefaultmapClause::addDefaultmapClause(
    OpenMPDirective *directive, OpenMPDefaultmapClauseBehavior behavior,
    OpenMPDefaultmapClauseCategory category) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_defaultmap);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDefaultmapClause>(behavior, category));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        if (((OpenMPDefaultmapClause *)(*it))->getBehavior() == behavior &&
            ((OpenMPDefaultmapClause *)(*it))->getCategory() == category) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDefaultmapClause>(behavior, category));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPDeviceTypeClause::addDeviceTypeClause(
    OpenMPDirective *directive, OpenMPDeviceTypeClauseKind device_type_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_device_type);
  OpenMPClause *new_clause = NULL;
  new_clause = directive->registerClause(
      std::make_unique<OpenMPDeviceTypeClause>(device_type_kind));
  current_clauses->push_back(new_clause);

  return new_clause;
};

OpenMPClause *OpenMPProcBindClause::addProcBindClause(
    OpenMPDirective *directive, OpenMPProcBindClauseKind proc_bind_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_proc_bind);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPProcBindClause>(proc_bind_kind));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    std::cerr << "Cannot have two procbind clause for the directive "
              << directive->getKind() << ", ignored\n";
  };
  return new_clause;
};

OpenMPClause *
OpenMPBindClause::addBindClause(OpenMPDirective *directive,
                                OpenMPBindClauseBinding bind_binding) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_bind);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPBindClause>(bind_binding));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    std::cerr << "Cannot have two bind clause for the directive "
              << directive->getKind() << ", ignored\n";
  };

  return new_clause;
};

OpenMPClause *
OpenMPLinearClause::addLinearClause(OpenMPDirective *directive,
                                    OpenMPLinearClauseModifier modifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_linear);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPLinearClause>(modifier));
    current_clauses->push_back(new_clause);
  } else {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPLinearClause>(modifier));
    current_clauses->push_back(new_clause);
  };
  return new_clause;
};

OpenMPClause *OpenMPExtImplementationDefinedRequirementClause::
    addExtImplementationDefinedRequirementClause(OpenMPDirective *directive) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_ext_implementation_defined_requirement);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPExtImplementationDefinedRequirementClause>());
    current_clauses->push_back(new_clause);
  } else {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPExtImplementationDefinedRequirementClause>());
    current_clauses->push_back(new_clause);
  };
  return new_clause;
};

void OpenMPExtImplementationDefinedRequirementClause::
    mergeExtImplementationDefinedRequirement(OpenMPDirective *directive,
                                             OpenMPClause *current_clause) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_ext_implementation_defined_requirement);

  for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
       it != current_clauses->end() - 1; it++) {
    if (((OpenMPExtImplementationDefinedRequirementClause *)(*it))
            ->getImplementationDefinedRequirement() ==
        ((OpenMPExtImplementationDefinedRequirementClause *)current_clause)
            ->getImplementationDefinedRequirement()) {
      directive->getClausesInOriginalOrder()->pop_back();
      std::cerr << "You can not have 2 same "
                   "ext_implementation_defined_requirement clauses, ignored\n";
      break;
    }
  }
};

void OpenMPLinearClause::mergeLinear(OpenMPDirective *directive,
                                     OpenMPClause *current_clause) {

  // Only merge if normalization is enabled
  if (!directive->getNormalizeClauses()) {
    return;
  }

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_linear);

  for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
       it != current_clauses->end() - 1; it++) {

    if (((OpenMPLinearClause *)(*it))->getModifier() ==
            ((OpenMPLinearClause *)current_clause)->getModifier() &&
        ((OpenMPLinearClause *)(*it))->getUserDefinedStep() ==
            ((OpenMPLinearClause *)current_clause)->getUserDefinedStep()) {
      std::vector<const char *> *expressions_previous_clause =
          ((OpenMPLinearClause *)(*it))->getExpressions();
      std::vector<const char *> *expressions_current_clause =
          current_clause->getExpressions();
      const auto &current_separators =
          static_cast<OpenMPLinearClause *>(current_clause)
              ->getExpressionSeparators();

      size_t idx = 0;
      for (std::vector<const char *>::iterator it_expr_current =
               expressions_current_clause->begin();
           it_expr_current != expressions_current_clause->end();
           it_expr_current++, ++idx) {
        bool not_normalize = false;
        for (std::vector<const char *>::iterator it_expr_previous =
                 expressions_previous_clause->begin();
             it_expr_previous != expressions_previous_clause->end();
             it_expr_previous++) {
          if (strcmp(*it_expr_current, *it_expr_previous) == 0) {
            not_normalize = true;
            break;
          }
        }
        if (!not_normalize) {
          OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_space;
          if (idx < current_separators.size()) {
            sep = current_separators[idx];
          }
          (*it)->addLangExpr(*it_expr_current, sep);
        }
      }
      current_clauses->pop_back();
      directive->getClausesInOriginalOrder()->pop_back();

      break;
    }
  }
};

OpenMPClause *OpenMPReductionClause::addReductionClause(
    OpenMPDirective *directive, OpenMPReductionClauseModifier modifier,
    OpenMPReductionClauseIdentifier identifier, char *user_defined_modifier,
    char *user_defined_identifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_reduction);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPReductionClause>(modifier, identifier));
    ((OpenMPReductionClause *)new_clause)
        ->setUserDefinedModifier(user_defined_modifier);
    if (identifier == OMPC_REDUCTION_IDENTIFIER_user &&
        user_defined_identifier) {
      ((OpenMPReductionClause *)new_clause)
          ->setUserDefinedIdentifier(user_defined_identifier);
    };
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); it++) {
        std::string current_user_defined_identifier_expression;
        if (user_defined_identifier) {
          current_user_defined_identifier_expression =
              std::string(user_defined_identifier);
        };
        std::string current_user_defined_modifier_expression;
        if (user_defined_modifier) {
          current_user_defined_modifier_expression =
              std::string(user_defined_modifier);
        };
        if (((OpenMPReductionClause *)(*it))->getModifier() == modifier &&
            ((OpenMPReductionClause *)(*it))->getIdentifier() == identifier &&
            current_user_defined_identifier_expression.compare(
                ((OpenMPReductionClause *)(*it))->getUserDefinedIdentifier()) ==
                0 &&
            current_user_defined_modifier_expression.compare(
                ((OpenMPReductionClause *)(*it))->getUserDefinedModifier()) ==
                0) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPReductionClause>(modifier, identifier));
    ((OpenMPReductionClause *)new_clause)
        ->setUserDefinedModifier(user_defined_modifier);
    if (identifier == OMPC_REDUCTION_IDENTIFIER_user)
      ((OpenMPReductionClause *)new_clause)
          ->setUserDefinedIdentifier(user_defined_identifier);
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPFromClause::addFromClause(OpenMPDirective *directive,
                                              OpenMPFromClauseKind from_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_from);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPFromClause>(from_kind));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        if (((OpenMPFromClause *)(*it))->getKind() == from_kind) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPFromClause>(from_kind));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPToClause::addToClause(OpenMPDirective *directive,
                                          OpenMPToClauseKind to_kind) {

  std::vector<OpenMPClause *> *current_clauses = directive->getClauses(OMPC_to);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause =
        directive->registerClause(std::make_unique<OpenMPToClause>(to_kind));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        if (((OpenMPToClause *)(*it))->getKind() == to_kind) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause =
        directive->registerClause(std::make_unique<OpenMPToClause>(to_kind));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *
OpenMPAffinityClause::addAffinityClause(OpenMPDirective *directive,
                                        OpenMPAffinityClauseModifier modifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_affinity);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAffinityClause>(modifier));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        if (((OpenMPAffinityClause *)(*it))->getModifier() == modifier) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAffinityClause>(modifier));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *
OpenMPDependClause::addDependClause(OpenMPDirective *directive,
                                    OpenMPDependClauseModifier modifier,
                                    OpenMPDependClauseType type) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_depend);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDependClause>(modifier, type));
    current_clauses->push_back(new_clause);
  } else {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDependClause>(modifier, type));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};
void OpenMPDependClause::mergeDepend(OpenMPDirective *directive,
                                     OpenMPClause *current_clause) {

  // Only merge if normalization is enabled
  if (!directive->getNormalizeClauses()) {
    return;
  }

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_depend);

  if (current_clauses->size() == true)
    return;

  for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
       it != current_clauses->end() - 1; it++) {
    if (((OpenMPDependClause *)(*it))->getModifier() !=
            OMPC_DEPEND_MODIFIER_unspecified &&
        ((OpenMPDependClause *)(*it))->getModifier() ==
            ((OpenMPDependClause *)current_clause)->getModifier() &&
        ((OpenMPDependClause *)(*it))->getType() ==
            ((OpenMPDependClause *)current_clause)->getType()) {
      const auto &prev_iters =
          static_cast<OpenMPDependClause *>(*it)->getIterators();
      const auto &curr_iters =
          static_cast<OpenMPDependClause *>(current_clause)->getIterators();
      if (prev_iters.size() == curr_iters.size()) {
        bool normalize = true;
        for (size_t idx = 0; idx < prev_iters.size(); ++idx) {
          const auto &p = prev_iters[idx];
          const auto &c = curr_iters[idx];
          if (p.qualifier != c.qualifier || p.var != c.var ||
              p.begin != c.begin || p.end != c.end || p.step != c.step) {
            normalize = false;
            break;
          }
        }
        if (normalize) {
          std::vector<const char *> *expressions_previous =
              ((OpenMPDependClause *)(*it))->getExpressions();
          std::vector<const char *> *expressions_current =
              current_clause->getExpressions();
          const auto &current_separators =
              static_cast<OpenMPDependClause *>(current_clause)
                  ->getExpressionSeparators();
          bool has_existing = !expressions_previous->empty();
          size_t idx = 0;
          for (std::vector<const char *>::iterator it_expr_current =
                   expressions_current->begin();
               it_expr_current != expressions_current->end();
               it_expr_current++, ++idx) {
            bool para_merge = true;
            for (std::vector<const char *>::iterator it_expr_previous =
                     expressions_previous->begin();
                 it_expr_previous != expressions_previous->end();
                 it_expr_previous++) {
              if (strcmp(*it_expr_current, *it_expr_previous) == 0) {
                para_merge = false;
              }
            }
            if (para_merge == true)
              (*it)->addLangExpr(
                  *it_expr_current,
                  (idx < current_separators.size())
                      ? (has_existing && current_separators[idx] ==
                                             OMPC_CLAUSE_SEP_space
                             ? OMPC_CLAUSE_SEP_comma
                             : current_separators[idx])
                      : OMPC_CLAUSE_SEP_comma);
          }
          current_clauses->pop_back();
          directive->getClausesInOriginalOrder()->pop_back();
          break;
        }
      } else {
        directive->setNormalizeClauses(false);
      }

    } else if (((OpenMPDependClause *)(*it))->getModifier() ==
                   OMPC_DEPEND_MODIFIER_unspecified &&
               ((OpenMPDependClause *)(*it))->getModifier() ==
                   ((OpenMPDependClause *)current_clause)->getModifier() &&
               ((OpenMPDependClause *)(*it))->getType() ==
                   ((OpenMPDependClause *)current_clause)->getType()) {
      std::vector<const char *> *expressions_previous =
          ((OpenMPDependClause *)(*it))->getExpressions();
      std::vector<const char *> *expressions_current =
          current_clause->getExpressions();
      const auto &current_separators =
          static_cast<OpenMPDependClause *>(current_clause)
              ->getExpressionSeparators();
      bool has_existing = !expressions_previous->empty();
      size_t idx = 0;
      for (std::vector<const char *>::iterator it_expr_current =
               expressions_current->begin();
           it_expr_current != expressions_current->end();
           it_expr_current++, ++idx) {
        bool para_merge = true;
        for (std::vector<const char *>::iterator it_expr_previous =
                 expressions_previous->begin();
             it_expr_previous != expressions_previous->end();
             it_expr_previous++) {
          if (strcmp(*it_expr_current, *it_expr_previous) == 0) {
            para_merge = false;
          }
        }
        if (para_merge == true)
          (*it)->addLangExpr(*it_expr_current,
                             (idx < current_separators.size())
                                 ? (has_existing && current_separators[idx] ==
                                                        OMPC_CLAUSE_SEP_space
                                        ? OMPC_CLAUSE_SEP_comma
                                        : current_separators[idx])
                                 : OMPC_CLAUSE_SEP_comma);
      }
      current_clauses->pop_back();
      directive->getClausesInOriginalOrder()->pop_back();
      break;
    }
  }
};

OpenMPClause *OpenMPDepobjUpdateClause::addDepobjUpdateClause(
    OpenMPDirective *directive, OpenMPDepobjUpdateClauseDependeceType type) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_depobj_update);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDepobjUpdateClause>(type));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        if (((OpenMPDepobjUpdateClause *)(*it))->getType() == type) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDepobjUpdateClause>(type));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPInReductionClause::addInReductionClause(
    OpenMPDirective *directive, OpenMPInReductionClauseIdentifier identifier,
    char *user_defined_identifier) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_in_reduction);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPInReductionClause>(identifier));
    if (identifier == OMPC_IN_REDUCTION_IDENTIFIER_user &&
        user_defined_identifier) {
      ((OpenMPInReductionClause *)new_clause)
          ->setUserDefinedIdentifier(user_defined_identifier);
    };
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        std::string current_user_defined_identifier_expression;
        if (user_defined_identifier) {
          current_user_defined_identifier_expression =
              std::string(user_defined_identifier);
        };
        if (((OpenMPInReductionClause *)(*it))->getIdentifier() == identifier &&
            current_user_defined_identifier_expression.compare(
                ((OpenMPInReductionClause *)(*it))
                    ->getUserDefinedIdentifier()) == 0) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPInReductionClause>(identifier));
    if (identifier == OMPC_IN_REDUCTION_IDENTIFIER_user)
      ((OpenMPInReductionClause *)new_clause)
          ->setUserDefinedIdentifier(user_defined_identifier);
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPAtomicDefaultMemOrderClause::addAtomicDefaultMemOrderClause(
    OpenMPDirective *directive,
    OpenMPAtomicDefaultMemOrderClauseKind atomic_default_mem_order_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_atomic_default_mem_order);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAtomicDefaultMemOrderClause>(
            atomic_default_mem_order_kind));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    std::cerr
        << "Cannot have two atomic_default_mem_orders clause for the directive "
        << directive->getKind() << ", ignored\n";
  }
  return new_clause;
};

OpenMPClause *OpenMPAllocatorClause::addAllocatorClause(
    OpenMPDirective *directive, OpenMPAllocatorClauseAllocator allocator,
    char *user_defined_allocator) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_allocator);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAllocatorClause>(allocator));
    if (allocator == OMPC_ALLOCATOR_ALLOCATOR_user)
      ((OpenMPAllocatorClause *)new_clause)
          ->setUserDefinedAllocator(user_defined_allocator);
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        std::string current_user_defined_allocator_expression;
        if (user_defined_allocator) {
          current_user_defined_allocator_expression =
              std::string(user_defined_allocator);
        };
        if (((OpenMPAllocatorClause *)(*it))->getAllocator() == allocator &&
            current_user_defined_allocator_expression.compare(
                ((OpenMPAllocatorClause *)(*it))->getUserDefinedAllocator()) ==
                0) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAllocatorClause>(allocator));
    if (allocator == OMPC_ALLOCATOR_ALLOCATOR_user)
      ((OpenMPAllocatorClause *)new_clause)
          ->setUserDefinedAllocator(user_defined_allocator);
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *
OpenMPAllocateClause::addAllocateClause(OpenMPDirective *directive,
                                        OpenMPAllocateClauseAllocator allocator,
                                        char *user_defined_allocator) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_allocate);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAllocateClause>(allocator));
    if (allocator == OMPC_ALLOCATE_ALLOCATOR_user)
      ((OpenMPAllocateClause *)new_clause)
          ->setUserDefinedAllocator(user_defined_allocator);
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        std::string current_user_defined_allocator_expression;
        if (user_defined_allocator) {
          current_user_defined_allocator_expression =
              std::string(user_defined_allocator);
        };
        if (((OpenMPAllocateClause *)(*it))->getAllocator() == allocator &&
            current_user_defined_allocator_expression.compare(
                ((OpenMPAllocateClause *)(*it))->getUserDefinedAllocator()) ==
                0) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAllocateClause>(allocator));
    if (allocator == OMPC_ALLOCATE_ALLOCATOR_user)
      ((OpenMPAllocateClause *)new_clause)
          ->setUserDefinedAllocator(user_defined_allocator);
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *
OpenMPInitializerClause::addInitializerClause(OpenMPDirective *directive,
                                              OpenMPInitializerClausePriv priv,
                                              char *user_defined_priv) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_initializer);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPInitializerClause>(priv));
    if (priv == OMPC_INITIALIZER_PRIV_user)
      ((OpenMPInitializerClause *)new_clause)
          ->setUserDefinedPriv(user_defined_priv);
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        std::string current_user_defined_priv_expression;
        if (user_defined_priv) {
          current_user_defined_priv_expression =
              normalizeRawExpression(user_defined_priv);
        };
        if (((OpenMPInitializerClause *)(*it))->getPriv() == priv &&
            current_user_defined_priv_expression.compare(
                ((OpenMPInitializerClause *)(*it))->getUserDefinedPriv()) ==
                0) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPInitializerClause>(priv));
    if (priv == OMPC_INITIALIZER_PRIV_user)
      ((OpenMPInitializerClause *)new_clause)
          ->setUserDefinedPriv(user_defined_priv);
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *
OpenMPDeviceClause::addDeviceClause(OpenMPDirective *directive,
                                    OpenMPDeviceClauseModifier modifier) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_device);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDeviceClause>(modifier));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        if (((OpenMPDeviceClause *)(*it))->getModifier() == modifier) {
          new_clause = (*it);
          return new_clause;
        }
      }
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDeviceClause>(modifier));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPScheduleClause::addScheduleClause(
    OpenMPDirective *directive, OpenMPScheduleClauseModifier modifier1,
    OpenMPScheduleClauseModifier modifier2,
    OpenMPScheduleClauseKind schedule_kind, char *user_defined_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_schedule);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause =
        directive->registerClause(std::make_unique<OpenMPScheduleClause>(
            modifier1, modifier2, schedule_kind));
    if (schedule_kind == OMPC_SCHEDULE_KIND_user)
      ((OpenMPScheduleClause *)new_clause)
          ->setUserDefinedKind(user_defined_kind);
    current_clauses->push_back(new_clause);
  } else {
    std::cerr << "Cannot have two schedule clause for the directive "
              << directive->getKind() << ", ignored\n";
  }
  return new_clause;
};

OpenMPClause *OpenMPDistScheduleClause::addDistScheduleClause(
    OpenMPDirective *directive,
    OpenMPDistScheduleClauseKind dist_schedule_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_dist_schedule);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDistScheduleClause>(dist_schedule_kind));
    current_clauses->push_back(new_clause);
  } else {
    std::cerr << "Cannot have two dist_schedule clause for the directive "
              << directive->getKind() << ", ignored\n";
  }
  return new_clause;
};

OpenMPClause *OpenMPLastprivateClause::addLastprivateClause(
    OpenMPDirective *directive, OpenMPLastprivateClauseModifier modifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_lastprivate);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPLastprivateClause>(modifier));
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {

        if (((OpenMPLastprivateClause *)(*it))->getModifier() == modifier) {
          new_clause = (*it);
          return new_clause;
        };
      };
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause = directive->registerClause(
        std::make_unique<OpenMPLastprivateClause>(modifier));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPIfClause::addIfClause(OpenMPDirective *directive,
                                          OpenMPIfClauseModifier modifier,
                                          char *user_defined_modifier) {

  std::vector<OpenMPClause *> *current_clauses = directive->getClauses(OMPC_if);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause =
        directive->registerClause(std::make_unique<OpenMPIfClause>(modifier));
    if (modifier == OMPC_IF_MODIFIER_user) {
      ((OpenMPIfClause *)new_clause)
          ->setUserDefinedModifier(user_defined_modifier);
    };
    current_clauses->push_back(new_clause);
  } else {
    // Only merge if normalization is enabled
    if (directive->getNormalizeClauses()) {
      for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
           it != current_clauses->end(); ++it) {
        std::string current_user_defined_modifier_expression;
        if (user_defined_modifier) {
          current_user_defined_modifier_expression =
              std::string(user_defined_modifier);
        };
        if (((OpenMPIfClause *)(*it))->getModifier() == modifier &&
            current_user_defined_modifier_expression.compare(
                ((OpenMPIfClause *)(*it))->getUserDefinedModifier()) == 0) {
          new_clause = (*it);
          return new_clause;
        };
      };
    }
    /* could not find the matching object for this clause, or normalization is
     * disabled */
    new_clause =
        directive->registerClause(std::make_unique<OpenMPIfClause>(modifier));
    if (modifier == OMPC_IF_MODIFIER_user) {
      ((OpenMPIfClause *)new_clause)
          ->setUserDefinedModifier(user_defined_modifier);
    }
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *
OpenMPDefaultClause::addDefaultClause(OpenMPDirective *directive,
                                      OpenMPDefaultClauseKind default_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_default);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDefaultClause>(default_kind));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    std::cerr << "Cannot have two default clause for the directive "
              << directive->getKind() << ", ignored\n";
  };

  return new_clause;
};

OpenMPClause *
OpenMPOrderClause::addOrderClause(OpenMPDirective *directive,
                                  OpenMPOrderClauseModifier order_modifier,
                                  OpenMPOrderClauseKind order_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_order);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPOrderClause>(order_modifier, order_kind));
    current_clauses->push_back(new_clause);
    // Add to clauses_in_original_order
    if (new_clause != NULL && new_clause->getClausePosition() == -1) {
      directive->getClausesInOriginalOrder()->push_back(new_clause);
      new_clause->setClausePosition(
          directive->getClausesInOriginalOrder()->size() - 1);
    }
  } else { /* could be an error since if clause may only appear once */
    std::cerr << "Cannot have two order clause for the directive "
              << directive->getKind() << ", ignored\n";
  };

  return new_clause;
};

OpenMPClause *
OpenMPOrderClause::addOrderClause(OpenMPDirective *directive,
                                  OpenMPOrderClauseKind order_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_order);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPOrderClause>(order_kind));
    current_clauses->push_back(new_clause);
    // Add to clauses_in_original_order
    if (new_clause != NULL && new_clause->getClausePosition() == -1) {
      directive->getClausesInOriginalOrder()->push_back(new_clause);
      new_clause->setClausePosition(
          directive->getClausesInOriginalOrder()->size() - 1);
    }
  } else { /* could be an error since if clause may only appear once */
    std::cerr << "Cannot have two order clause for the directive "
              << directive->getKind() << ", ignored\n";
  };

  return new_clause;
};

OpenMPClause *
OpenMPAlignedClause::addAlignedClause(OpenMPDirective *directive) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_aligned);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause =
        directive->registerClause(std::make_unique<OpenMPAlignedClause>());
    current_clauses->push_back(new_clause);
  }

  return new_clause;
};

OpenMPClause *OpenMPWhenClause::addWhenClause(OpenMPDirective *directive) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_when);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
  };
  new_clause = directive->registerClause(std::make_unique<OpenMPWhenClause>());
  current_clauses->push_back(new_clause);

  return new_clause;
};

OpenMPClause *
OpenMPOtherwiseClause::addOtherwiseClause(OpenMPDirective *directive) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_otherwise);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
  };
  new_clause =
      directive->registerClause(std::make_unique<OpenMPOtherwiseClause>());
  current_clauses->push_back(new_clause);

  return new_clause;
};

OpenMPClause *OpenMPMatchClause::addMatchClause(OpenMPDirective *directive) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_match);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause =
        directive->registerClause(std::make_unique<OpenMPMatchClause>());
    current_clauses->push_back(new_clause);
  } else {
    /* we can have multiple clause and we merge them together now, thus we
     * return the object that is already created */
    new_clause = current_clauses->at(0);
  }
  return new_clause;
};

OpenMPClause *OpenMPUsesAllocatorsClause::addUsesAllocatorsClause(
    OpenMPDirective *directive) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_uses_allocators);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
  };
  new_clause =
      directive->registerClause(std::make_unique<OpenMPUsesAllocatorsClause>());
  current_clauses->push_back(new_clause);

  return new_clause;
};

// Helper function to convert OpenMPDirectiveKind to string without trailing
// space.
static std::string OpenMPDirectiveKindToString(OpenMPDirectiveKind kind) {
  OpenMPDirective temp(kind);
  std::string result = temp.toString();
  // Trim trailing space if present
  if (!result.empty() && result.back() == ' ') {
    result.pop_back();
  }
  return result;
}

std::string OpenMPAbsentClause::toString() {
  std::string result = "absent(";
  bool first = true;
  for (auto kind : directive_list) {
    if (!first) {
      result += ", ";
    }
    result += OpenMPDirectiveKindToString(kind);
    first = false;
  }
  result += ") ";
  return result;
}

std::string OpenMPContainsClause::toString() {
  std::string result = "contains(";
  bool first = true;
  for (auto kind : directive_list) {
    if (!first) {
      result += ", ";
    }
    result += OpenMPDirectiveKindToString(kind);
    first = false;
  }
  result += ") ";
  return result;
}

std::string OpenMPGraphIdClause::toString() {
  return "graph_id(" + expressionToString() + ") ";
}

std::string OpenMPGraphResetClause::toString() { return "graph_reset "; }

std::string OpenMPTransparentClause::toString() {
  std::string str = expressionToString();
  if (str.empty())
    return "transparent ";
  return "transparent(" + str + ") ";
}

std::string OpenMPReplayableClause::toString() { return "replayable "; }

std::string OpenMPThreadsetClause::toString() {
  return "threadset(" + expressionToString() + ") ";
}

std::string OpenMPIndirectClause::toString() {
  std::string str = expressionToString();
  if (str.empty())
    return "indirect ";
  return "indirect(" + str + ") ";
}

std::string OpenMPLocalClause::toString() {
  return "local(" + expressionToString() + ") ";
}

std::string OpenMPInitCompleteClause::toString() { return "init_complete "; }

std::string OpenMPSafesyncClause::toString() {
  std::string str = expressionToString();
  if (str.empty())
    return "safesync ";
  return "safesync(" + str + ") ";
}

std::string OpenMPDeviceSafesyncClause::toString() {
  std::string str = expressionToString();
  if (str.empty())
    return "device_safesync ";
  return "device_safesync(" + str + ") ";
}

std::string OpenMPMemscopeClause::toString() {
  const char *value = "device";
  switch (scope) {
  case OMPC_MEMSCOPE_all:
    value = "all";
    break;
  case OMPC_MEMSCOPE_cgroup:
    value = "cgroup";
    break;
  case OMPC_MEMSCOPE_device:
    value = "device";
    break;
  case OMPC_MEMSCOPE_unknown:
    break;
  }

  return std::string("memscope(") + value + ") ";
}

std::string OpenMPLooprangeClause::toString() {
  return "looprange(" + expressionToString() + ") ";
}

std::string OpenMPPermutationClause::toString() {
  return "permutation(" + expressionToString() + ") ";
}

std::string OpenMPCountsClause::toString() {
  return "counts(" + expressionToString() + ") ";
}

std::string OpenMPInductorClause::toString() {
  return "inductor(" + expressionToString() + ") ";
}

std::string OpenMPCollectorClause::toString() {
  return "collector(" + expressionToString() + ") ";
}

std::string OpenMPCombinerClause::toString() {
  return "combiner(" + expressionToString() + ") ";
}

std::string OpenMPNoOpenmpClause::toString() { return "no_openmp "; }

std::string OpenMPNoOpenmpConstructsClause::toString() {
  std::string str = expressionToString();
  if (str.empty())
    return "no_openmp_constructs ";
  return "no_openmp_constructs(" + str + ") ";
}

std::string OpenMPNoOpenmpRoutinesClause::toString() {
  return "no_openmp_routines ";
}

std::string OpenMPNoParallelismClause::toString() { return "no_parallelism "; }

std::string OpenMPNocontextClause::toString() {
  return "nocontext(" + expressionToString() + ") ";
}

std::string OpenMPNovariantsClause::toString() {
  return "novariants(" + expressionToString() + ") ";
}

std::string OpenMPEnterClause::toString() {
  return "enter(" + expressionToString() + ") ";
}

std::string OpenMPUseClause::toString() {
  return "use(" + expressionToString() + ") ";
}

std::string OpenMPHoldsClause::toString() {
  return "holds(" + expressionToString() + ") ";
}
