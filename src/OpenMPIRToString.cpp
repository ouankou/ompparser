/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "OpenMPIR.h"
#include <stdarg.h>

namespace {
std::string formatDirectiveName(const char *name) {
  std::string result;
  for (const char *ptr = name; *ptr != '\0'; ++ptr) {
    result += (*ptr == '_') ? ' ' : *ptr;
  }
  result += ' ';
  return result;
}

std::string getDirectiveSpelling(OpenMPDirectiveKind kind) {
  switch (kind) {
#define OPENMP_DIRECTIVE(Name)                                                 \
  case OMPD_##Name:                                                            \
    return formatDirectiveName(#Name);
#define OPENMP_DIRECTIVE_EXT(Name, Str)                                        \
  case OMPD_##Name:                                                            \
    return std::string(Str);
#include "OpenMPKinds.def"
#undef OPENMP_DIRECTIVE
#undef OPENMP_DIRECTIVE_EXT
  }
  return std::string();
}

std::string getClauseSpelling(OpenMPClauseKind kind) {
  switch (kind) {
#define OPENMP_CLAUSE(Name, Class)                                             \
  case OMPC_##Name:                                                            \
    return std::string(#Name " ");
#define OPENMP_CLAUSE_EXT(Name, Class, Str)                                    \
  case OMPC_##Name:                                                            \
    return std::string(Str);
#include "OpenMPKinds.def"
#undef OPENMP_CLAUSE_EXT
#undef OPENMP_CLAUSE
  }
  return std::string();
}
} // namespace

std::string OpenMPDirective::generatePragmaString(std::string prefix,
                                                  std::string beginning_symbol,
                                                  std::string ending_symbol) {

  if (this->getBaseLang() == Lang_Fortran && prefix == "#pragma omp ") {
    prefix = "!$omp ";
  };
  std::string result = prefix;

  result += this->toString();

  result += beginning_symbol;

  switch (this->getKind()) {

  case OMPD_declare_variant: {
    result += "(" +
              ((OpenMPDeclareVariantDirective *)this)->getVariantFuncID() +
              ") ";
    break;
  }
  case OMPD_allocate: {
    const auto &list =
        static_cast<const OpenMPAllocateDirective *>(this)->getAllocateList();
    result += "(";
    for (auto it = list.begin(); it != list.end(); ++it) {
      if (it != list.begin())
        result += ",";
      result += *it;
    }
    result += ") ";
    break;
  }
  case OMPD_threadprivate: {
    const auto &list = static_cast<const OpenMPThreadprivateDirective *>(this)
                           ->getThreadprivateList();
    result += "(";
    for (auto it = list.begin(); it != list.end(); ++it) {
      if (it != list.begin())
        result += ",";
      result += *it;
    }
    result += ")";
    break;
  }
  case OMPD_groupprivate: {
    const auto &list = static_cast<const OpenMPGroupprivateDirective *>(this)
                           ->getGroupprivateList();
    result += "(";
    for (auto it = list.begin(); it != list.end(); ++it) {
      if (it != list.begin())
        result += ",";
      result += *it;
    }
    result += ")";
    break;
  }
  case OMPD_declare_reduction: {
    std::vector<const char *> *list =
        ((OpenMPDeclareReductionDirective *)this)->getTypenameList();
    std::vector<const char *>::iterator list_item;
    std::string id = ((OpenMPDeclareReductionDirective *)this)->getIdentifier();
    std::string combiner =
        ((OpenMPDeclareReductionDirective *)this)->getCombiner();
    std::vector<OpenMPClause *> *combiner_clauses =
        this->getClauses(OMPC_combiner);
    const bool has_combiner_clause =
        (combiner_clauses != nullptr && !combiner_clauses->empty());

    // Remove trailing space from directive name before adding opening paren
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
    result += "(";
    result += id;
    result += " : ";
    bool first_type = true;
    for (list_item = list->begin(); list_item != list->end(); list_item++) {
      std::string type = *list_item ? std::string(*list_item) : "";
      if (!first_type) {
        result += ", ";
      } else {
        first_type = false;
      }
      result += type;
    }
    if (!combiner.empty() && !has_combiner_clause) {
      result += " : ";
      result += combiner;
    }
    result += ") ";
    break;
  }
  case OMPD_declare_induction: {
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
    result += "(";
    std::vector<OpenMPClause *> *ind_clauses = this->getClauses(OMPC_induction);
    if (ind_clauses != nullptr && !ind_clauses->empty()) {
      auto *ind_clause =
          dynamic_cast<OpenMPInductionClause *>(ind_clauses->at(0));
      if (ind_clause != nullptr) {
        result += ind_clause->specificationToString();
      } else {
        result += ind_clauses->at(0)->expressionToString();
      }
    }
    result += ") ";
    goto default_case;
  }
  case OMPD_declare_mapper: {
    OpenMPDeclareMapperDirectiveIdentifier identifier =
        ((OpenMPDeclareMapperDirective *)this)->getIdentifier();
    // Remove trailing space from directive name before adding opening paren
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
    result += "(";
    if (identifier != OMPD_DECLARE_MAPPER_IDENTIFIER_unspecified) {
      switch (identifier) {
      case OMPD_DECLARE_MAPPER_IDENTIFIER_default: {
        result += "default";
        break;
      }
      case OMPD_DECLARE_MAPPER_IDENTIFIER_user: {
        std::string id =
            ((OpenMPDeclareMapperDirective *)this)->getUserDefinedIdentifier();
        result += id;
        break;
      }
      default:;
      };
      if (this->getBaseLang() == Lang_Fortran) {
        result += ": ";
      } else {
        result += " : ";
      }
    }

    std::string declare_mapper_type =
        ((OpenMPDeclareMapperDirective *)this)->getDeclareMapperType();
    result += declare_mapper_type;
    std::string declare_mapper_variable =
        ((OpenMPDeclareMapperDirective *)this)->getDeclareMapperVar();
    // Fortran requires :: separator between type and variable
    if (this->getBaseLang() == Lang_Fortran) {
      result += " :: ";
    } else if (((OpenMPDeclareMapperDirective *)this)->hasTypeVarSpace()) {
      result += " ";
    }
    result += declare_mapper_variable;
    result += ")";
    goto default_case;
  }
  case OMPD_declare_simd: {
    std::string proc_name = ((OpenMPDeclareSimdDirective *)this)->getProcName();
    if (proc_name != "") {
      result += "(";
      result += proc_name;
      result += ") ";
    }
    break;
  }
  case OMPD_end: {
    auto *end_directive = static_cast<OpenMPEndDirective *>(this);
    OpenMPDirective *paired = end_directive->getPairedDirective();
    if (this->getBaseLang() == Lang_Fortran && paired != nullptr &&
        paired->getKind() == OMPD_do) {
      result = prefix;
      if (end_directive->getUseCompactEndDo()) {
        result += "enddo";
      } else {
        result += "end do";
      }
      result += " ";
      goto default_case;
    } else if (paired != nullptr) {
      result += paired->generatePragmaString("", "", "");
      goto default_case;
    }
    break;
  }
  case OMPD_declare_target: {
    std::vector<std::string> *list =
        ((OpenMPDeclareTargetDirective *)this)->getExtendedList();
    if (list->size() > 0) {
      std::vector<std::string>::iterator list_item;
      result += "(";
      for (list_item = list->begin(); list_item != list->end(); list_item++) {
        if (list_item != list->begin())
          result += ",";
        result += *list_item;
      }
      result += ") ";
    }
    // Don't break - we need to fall through to default to output clauses
    goto default_case;
  }
  case OMPD_critical: {
    std::string name = ((OpenMPCriticalDirective *)this)->getCriticalName();
    if (name != "") {
      result += "(";
      result += name;
      result += ") ";
    }
    break;
  }
  case OMPD_flush: {
    std::vector<std::string> *list =
        ((OpenMPFlushDirective *)this)->getFlushList();
    if (list->size() > 0) {
      std::vector<std::string>::iterator list_item;
      result += "(";
      for (list_item = list->begin(); list_item != list->end(); list_item++) {
        if (list_item != list->begin())
          result += ",";
        result += *list_item;
      }
      result += ") ";
    }
    break;
  }
  case OMPD_depobj: {
    std::string depobj = ((OpenMPDepobjDirective *)this)->getDepobj();
    result += "(";
    result += depobj;
    result += ") ";
    break;
  }
  default:
  default_case:;
  };

  std::vector<OpenMPClause *> *clauses = this->getClausesInOriginalOrder();
  if (this->getKind() == OMPD_end && clauses->empty()) {
    auto *end_dir = static_cast<OpenMPEndDirective *>(this);
    OpenMPDirective *paired = end_dir->getPairedDirective();
    if (paired != nullptr &&
        (paired->getKind() == OMPD_do || paired->getKind() == OMPD_do_simd)) {
      clauses = paired->getClausesInOriginalOrder();
    }
  }
  if (clauses->size() != 0) {
    bool first_clause = true;
    std::vector<OpenMPClause *>::iterator iter;
    for (iter = clauses->begin(); iter != clauses->end(); iter++) {
      if (this->getKind() == OMPD_declare_induction &&
          (*iter)->getKind() == OMPC_induction) {
        continue;
      }
      std::string clause_str = (*iter)->toString();
      if (clause_str.empty()) {
        continue;
      }
      if (!first_clause &&
          (*iter)->getPrecedingSeparator() == OMPC_CLAUSE_SEP_comma) {
        if (!result.empty() && result.back() == ' ') {
          result.pop_back();
        }
        result += ",";
        if (!clause_str.empty() && clause_str.front() != ' ') {
          result += " ";
        }
      } else if (!result.empty() && result.back() != ' ') {
        result += " ";
      }
      result += clause_str;
      first_clause = false;
    }
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
  } else {
    // No clauses - remove trailing space from directive name to match
    // clang-format
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
  }
  result += ending_symbol;

  return result;
};

std::string OpenMPDirective::toString() {
  switch (this->getKind()) {
  case OMPD_parallel_do:
    if (this->getCompactParallelDo()) {
      return "paralleldo ";
    }
    break;
  case OMPD_declare_target:
    if (this->getDeclareTargetUnderscore()) {
      return "declare_target ";
    }
    break;
  case OMPD_begin_declare_target:
    if (this->getDeclareTargetUnderscore()) {
      return "begin declare_target ";
    }
    break;
  case OMPD_end_declare_target:
    if (this->getDeclareTargetUnderscore()) {
      return "end declare_target ";
    }
    break;
  default:
    break;
  }

  std::string result = getDirectiveSpelling(this->getKind());
  if (result.empty()) {
    printf("The directive enum is not supported yet.\n");
  }

  return result;
};

std::string OpenMPClause::expressionToString() {

  std::string result;
  std::vector<const char *> *expr = this->getExpressions();
  if (expr != NULL) {
    // For init/adjust_args clauses with exactly 2 expressions, use ":"
    // separator
    if ((this->getKind() == OMPC_init || this->getKind() == OMPC_adjust_args) &&
        expr->size() == 2) {
      result = std::string((*expr)[0]) + ": " + std::string((*expr)[1]);
    } else {
      for (size_t idx = 0; idx < expr->size(); ++idx) {
        if (idx > 0) {
          OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_space;
          if (idx < expression_separators.size()) {
            sep = expression_separators[idx];
          }
          result += (sep == OMPC_CLAUSE_SEP_comma) ? ", " : " ";
        }
        result += std::string((*expr)[idx]);
      }
    }
  }

  return result;
};

std::string OpenMPClause::toString() {

  std::string result = getClauseSpelling(this->getKind());
  if (result.empty()) {
    printf("The clause enum is not supported yet.\n");
  }

  std::string clause_string = "(";
  clause_string += this->expressionToString();
  clause_string += ")";
  if (clause_string.size() > 2) {
    // clang-format: space before ( only for 'if' clause
    if (this->getKind() != OMPC_if && !result.empty() && result.back() == ' ') {
      result.pop_back(); // Remove trailing space before (
    }
    result += clause_string;
    result += " "; // Add space after )
  }

  return result;
};

namespace {
std::string iteratorToString(const std::string &qualifier,
                             const std::string &var, const std::string &begin,
                             const std::string &end, const std::string &step) {
  std::string result;
  if (!qualifier.empty()) {
    result += qualifier + " ";
  }
  result += var;
  result += "=";
  result += begin;
  result += ":";
  result += end;
  if (!step.empty()) {
    result += ":";
    result += step;
  }
  return result;
}
} // namespace

std::string OpenMPExtImplementationDefinedRequirementClause::toString() {
  std::string result = "";
  std::string parameter_string;

  std::string ext_user_defined_implementation =
      this->getImplementationDefinedRequirement();
  result += "ext_" + ext_user_defined_implementation;
  result += " ";
  return result;
};

std::string OpenMPAtomicDefaultMemOrderClause::toString() {
  std::string result = "atomic_default_mem_order(";
  std::string parameter_string;
  OpenMPAtomicDefaultMemOrderClauseKind kind = this->getKind();
  switch (kind) {
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_seq_cst:
    parameter_string = "seq_cst";
    break;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acq_rel:
    parameter_string = "acq_rel";
    break;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_relaxed:
    parameter_string = "relaxed";
    break;
  default:
    std::cout << "The parameter of tomic_default_mem_order clause is not "
                 "supported.\n";
  };

  if (parameter_string.size() > 0) {
    result += parameter_string + ") ";
  } else {
    return std::string();
  }

  return result;
};

std::string OpenMPInReductionClause::toString() {

  std::string result = "in_reduction ";
  std::string clause_string = "(";
  OpenMPInReductionClauseIdentifier identifier = this->getIdentifier();
  switch (identifier) {
  case OMPC_IN_REDUCTION_IDENTIFIER_plus:
    clause_string += "+";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_minus:
    clause_string += "-";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_mul:
    clause_string += "*";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_bitand:
    clause_string += "&";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_bitor:
    clause_string += "|";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_bitxor:
    clause_string += "^";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_logand:
    clause_string += "&&";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_logor:
    clause_string += "||";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_min:
    clause_string += "min";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_max:
    clause_string += "max";
    break;
  case OMPC_IN_REDUCTION_IDENTIFIER_user:
    clause_string += this->getUserDefinedIdentifier();
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPDependClause::toString() {
  OpenMPDependClauseModifier modifier = this->getModifier();
  const auto &iterator_defs = this->getIterators();
  std::string result = "depend ";
  std::string clause_string = "(";

  OpenMPDependClauseType type = this->getType();
  if (modifier == OMPC_DEPEND_MODIFIER_iterator) {
    clause_string += "iterator";
    clause_string += " ( ";
    for (size_t i = 0; i < iterator_defs.size(); ++i) {
      if (i > 0) {
        clause_string += ", ";
      }
      clause_string += iteratorToString(
          iterator_defs[i].qualifier, iterator_defs[i].var,
          iterator_defs[i].begin, iterator_defs[i].end, iterator_defs[i].step);
    }
    clause_string += " )";
  }

  if (clause_string.size() > 1) {
    clause_string += ", ";
  };
  switch (type) {
  case OMPC_DEPENDENCE_TYPE_in:
    clause_string += "in";
    break;
  case OMPC_DEPENDENCE_TYPE_out:
    clause_string += "out";
    break;
  case OMPC_DEPENDENCE_TYPE_inout:
    clause_string += "inout";
    break;
  case OMPC_DEPENDENCE_TYPE_inoutset:
    clause_string += "inoutset";
    break;
  case OMPC_DEPENDENCE_TYPE_mutexinoutset:
    clause_string += "mutexinoutset";
    break;
  case OMPC_DEPENDENCE_TYPE_depobj:
    clause_string += "depobj";
    break;
  case OMPC_DEPENDENCE_TYPE_source:
    clause_string += "source";
    break;
  case OMPC_DEPENDENCE_TYPE_sink:
    clause_string += "sink";
    break;
  default:;
  }

  if (clause_string.size() > 1 && type != OMPC_DEPENDENCE_TYPE_source) {
    clause_string += " : ";
  };
  if (type == OMPC_DEPENDENCE_TYPE_sink) {
    clause_string += this->getDependenceVector();
  }
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    result += clause_string;
  };
  return result;
};

std::string OpenMPDoacrossClause::toString() {
  std::string result = "doacross (";

  OpenMPDoacrossClauseType type = this->getType();
  switch (type) {
  case OMPC_DOACROSS_TYPE_source:
    result += "source:";
    break;
  case OMPC_DOACROSS_TYPE_sink:
    result += "sink:";
    break;
  default:
    result += "unknown:";
  }

  if (type == OMPC_DOACROSS_TYPE_source && this->hasSourceExpression()) {
    result += " " + this->getSourceExpression().text;
  } else if (type == OMPC_DOACROSS_TYPE_sink) {
    const auto &args = this->getSinkArgs();
    for (size_t idx = 0; idx < args.size(); ++idx) {
      if (idx > 0) {
        result += (args[idx].separator == OMPC_CLAUSE_SEP_comma) ? ", " : " ";
      }
      result += args[idx].text;
    }
  } else {
    // Fallback to legacy expression list if present
    std::string expr_string = this->expressionToString();
    if (!expr_string.empty()) {
      result += " " + expr_string;
    }
  }

  result += ") ";
  return result;
};

std::string OpenMPDepobjUpdateClause::toString() {

  std::string result = "update ";
  std::string clause_string = "(";
  OpenMPDepobjUpdateClauseDependeceType type = this->getType();
  switch (type) {
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_source:
    clause_string += "source";
    break;
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_in:
    clause_string += "in";
    break;
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_out:
    clause_string += "out";
    break;
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_inout:
    clause_string += "inout";
    break;
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_inoutset:
    clause_string += "inoutset";
    break;
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_mutexinoutset:
    clause_string += "mutexinoutset";
    break;
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_depobj:
    clause_string += "depobj";
    break;
  case OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_sink:
    clause_string += "sink";
    break;
  default:;
  }
  clause_string += ") ";
  if (clause_string.size() > 3) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPAffinityClause::toString() {
  const auto &iterators_definition_class = this->getIteratorsDefinitionClass();
  std::string result = "affinity ";
  std::string clause_string = "(";
  OpenMPAffinityClauseModifier modifier = this->getModifier();
  switch (modifier) {
  case OMPC_AFFINITY_MODIFIER_iterator:
    clause_string += "iterator";
    clause_string += " ( ";
    for (size_t i = 0; i < iterators_definition_class.size(); i++) {
      if (i > 0) {
        clause_string += ", ";
      }
      clause_string += iteratorToString(iterators_definition_class[i].qualifier,
                                        iterators_definition_class[i].var,
                                        iterators_definition_class[i].begin,
                                        iterators_definition_class[i].end,
                                        iterators_definition_class[i].step);
    };
    clause_string += " )";
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPToClause::toString() {

  std::string result = "to ";
  std::string clause_string = "(";
  OpenMPToClauseKind to_kind = this->getKind();
  const auto &iterator_defs = this->getIterators();
  switch (to_kind) {
  case OMPC_TO_mapper:
    clause_string += "mapper";
    clause_string += "(";
    clause_string += this->getMapperIdentifier();
    clause_string += ")";
    break;
  case OMPC_TO_iterator:
    clause_string += "iterator";
    clause_string += "(";
    for (size_t i = 0; i < iterator_defs.size(); ++i) {
      if (i > 0) {
        clause_string += ", ";
      }
      clause_string += iteratorToString(
          iterator_defs[i].qualifier, iterator_defs[i].var,
          iterator_defs[i].begin, iterator_defs[i].end, iterator_defs[i].step);
    }
    clause_string += ")";
    break;
  case OMPC_TO_present:
    clause_string += "present";
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    if (to_kind == OMPC_TO_iterator) {
      if (!result.empty() && result.back() == ' ') {
        result.pop_back();
      }
    } else if (!result.empty() && result.back() != ' ') {
      result += " ";
    }
    result += clause_string;
  };

  return result;
};

std::string OpenMPFromClause::toString() {

  std::string result = "from ";
  std::string clause_string = "(";
  OpenMPFromClauseKind from_kind = this->getKind();
  const auto &iterator_defs = this->getIterators();
  switch (from_kind) {
  case OMPC_FROM_mapper:
    clause_string += "mapper";
    clause_string += "(";
    clause_string += this->getMapperIdentifier();
    clause_string += ")";
    break;
  case OMPC_FROM_iterator:
    clause_string += "iterator";
    clause_string += "(";
    for (size_t i = 0; i < iterator_defs.size(); ++i) {
      if (i > 0) {
        clause_string += ", ";
      }
      clause_string += iteratorToString(
          iterator_defs[i].qualifier, iterator_defs[i].var,
          iterator_defs[i].begin, iterator_defs[i].end, iterator_defs[i].step);
    }
    clause_string += ")";
    break;
  case OMPC_FROM_present:
    clause_string += "present";
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    if (from_kind == OMPC_FROM_iterator) {
      if (!result.empty() && result.back() == ' ') {
        result.pop_back();
      }
    } else if (!result.empty() && result.back() != ' ') {
      result += " ";
    }
    result += clause_string;
  };

  return result;
};

std::string OpenMPDefaultmapClause::toString() {

  std::string result = "defaultmap ";
  std::string clause_string = "(";
  OpenMPDefaultmapClauseBehavior behavior = this->getBehavior();
  OpenMPDefaultmapClauseCategory category = this->getCategory();
  switch (behavior) {
  case OMPC_DEFAULTMAP_BEHAVIOR_alloc:
    clause_string += "alloc";
    break;
  case OMPC_DEFAULTMAP_BEHAVIOR_to:
    clause_string += "to";
    break;
  case OMPC_DEFAULTMAP_BEHAVIOR_from:
    clause_string += "from";
    break;
  case OMPC_DEFAULTMAP_BEHAVIOR_tofrom:
    clause_string += "tofrom";
    break;
  case OMPC_DEFAULTMAP_BEHAVIOR_firstprivate:
    clause_string += "firstprivate";
    break;
  case OMPC_DEFAULTMAP_BEHAVIOR_none:
    clause_string += "none";
    break;
  case OMPC_DEFAULTMAP_BEHAVIOR_default:
    clause_string += "default";
    break;
  case OMPC_DEFAULTMAP_BEHAVIOR_present:
    clause_string += "present";
    break;
  default:;
  }
  if (category != OMPC_DEFAULTMAP_CATEGORY_unspecified) {
    clause_string += ": ";
  };
  switch (category) {
  case OMPC_DEFAULTMAP_CATEGORY_scalar:
    clause_string += "scalar";
    break;
  case OMPC_DEFAULTMAP_CATEGORY_aggregate:
    clause_string += "aggregate";
    break;
  case OMPC_DEFAULTMAP_CATEGORY_pointer:
    clause_string += "pointer";
    break;
  case OMPC_DEFAULTMAP_CATEGORY_all:
    clause_string += "all";
    break;
  case OMPC_DEFAULTMAP_CATEGORY_allocatable:
    clause_string += "allocatable";
    break;
  default:;
  }
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    if (!result.empty() && result.back() != ' ') {
      result += " ";
    }
    result += clause_string;
  };

  return result;
};

std::string OpenMPDeviceClause::toString() {

  std::string result = "device ";
  std::string clause_string = "(";
  OpenMPDeviceClauseModifier modifier = this->getModifier();
  switch (modifier) {
  case OMPC_DEVICE_MODIFIER_ancestor:
    clause_string += "ancestor";
    break;
  case OMPC_DEVICE_MODIFIER_device_num:
    clause_string += "device_num";
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    if (!result.empty() && result.back() != ' ') {
      result += " ";
    }
    result += clause_string;
  };

  return result;
};

std::string OpenMPDeviceTypeClause::toString() {

  std::string result = "device_type (";
  std::string parameter_string;
  OpenMPDeviceTypeClauseKind device_type_kind = this->getDeviceTypeClauseKind();
  switch (device_type_kind) {
  case OMPC_DEVICE_TYPE_host:
    parameter_string = "host";
    break;
  case OMPC_DEVICE_TYPE_nohost:
    parameter_string = "nohost";
    break;
  case OMPC_DEVICE_TYPE_any:
    parameter_string = "any";
    break;
  default:
    std::cout << "The parameter of device_type clause is not supported.\n";
  };

  if (parameter_string.size() > 0) {
    result += parameter_string + ") ";
  } else {
    return std::string();
  }

  return result;
}

std::string OpenMPTaskReductionClause::toString() {

  std::string result = "task_reduction ";
  std::string clause_string = "(";
  OpenMPTaskReductionClauseIdentifier identifier = this->getIdentifier();
  switch (identifier) {
  case OMPC_TASK_REDUCTION_IDENTIFIER_plus:
    clause_string += "+";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_minus:
    clause_string += "-";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_mul:
    clause_string += "*";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_bitand:
    clause_string += "&";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_bitor:
    clause_string += "|";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_bitxor:
    clause_string += "^";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_logand:
    clause_string += "&&";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_logor:
    clause_string += "||";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_min:
    clause_string += "min";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_max:
    clause_string += "max";
    break;
  case OMPC_TASK_REDUCTION_IDENTIFIER_user:
    clause_string += this->getUserDefinedIdentifier();
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 3) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPMapClause::toString() {

  std::string result = "map ";
  std::string clause_string = "(";
  OpenMPMapClauseModifier modifier1 = this->getModifier1();
  OpenMPMapClauseModifier modifier2 = this->getModifier2();
  OpenMPMapClauseModifier modifier3 = this->getModifier3();
  const auto &iterator_defs = this->getIterators();
  bool has_content = false;

  OpenMPMapClauseType type = this->getType();
  OpenMPMapClauseRefModifier ref_modifier = this->getRefModifier();

  switch (ref_modifier) {
  case OMPC_MAP_REF_MODIFIER_ref_ptee:
    clause_string += "ref_ptee";
    has_content = true;
    break;
  case OMPC_MAP_REF_MODIFIER_ref_ptr:
    clause_string += "ref_ptr";
    has_content = true;
    break;
  case OMPC_MAP_REF_MODIFIER_ref_ptr_ptee:
    clause_string += "ref_ptr_ptee";
    has_content = true;
    break;
  default:;
  }

  // Helper to append modifier text with separator
  auto append_modifier = [&](const std::string &text) {
    if (has_content) {
      clause_string += ", ";
    }
    clause_string += text;
    has_content = true;
  };

  switch (modifier1) {
  case OMPC_MAP_MODIFIER_always:
    append_modifier("always");
    break;
  case OMPC_MAP_MODIFIER_close:
    append_modifier("close");
    break;
  case OMPC_MAP_MODIFIER_present:
    append_modifier("present");
    break;
  case OMPC_MAP_MODIFIER_self:
    append_modifier("self");
    break;
  case OMPC_MAP_MODIFIER_mapper:
    if (has_content) {
      clause_string += ", ";
    }
    clause_string += "mapper(";
    clause_string += this->getMapperIdentifier();
    clause_string += ")";
    has_content = true;
    break;
  case OMPC_MAP_MODIFIER_iterator: {
    if (has_content) {
      clause_string += ", ";
    }
    clause_string += "iterator(";
    for (size_t i = 0; i < iterator_defs.size(); ++i) {
      if (i > 0) {
        clause_string += ", ";
      }
      clause_string += iteratorToString(
          iterator_defs[i].qualifier, iterator_defs[i].var,
          iterator_defs[i].begin, iterator_defs[i].end, iterator_defs[i].step);
    }
    clause_string += ")";
    has_content = true;
    break;
  }
  default:;
  }
  switch (modifier2) {
  case OMPC_MAP_MODIFIER_always:
    append_modifier("always");
    break;
  case OMPC_MAP_MODIFIER_close:
    append_modifier("close");
    break;
  case OMPC_MAP_MODIFIER_present:
    append_modifier("present");
    break;
  case OMPC_MAP_MODIFIER_self:
    append_modifier("self");
    break;
  case OMPC_MAP_MODIFIER_mapper:
    if (has_content) {
      clause_string += ", ";
    }
    clause_string += "mapper(";
    clause_string += this->getMapperIdentifier();
    clause_string += ")";
    has_content = true;
    break;
  default:;
  }
  switch (modifier3) {
  case OMPC_MAP_MODIFIER_always:
    append_modifier("always");
    break;
  case OMPC_MAP_MODIFIER_close:
    append_modifier("close");
    break;
  case OMPC_MAP_MODIFIER_present:
    append_modifier("present");
    break;
  case OMPC_MAP_MODIFIER_self:
    append_modifier("self");
    break;
  case OMPC_MAP_MODIFIER_mapper:
    if (has_content) {
      clause_string += ", ";
    }
    clause_string += "mapper(";
    clause_string += this->getMapperIdentifier();
    clause_string += ")";
    has_content = true;
    break;
  default:;
  }

  switch (type) {
  case OMPC_MAP_TYPE_to:
    append_modifier("to");
    break;
  case OMPC_MAP_TYPE_from:
    append_modifier("from");
    break;
  case OMPC_MAP_TYPE_tofrom:
    append_modifier("tofrom");
    break;
  case OMPC_MAP_TYPE_storage:
    append_modifier("storage");
    break;
  case OMPC_MAP_TYPE_alloc:
    append_modifier("alloc");
    break;
  case OMPC_MAP_TYPE_release:
    append_modifier("release");
    break;
  case OMPC_MAP_TYPE_delete:
    append_modifier("delete");
    break;
  case OMPC_MAP_TYPE_present:
    append_modifier("present");
    break;
  case OMPC_MAP_TYPE_self:
    append_modifier("self");
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  const auto &items = this->getItems();
  if (!items.empty()) {
    for (size_t idx = 0; idx < items.size(); ++idx) {
      if (idx > 0) {
        clause_string +=
            (items[idx].separator == OMPC_CLAUSE_SEP_comma) ? ", " : " ";
      }
      clause_string += items[idx].text;
    }
  } else {
    clause_string += this->expressionToString();
  }
  clause_string += ")";
  if (clause_string.size() > 2) {
    // clang-format: no space before ( for map clause
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
    result += clause_string;
    result += " ";
  };

  return result;
};

std::string OpenMPReductionClause::toString() {

  std::string result = "reduction ";
  std::string clause_string = "(";
  OpenMPReductionClauseModifier modifier = this->getModifier();
  OpenMPReductionClauseIdentifier identifier = this->getIdentifier();
  if (!this->getUserDefinedModifier().empty()) {
    clause_string += this->getUserDefinedModifier();
  } else {
    switch (modifier) {
    case OMPC_REDUCTION_MODIFIER_default:
      clause_string += "default";
      break;
    case OMPC_REDUCTION_MODIFIER_inscan:
      clause_string += "inscan";
      break;
    case OMPC_REDUCTION_MODIFIER_task:
      clause_string += "task";
      break;
    default:;
    }
  }
  if (clause_string.size() > 1) {
    clause_string += ", ";
  }
  switch (identifier) {
  case OMPC_REDUCTION_IDENTIFIER_plus:
    clause_string += "+";
    break;
  case OMPC_REDUCTION_IDENTIFIER_minus:
    clause_string += "-";
    break;
  case OMPC_REDUCTION_IDENTIFIER_mul:
    clause_string += "*";
    break;
  case OMPC_REDUCTION_IDENTIFIER_bitand:
    clause_string += "&";
    break;
  case OMPC_REDUCTION_IDENTIFIER_bitor:
    clause_string += "|";
    break;
  case OMPC_REDUCTION_IDENTIFIER_bitxor:
    clause_string += "^";
    break;
  case OMPC_REDUCTION_IDENTIFIER_logand:
    clause_string += "&&";
    break;
  case OMPC_REDUCTION_IDENTIFIER_logor:
    clause_string += "||";
    break;
  case OMPC_REDUCTION_IDENTIFIER_min:
    clause_string += "min";
    break;
  case OMPC_REDUCTION_IDENTIFIER_max:
    clause_string += "max";
    break;
  case OMPC_REDUCTION_IDENTIFIER_user:
    clause_string += this->getUserDefinedIdentifier();
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += " : ";
  };
  const auto &operands = this->getOperands();
  for (size_t idx = 0; idx < operands.size(); ++idx) {
    if (idx > 0) {
      clause_string +=
          (operands[idx].separator == OMPC_CLAUSE_SEP_comma) ? ", " : " ";
    }
    clause_string += operands[idx].text;
  }
  clause_string += ")";
  if (clause_string.size() > 2) {
    // clang-format: no space before ( for reduction clause
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
    result += clause_string;
    result += " ";
  };

  return result;
};

std::string OpenMPLastprivateClause::toString() {

  std::string result = "lastprivate ";
  std::string clause_string = "(";
  OpenMPLastprivateClauseModifier modifier = this->getModifier();
  switch (modifier) {
  case OMPC_LASTPRIVATE_MODIFIER_conditional:
    clause_string += "conditional";
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += ": ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 2) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPLinearClause::toString() {

  std::string result = "linear ";
  std::string clause_string = "(";
  OpenMPLinearClauseModifier modifier = this->getModifier();
  bool has_modifier = (modifier != OMPC_LINEAR_MODIFIER_unspecified);
  bool modifier_first = this->isModifierFirstSyntax();
  std::string user_defined_step = this->getUserDefinedStep();
  bool has_step = !user_defined_step.empty();

  // Check if we have a modifier to output
  if (has_modifier && modifier_first) {
    // Modifier-first syntax: linear(mod(vars))
    switch (modifier) {
    case OMPC_LINEAR_MODIFIER_val:
      clause_string += "val";
      break;
    case OMPC_LINEAR_MODIFIER_ref:
      clause_string += "ref";
      break;
    case OMPC_LINEAR_MODIFIER_uval:
      clause_string += "uval";
      break;
    default:;
    }
    clause_string += "( ";
    clause_string += this->expressionToString();
    clause_string += ") ";
    if (has_step) {
      clause_string += ":";
    }
  } else {
    // Variable-first syntax: linear(vars) or linear(vars: mod) or linear(vars:
    // mod, step)
    clause_string += this->expressionToString();
    if (has_modifier) {
      clause_string += " : ";
      switch (modifier) {
      case OMPC_LINEAR_MODIFIER_val:
        clause_string += "val";
        break;
      case OMPC_LINEAR_MODIFIER_ref:
        clause_string += "ref";
        break;
      case OMPC_LINEAR_MODIFIER_uval:
        clause_string += "uval";
        break;
      default:;
      }
    }
    if (has_step) {
      if (has_modifier) {
        clause_string += ", ";
      } else {
        clause_string += ":";
      }
    }
  }
  if (has_step) {
    clause_string += user_defined_step;
  }
  clause_string += ") ";
  result += clause_string;
  return result;
};

std::string OpenMPAlignedClause::toString() {

  std::string result = "aligned ";
  std::string clause_string = "(";
  clause_string += this->expressionToString();
  if (this->getUserDefinedAlignment() != "") {
    clause_string += ":";
    clause_string += this->getUserDefinedAlignment();
  }
  clause_string += ") ";
  result += clause_string;
  return result;
};

std::string OpenMPDistScheduleClause::toString() {

  std::string result = "dist_schedule ";
  std::string clause_string = "(";
  OpenMPDistScheduleClauseKind kind = this->getKind();
  switch (kind) {
  case OMPC_DIST_SCHEDULE_KIND_static:
    clause_string += "static";
    break;
  default:;
  }
  if (this->getChunkSize() != "") {
    clause_string += ", ";
    clause_string += this->getChunkSize();
  }
  clause_string += ") ";
  result += clause_string;
  return result;
};

std::string OpenMPScheduleClause::toString() {

  std::string result = "schedule ";
  std::string clause_string = "(";
  OpenMPScheduleClauseModifier modifier1 = this->getModifier1();
  OpenMPScheduleClauseModifier modifier2 = this->getModifier2();
  OpenMPScheduleClauseKind kind = this->getKind();
  switch (modifier1) {
  case OMPC_SCHEDULE_MODIFIER_monotonic:
    clause_string += "monotonic";
    break;
  case OMPC_SCHEDULE_MODIFIER_nonmonotonic:
    clause_string += "nonmonotonic";
    break;
  case OMPC_SCHEDULE_MODIFIER_simd:
    clause_string += "simd";
    break;
  default:;
  }
  switch (modifier2) {
  case OMPC_SCHEDULE_MODIFIER_monotonic:
    clause_string += ",monotonic";
    break;
  case OMPC_SCHEDULE_MODIFIER_nonmonotonic:
    clause_string += ",nonmonotonic";
    break;
  case OMPC_SCHEDULE_MODIFIER_simd:
    clause_string += ",simd";
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += ":";
  }
  switch (kind) {
  case OMPC_SCHEDULE_KIND_static:
    clause_string += "static";
    break;
  case OMPC_SCHEDULE_KIND_dynamic:
    clause_string += "dynamic";
    break;
  case OMPC_SCHEDULE_KIND_guided:
    clause_string += "guided";
    break;
  case OMPC_SCHEDULE_KIND_auto:
    clause_string += "auto";
    break;
  case OMPC_SCHEDULE_KIND_runtime:
    clause_string += "runtime";
    break;
  default:;
  }
  if (this->getChunkSize() != "") {
    clause_string += ", ";
    clause_string += this->getChunkSize();
  }
  clause_string += ")";
  if (clause_string.size() > 2) {
    // clang-format: no space before ( for schedule clause
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }
    result += clause_string;
    result += " ";
  };

  return result;
};

std::string OpenMPIfClause::toString() {

  std::string result = "if ";
  std::string clause_string = "(";
  OpenMPIfClauseModifier modifier = this->getModifier();
  switch (modifier) {
  case OMPC_IF_MODIFIER_parallel:
    clause_string += "parallel";
    break;
  case OMPC_IF_MODIFIER_simd:
    clause_string += "simd";
    break;
  case OMPC_IF_MODIFIER_task:
    clause_string += "task";
    break;
  case OMPC_IF_MODIFIER_taskloop:
    clause_string += "taskloop";
    break;
  case OMPC_IF_MODIFIER_teams:
    clause_string += "teams";
    break;
  case OMPC_IF_MODIFIER_task_iteration:
    clause_string += "task_iteration";
    break;
  case OMPC_IF_MODIFIER_taskgraph:
    clause_string += "taskgraph";
    break;
  case OMPC_IF_MODIFIER_cancel:
    clause_string += "cancel";
    break;
  case OMPC_IF_MODIFIER_target_data:
    clause_string += "target data";
    break;
  case OMPC_IF_MODIFIER_target_enter_data:
    clause_string += "target enter data";
    break;
  case OMPC_IF_MODIFIER_target_exit_data:
    clause_string += "target exit data";
    break;
  case OMPC_IF_MODIFIER_target:
    clause_string += "target";
    break;
  case OMPC_IF_MODIFIER_target_update:
    clause_string += "target update";
    break;
  default:;
  }
  if (clause_string.size() > 1) {
    clause_string += ": ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 2) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPFirstprivateClause::toString() {
  std::string result = "";
  if (expressions.empty()) {
    return "";
  }

  bool current_chunk_saved = saved_statuses.empty() ? false : saved_statuses[0];
  std::string current_chunk_str = "firstprivate(";
  if (current_chunk_saved) {
    current_chunk_str += "saved: ";
  }

  for (size_t i = 0; i < expressions.size(); ++i) {
    bool is_saved = (i < saved_statuses.size()) ? saved_statuses[i] : false;

    if (i > 0) {
      if (is_saved != current_chunk_saved) {
        // Close current chunk
        current_chunk_str += ")";
        result += current_chunk_str + " ";

        // Start new chunk
        current_chunk_saved = is_saved;
        current_chunk_str = "firstprivate(";
        if (current_chunk_saved) {
          current_chunk_str += "saved: ";
        }
      } else {
        // Append separator
        if (expression_separators[i] == OMPC_CLAUSE_SEP_comma) {
          current_chunk_str += ", ";
        } else {
          current_chunk_str += " ";
        }
      }
    }
    current_chunk_str += expressions[i];
  }
  current_chunk_str += ")";
  result += current_chunk_str + " ";

  return result;
}

std::string OpenMPInitializerClause::toString() {

  std::string result = "initializer";
  std::string clause_string = "(";
  std::string priv = this->expressionToString();
  clause_string += priv;
  clause_string += ")";
  if (clause_string.size() > 2) {
    result += clause_string;
    result += " ";
  };

  return result;
};

std::string OpenMPApplyClause::toString() {
  std::string result = "apply(";
  bool need_colon = false;
  if (!label.empty()) {
    result += label;
    need_colon = true;
  }
  if (!transforms.empty()) {
    if (need_colon) {
      result += ": ";
    }
    for (size_t i = 0; i < transforms.size(); ++i) {
      if (i > 0) {
        if (transforms[i].separator == OMPC_CLAUSE_SEP_space) {
          result += " ";
        } else {
          result += ", ";
        }
      }
      const auto &t = transforms[i];
      switch (t.kind) {
      case OMPC_APPLY_TRANSFORM_unroll:
        result += "unroll";
        break;
      case OMPC_APPLY_TRANSFORM_unroll_partial:
        result += "unroll partial";
        if (!t.argument.empty()) {
          result += "(" + t.argument + ")";
        }
        break;
      case OMPC_APPLY_TRANSFORM_unroll_full:
        result += "unroll full";
        break;
      case OMPC_APPLY_TRANSFORM_reverse:
        result += "reverse";
        break;
      case OMPC_APPLY_TRANSFORM_interchange:
        result += "interchange";
        break;
      case OMPC_APPLY_TRANSFORM_nothing:
        result += "nothing";
        break;
      case OMPC_APPLY_TRANSFORM_tile_sizes:
        result += "tile sizes(" + t.argument + ")";
        break;
      case OMPC_APPLY_TRANSFORM_apply:
        if (t.nested_apply) {
          std::string nested = t.nested_apply->toString();
          if (!nested.empty() && nested.back() == ' ') {
            nested.pop_back();
          }
          result += nested;
        } else {
          result += "apply";
        }
        break;
      case OMPC_APPLY_TRANSFORM_unknown:
      default:
        result += t.argument;
        break;
      }
    }
  }
  result += ")";
  result += " ";
  return result;
}

std::string OpenMPInductionClause::specificationToString() const {
  std::string result;
  bool first = true;

  for (const auto &item : sequence) {
    if (!first) {
      result += ", ";
    }
    switch (item.kind) {
    case ItemStep:
      result += "step(" + step_expression + ")";
      break;
    case ItemBinding:
      if (item.index < bindings.size()) {
        const auto &binding = bindings[item.index];
        if (!binding.label.empty()) {
          result += binding.label + ": " + binding.expression;
        } else {
          result += binding.expression;
        }
      }
      break;
    case ItemPassthrough:
      if (item.index < passthrough_items.size()) {
        result += passthrough_items[item.index];
      }
      break;
    }
    first = false;
  }

  return result;
}

std::string OpenMPInductionClause::toString() {
  std::string result = "induction(";
  result += specificationToString();
  result += ") ";
  return result;
}

std::string OpenMPAllocateClause::toString() {

  std::string result = "allocate ";
  std::string clause_string = "(";
  OpenMPAllocateClauseAllocator allocator = this->getAllocator();
  std::vector<std::string> parameters;
  switch (allocator) {
  case OMPC_ALLOCATE_ALLOCATOR_default:
    parameters.emplace_back("omp_default_mem_alloc");
    break;
  case OMPC_ALLOCATE_ALLOCATOR_large_cap:
    parameters.emplace_back("omp_large_cap_mem_alloc");
    break;
  case OMPC_ALLOCATE_ALLOCATOR_cons_mem:
    parameters.emplace_back("omp_const_mem_alloc");
    break;
  case OMPC_ALLOCATE_ALLOCATOR_high_bw:
    parameters.emplace_back("omp_high_bw_mem_alloc");
    break;
  case OMPC_ALLOCATE_ALLOCATOR_low_lat:
    parameters.emplace_back("omp_low_lat_mem_alloc");
    break;
  case OMPC_ALLOCATE_ALLOCATOR_cgroup:
    parameters.emplace_back("omp_cgroup_mem_alloc");
    break;
  case OMPC_ALLOCATE_ALLOCATOR_pteam:
    parameters.emplace_back("omp_pteam_mem_alloc");
    break;
  case OMPC_ALLOCATE_ALLOCATOR_thread:
    parameters.emplace_back("omp_thread_mem_alloc");
    break;
  default:;
  }
  if (this->getUserDefinedAllocator() != "") {
    parameters.push_back(this->getUserDefinedAllocator());
  }
  for (const auto &extra : this->getExtraAllocatorParameters()) {
    parameters.push_back(extra);
  }
  for (size_t i = 0; i < parameters.size(); ++i) {
    if (i > 0) {
      clause_string += ", ";
    }
    clause_string += parameters[i];
  }
  if (clause_string.size() > 1) {
    clause_string += ": ";
  };
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 2) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPAllocatorClause::toString() {

  std::string result = "allocator ";
  std::string clause_string = "(";
  OpenMPAllocatorClauseAllocator allocator = this->getAllocator();
  switch (allocator) {
  case OMPC_ALLOCATOR_ALLOCATOR_default:
    clause_string += "omp_default_mem_alloc";
    break;
  case OMPC_ALLOCATOR_ALLOCATOR_large_cap:
    clause_string += "omp_large_cap_mem_alloc";
    break;
  case OMPC_ALLOCATOR_ALLOCATOR_cons_mem:
    clause_string += "omp_const_mem_alloc";
    break;
  case OMPC_ALLOCATOR_ALLOCATOR_high_bw:
    clause_string += "omp_high_bw_mem_alloc";
    break;
  case OMPC_ALLOCATOR_ALLOCATOR_low_lat:
    clause_string += "omp_low_lat_mem_alloc";
    break;
  case OMPC_ALLOCATOR_ALLOCATOR_cgroup:
    clause_string += "omp_cgroup_mem_alloc";
    break;
  case OMPC_ALLOCATOR_ALLOCATOR_pteam:
    clause_string += "omp_pteam_mem_alloc";
    break;
  case OMPC_ALLOCATOR_ALLOCATOR_thread:
    clause_string += "omp_thread_mem_alloc";
    break;
  default:;
  }
  if (this->getUserDefinedAllocator() != "") {
    clause_string += this->getUserDefinedAllocator();
  }
  clause_string += this->expressionToString();
  clause_string += ") ";
  if (clause_string.size() > 2) {
    result += clause_string;
  };

  return result;
};

std::string OpenMPVariantClause::toString() {

  std::string result;
  std::string parameter_string;
  std::string beginning_symbol;
  std::string ending_symbol;
  std::pair<std::string, std::string> *parameter_pair_string = NULL;
  OpenMPDirective *variant_directive = NULL;
  OpenMPClauseKind clause_kind = this->getKind();
  switch (clause_kind) {
  case OMPC_when:
    result = "when";
    break;
  case OMPC_match:
    result = "match";
    break;
  case OMPC_otherwise:
    result = "otherwise";
    break;
  default:
    std::cout << "The variant clause is not supported.\n";
  };
  result += " (";

  // For otherwise clause, skip context selectors and just output the directive
  if (clause_kind == OMPC_otherwise) {
    variant_directive = ((OpenMPOtherwiseClause *)this)->getVariantDirective();
    if (variant_directive != NULL) {
      result += variant_directive->generatePragmaString("", "", "");
    }
    result += ") ";
    return result;
  }

  // Builders for each selector category
  auto buildUserSelector = [&]() -> std::string {
    std::string s;
    auto *expr = this->getUserCondition();
    if (!expr) {
      return s;
    }
    if (!expr->score.empty()) {
      s = "user = {condition(score(" + expr->score + "): " + expr->expression +
          ")}";
    } else if (!expr->expression.empty()) {
      s = "user = {condition(" + expr->expression + ")}";
    }
    return s;
  };

  auto buildConstructSelector = [&]() -> std::string {
    std::string s;
    auto *parameter_pair_directives = this->getConstructDirective();
    if (parameter_pair_directives->empty()) {
      return s;
    }
    s = "construct = {";
    bool first = true;
    for (auto &entry : *parameter_pair_directives) {
      if (entry.first != "") {
        beginning_symbol = "score(" + entry.first + "): ";
        ending_symbol = ")";
      } else if (entry.second->getAllClauses()->size() != 0) {
        beginning_symbol = "(";
        ending_symbol = ")";
      } else {
        beginning_symbol = "";
        ending_symbol = "";
      };
      if (!first) {
        s += ", ";
      }
      first = false;
      s += entry.second->generatePragmaString("", beginning_symbol,
                                              ending_symbol);
    }
    s += "}";
    return s;
  };

  auto buildDeviceSelector = [&](bool use_target_device) -> std::string {
    std::vector<std::string> parts;
    std::string local;
    // kind
    std::pair<std::string, OpenMPClauseContextKind> *context_kind =
        this->getContextKind();
    switch (context_kind->second) {
    case OMPC_CONTEXT_KIND_host:
      local = "host";
      break;
    case OMPC_CONTEXT_KIND_nohost:
      local = "nohost";
      break;
    case OMPC_CONTEXT_KIND_any:
      local = "any";
      break;
    case OMPC_CONTEXT_KIND_cpu:
      local = "cpu";
      break;
    case OMPC_CONTEXT_KIND_gpu:
      local = "gpu";
      break;
    case OMPC_CONTEXT_KIND_fpga:
      local = "fpga";
      break;
    case OMPC_CONTEXT_KIND_unknown:
      break;
    default:
      std::cout << "The context kind is not supported.\n";
    };
    if (context_kind->first.size() > 0) {
      parts.push_back("kind(score(" + context_kind->first + "): " + local +
                      ")");
    } else if (!local.empty()) {
      parts.push_back("kind(" + local + ")");
    }
    // arch
    parameter_pair_string = nullptr;
    auto *arch = this->getArchExpression();
    if (arch && !arch->score.empty()) {
      parts.push_back("arch(score(" + arch->score + "): " + arch->expression +
                      ")");
    } else if (arch && !arch->expression.empty()) {
      parts.push_back("arch(" + arch->expression + ")");
    }
    // isa
    auto *isa = this->getIsaExpression();
    if (isa && !isa->score.empty()) {
      parts.push_back("isa(score(" + isa->score + "): " + isa->expression +
                      ")");
    } else if (isa && !isa->expression.empty()) {
      parts.push_back("isa(" + isa->expression + ")");
    }
    // device_num
    auto *devnum = this->getDeviceNumExpression();
    if (devnum && !devnum->score.empty()) {
      parts.push_back("device_num(score(" + devnum->score +
                      "): " + devnum->expression + ")");
    } else if (devnum && !devnum->expression.empty()) {
      parts.push_back("device_num(" + devnum->expression + ")");
    }
    if (parts.empty()) {
      return std::string();
    }
    std::string selector_name = use_target_device ? "target_device" : "device";
    std::string joined;
    for (size_t i = 0; i < parts.size(); ++i) {
      if (i > 0) {
        joined += ", ";
      }
      joined += parts[i];
    }
    return selector_name + " = {" + joined + "}";
  };

  auto buildImplementationSelector = [&]() -> std::string {
    std::string vendor_string;
    std::pair<std::string, OpenMPClauseContextVendor> *context_vendor =
        this->getImplementationKind();
    switch (context_vendor->second) {
    case OMPC_CONTEXT_VENDOR_amd:
      vendor_string = "amd";
      break;
    case OMPC_CONTEXT_VENDOR_arm:
      vendor_string = "arm";
      break;
    case OMPC_CONTEXT_VENDOR_bsc:
      vendor_string = "bsc";
      break;
    case OMPC_CONTEXT_VENDOR_cray:
      vendor_string = "cray";
      break;
    case OMPC_CONTEXT_VENDOR_fujitsu:
      vendor_string = "fujitsu";
      break;
    case OMPC_CONTEXT_VENDOR_gnu:
      vendor_string = "gnu";
      break;
    case OMPC_CONTEXT_VENDOR_ibm:
      vendor_string = "ibm";
      break;
    case OMPC_CONTEXT_VENDOR_intel:
      vendor_string = "intel";
      break;
    case OMPC_CONTEXT_VENDOR_llvm:
      vendor_string = "llvm";
      break;
    case OMPC_CONTEXT_VENDOR_nvidia:
      vendor_string = "nvidia";
      break;
    case OMPC_CONTEXT_VENDOR_pgi:
      vendor_string = "pgi";
      break;
    case OMPC_CONTEXT_VENDOR_ti:
      vendor_string = "ti";
      break;
    case OMPC_CONTEXT_VENDOR_unknown:
      vendor_string = "unknown";
      break;
    case OMPC_CONTEXT_VENDOR_unspecified:
      break;
    default:
      std::cout << "The context vendor is not supported.\n";
    };
    std::vector<std::string> parts;
    if (context_vendor->first.size() > 0) {
      parts.push_back("vendor(score(" + context_vendor->first +
                      "): " + vendor_string + ")");
    } else if (!vendor_string.empty()) {
      parts.push_back("vendor(" + vendor_string + ")");
    }

    // user-defined implementation selector
    auto *impl_expr = this->getImplementationExpression();
    if (impl_expr &&
        (!impl_expr->score.empty() || !impl_expr->expression.empty() ||
         impl_expr->kind != OMPC_IMPL_EXPR_unknown)) {
      const std::string score = impl_expr->score;
      switch (impl_expr->kind) {
      case OMPC_IMPL_EXPR_requires:
        if (!impl_expr->expression.empty()) {
          std::string entry = "requires(";
          if (!score.empty()) {
            entry += "score(" + score + "): ";
          }
          entry += impl_expr->expression;
          entry += ")";
          parts.push_back(entry);
        }
        break;
      case OMPC_IMPL_EXPR_user:
      case OMPC_IMPL_EXPR_unknown:
      default:
        if (!impl_expr->expression.empty()) {
          std::string entry;
          if (!score.empty()) {
            entry = "user(score(" + score + "): " + impl_expr->expression + ")";
          } else {
            entry = "user(" + impl_expr->expression + ")";
          }
          parts.push_back(entry);
        }
        break;
      }
    }

    // extension
    auto *ext_expr = this->getExtensionExpression();
    if (ext_expr && ext_expr->score.size() > 0) {
      parts.push_back("extension(score(" + ext_expr->score +
                      "): " + ext_expr->expression + ")");
    } else if (ext_expr && ext_expr->expression.size() > 0) {
      parts.push_back("extension(" + ext_expr->expression + ")");
    }

    if (parts.empty()) {
      return std::string();
    }
    std::string joined;
    for (size_t i = 0; i < parts.size(); ++i) {
      if (i > 0) {
        joined += ", ";
      }
      joined += parts[i];
    }
    return "implementation = {" + joined + "}";
  };

  std::vector<std::string> selector_strings;
  auto append_selector = [&](const std::string &value) {
    if (!value.empty()) {
      selector_strings.push_back(value);
    }
  };

  const auto &selector_order = this->getSelectorOrder();
  if (!selector_order.empty()) {
    for (auto kind : selector_order) {
      switch (kind) {
      case OMPC_SELECTOR_user:
        append_selector(buildUserSelector());
        break;
      case OMPC_SELECTOR_construct:
        append_selector(buildConstructSelector());
        break;
      case OMPC_SELECTOR_device:
        append_selector(buildDeviceSelector(false));
        break;
      case OMPC_SELECTOR_target_device:
        append_selector(buildDeviceSelector(true));
        break;
      case OMPC_SELECTOR_implementation:
        append_selector(buildImplementationSelector());
        break;
      default:
        break;
      }
    }
  } else {
    // Fallback to legacy ordering
    append_selector(buildUserSelector());
    append_selector(buildConstructSelector());
    append_selector(buildDeviceSelector(this->getIsTargetDeviceSelector()));
    append_selector(buildImplementationSelector());
  }

  for (size_t i = 0; i < selector_strings.size(); ++i) {
    if (i > 0) {
      result += ", ";
    }
    result += selector_strings[i];
  }

  if (clause_kind == OMPC_when) {
    std::string clause_string = " : ";
    variant_directive = ((OpenMPWhenClause *)this)->getVariantDirective();
    if (variant_directive != NULL) {
      clause_string += variant_directive->generatePragmaString("");
    };
    result += clause_string;
  }

  result += ") ";

  return result;
};

std::string OpenMPDefaultClause::toString() {

  std::string result = "default (";
  std::string parameter_string;
  OpenMPDefaultClauseKind default_kind = this->getDefaultClauseKind();
  switch (default_kind) {
  case OMPC_DEFAULT_shared:
    parameter_string = "shared";
    break;
  case OMPC_DEFAULT_private:
    parameter_string = "private";
    break;
  case OMPC_DEFAULT_firstprivate:
    parameter_string = "firstprivate";
    break;
  case OMPC_DEFAULT_none:
    parameter_string = "none";
    break;
  case OMPC_DEFAULT_variant:
    parameter_string =
        this->getVariantDirective()->generatePragmaString("", "", "");
    break;
  default:
    std::cout << "The parameter of default clause is not supported.\n";
  };

  if (parameter_string.size() > 0) {
    result += parameter_string + ") ";
  } else {
    return std::string();
  }

  return result;
};

std::string OpenMPScanClause::toString() {

  std::string result;
  switch (this->getKind()) {
  case OMPC_inclusive:
    result = "inclusive(";
    break;
  case OMPC_exclusive:
    result = "exclusive(";
    break;
  default:
    return std::string();
  }

  const auto &ops = this->getOperands();
  if (!ops.empty()) {
    for (size_t idx = 0; idx < ops.size(); ++idx) {
      if (idx > 0) {
        result += (ops[idx].separator == OMPC_CLAUSE_SEP_comma) ? ", " : " ";
      }
      result += ops[idx].text;
    }
  } else {
    std::string fallback = this->expressionToString();
    if (!fallback.empty()) {
      result += fallback;
    }
  }

  result += ") ";
  return result;
}

std::string OpenMPOrderClause::toString() {

  std::string result = "order (";
  std::string modifier_string;
  std::string parameter_string;

  OpenMPOrderClauseModifier order_modifier = this->getOrderClauseModifier();
  switch (order_modifier) {
  case OMPC_ORDER_MODIFIER_reproducible:
    modifier_string = "reproducible:";
    break;
  case OMPC_ORDER_MODIFIER_unconstrained:
    modifier_string = "unconstrained:";
    break;
  case OMPC_ORDER_MODIFIER_unspecified:
    // No modifier
    break;
  };

  OpenMPOrderClauseKind order_kind = this->getOrderClauseKind();
  switch (order_kind) {
  case OMPC_ORDER_concurrent:
    parameter_string = "concurrent";
    break;
  default:
    std::cout << "The parameter of order clause is not supported.\n";
  };

  if (parameter_string.size() > 0) {
    result += modifier_string + parameter_string;
  } else {
    return std::string();
  }

  const auto &ops = this->getOperands();
  if (!ops.empty()) {
    result += " : ";
    for (size_t idx = 0; idx < ops.size(); ++idx) {
      if (idx > 0) {
        result += (ops[idx].separator == OMPC_CLAUSE_SEP_comma) ? ", " : " ";
      }
      result += ops[idx].text;
    }
  }
  result += ") ";

  return result;
};

std::string OpenMPBindClause::toString() {

  std::string result = "bind (";
  std::string parameter_string;
  OpenMPBindClauseBinding bind_binding = this->getBindClauseBinding();
  switch (bind_binding) {
  case OMPC_BIND_teams:
    parameter_string = "teams";
    break;
  case OMPC_BIND_parallel:
    parameter_string = "parallel";
    break;
  case OMPC_BIND_thread:
    parameter_string = "thread";
    break;
  default:
    std::cout << "The parameter of bind clause is not supported.\n";
  };

  if (parameter_string.size() > 0) {
    result += parameter_string + ") ";
  } else {
    return std::string();
  }

  return result;
};

std::string OpenMPProcBindClause::toString() {

  std::string result = "proc_bind (";
  std::string parameter_string;
  OpenMPProcBindClauseKind proc_bind_kind = this->getProcBindClauseKind();
  switch (proc_bind_kind) {
  case OMPC_PROC_BIND_close:
    parameter_string = "close";
    break;
  case OMPC_PROC_BIND_master:
    parameter_string = "master";
    break;
  case OMPC_PROC_BIND_primary:
    parameter_string = "primary";
    break;
  case OMPC_PROC_BIND_spread:
    parameter_string = "spread";
    break;
  default:
    std::cout << "The parameter of proc_bind clause is not supported.\n";
  };

  if (parameter_string.size() > 0) {
    result += parameter_string + ") ";
  } else {
    return std::string();
  }

  return result;
};

std::string OpenMPUsesAllocatorsClause::toString() {
  std::vector<usesAllocatorParameter *> *usesAllocatorsAllocatorSequence =
      this->getUsesAllocatorsAllocatorSequence();
  std::string result = "uses_allocators(";
  bool first = true;
  for (unsigned int i = 0; i < usesAllocatorsAllocatorSequence->size(); i++) {
    auto *entry = usesAllocatorsAllocatorSequence->at(i);
    if (!first) {
      result += ", ";
    }
    first = false;
    switch (entry->getUsesAllocatorsAllocator()) {
    case OMPC_USESALLOCATORS_ALLOCATOR_default:
      result += "omp_default_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_large_cap:
      result += "omp_large_cap_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_cons_mem:
      result += "omp_const_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_high_bw:
      result += "omp_high_bw_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_low_lat:
      result += "omp_low_lat_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_cgroup:
      result += "omp_cgroup_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_pteam:
      result += "omp_pteam_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_thread:
      result += "omp_thread_mem_alloc";
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_user:
      result += entry->getAllocatorUser();
      break;
    case OMPC_USESALLOCATORS_ALLOCATOR_unspecified: {
      if (!entry->getAllocatorTraitsArray().empty()) {
        result += "traits(" + entry->getAllocatorTraitsArray() + ")";
        if (!entry->getAllocatorUser().empty()) {
          result += ":" + entry->getAllocatorUser();
        }
      } else if (!entry->getAllocatorUser().empty()) {
        result += entry->getAllocatorUser();
      }
      break;
    }
    default:;
    }

    if (entry->getUsesAllocatorsAllocator() !=
            OMPC_USESALLOCATORS_ALLOCATOR_unspecified &&
        !entry->getAllocatorTraitsArray().empty()) {
      result += "(" + entry->getAllocatorTraitsArray() + ")";
    }
  }
  result += ") ";
  return result;
};

std::string OpenMPFailClause::toString() {
  std::string result = "fail (";

  OpenMPFailClauseMemoryOrder memory_order = this->getMemoryOrder();
  switch (memory_order) {
  case OMPC_FAIL_seq_cst:
    result += "seq_cst";
    break;
  case OMPC_FAIL_acquire:
    result += "acquire";
    break;
  case OMPC_FAIL_relaxed:
    result += "relaxed";
    break;
  default:
    result += "unknown";
  }

  result += ") ";
  return result;
};

std::string OpenMPSeverityClause::toString() {
  std::string result = "severity (";

  OpenMPSeverityClauseKind severity_kind = this->getSeverityKind();
  switch (severity_kind) {
  case OMPC_SEVERITY_fatal:
    result += "fatal";
    break;
  case OMPC_SEVERITY_warning:
    result += "warning";
    break;
  default:
    result += "unknown";
  }

  result += ") ";
  return result;
};

std::string OpenMPAtClause::toString() {
  std::string result = "at (";

  OpenMPAtClauseKind at_kind = this->getAtKind();
  switch (at_kind) {
  case OMPC_AT_compilation:
    result += "compilation";
    break;
  case OMPC_AT_execution:
    result += "execution";
    break;
  default:
    result += "unknown";
  }

  result += ") ";
  return result;
};

std::string OpenMPGrainsizeClause::toString() {
  std::string result = "grainsize (";

  OpenMPGrainsizeClauseModifier modifier = this->getModifier();
  if (modifier == OMPC_GRAINSIZE_MODIFIER_strict) {
    result += "strict:";
  }

  std::string expr_string = this->expressionToString();
  if (expr_string.size() > 0) {
    if (modifier == OMPC_GRAINSIZE_MODIFIER_strict) {
      result += " " + expr_string;
    } else {
      result += expr_string;
    }
  }

  result += ") ";
  return result;
};

std::string OpenMPNumTasksClause::toString() {
  std::string result = "num_tasks (";

  OpenMPNumTasksClauseModifier modifier = this->getModifier();
  if (modifier == OMPC_NUM_TASKS_MODIFIER_strict) {
    result += "strict:";
  }

  std::string expr_string = this->expressionToString();
  if (expr_string.size() > 0) {
    if (modifier == OMPC_NUM_TASKS_MODIFIER_strict) {
      result += " " + expr_string;
    } else {
      result += expr_string;
    }
  }

  result += ") ";
  return result;
};

std::string OpenMPNumThreadsClause::toString() {
  std::string result = "num_threads(";
  if (this->isStrict()) {
    result += "strict:";
  }
  std::string expr_string = this->expressionToString();
  if (!expr_string.empty()) {
    result += expr_string;
  }
  result += ") ";
  return result;
};
