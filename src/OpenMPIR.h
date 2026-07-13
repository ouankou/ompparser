/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#ifndef OMPPARSER_OPENMPAST_H
#define OMPPARSER_OPENMPAST_H

#include <algorithm>
#include <fstream>
#include <iostream>

#include "OpenMPKinds.h"
#include <cassert>
#include <map>
#include <memory>
#include <ostream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "OpenMPParser.h"

enum OpenMPBaseLang { Lang_C, Lang_Cplusplus, Lang_Fortran, Lang_unknown };

enum OpenMPFortranSentinelKind { OMPFS_omp, OMPFS_ompx };

enum OpenMPExprParseMode {
  OMP_EXPR_PARSE_none,
  OMP_EXPR_PARSE_expression,
  OMP_EXPR_PARSE_variable_list,
  OMP_EXPR_PARSE_array_section,
  OMP_EXPR_PARSE_verbatim
};

int openmpGetCurrentTokenLine();
int openmpGetCurrentTokenColumn();
bool openmpGetLexemeSourceRange(const char *lexeme,
                                ompparser::SourceRange &range);

class SourceLocation {
  int line;
  int column;

  SourceLocation *parent_construct;

public:
  SourceLocation(int _line = 0, int _col = 0,
                 SourceLocation *_parent_construct = NULL)
      : line(_line), column(_col), parent_construct(_parent_construct) {
    if (line <= 0 || column <= 0) {
      int parsed_line = openmpGetCurrentTokenLine();
      int parsed_column = openmpGetCurrentTokenColumn();
      if (line <= 0) {
        line = parsed_line;
      }
      if (column <= 0) {
        column = parsed_column;
      }
    }
  };
  void setParentConstruct(SourceLocation *_parent_construct) {
    parent_construct = _parent_construct;
  };
  SourceLocation *getParentConstruct() { return parent_construct; };
  int getLine() const { return line; };
  void setLine(int _line) { line = _line; };
  int getColumn() const { return column; };
  void setColumn(int _column) { column = _column; };
};

struct OpenMPExpressionItem {
  ompparser::HostFragment fragment;
  OpenMPClauseSeparator separator = OMPC_CLAUSE_SEP_space;
  OpenMPExprParseMode parse_mode = OMP_EXPR_PARSE_expression;

  OpenMPExpressionItem() = default;
  OpenMPExpressionItem(
      std::string spelling,
      OpenMPClauseSeparator expression_separator = OMPC_CLAUSE_SEP_space,
      OpenMPExprParseMode mode = OMP_EXPR_PARSE_expression)
      : separator(expression_separator), parse_mode(mode) {
    fragment.spelling = std::move(spelling);
  }
};

struct OpenMPIterator {
  ompparser::HostFragment qualifier;
  ompparser::HostFragment variable;
  ompparser::HostFragment begin;
  ompparser::HostFragment end;
  ompparser::HostFragment step;

  void set(const std::string &qualifier_spelling,
           const std::string &variable_spelling,
           const std::string &begin_spelling, const std::string &end_spelling,
           const std::string &step_spelling = std::string()) {
    qualifier.spelling = qualifier_spelling;
    qualifier.role = ompparser::HostFragmentRole::Type;
    variable.spelling = variable_spelling;
    variable.role = ompparser::HostFragmentRole::Declarator;
    begin.spelling = begin_spelling;
    begin.role = ompparser::HostFragmentRole::Expression;
    end.spelling = end_spelling;
    end.role = ompparser::HostFragmentRole::Expression;
    step.spelling = step_spelling;
    step.role = ompparser::HostFragmentRole::Expression;
  }

  void visitHostFragments(const ompparser::HostFragmentVisitor &visitor) {
    if (!qualifier.spelling.empty()) {
      visitor(qualifier);
    }
    visitor(variable);
    visitor(begin);
    visitor(end);
    if (!step.spelling.empty()) {
      visitor(step);
    }
  }
};

/**
 * The class or baseclass for all the clause classes. For all the clauses that
 * only take 0 to multiple expression or variables, we use this class to create
 * objects. For all other clauses, which requires at least one parameters, we
 * will have an inherit class from this one, and the superclass contains fields
 * for the parameters
 */
class OpenMPClause : public SourceLocation {
public:
protected:
  OpenMPClauseKind kind;
  OpenMPDirectiveKind directive_kind = OMPD_unknown;
  bool has_directive_name_modifier = false;
  OpenMPDirectiveKind directive_name_modifier = OMPD_unknown;
  // the clause position in the vector of clauses in original order
  int clause_position = -1;
  OpenMPClauseSeparator separator = OMPC_CLAUSE_SEP_space;

  std::vector<OpenMPExpressionItem> expressions;
  mutable std::vector<const char *> legacy_expression_view;

public:
  OpenMPClause(OpenMPClauseKind k, int _line = 0, int _col = 0)
      : SourceLocation(_line, _col), kind(k) {};

  virtual ~OpenMPClause() = default;

  OpenMPClauseKind getKind() const { return kind; };
  OpenMPDirectiveKind getDirectiveKind() const { return directive_kind; }
  void setDirectiveKind(OpenMPDirectiveKind value) { directive_kind = value; }
  void setDirectiveNameModifier(OpenMPDirectiveKind value) {
    has_directive_name_modifier = true;
    directive_name_modifier = value;
  }
  bool hasDirectiveNameModifier() const { return has_directive_name_modifier; }
  OpenMPDirectiveKind getDirectiveNameModifier() const {
    return directive_name_modifier;
  }
  int getClausePosition() const { return clause_position; };
  void setClausePosition(int _clause_position) {
    clause_position = _clause_position;
  };
  void setPrecedingSeparator(OpenMPClauseSeparator sep) { separator = sep; }
  OpenMPClauseSeparator getPrecedingSeparator() const { return separator; }

  // Typed host-language fragments owned by this clause. Optional host hooks
  // may attach semantic nodes without changing their source spelling/ranges.
  virtual void
  addLangExpr(const char *expression,
              OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_space, int line = 0,
              int col = 0,
              OpenMPExprParseMode parse_mode = OMP_EXPR_PARSE_none);

  std::vector<const char *> *getExpressions();
  const std::vector<const char *> *getExpressions() const;
  const std::vector<OpenMPExpressionItem> &getExpressionItems() const {
    return expressions;
  }
  std::vector<OpenMPExpressionItem> &getExpressionItems() {
    return expressions;
  }
  virtual void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) {
    for (OpenMPExpressionItem &expression : expressions) {
      visitor(expression.fragment);
    }
  }
  std::shared_ptr<const ompparser::HostSemanticNode>
  getExpressionNode(size_t index) const {
    return index < expressions.size() ? expressions[index].fragment.semantic
                                      : nullptr;
  }
  void setExpressionNode(
      size_t index,
      std::shared_ptr<const ompparser::HostSemanticNode> semantic) {
    if (index < expressions.size()) {
      expressions[index].fragment.semantic = std::move(semantic);
    }
  }
  OpenMPExprParseMode getExpressionParseMode(size_t index) const {
    return index < expressions.size() ? expressions[index].parse_mode
                                      : OMP_EXPR_PARSE_none;
  }

  virtual std::string toString();
  std::string expressionToString();
  virtual void generateDOT(std::ostream &, int, int, std::string) const;
};

/**
 * The class for all the OpenMP directives
 */
class OpenMPDirective : public SourceLocation {
protected:
  OpenMPDirectiveKind kind;
  OpenMPBaseLang lang;
  bool use_declare_target_underscore = false;
  bool compact_parallel_do = false;
  bool requires_explicit_end = false;
  OpenMPFortranSentinelKind fortran_sentinel = OMPFS_omp;
  std::string implementation_defined_payload;

  // Non-owning view of every clause occurrence in source order.
  std::vector<OpenMPClause *> clauses_in_original_order;

  // Non-owning index of source occurrences by clause kind.
  std::map<OpenMPClauseKind, std::vector<OpenMPClause *>> clauses;

  // Owned storage for clause objects to ensure automatic cleanup
  std::vector<std::unique_ptr<OpenMPClause>> clause_storage;
  std::vector<std::string> construction_errors;

  // Checked compatibility entry point used by the legacy grammar actions.
  OpenMPClause *addOpenMPClause(OpenMPClauseKind kind, int *parameters);
  using ClauseArgument = std::variant<int, std::string>;
  OpenMPClause *
  addOpenMPClauseWithArguments(OpenMPClauseKind kind,
                               const std::vector<ClauseArgument> &arguments);

public:
  OpenMPDirective(OpenMPDirectiveKind k, OpenMPBaseLang _lang = Lang_unknown,
                  int _line = 0, int _col = 0)
      : SourceLocation(_line, _col), kind(k), lang(_lang) {};

  virtual ~OpenMPDirective() = default;

  OpenMPDirectiveKind getKind() const { return kind; };

  std::map<OpenMPClauseKind, std::vector<OpenMPClause *>> &getAllClauses() {
    return clauses;
  };
  const std::map<OpenMPClauseKind, std::vector<OpenMPClause *>> &
  getAllClauses() const {
    return clauses;
  }

  std::vector<OpenMPClause *> *getClauses(OpenMPClauseKind kind) {
    return &clauses[kind];
  };
  const std::vector<OpenMPClause *> *findClauses(OpenMPClauseKind kind) const {
    auto iter = clauses.find(kind);
    return iter == clauses.end() ? nullptr : &iter->second;
  }
  std::vector<OpenMPClause *> *getClausesInOriginalOrder() {
    return &clauses_in_original_order;
  };
  const std::vector<OpenMPClause *> &getClausesInOriginalOrder() const {
    return clauses_in_original_order;
  }

  std::string toString() const;

  /* generate DOT representation of the directive */
  void generateDOT(std::ostream &, int, int, std::string, std::string) const;
  std::string generateDOTString() const;
  void generateDOT() const;
  std::string generatePragmaString(std::string _prefix = "#pragma omp ") const;
  std::string generateContextTraitString() const;
  template <typename... Args>
  OpenMPClause *addOpenMPClause(int raw_kind, Args &&...raw_arguments) {
    std::vector<ClauseArgument> arguments;
    arguments.reserve(sizeof...(Args));
    if constexpr (sizeof...(Args) > 0) {
      auto append_argument = [&arguments](auto &&raw_argument) {
        using Argument = std::decay_t<decltype(raw_argument)>;
        if constexpr (std::is_same_v<Argument, std::string>) {
          arguments.emplace_back(
              std::forward<decltype(raw_argument)>(raw_argument));
        } else if constexpr (std::is_pointer_v<Argument> &&
                             std::is_same_v<
                                 std::remove_cv_t<
                                     std::remove_pointer_t<Argument>>,
                                 char>) {
          arguments.emplace_back(raw_argument ? std::string(raw_argument)
                                              : std::string());
        } else if constexpr (std::is_enum_v<Argument> ||
                             std::is_integral_v<Argument>) {
          arguments.emplace_back(static_cast<int>(raw_argument));
        } else {
          static_assert(!sizeof(Argument),
                        "unsupported OpenMP clause argument");
        }
      };
      (append_argument(std::forward<Args>(raw_arguments)), ...);
    }
    return addOpenMPClauseWithArguments(static_cast<OpenMPClauseKind>(raw_kind),
                                        arguments);
  }
  void setBaseLang(OpenMPBaseLang _lang) { lang = _lang; };
  OpenMPBaseLang getBaseLang() const { return lang; };
  void setDeclareTargetUnderscore(bool use_underscore) {
    use_declare_target_underscore = use_underscore;
  }
  bool getDeclareTargetUnderscore() const {
    return use_declare_target_underscore;
  }
  void setCompactParallelDo(bool compact) { compact_parallel_do = compact; }
  bool getCompactParallelDo() const { return compact_parallel_do; }
  void setRequiresExplicitEnd(bool value) { requires_explicit_end = value; }
  bool getRequiresExplicitEnd() const { return requires_explicit_end; }
  void setFortranSentinel(OpenMPFortranSentinelKind sentinel) {
    fortran_sentinel = sentinel;
  }
  OpenMPFortranSentinelKind getFortranSentinel() const {
    return fortran_sentinel;
  }
  void setImplementationDefinedPayload(const std::string &payload) {
    implementation_defined_payload = payload;
  }
  const std::string &getImplementationDefinedPayload() const {
    return implementation_defined_payload;
  }

  // Registers a clause for automatic lifetime management
  // Takes ownership of the clause and returns a raw pointer for use
  OpenMPClause *registerClause(std::unique_ptr<OpenMPClause> clause);
  void adoptClausesFrom(OpenMPDirective &source);
  virtual void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) {
    for (OpenMPClause *clause : clauses_in_original_order) {
      if (clause != nullptr) {
        clause->visitHostFragments(visitor);
      }
    }
  }
  bool validateInvariants(std::vector<std::string> &errors) const;
};

// atomic directive
class OpenMPAtomicDirective : public OpenMPDirective {
protected:
  std::map<OpenMPClauseKind, std::vector<OpenMPClause *> *>
      clauses_atomic_after;
  std::map<OpenMPClauseKind, std::vector<OpenMPClause *> *>
      clauses_atomic_clauses;
  std::vector<std::unique_ptr<std::vector<OpenMPClause *>>>
      atomic_clause_vector_storage;

public:
  OpenMPAtomicDirective() : OpenMPDirective(OMPD_atomic) {};
  std::vector<OpenMPClause *> *getClausesAtomicAfter(OpenMPClauseKind kind) {
    if (clauses_atomic_after.count(kind) == 0) {
      auto vec = std::make_unique<std::vector<OpenMPClause *>>();
      clauses_atomic_after[kind] = vec.get();
      atomic_clause_vector_storage.push_back(std::move(vec));
    }
    return clauses_atomic_after[kind];
  };
  std::vector<OpenMPClause *> *getAtomicClauses(OpenMPClauseKind kind) {
    if (clauses_atomic_clauses.count(kind) == 0) {
      auto vec = std::make_unique<std::vector<OpenMPClause *>>();
      clauses_atomic_clauses[kind] = vec.get();
      atomic_clause_vector_storage.push_back(std::move(vec));
    }
    return clauses_atomic_clauses[kind];
  };
  std::map<OpenMPClauseKind, std::vector<OpenMPClause *> *> *
  getAllClausesAtomicAfter() {
    return &clauses_atomic_after;
  };
  std::map<OpenMPClauseKind, std::vector<OpenMPClause *> *> *
  getAllAtomicClauses() {
    return &clauses_atomic_clauses;
  };
};

// fail clause for atomic compare (OpenMP 5.1)
class OpenMPFailClause : public OpenMPClause {
protected:
  OpenMPFailClauseMemoryOrder memory_order;

public:
  OpenMPFailClause(OpenMPFailClauseMemoryOrder _memory_order)
      : OpenMPClause(OMPC_fail), memory_order(_memory_order) {};

  OpenMPFailClauseMemoryOrder getMemoryOrder() { return memory_order; };

  std::string toString() override;
};

class OpenMPSeverityClause : public OpenMPClause {
protected:
  OpenMPSeverityClauseKind severity_kind;

public:
  OpenMPSeverityClause(OpenMPSeverityClauseKind _severity_kind)
      : OpenMPClause(OMPC_severity), severity_kind(_severity_kind) {};

  OpenMPSeverityClauseKind getSeverityKind() { return severity_kind; };

  std::string toString() override;
};

class OpenMPAtClause : public OpenMPClause {
protected:
  OpenMPAtClauseKind at_kind;

public:
  OpenMPAtClause(OpenMPAtClauseKind _at_kind)
      : OpenMPClause(OMPC_at), at_kind(_at_kind) {};

  OpenMPAtClauseKind getAtKind() { return at_kind; };

  std::string toString() override;
};

// Suffix-form ends own a complete parsed directive. Standalone end markers
// retain only a synthetic directive kind because their begin-side data is not
// present in the same pragma.
enum class OpenMPPairedDirectiveRole { Complete, KindOnly };

class OpenMPEndDirective : public OpenMPDirective {
protected:
  OpenMPDirective *paired_directive = nullptr;
  std::unique_ptr<OpenMPDirective> paired_directive_storage;
  OpenMPPairedDirectiveRole paired_directive_role =
      OpenMPPairedDirectiveRole::Complete;
  ompparser::HostFragment end_argument;
  bool use_compact_enddo = false;

public:
  OpenMPEndDirective() : OpenMPDirective(OMPD_end) {};
  void setPairedDirective(
      std::unique_ptr<OpenMPDirective> _paired_directive,
      OpenMPPairedDirectiveRole role = OpenMPPairedDirectiveRole::Complete) {
    paired_directive_storage = std::move(_paired_directive);
    paired_directive = paired_directive_storage.get();
    paired_directive_role = role;
  };
  void setPairedDirective(
      OpenMPDirective *_paired_directive,
      OpenMPPairedDirectiveRole role = OpenMPPairedDirectiveRole::Complete) {
    paired_directive_storage.reset();
    paired_directive = _paired_directive;
    paired_directive_role = role;
  };
  OpenMPDirective *getPairedDirective() { return paired_directive; };
  const OpenMPDirective *getPairedDirective() const { return paired_directive; }
  OpenMPPairedDirectiveRole getPairedDirectiveRole() const {
    return paired_directive_role;
  }
  void setEndArgument(const ompparser::HostFragment &argument) {
    end_argument = argument;
  }
  const ompparser::HostFragment &getEndArgument() const { return end_argument; }
  void setUseCompactEndDo(bool compact) { use_compact_enddo = compact; }
  bool getUseCompactEndDo() const { return use_compact_enddo; }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!end_argument.spelling.empty()) {
      visitor(end_argument);
    }
    OpenMPDirective::visitHostFragments(visitor);
    if (paired_directive != nullptr &&
        paired_directive_role == OpenMPPairedDirectiveRole::Complete) {
      paired_directive->visitHostFragments(visitor);
    }
  }
};

class OpenMPRequiresDirective : public OpenMPDirective {
protected:
public:
  OpenMPRequiresDirective() : OpenMPDirective(OMPD_requires) {};
};

// declare variant directive
class OpenMPDeclareVariantDirective : public OpenMPDirective {
protected:
  ompparser::HostFragment variant_func_id;

public:
  OpenMPDeclareVariantDirective() : OpenMPDirective(OMPD_declare_variant) {};
  void setVariantFuncID(const char *_variant_func_id);
  const std::string &getVariantFuncID() const {
    return variant_func_id.spelling;
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!variant_func_id.spelling.empty()) {
      visitor(variant_func_id);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// allocate directive
class OpenMPAllocateDirective : public OpenMPDirective {
protected:
  std::vector<ompparser::HostFragment> allocate_list;

public:
  OpenMPAllocateDirective() : OpenMPDirective(OMPD_allocate) {};
  void addAllocateList(const char *_allocate_list);
  const std::vector<ompparser::HostFragment> &getAllocateList() const {
    return allocate_list;
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (ompparser::HostFragment &item : allocate_list) {
      visitor(item);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// threadprivate directive
class OpenMPThreadprivateDirective : public OpenMPDirective {
protected:
  std::vector<ompparser::HostFragment> threadprivate_list;

public:
  OpenMPThreadprivateDirective() : OpenMPDirective(OMPD_threadprivate) {};
  void addThreadprivateList(const char *_threadprivate_list);
  const std::vector<ompparser::HostFragment> &getThreadprivateList() const {
    return threadprivate_list;
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (ompparser::HostFragment &item : threadprivate_list) {
      visitor(item);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// groupprivate directive
class OpenMPGroupprivateDirective : public OpenMPDirective {
protected:
  std::vector<ompparser::HostFragment> groupprivate_list;

public:
  OpenMPGroupprivateDirective() : OpenMPDirective(OMPD_groupprivate) {};
  void addGroupprivateList(const char *_groupprivate_list);
  const std::vector<ompparser::HostFragment> &getGroupprivateList() const {
    return groupprivate_list;
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (ompparser::HostFragment &item : groupprivate_list) {
      visitor(item);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// declare simd directive
class OpenMPDeclareSimdDirective : public OpenMPDirective {
protected:
  ompparser::HostFragment proc_name;

public:
  OpenMPDeclareSimdDirective() : OpenMPDirective(OMPD_declare_simd) {};
  void addProcName(const char *_proc_name);
  const std::string &getProcName() const { return proc_name.spelling; }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!proc_name.spelling.empty()) {
      visitor(proc_name);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// declare reduction directive
class OpenMPDeclareReductionDirective : public OpenMPDirective {
protected:
  std::vector<ompparser::HostFragment> typename_list;
  std::string identifier;
  ompparser::HostFragment combiner;

public:
  OpenMPDeclareReductionDirective()
      : OpenMPDirective(OMPD_declare_reduction) {};
  void addTypenameList(const char *_typename_list);
  const std::vector<ompparser::HostFragment> &getTypenameList() const {
    return typename_list;
  }
  void setIdentifier(std::string _identifier) { identifier = _identifier; }
  const std::string &getIdentifier() const { return identifier; }
  void setCombiner(const char *_combiner);
  const ompparser::HostFragment &getCombinerFragment() const {
    return combiner;
  }
  const std::string &getCombiner() const { return combiner.spelling; }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (ompparser::HostFragment &type : typename_list) {
      visitor(type);
    }
    if (!combiner.spelling.empty()) {
      visitor(combiner);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// declare mapper directive
class OpenMPDeclareMapperDirective : public OpenMPDirective {
protected:
  OpenMPDeclareMapperDirectiveIdentifier identifier =
      OMPD_DECLARE_MAPPER_IDENTIFIER_unspecified; // modifier
  ompparser::HostFragment user_defined_identifier;
  ompparser::HostFragment type;
  ompparser::HostFragment var;
  bool type_var_has_space = false;

public:
  OpenMPDeclareMapperDirective(
      OpenMPDeclareMapperDirectiveIdentifier _identifier)
      : OpenMPDirective(OMPD_declare_mapper) {
    identifier = _identifier;
  };
  void setIdentifier(OpenMPDeclareMapperDirectiveIdentifier _identifier) {
    identifier = _identifier;
  };
  OpenMPDeclareMapperDirectiveIdentifier getIdentifier() const {
    return identifier;
  }
  void setUserDefinedIdentifier(const char *_user_defined_identifier);
  const std::string &getUserDefinedIdentifier() const {
    return user_defined_identifier.spelling;
  }
  const std::string &getDeclareMapperType() const { return type.spelling; }
  const std::string &getDeclareMapperVar() const { return var.spelling; }
  void setDeclareMapperType(const char *_declare_mapper_type);
  void setDeclareMapperVar(const char *_declare_mapper_variable);
  void setTypeVarHasSpace(bool has_space) { type_var_has_space = has_space; }
  bool hasTypeVarSpace() const { return type_var_has_space; }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_identifier.spelling.empty()) {
      visitor(user_defined_identifier);
    }
    if (!type.spelling.empty()) {
      visitor(type);
    }
    if (!var.spelling.empty()) {
      visitor(var);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// reduction clause
class OpenMPReductionClause : public OpenMPClause {

protected:
  OpenMPReductionClauseModifier modifier =
      OMPC_REDUCTION_MODIFIER_unknown; // modifier
  OpenMPReductionClauseIdentifier identifier =
      OMPC_REDUCTION_IDENTIFIER_unknown; // identifier
  ompparser::HostFragment user_defined_identifier;
  ompparser::HostFragment user_defined_modifier;

public:
  OpenMPReductionClause() : OpenMPClause(OMPC_reduction) {}

  OpenMPReductionClause(OpenMPReductionClauseModifier _modifier,
                        OpenMPReductionClauseIdentifier _identifier)
      : OpenMPClause(OMPC_reduction), modifier(_modifier),
        identifier(_identifier) {};

  OpenMPReductionClauseModifier getModifier() const { return modifier; };

  OpenMPReductionClauseIdentifier getIdentifier() const { return identifier; };

  void setUserDefinedIdentifier(const char *_identifier);

  const std::string &getUserDefinedIdentifier() const {
    return user_defined_identifier.spelling;
  }
  void setUserDefinedModifier(const char *_modifier);

  const std::string &getUserDefinedModifier() const {
    return user_defined_modifier.spelling;
  }
  void addOperand(const std::string &operand,
                  OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addLangExpr(operand.c_str(), sep, 0, 0, OMP_EXPR_PARSE_variable_list);
  }
  const std::vector<OpenMPExpressionItem> &getOperands() const {
    return expressions;
  }
  void clearOperands() { expressions.clear(); }

  static OpenMPClause *addReductionClause(OpenMPDirective *,
                                          OpenMPReductionClauseModifier,
                                          OpenMPReductionClauseIdentifier,
                                          const char *, const char *);

  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_modifier.spelling.empty()) {
      visitor(user_defined_modifier);
    }
    if (!user_defined_identifier.spelling.empty()) {
      visitor(user_defined_identifier);
    }
    OpenMPClause::visitHostFragments(visitor);
  }

  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// ext_implementation_defined_requirement clause
class OpenMPExtImplementationDefinedRequirementClause : public OpenMPClause {

protected:
  std::string implementation_defined_requirement;

public:
  OpenMPExtImplementationDefinedRequirementClause()
      : OpenMPClause(OMPC_ext_implementation_defined_requirement) {}

  void setImplementationDefinedRequirement(
      const char *_implementation_defined_requirement) {
    implementation_defined_requirement = _implementation_defined_requirement;
  };
  std::string getImplementationDefinedRequirement() {
    return implementation_defined_requirement;
  };

  static OpenMPClause *
  addExtImplementationDefinedRequirementClause(OpenMPDirective *);
  std::string toString() override;
};

// initializer clause
class OpenMPInitializerClause : public OpenMPClause {
protected:
  OpenMPInitializerClausePriv priv; // initializer priv
public:
  OpenMPInitializerClause(OpenMPInitializerClausePriv _priv)
      : OpenMPClause(OMPC_initializer), priv(_priv) {};

  OpenMPInitializerClausePriv getPriv() const { return priv; };

  void setUserDefinedPriv(const char *_priv);

  std::string getUserDefinedPriv() const {
    return expressions.empty() ? std::string()
                               : expressions.front().fragment.spelling;
  };
  static OpenMPClause *addInitializerClause(OpenMPDirective *,
                                            OpenMPInitializerClausePriv,
                                            const char *);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

class OpenMPApplyClause : public OpenMPClause {
public:
  struct ApplyTransform {
    OpenMPApplyTransformKind kind = OMPC_APPLY_TRANSFORM_unknown;
    ompparser::HostFragment argument;
    std::unique_ptr<OpenMPApplyClause> nested_apply;
    OpenMPClauseSeparator separator = OMPC_CLAUSE_SEP_comma;
  };

protected:
  ompparser::HostFragment label;
  std::vector<ApplyTransform> transforms;

public:
  OpenMPApplyClause() : OpenMPClause(OMPC_apply) {};

  void setLabel(const char *value);
  void addTransformation(OpenMPApplyTransformKind kind,
                         const char *argument = nullptr,
                         OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma);
  void addNestedApply(OpenMPApplyClause *nested,
                      OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma);
  const std::string &getLabel() const { return label.spelling; }
  const std::vector<ApplyTransform> &getTransformations() const {
    return transforms;
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!label.spelling.empty()) {
      visitor(label);
    }
    for (ApplyTransform &transform : transforms) {
      if (!transform.argument.spelling.empty()) {
        visitor(transform.argument);
      }
      if (transform.nested_apply != nullptr) {
        transform.nested_apply->visitHostFragments(visitor);
      }
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string toString() override;
};

class OpenMPInductionClause : public OpenMPClause {
public:
  struct Binding {
    ompparser::HostFragment label;
    ompparser::HostFragment expression;
  };

private:
  enum ItemKind { ItemStep, ItemBinding, ItemPassthrough };
  struct ItemRef {
    ItemKind kind;
    size_t index;
  };

  ompparser::HostFragment step_expression;
  std::vector<Binding> bindings;
  std::vector<ompparser::HostFragment> passthrough_items;
  std::vector<ItemRef> sequence;

public:
  OpenMPInductionClause() : OpenMPClause(OMPC_induction) {}

  void addStepExpression(const char *expression);
  void addBinding(const char *label, const char *expression);
  void addPassthroughItem(const char *expression);
  const std::string &getStepExpression() const {
    return step_expression.spelling;
  }
  const std::vector<Binding> &getBindings() const { return bindings; }
  const std::vector<ompparser::HostFragment> &getPassthroughItems() const {
    return passthrough_items;
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!step_expression.spelling.empty()) {
      visitor(step_expression);
    }
    for (Binding &binding : bindings) {
      if (!binding.label.spelling.empty()) {
        visitor(binding.label);
      }
      visitor(binding.expression);
    }
    for (ompparser::HostFragment &item : passthrough_items) {
      visitor(item);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string specificationToString() const;
  std::string toString() override;
};

enum class OpenMPInitModifierCategory {
  InteropType,
  DirectiveName,
  PreferType,
  Depinfo
};

struct OpenMPInitModifier {
  OpenMPInitModifierCategory category = OpenMPInitModifierCategory::InteropType;
  OpenMPInitClauseKind interop_type = OMPC_INIT_KIND_unknown;
  OpenMPDirectiveKind directive_name = OMPD_unknown;
  OpenMPDependClauseType dependence_type = OMPC_DEPENDENCE_TYPE_unknown;
  ompparser::HostFragment argument;
};

class OpenMPInitModifierList {
private:
  std::vector<OpenMPInitModifier> modifiers;

public:
  void addInteropType(OpenMPInitClauseKind value);
  void addDirectiveName(OpenMPDirectiveKind value);
  void addPreferType(const char *specification);
  void addPreferType(const std::string &specification);
  void addDepinfo(OpenMPDependClauseType type, const char *locator);
  void addDepinfo(OpenMPDependClauseType type, const std::string &locator);

  const std::vector<OpenMPInitModifier> &getModifiers() const {
    return modifiers;
  }
  void visitHostFragments(const ompparser::HostFragmentVisitor &visitor) {
    for (OpenMPInitModifier &modifier : modifiers) {
      if (!modifier.argument.spelling.empty()) {
        visitor(modifier.argument);
      }
    }
  }
  std::string toString() const;
};

class OpenMPInitClause : public OpenMPClause {
private:
  OpenMPInitModifierList modifiers;
  ompparser::HostFragment operand;

public:
  OpenMPInitClause() : OpenMPClause(OMPC_init) {}

  OpenMPInitModifierList &getModifiers() { return modifiers; }
  const OpenMPInitModifierList &getModifiers() const { return modifiers; }

  void setOperand(const char *value);
  void setOperand(const std::string &value);
  const std::string &getOperand() const { return operand.spelling; }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    modifiers.visitHostFragments(visitor);
    if (!operand.spelling.empty()) {
      visitor(operand);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string toString() override;
};

class OpenMPAdjustArgsClause : public OpenMPClause {
private:
  OpenMPAdjustArgsModifier modifier = OMPC_ADJUST_ARGS_unknown;
  std::vector<ompparser::HostFragment> arguments;

public:
  OpenMPAdjustArgsClause() : OpenMPClause(OMPC_adjust_args) {}

  void setModifier(OpenMPAdjustArgsModifier value) { modifier = value; }
  OpenMPAdjustArgsModifier getModifier() const { return modifier; }
  void addArgument(const char *arg);
  const std::vector<ompparser::HostFragment> &getArguments() const {
    return arguments;
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (ompparser::HostFragment &argument : arguments) {
      visitor(argument);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string toString() override;
};

class OpenMPAppendArgsClause : public OpenMPClause {
public:
  struct Operation {
    OpenMPAppendArgsModifier kind = OMPC_APPEND_ARGS_unknown;
    OpenMPInitModifierList modifiers;
  };

private:
  std::vector<Operation> operations;

public:
  OpenMPAppendArgsClause() : OpenMPClause(OMPC_append_args) {}

  void addInteropOperation();
  OpenMPInitModifierList *getCurrentOperationModifiers();
  std::size_t getOperationCount() const { return operations.size(); }
  const std::vector<Operation> &getOperations() const { return operations; }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (Operation &operation : operations) {
      operation.modifiers.visitHostFragments(visitor);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string toString() override;
};

// allocate clause
class OpenMPAllocateClause : public OpenMPClause {
protected:
  OpenMPAllocateClauseAllocator allocator; // Allocate allocator
  ompparser::HostFragment user_defined_allocator;
  std::vector<ompparser::HostFragment> extra_allocator_parameters;

public:
  OpenMPAllocateClause(OpenMPAllocateClauseAllocator _allocator)
      : OpenMPClause(OMPC_allocate), allocator(_allocator) {};

  OpenMPAllocateClauseAllocator getAllocator() const { return allocator; };

  void setUserDefinedAllocator(const char *_allocator);

  const std::string &getUserDefinedAllocator() const {
    return user_defined_allocator.spelling;
  }
  const std::vector<ompparser::HostFragment> &
  getExtraAllocatorParameters() const {
    return extra_allocator_parameters;
  }
  void addExtraAllocatorParameter(const char *param);
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_allocator.spelling.empty()) {
      visitor(user_defined_allocator);
    }
    for (ompparser::HostFragment &parameter : extra_allocator_parameters) {
      visitor(parameter);
    }
    OpenMPClause::visitHostFragments(visitor);
  }

  static OpenMPClause *addAllocateClause(OpenMPDirective *,
                                         OpenMPAllocateClauseAllocator,
                                         const char *);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// allocator
class OpenMPAllocatorClause : public OpenMPClause {
protected:
  OpenMPAllocatorClauseAllocator allocator; // Allocate allocator
  ompparser::HostFragment user_defined_allocator;

public:
  OpenMPAllocatorClause(OpenMPAllocatorClauseAllocator _allocator)
      : OpenMPClause(OMPC_allocator), allocator(_allocator) {};

  OpenMPAllocatorClauseAllocator getAllocator() const { return allocator; };

  void setUserDefinedAllocator(const char *_allocator);

  const std::string &getUserDefinedAllocator() const {
    return user_defined_allocator.spelling;
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_allocator.spelling.empty()) {
      visitor(user_defined_allocator);
    }
    OpenMPClause::visitHostFragments(visitor);
  }

  static OpenMPClause *addAllocatorClause(OpenMPDirective *,
                                          OpenMPAllocatorClauseAllocator,
                                          const char *);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// lastprivate Clause
class OpenMPLastprivateClause : public OpenMPClause {
protected:
  OpenMPLastprivateClauseModifier modifier =
      OMPC_LASTPRIVATE_MODIFIER_unspecified; // lastprivate modifier

public:
  OpenMPLastprivateClause() : OpenMPClause(OMPC_lastprivate) {}

  OpenMPLastprivateClause(OpenMPLastprivateClauseModifier _modifier)
      : OpenMPClause(OMPC_lastprivate), modifier(_modifier) {};

  OpenMPLastprivateClauseModifier getModifier() const { return modifier; };

  static OpenMPClause *addLastprivateClause(OpenMPDirective *,
                                            OpenMPLastprivateClauseModifier);

  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// linear Clause
class OpenMPLinearClause : public OpenMPClause {
protected:
  OpenMPLinearClauseModifier modifier; // linear modifier

  ompparser::HostFragment user_defined_step;
  bool modifier_first_syntax =
      false; // true if syntax is modifier(vars), false if vars: modifier

public:
  OpenMPLinearClause(OpenMPLinearClauseModifier _modifier)
      : OpenMPClause(OMPC_linear), modifier(_modifier) {};

  OpenMPLinearClauseModifier getModifier() const { return modifier; };

  void setModifier(OpenMPLinearClauseModifier _modifier) {
    modifier = _modifier;
  };

  void setUserDefinedStep(const char *_step);

  const std::string &getUserDefinedStep() const {
    return user_defined_step.spelling;
  };

  void setModifierFirstSyntax(bool value) { modifier_first_syntax = value; };

  bool isModifierFirstSyntax() { return modifier_first_syntax; };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_step.spelling.empty()) {
      visitor(user_defined_step);
    }
    OpenMPClause::visitHostFragments(visitor);
  }

  static OpenMPClause *addLinearClause(OpenMPDirective *,
                                       OpenMPLinearClauseModifier);

  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// aligned Clause
class OpenMPAlignedClause : public OpenMPClause {
protected:
  ompparser::HostFragment user_defined_alignment;

public:
  OpenMPAlignedClause() : OpenMPClause(OMPC_aligned) {};

  void setUserDefinedAlignment(const char *_alignment);

  const std::string &getUserDefinedAlignment() const {
    return user_defined_alignment.spelling;
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_alignment.spelling.empty()) {
      visitor(user_defined_alignment);
    }
    OpenMPClause::visitHostFragments(visitor);
  }

  std::string toString() override;
  static OpenMPClause *addAlignedClause(OpenMPDirective *);
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// dist_schedule Clause
class OpenMPDistScheduleClause : public OpenMPClause {

protected:
  OpenMPDistScheduleClauseKind dist_schedule_kind =
      OMPC_DIST_SCHEDULE_KIND_unknown; // kind
  ompparser::HostFragment chunk_size;

public:
  OpenMPDistScheduleClause() : OpenMPClause(OMPC_dist_schedule) {}

  OpenMPDistScheduleClause(OpenMPDistScheduleClauseKind _dist_schedule_kind)
      : OpenMPClause(OMPC_dist_schedule),
        dist_schedule_kind(_dist_schedule_kind) {};

  OpenMPDistScheduleClauseKind getKind() const { return dist_schedule_kind; };

  void setChunkSize(const char *_chunk_size);

  const std::string &getChunkSize() const { return chunk_size.spelling; };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!chunk_size.spelling.empty()) {
      visitor(chunk_size);
    }
    OpenMPClause::visitHostFragments(visitor);
  }

  static OpenMPClause *addDistScheduleClause(OpenMPDirective *,
                                             OpenMPDistScheduleClauseKind);

  std::string toString() override;

  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// schedule Clause
class OpenMPScheduleClause : public OpenMPClause {

protected:
  OpenMPScheduleClauseModifier modifier1 =
      OMPC_SCHEDULE_MODIFIER_unspecified; // modifier1
  OpenMPScheduleClauseModifier modifier2 =
      OMPC_SCHEDULE_MODIFIER_unspecified; // modifier2
  OpenMPScheduleClauseKind schedulekind =
      OMPC_SCHEDULE_KIND_unspecified; // identifier
  ompparser::HostFragment user_defined_kind;
  ompparser::HostFragment chunk_size;

public:
  OpenMPScheduleClause() : OpenMPClause(OMPC_schedule) {}

  OpenMPScheduleClause(OpenMPScheduleClauseModifier _modifier1,
                       OpenMPScheduleClauseModifier _modifier2,
                       OpenMPScheduleClauseKind _schedulekind)
      : OpenMPClause(OMPC_schedule), modifier1(_modifier1),
        modifier2(_modifier2), schedulekind(_schedulekind) {};

  OpenMPScheduleClauseModifier getModifier1() const { return modifier1; };
  OpenMPScheduleClauseModifier getModifier2() const { return modifier2; };
  OpenMPScheduleClauseKind getKind() const { return schedulekind; };

  void setUserDefinedKind(const char *schedulekind);

  const std::string &getUserDefinedKind() const {
    return user_defined_kind.spelling;
  };

  static OpenMPClause *addScheduleClause(OpenMPDirective *,
                                         OpenMPScheduleClauseModifier,
                                         OpenMPScheduleClauseModifier,
                                         OpenMPScheduleClauseKind,
                                         const char *);

  void setChunkSize(const char *_step);

  const std::string &getChunkSize() const { return chunk_size.spelling; };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_kind.spelling.empty()) {
      visitor(user_defined_kind);
    }
    if (!chunk_size.spelling.empty()) {
      visitor(chunk_size);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// grainsize clause with optional strict modifier (OpenMP 5.1)
class OpenMPGrainsizeClause : public OpenMPClause {
protected:
  OpenMPGrainsizeClauseModifier modifier;

public:
  OpenMPGrainsizeClause()
      : OpenMPClause(OMPC_grainsize),
        modifier(OMPC_GRAINSIZE_MODIFIER_unspecified) {}

  OpenMPGrainsizeClause(OpenMPGrainsizeClauseModifier _modifier)
      : OpenMPClause(OMPC_grainsize), modifier(_modifier) {};

  OpenMPGrainsizeClauseModifier getModifier() const { return modifier; };

  std::string toString() override;
};

// num_tasks clause with optional strict modifier (OpenMP 5.1)
class OpenMPNumTasksClause : public OpenMPClause {
protected:
  OpenMPNumTasksClauseModifier modifier;

public:
  OpenMPNumTasksClause()
      : OpenMPClause(OMPC_num_tasks),
        modifier(OMPC_NUM_TASKS_MODIFIER_unspecified) {}

  OpenMPNumTasksClause(OpenMPNumTasksClauseModifier _modifier)
      : OpenMPClause(OMPC_num_tasks), modifier(_modifier) {};

  OpenMPNumTasksClauseModifier getModifier() const { return modifier; };

  std::string toString() override;
};

// num_threads clause with optional strict modifier (OpenMP 5.2)
class OpenMPNumThreadsClause : public OpenMPClause {
protected:
  bool strict = false;

public:
  OpenMPNumThreadsClause() : OpenMPClause(OMPC_num_threads) {};
  void setStrict(bool v) { strict = v; }
  bool isStrict() const { return strict; }
  std::string toString() override;
};

// OpenMP clauses with variant directives, such as WHEN and MATCH clauses.
class OpenMPVariantClause : public OpenMPClause {
protected:
  struct ScoredExpression {
    ompparser::HostFragment score;
    ompparser::HostFragment expression;
  };
  struct ImplementationExpression {
    OpenMPImplementationExprKind kind = OMPC_IMPL_EXPR_unknown;
    ompparser::HostFragment score;
    ompparser::HostFragment expression;
  };
  struct ScoredConstruct {
    ompparser::HostFragment score;
    OpenMPDirective *directive = nullptr;
  };
  struct ScoredContextKind {
    ompparser::HostFragment score;
    OpenMPClauseContextKind kind = OMPC_CONTEXT_KIND_unknown;
  };
  struct ScoredContextVendor {
    ompparser::HostFragment score;
    OpenMPClauseContextVendor vendor = OMPC_CONTEXT_VENDOR_unspecified;
  };
  struct DeviceSelectorData {
    ScoredExpression arch_expression;
    ScoredExpression isa_expression;
    ScoredContextKind context_kind_name;
    ScoredExpression device_num_expression;
    std::size_t arch_expression_count = 0;
    std::size_t isa_expression_count = 0;
    std::size_t context_kind_count = 0;
    std::size_t device_num_expression_count = 0;
  };

  ScoredExpression user_condition_expression;
  std::vector<ScoredConstruct> construct_directives;
  std::vector<std::unique_ptr<OpenMPDirective>> construct_directive_storage;
  DeviceSelectorData device_selector;
  DeviceSelectorData target_device_selector;
  ScoredExpression extension_expression;
  ScoredContextVendor context_vendor_name;
  ImplementationExpression implementation_user_defined_expression;
  std::size_t user_condition_count = 0;
  std::size_t extension_expression_count = 0;
  std::size_t implementation_kind_count = 0;
  std::size_t implementation_expression_count = 0;
  bool is_target_device_selector = false;
  // Preserve selector order as it appeared in the source
  std::vector<OpenMPContextSelectorSequenceKind> selector_order;

public:
  OpenMPVariantClause(OpenMPClauseKind _kind) : OpenMPClause(_kind) {};

  void setUserCondition(const char *_score,
                        const char *_user_condition_expression);
  ScoredExpression *getUserCondition() { return &user_condition_expression; };
  const ScoredExpression *getUserCondition() const {
    return &user_condition_expression;
  }
  void addConstructDirective(const char *_score,
                             OpenMPDirective *_construct_directive) {
    if (_construct_directive == nullptr) {
      return;
    }
    addConstructDirectiveImpl(_score, _construct_directive);
  };
  void
  addConstructDirective(const char *_score,
                        std::unique_ptr<OpenMPDirective> _construct_directive) {
    if (_construct_directive == nullptr) {
      return;
    }
    auto *construct_directive = _construct_directive.get();
    construct_directive_storage.push_back(std::move(_construct_directive));
    addConstructDirectiveImpl(_score, construct_directive);
  };
  std::vector<ScoredConstruct> *getConstructDirective() {
    return &construct_directives;
  };
  const std::vector<ScoredConstruct> &getConstructDirective() const {
    return construct_directives;
  }
  void setArchExpression(const char *_score, const char *_arch_expression);
  ScoredExpression *getArchExpression() {
    return getArchExpression(is_target_device_selector);
  }
  const ScoredExpression *getArchExpression() const {
    return getArchExpression(is_target_device_selector);
  }
  ScoredExpression *getArchExpression(bool target_device) {
    return &getDeviceSelectorData(target_device).arch_expression;
  }
  const ScoredExpression *getArchExpression(bool target_device) const {
    return &getDeviceSelectorData(target_device).arch_expression;
  }
  void setIsaExpression(const char *_score, const char *_isa_expression);
  ScoredExpression *getIsaExpression() {
    return getIsaExpression(is_target_device_selector);
  }
  const ScoredExpression *getIsaExpression() const {
    return getIsaExpression(is_target_device_selector);
  }
  ScoredExpression *getIsaExpression(bool target_device) {
    return &getDeviceSelectorData(target_device).isa_expression;
  }
  const ScoredExpression *getIsaExpression(bool target_device) const {
    return &getDeviceSelectorData(target_device).isa_expression;
  }
  void setContextKind(const char *_score,
                      OpenMPClauseContextKind _context_kind_name);
  ScoredContextKind *getContextKind() {
    return getContextKind(is_target_device_selector);
  }
  const ScoredContextKind *getContextKind() const {
    return getContextKind(is_target_device_selector);
  }
  ScoredContextKind *getContextKind(bool target_device) {
    return &getDeviceSelectorData(target_device).context_kind_name;
  }
  const ScoredContextKind *getContextKind(bool target_device) const {
    return &getDeviceSelectorData(target_device).context_kind_name;
  }
  void setDeviceNumExpression(const char *_score,
                              const char *_device_num_expression);
  ScoredExpression *getDeviceNumExpression() {
    return getDeviceNumExpression(is_target_device_selector);
  }
  const ScoredExpression *getDeviceNumExpression() const {
    return getDeviceNumExpression(is_target_device_selector);
  }
  ScoredExpression *getDeviceNumExpression(bool target_device) {
    return &getDeviceSelectorData(target_device).device_num_expression;
  }
  const ScoredExpression *getDeviceNumExpression(bool target_device) const {
    return &getDeviceSelectorData(target_device).device_num_expression;
  }
  void setExtensionExpression(const char *_score,
                              const char *_extension_expression);
  ScoredExpression *getExtensionExpression() { return &extension_expression; };
  const ScoredExpression *getExtensionExpression() const {
    return &extension_expression;
  }
  void setImplementationKind(const char *_score,
                             OpenMPClauseContextVendor _context_vendor_name);
  ScoredContextVendor *getImplementationKind() { return &context_vendor_name; };
  const ScoredContextVendor *getImplementationKind() const {
    return &context_vendor_name;
  }
  void setImplementationRequiresExpression(const char *_score,
                                           const char *args);
  void setImplementationUserExpression(const char *_score,
                                       const char *_implementation_expression);
  ImplementationExpression *getImplementationExpression() {
    return &implementation_user_defined_expression;
  };
  const ImplementationExpression *getImplementationExpression() const {
    return &implementation_user_defined_expression;
  }
  void setIsTargetDeviceSelector(bool _is_target_device) {
    is_target_device_selector = _is_target_device;
  };
  bool getIsTargetDeviceSelector() const { return is_target_device_selector; };
  void addSelectorKind(OpenMPContextSelectorSequenceKind kind) {
    selector_order.push_back(kind);
  };
  const std::vector<OpenMPContextSelectorSequenceKind> &
  getSelectorOrder() const {
    return selector_order;
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override;
  bool validateSelectorInvariants(std::vector<std::string> &errors) const;
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;

private:
  DeviceSelectorData &getDeviceSelectorData(bool target_device) {
    return target_device ? target_device_selector : device_selector;
  }
  const DeviceSelectorData &getDeviceSelectorData(bool target_device) const {
    return target_device ? target_device_selector : device_selector;
  }
  void addConstructDirectiveImpl(const char *_score,
                                 OpenMPDirective *_construct_directive);
};

// When Clause
class OpenMPWhenClause : public OpenMPVariantClause {
protected:
  OpenMPDirective *variant_directive =
      NULL; // variant directive inside the WHEN clause
  std::unique_ptr<OpenMPDirective> variant_directive_storage;

public:
  OpenMPWhenClause() : OpenMPVariantClause(OMPC_when) {};
  OpenMPDirective *getVariantDirective() { return variant_directive; };
  const OpenMPDirective *getVariantDirective() const {
    return variant_directive;
  }
  void
  setVariantDirective(std::unique_ptr<OpenMPDirective> _variant_directive) {
    variant_directive_storage = std::move(_variant_directive);
    variant_directive = variant_directive_storage.get();
  };
  void setVariantDirective(OpenMPDirective *_variant_directive) {
    variant_directive_storage.reset();
    variant_directive = _variant_directive;
  };

  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override;

  static OpenMPClause *addWhenClause(OpenMPDirective *directive);
};

// Otherwise Clause
class OpenMPOtherwiseClause : public OpenMPVariantClause {
protected:
  OpenMPDirective *variant_directive =
      NULL; // variant directive inside the OTHERWISE clause
  std::unique_ptr<OpenMPDirective> variant_directive_storage;

public:
  OpenMPOtherwiseClause() : OpenMPVariantClause(OMPC_otherwise) {};
  OpenMPDirective *getVariantDirective() { return variant_directive; };
  const OpenMPDirective *getVariantDirective() const {
    return variant_directive;
  }
  void
  setVariantDirective(std::unique_ptr<OpenMPDirective> _variant_directive) {
    variant_directive_storage = std::move(_variant_directive);
    variant_directive = variant_directive_storage.get();
  };
  void setVariantDirective(OpenMPDirective *_variant_directive) {
    variant_directive_storage.reset();
    variant_directive = _variant_directive;
  };

  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override;

  static OpenMPClause *addOtherwiseClause(OpenMPDirective *directive);
};

// Match Clause
class OpenMPMatchClause : public OpenMPVariantClause {
protected:
public:
  OpenMPMatchClause() : OpenMPVariantClause(OMPC_match) {};

  static OpenMPClause *addMatchClause(OpenMPDirective *directive);
};

// ProcBind Clause
class OpenMPProcBindClause : public OpenMPClause {

protected:
  OpenMPProcBindClauseKind proc_bind_kind; // proc_bind

public:
  OpenMPProcBindClause(OpenMPProcBindClauseKind _proc_bind_kind)
      : OpenMPClause(OMPC_proc_bind), proc_bind_kind(_proc_bind_kind) {};

  OpenMPProcBindClauseKind getProcBindClauseKind() { return proc_bind_kind; };
  static OpenMPClause *addProcBindClause(OpenMPDirective *,
                                         OpenMPProcBindClauseKind);
  std::string toString() override;
};

// Bind Clause
class OpenMPBindClause : public OpenMPClause {

protected:
  OpenMPBindClauseBinding bind_binding;

public:
  OpenMPBindClause(OpenMPBindClauseBinding _bind_binding)
      : OpenMPClause(OMPC_bind), bind_binding(_bind_binding) {};

  OpenMPBindClauseBinding getBindClauseBinding() { return bind_binding; };
  static OpenMPClause *addBindClause(OpenMPDirective *,
                                     OpenMPBindClauseBinding);
  std::string toString() override;
};

// Default Clause
class OpenMPDefaultClause : public OpenMPClause {

protected:
  OpenMPDefaultClauseKind default_kind = OMPC_DEFAULT_unknown;
  OpenMPDefaultmapClauseCategory category =
      OMPC_DEFAULTMAP_CATEGORY_unspecified;
  OpenMPDirective *variant_directive =
      NULL; // variant directive inside the DEFAULT clause
  std::unique_ptr<OpenMPDirective> variant_directive_storage;

public:
  OpenMPDefaultClause(OpenMPDefaultClauseKind _default_kind,
                      OpenMPDefaultmapClauseCategory _category =
                          OMPC_DEFAULTMAP_CATEGORY_unspecified)
      : OpenMPClause(OMPC_default), default_kind(_default_kind),
        category(_category) {};

  OpenMPDefaultClauseKind getDefaultClauseKind() const { return default_kind; };
  OpenMPDefaultmapClauseCategory getCategory() const { return category; }

  OpenMPDirective *getVariantDirective() { return variant_directive; };
  const OpenMPDirective *getVariantDirective() const {
    return variant_directive;
  }
  void
  setVariantDirective(std::unique_ptr<OpenMPDirective> _variant_directive) {
    variant_directive_storage = std::move(_variant_directive);
    variant_directive = variant_directive_storage.get();
  };
  void setVariantDirective(OpenMPDirective *_variant_directive) {
    variant_directive_storage.reset();
    variant_directive = _variant_directive;
  };

  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override;

  static OpenMPClause *addDefaultClause(OpenMPDirective *,
                                        OpenMPDefaultClauseKind,
                                        OpenMPDefaultmapClauseCategory);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// Order Clause
class OpenMPOrderClause : public OpenMPClause {

protected:
  OpenMPOrderClauseModifier order_modifier = OMPC_ORDER_MODIFIER_unspecified;
  OpenMPOrderClauseKind order_kind = OMPC_ORDER_unspecified;

public:
  OpenMPOrderClause(OpenMPOrderClauseModifier _order_modifier,
                    OpenMPOrderClauseKind _order_kind)
      : OpenMPClause(OMPC_order), order_modifier(_order_modifier),
        order_kind(_order_kind) {};

  OpenMPOrderClause(OpenMPOrderClauseKind _order_kind)
      : OpenMPClause(OMPC_order), order_kind(_order_kind) {};

  OpenMPOrderClauseModifier getOrderClauseModifier() { return order_modifier; };
  OpenMPOrderClauseKind getOrderClauseKind() { return order_kind; };
  void addOperand(const std::string &expr,
                  OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addLangExpr(expr.c_str(), sep, 0, 0, OMP_EXPR_PARSE_variable_list);
  }
  const std::vector<OpenMPExpressionItem> &getOperands() const {
    return expressions;
  }
  void clearOperands() { expressions.clear(); }

  static OpenMPClause *addOrderClause(OpenMPDirective *,
                                      OpenMPOrderClauseModifier,
                                      OpenMPOrderClauseKind);
  static OpenMPClause *addOrderClause(OpenMPDirective *, OpenMPOrderClauseKind);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// inclusive/exclusive scan clauses
class OpenMPScanClause : public OpenMPClause {
public:
  OpenMPScanClause(OpenMPClauseKind kind) : OpenMPClause(kind) {}

  void addOperand(const std::string &expr,
                  OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addLangExpr(expr.c_str(), sep, 0, 0, OMP_EXPR_PARSE_variable_list);
  }

  const std::vector<OpenMPExpressionItem> &getOperands() const {
    return expressions;
  }

  void clearOperands() { expressions.clear(); }

  std::string toString() override;
};

class OpenMPFirstprivateClause : public OpenMPClause {
  bool saved = false;
  bool has_directive_name_modifier = false;
  OpenMPDirectiveKind directive_name_modifier = OMPD_unknown;

public:
  OpenMPFirstprivateClause() : OpenMPClause(OMPC_firstprivate) {}

  void setSaved(bool value = true) { saved = value; }
  void setCurrentDirectiveNameModifier(OpenMPDirectiveKind value) {
    has_directive_name_modifier = true;
    directive_name_modifier = value;
  }
  void clearCurrentDirectiveNameModifier() {
    has_directive_name_modifier = false;
    directive_name_modifier = OMPD_unknown;
  }
  bool isSaved() const { return saved; }
  bool hasDirectiveNameModifier() const { return has_directive_name_modifier; }
  OpenMPDirectiveKind getDirectiveNameModifier() const {
    return directive_name_modifier;
  }

  std::string toString() override;
};

// if Clause
class OpenMPIfClause : public OpenMPClause {

protected:
  OpenMPIfClauseModifier modifier; // linear modifier
  ompparser::HostFragment user_defined_modifier;

public:
  OpenMPIfClause(OpenMPIfClauseModifier _modifier)
      : OpenMPClause(OMPC_if), modifier(_modifier) {};

  OpenMPIfClauseModifier getModifier() const { return modifier; };

  void setUserDefinedModifier(const char *_modifier);

  const std::string &getUserDefinedModifier() const {
    return user_defined_modifier.spelling;
  };

  static OpenMPClause *addIfClause(OpenMPDirective *directive,
                                   OpenMPIfClauseModifier modifier,
                                   const char *user_defined_modifier);

  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_modifier.spelling.empty()) {
      visitor(user_defined_modifier);
    }
    OpenMPClause::visitHostFragments(visitor);
  }

  std::string toString() override;

  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// in_reduction clause
class OpenMPInReductionClause : public OpenMPClause {

protected:
  OpenMPInReductionClauseIdentifier identifier =
      OMPC_IN_REDUCTION_IDENTIFIER_unknown; // identifier
  ompparser::HostFragment user_defined_identifier;

public:
  OpenMPInReductionClause() : OpenMPClause(OMPC_in_reduction) {}

  OpenMPInReductionClause(OpenMPInReductionClauseIdentifier _identifier)
      : OpenMPClause(OMPC_in_reduction), identifier(_identifier),
        user_defined_identifier() {};

  OpenMPInReductionClauseIdentifier getIdentifier() const {
    return identifier;
  };

  void setUserDefinedIdentifier(const char *_identifier);

  const std::string &getUserDefinedIdentifier() const {
    return user_defined_identifier.spelling;
  }
  void addOperand(const std::string &operand,
                  OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addLangExpr(operand.c_str(), sep, 0, 0, OMP_EXPR_PARSE_variable_list);
  }
  const std::vector<OpenMPExpressionItem> &getOperands() const {
    return expressions;
  }
  void clearOperands() { expressions.clear(); }

  static OpenMPClause *addInReductionClause(OpenMPDirective *,
                                            OpenMPInReductionClauseIdentifier,
                                            const char *);
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_identifier.spelling.empty()) {
      visitor(user_defined_identifier);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// depend clause
class OpenMPDependClause : public OpenMPClause {

protected:
  OpenMPDependClauseModifier modifier =
      OMPC_DEPEND_MODIFIER_unspecified;                       // modifier
  OpenMPDependClauseType type = OMPC_DEPENDENCE_TYPE_unknown; // type
  ompparser::HostFragment dependence_vector;
  std::vector<OpenMPIterator> iterators;

public:
  OpenMPDependClause() : OpenMPClause(OMPC_depend) {}

  OpenMPDependClause(OpenMPDependClauseModifier _modifier,
                     OpenMPDependClauseType _type)
      : OpenMPClause(OMPC_depend), modifier(_modifier), type(_type) {};
  OpenMPDependClauseModifier getModifier() const { return modifier; };
  OpenMPDependClauseType getType() const { return type; };
  void addDependenceVector(const char *_dependence_vector);
  const std::string &getDependenceVector() const {
    return dependence_vector.spelling;
  }
  void addIterator(const OpenMPIterator &it) { iterators.push_back(it); }
  const std::vector<OpenMPIterator> &getIterators() const { return iterators; }
  void clearIterators() { iterators.clear(); }
  void setDependIteratorsDefinitionClass(
      const std::vector<std::vector<const char *>> &definition_class) {
    iterators.clear();
    for (const auto &vec : definition_class) {
      if (vec.size() < 4) {
        continue;
      }
      OpenMPIterator it;
      it.set(
          vec[0] ? std::string(vec[0]) : "", vec[1] ? std::string(vec[1]) : "",
          vec[2] ? std::string(vec[2]) : "", vec[3] ? std::string(vec[3]) : "",
          (vec.size() > 4 && vec[4]) ? std::string(vec[4]) : "");
      iterators.push_back(it);
    }
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!dependence_vector.spelling.empty()) {
      visitor(dependence_vector);
    }
    for (OpenMPIterator &iterator : iterators) {
      iterator.visitHostFragments(visitor);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  static OpenMPClause *addDependClause(OpenMPDirective *,
                                       OpenMPDependClauseModifier,
                                       OpenMPDependClauseType);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// doacross clause (OpenMP 5.2)
class OpenMPDoacrossClause : public OpenMPClause {

protected:
  OpenMPDoacrossClauseType type = OMPC_DOACROSS_TYPE_unknown; // source or sink
  bool has_source_expr = false;
  OpenMPExpressionItem source_expr;
  std::vector<OpenMPExpressionItem> sink_args;

public:
  OpenMPDoacrossClause() : OpenMPClause(OMPC_doacross) {}

  OpenMPDoacrossClause(OpenMPDoacrossClauseType _type)
      : OpenMPClause(OMPC_doacross), type(_type) {};

  OpenMPDoacrossClauseType getType() const { return type; };
  void setSourceExpression(const std::string &expr,
                           OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_space) {
    has_source_expr = true;
    source_expr = OpenMPExpressionItem{expr, sep};
    addLangExpr(expr.c_str(), sep, 0, 0, OMP_EXPR_PARSE_expression);
  }
  bool hasSourceExpression() const { return has_source_expr; }
  const OpenMPExpressionItem &getSourceExpression() const {
    return source_expr;
  }
  void addSinkArg(const std::string &expr,
                  OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    sink_args.push_back(OpenMPExpressionItem{expr, sep});
    addLangExpr(expr.c_str(), sep, 0, 0, OMP_EXPR_PARSE_variable_list);
  }
  const std::vector<OpenMPExpressionItem> &getSinkArgs() const {
    return sink_args;
  }
  void clearSinkArgs() { sink_args.clear(); }

  std::string toString() override;
};

// affinity clause
class OpenMPAffinityClause : public OpenMPClause {

protected:
  OpenMPAffinityClauseModifier modifier =
      OMPC_AFFINITY_MODIFIER_unspecified; // modifier
  std::vector<OpenMPIterator> iterators;

public:
  OpenMPAffinityClause() : OpenMPClause(OMPC_affinity) {}

  OpenMPAffinityClause(OpenMPAffinityClauseModifier _modifier)
      : OpenMPClause(OMPC_affinity), modifier(_modifier) {};
  void addIteratorsDefinitionClass(
      const std::vector<const char *> &iterator_definition) {
    if (iterator_definition.size() < 4) {
      return;
    }
    OpenMPIterator it;
    it.set(iterator_definition[0] ? std::string(iterator_definition[0]) : "",
           iterator_definition[1] ? std::string(iterator_definition[1]) : "",
           iterator_definition[2] ? std::string(iterator_definition[2]) : "",
           iterator_definition[3] ? std::string(iterator_definition[3]) : "",
           (iterator_definition.size() > 4 && iterator_definition[4])
               ? std::string(iterator_definition[4])
               : "");
    iterators.push_back(it);
  };
  const std::vector<OpenMPIterator> &getIterators() const { return iterators; };
  const std::vector<OpenMPIterator> &getIteratorsDefinitionClass() const {
    return getIterators();
  };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (OpenMPIterator &iterator : iterators) {
      iterator.visitHostFragments(visitor);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  OpenMPAffinityClauseModifier getModifier() const { return modifier; };
  static OpenMPClause *addAffinityClause(OpenMPDirective *,
                                         OpenMPAffinityClauseModifier);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// atomic_default_mem_order clause
class OpenMPAtomicDefaultMemOrderClause : public OpenMPClause {

protected:
  OpenMPAtomicDefaultMemOrderClauseKind kind;

public:
  OpenMPAtomicDefaultMemOrderClause(OpenMPAtomicDefaultMemOrderClauseKind _kind)
      : OpenMPClause(OMPC_atomic_default_mem_order), kind(_kind) {};

  OpenMPAtomicDefaultMemOrderClauseKind getKind() const { return kind; };

  static OpenMPClause *
  addAtomicDefaultMemOrderClause(OpenMPDirective *directive,
                                 OpenMPAtomicDefaultMemOrderClauseKind kind);

  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// device clause
class OpenMPDeviceClause : public OpenMPClause {

protected:
  OpenMPDeviceClauseModifier modifier =
      OMPC_DEVICE_MODIFIER_unspecified; // modifier

public:
  OpenMPDeviceClause() : OpenMPClause(OMPC_device) {}

  OpenMPDeviceClause(OpenMPDeviceClauseModifier _modifier)
      : OpenMPClause(OMPC_device), modifier(_modifier) {};

  OpenMPDeviceClauseModifier getModifier() const { return modifier; };

  static OpenMPClause *addDeviceClause(OpenMPDirective *directive,
                                       OpenMPDeviceClauseModifier modifier);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// to clause
class OpenMPToClause : public OpenMPClause {

protected:
  OpenMPToClauseKind to_kind = OMPC_TO_unspecified;
  ompparser::HostFragment mapper_identifier;
  std::vector<OpenMPIterator> iterators;

public:
  OpenMPToClause() : OpenMPClause(OMPC_to) {}
  OpenMPToClause(OpenMPToClauseKind _to_kind)
      : OpenMPClause(OMPC_to), to_kind(_to_kind) {};
  OpenMPToClauseKind getKind() const { return to_kind; };
  void setMapperIdentifier(const char *_identifier);
  const std::string &getMapperIdentifier() const {
    return mapper_identifier.spelling;
  };
  std::shared_ptr<const ompparser::HostSemanticNode>
  getMapperIdentifierNode() const {
    return mapper_identifier.semantic;
  }
  void addIterator(const std::string &qualifier, const std::string &var,
                   const std::string &begin, const std::string &end,
                   const std::string &step = std::string()) {
    OpenMPIterator it;
    it.set(qualifier, var, begin, end, step);
    iterators.push_back(it);
  }
  const std::vector<OpenMPIterator> &getIterators() const { return iterators; }
  void clearIterators() { iterators.clear(); }
  void addItem(const char *expr,
               OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addLangExpr(expr, sep, 0, 0, OMP_EXPR_PARSE_array_section);
  }
  void addItem(const std::string &expr,
               OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addItem(expr.c_str(), sep);
  }
  const std::vector<OpenMPExpressionItem> &getItems() const {
    return expressions;
  }
  void clearItems() { expressions.clear(); }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (OpenMPIterator &iterator : iterators) {
      iterator.visitHostFragments(visitor);
    }
    if (!mapper_identifier.spelling.empty()) {
      visitor(mapper_identifier);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  static OpenMPClause *addToClause(OpenMPDirective *, OpenMPToClauseKind);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// from clause
class OpenMPFromClause : public OpenMPClause {

protected:
  OpenMPFromClauseKind from_kind = OMPC_FROM_unspecified;
  ompparser::HostFragment mapper_identifier;
  std::vector<OpenMPIterator> iterators;

public:
  OpenMPFromClause() : OpenMPClause(OMPC_from) {}
  OpenMPFromClause(OpenMPFromClauseKind _from_kind)
      : OpenMPClause(OMPC_from), from_kind(_from_kind) {};
  OpenMPFromClauseKind getKind() const { return from_kind; };

  void setMapperIdentifier(const char *_identifier);
  const std::string &getMapperIdentifier() const {
    return mapper_identifier.spelling;
  };
  std::shared_ptr<const ompparser::HostSemanticNode>
  getMapperIdentifierNode() const {
    return mapper_identifier.semantic;
  }
  void addIterator(const std::string &qualifier, const std::string &var,
                   const std::string &begin, const std::string &end,
                   const std::string &step = std::string()) {
    OpenMPIterator it;
    it.set(qualifier, var, begin, end, step);
    iterators.push_back(it);
  }
  const std::vector<OpenMPIterator> &getIterators() const { return iterators; }
  void clearIterators() { iterators.clear(); }
  void addItem(const char *expr,
               OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addLangExpr(expr, sep, 0, 0, OMP_EXPR_PARSE_array_section);
  }
  void addItem(const std::string &expr,
               OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addItem(expr.c_str(), sep);
  }
  const std::vector<OpenMPExpressionItem> &getItems() const {
    return expressions;
  }
  void clearItems() { expressions.clear(); }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (OpenMPIterator &iterator : iterators) {
      iterator.visitHostFragments(visitor);
    }
    if (!mapper_identifier.spelling.empty()) {
      visitor(mapper_identifier);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  static OpenMPClause *addFromClause(OpenMPDirective *, OpenMPFromClauseKind);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// defaultmap Clause
class OpenMPDefaultmapClause : public OpenMPClause {
protected:
  OpenMPDefaultmapClauseBehavior behavior; // defaultmap behavior
  OpenMPDefaultmapClauseCategory category; // defaultmap category

public:
  OpenMPDefaultmapClause(OpenMPDefaultmapClauseBehavior _behavior,
                         OpenMPDefaultmapClauseCategory _category)
      : OpenMPClause(OMPC_defaultmap), behavior(_behavior),
        category(_category) {};

  OpenMPDefaultmapClauseBehavior getBehavior() const { return behavior; };
  OpenMPDefaultmapClauseCategory getCategory() const { return category; };

  static OpenMPClause *addDefaultmapClause(OpenMPDirective *,
                                           OpenMPDefaultmapClauseBehavior,
                                           OpenMPDefaultmapClauseCategory);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// device type Clause
class OpenMPDeviceTypeClause : public OpenMPClause {

protected:
  OpenMPDeviceTypeClauseKind device_type_kind = OMPC_DEVICE_TYPE_unknown;

public:
  OpenMPDeviceTypeClause(OpenMPDeviceTypeClauseKind _device_type_kind)
      : OpenMPClause(OMPC_device_type), device_type_kind(_device_type_kind) {};

  OpenMPDeviceTypeClauseKind getDeviceTypeClauseKind() const {
    return device_type_kind;
  };

  static OpenMPClause *
  addDeviceTypeClause(OpenMPDirective *directive,
                      OpenMPDeviceTypeClauseKind devicetypeKind);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
class OpenMPTaskReductionClause : public OpenMPClause {
  // task reduction clause
protected:
  OpenMPTaskReductionClauseIdentifier identifier =
      OMPC_TASK_REDUCTION_IDENTIFIER_unknown; // identifier
  ompparser::HostFragment user_defined_identifier;

public:
  OpenMPTaskReductionClause() : OpenMPClause(OMPC_task_reduction) {}

  OpenMPTaskReductionClause(OpenMPTaskReductionClauseIdentifier _identifier)
      : OpenMPClause(OMPC_task_reduction), identifier(_identifier),
        user_defined_identifier() {};

  OpenMPTaskReductionClauseIdentifier getIdentifier() const {
    return identifier;
  };

  void setUserDefinedIdentifier(const char *_identifier);

  const std::string &getUserDefinedIdentifier() const {
    return user_defined_identifier.spelling;
  }
  void addOperand(const std::string &operand,
                  OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma) {
    addLangExpr(operand.c_str(), sep, 0, 0, OMP_EXPR_PARSE_variable_list);
  }
  const std::vector<OpenMPExpressionItem> &getOperands() const {
    return expressions;
  }
  void clearOperands() { expressions.clear(); }

  static OpenMPClause *
  addTaskReductionClause(OpenMPDirective *, OpenMPTaskReductionClauseIdentifier,
                         const char *);
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!user_defined_identifier.spelling.empty()) {
      visitor(user_defined_identifier);
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// map clause
class OpenMPMapClause : public OpenMPClause {
public:
  enum DistDataPolicyKind {
    DIST_DATA_duplicate,
    DIST_DATA_block,
    DIST_DATA_cyclic,
    DIST_DATA_unknown
  };
  struct DistDataPolicy {
    DistDataPolicyKind kind = DIST_DATA_unknown;
    ompparser::HostFragment argument;
  };

protected:
  OpenMPMapClauseModifier modifier1 = OMPC_MAP_MODIFIER_unspecified;
  OpenMPMapClauseModifier modifier2 = OMPC_MAP_MODIFIER_unspecified;
  OpenMPMapClauseModifier modifier3 = OMPC_MAP_MODIFIER_unspecified;
  OpenMPMapClauseType type = OMPC_MAP_TYPE_unknown;
  OpenMPMapClauseRefModifier ref_modifier = OMPC_MAP_REF_MODIFIER_unspecified;
  ompparser::HostFragment mapper_identifier;
  std::vector<OpenMPIterator> iterators;
  std::vector<std::vector<DistDataPolicy>> dist_data_policies;

  void addItemWithRange(const std::string &expr, OpenMPClauseSeparator sep,
                        const ompparser::SourceRange *source_range);

public:
  OpenMPMapClause() : OpenMPClause(OMPC_map) {}

  OpenMPMapClause(OpenMPMapClauseModifier _modifier1,
                  OpenMPMapClauseModifier _modifier2,
                  OpenMPMapClauseModifier _modifier3, OpenMPMapClauseType _type,
                  OpenMPMapClauseRefModifier _ref_modifier,
                  std::string _mapper_identifier)
      : OpenMPClause(OMPC_map), modifier1(_modifier1), modifier2(_modifier2),
        modifier3(_modifier3), type(_type), ref_modifier(_ref_modifier) {
    mapper_identifier.spelling = std::move(_mapper_identifier);
    mapper_identifier.role = ompparser::HostFragmentRole::Declarator;
  };

  OpenMPMapClauseModifier getModifier1() const { return modifier1; };
  OpenMPMapClauseModifier getModifier2() const { return modifier2; };
  OpenMPMapClauseModifier getModifier3() const { return modifier3; };
  OpenMPMapClauseType getType() const { return type; };
  OpenMPMapClauseRefModifier getRefModifier() const { return ref_modifier; }
  void setRefModifier(OpenMPMapClauseRefModifier value) {
    ref_modifier = value;
  }
  const std::string &getMapperIdentifier() const {
    return mapper_identifier.spelling;
  };
  void addIterator(const OpenMPIterator &it) { iterators.push_back(it); }
  void addIterator(const std::string &qualifier, const std::string &var,
                   const std::string &begin, const std::string &end,
                   const std::string &step = std::string()) {
    OpenMPIterator it;
    it.set(qualifier, var, begin, end, step);
    iterators.push_back(it);
  }
  const std::vector<OpenMPIterator> &getIterators() const { return iterators; }
  void clearIterators() { iterators.clear(); }
  void addItem(const char *expr,
               OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma);
  void addItem(const std::string &expr,
               OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_comma);
  const std::vector<OpenMPExpressionItem> &getItems() const {
    return expressions;
  }
  const std::vector<std::vector<DistDataPolicy>> &getDistDataPolicies() const {
    return dist_data_policies;
  }
  void clearItems() {
    expressions.clear();
    dist_data_policies.clear();
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (OpenMPIterator &iterator : iterators) {
      iterator.visitHostFragments(visitor);
    }
    if (!mapper_identifier.spelling.empty()) {
      visitor(mapper_identifier);
    }
    for (auto &policy_list : dist_data_policies) {
      for (DistDataPolicy &policy : policy_list) {
        if (!policy.argument.spelling.empty()) {
          visitor(policy.argument);
        }
      }
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  static OpenMPClause *addMapClause(OpenMPDirective *, OpenMPMapClauseModifier,
                                    OpenMPMapClauseModifier,
                                    OpenMPMapClauseModifier,
                                    OpenMPMapClauseType,
                                    OpenMPMapClauseRefModifier, std::string);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};

// declare target directive
class OpenMPDeclareTargetDirective : public OpenMPDirective {
protected:
  std::vector<ompparser::HostFragment> extended_list;

public:
  OpenMPDeclareTargetDirective() : OpenMPDirective(OMPD_declare_target) {};
  void addExtendedList(const char *_extended_list);
  const std::vector<ompparser::HostFragment> &getExtendedList() const {
    return extended_list;
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (ompparser::HostFragment &item : extended_list) {
      visitor(item);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// flush directive
class OpenMPFlushDirective : public OpenMPDirective {
protected:
  std::vector<ompparser::HostFragment> flush_list;

public:
  OpenMPFlushDirective() : OpenMPDirective(OMPD_flush) {};
  void addFlushList(const char *_flush_list);
  const std::vector<ompparser::HostFragment> &getFlushList() const {
    return flush_list;
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (ompparser::HostFragment &item : flush_list) {
      visitor(item);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};

// critical directive
class OpenMPCriticalDirective : public OpenMPDirective {

protected:
  ompparser::HostFragment critical_name;

public:
  OpenMPCriticalDirective() : OpenMPDirective(OMPD_critical) {}
  void setCriticalName(const char *_name) {
    critical_name.spelling =
        _name != nullptr ? std::string(_name) : std::string();
    critical_name.role = ompparser::HostFragmentRole::Declarator;
    critical_name.semantic.reset();
    openmpGetLexemeSourceRange(_name, critical_name.range);
  };
  const std::string &getCriticalName() const { return critical_name.spelling; };
  const ompparser::HostFragment &getCriticalNameFragment() const {
    return critical_name;
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!critical_name.spelling.empty()) {
      visitor(critical_name);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
};
// DepobjUpdate clause
class OpenMPDepobjUpdateClause : public OpenMPClause {

protected:
  OpenMPDepobjUpdateClauseDependeceType type =
      OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_unknown;

public:
  OpenMPDepobjUpdateClause() : OpenMPClause(OMPC_depobj_update) {}

  OpenMPDepobjUpdateClause(OpenMPDepobjUpdateClauseDependeceType _type)
      : OpenMPClause(OMPC_depobj_update), type(_type) {};

  OpenMPDepobjUpdateClauseDependeceType getType() const { return type; };
  static OpenMPClause *
  addDepobjUpdateClause(OpenMPDirective *,
                        OpenMPDepobjUpdateClauseDependeceType);
  std::string toString() override;
  void generateDOT(std::ostream &, int, int, std::string) const override;
};
// depobj directive
class OpenMPDepobjDirective : public OpenMPDirective {

protected:
  ompparser::HostFragment depobj;

public:
  OpenMPDepobjDirective() : OpenMPDirective(OMPD_depobj) {}
  void addDepobj(const char *_depobj);
  const std::string &getDepobj() const { return depobj.spelling; };
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    if (!depobj.spelling.empty()) {
      visitor(depobj);
    }
    OpenMPDirective::visitHostFragments(visitor);
  }
  void generateDOT(std::ostream &, int, int, std::string) const;
};
// ordered directive
class OpenMPOrderedDirective : public OpenMPDirective {

protected:
public:
  OpenMPOrderedDirective() : OpenMPDirective(OMPD_ordered) {}
  void generateDOT(std::ostream &, int, int, std::string) const;
};
// uses_allocators clause_parameters
class usesAllocatorParameter {
protected:
  OpenMPUsesAllocatorsClauseAllocator allocator;
  ompparser::HostFragment allocator_traits_array;
  ompparser::HostFragment allocator_user;

public:
  usesAllocatorParameter(OpenMPUsesAllocatorsClauseAllocator _allocator,
                         ompparser::HostFragment _allocator_traits_array,
                         ompparser::HostFragment _allocator_user)
      : allocator(_allocator),
        allocator_traits_array(std::move(_allocator_traits_array)),
        allocator_user(std::move(_allocator_user)) {};
  OpenMPUsesAllocatorsClauseAllocator getUsesAllocatorsAllocator() const {
    return allocator;
  };
  const std::string &getAllocatorTraitsArray() const {
    return allocator_traits_array.spelling;
  }
  const std::string &getAllocatorUser() const {
    return allocator_user.spelling;
  }
  void visitHostFragments(const ompparser::HostFragmentVisitor &visitor) {
    if (!allocator_traits_array.spelling.empty()) {
      visitor(allocator_traits_array);
    }
    if (!allocator_user.spelling.empty()) {
      visitor(allocator_user);
    }
  }
};
// uses_allocators clause
class OpenMPUsesAllocatorsClause : public OpenMPClause {
protected:
  // Owned storage for allocator parameters
  std::vector<std::unique_ptr<usesAllocatorParameter>>
      usesAllocatorsAllocatorSequenceStorage;
  // View for compatibility with existing code
  std::vector<usesAllocatorParameter *> usesAllocatorsAllocatorSequenceView;

public:
  OpenMPUsesAllocatorsClause() : OpenMPClause(OMPC_uses_allocators) {};
  void addUsesAllocatorsAllocatorSequence(
      OpenMPUsesAllocatorsClauseAllocator _allocator,
      const char *_allocator_traits_array, const char *_allocator_user);
  std::vector<usesAllocatorParameter *> *getUsesAllocatorsAllocatorSequence() {
    return &usesAllocatorsAllocatorSequenceView;
  };
  const std::vector<usesAllocatorParameter *> &
  getUsesAllocatorsAllocatorSequence() const {
    return usesAllocatorsAllocatorSequenceView;
  }
  void
  visitHostFragments(const ompparser::HostFragmentVisitor &visitor) override {
    for (usesAllocatorParameter *parameter :
         usesAllocatorsAllocatorSequenceView) {
      if (parameter != nullptr) {
        parameter->visitHostFragments(visitor);
      }
    }
    OpenMPClause::visitHostFragments(visitor);
  }
  static OpenMPClause *addUsesAllocatorsClause(OpenMPDirective *directive);
  std::string toString() override;
};

// absent clause
class OpenMPAbsentClause : public OpenMPClause {
protected:
  std::vector<OpenMPDirectiveKind> directive_list;

public:
  OpenMPAbsentClause() : OpenMPClause(OMPC_absent) {};

  void addDirective(OpenMPDirectiveKind kind) {
    directive_list.push_back(kind);
  }

  const std::vector<OpenMPDirectiveKind> &getDirectives() const {
    return directive_list;
  }

  std::string toString() override;
};

// contains clause
class OpenMPContainsClause : public OpenMPClause {
protected:
  std::vector<OpenMPDirectiveKind> directive_list;

public:
  OpenMPContainsClause() : OpenMPClause(OMPC_contains) {};

  void addDirective(OpenMPDirectiveKind kind) {
    directive_list.push_back(kind);
  }

  const std::vector<OpenMPDirectiveKind> &getDirectives() const {
    return directive_list;
  }

  std::string toString() override;
};

// graph_id clause
class OpenMPGraphIdClause : public OpenMPClause {
public:
  OpenMPGraphIdClause() : OpenMPClause(OMPC_graph_id) {}
  std::string toString() override;
};

// graph_reset clause
class OpenMPGraphResetClause : public OpenMPClause {
public:
  OpenMPGraphResetClause() : OpenMPClause(OMPC_graph_reset) {}
  std::string toString() override;
};

// transparent clause
class OpenMPTransparentClause : public OpenMPClause {
public:
  OpenMPTransparentClause() : OpenMPClause(OMPC_transparent) {}
  std::string toString() override;
};

// replayable clause
class OpenMPReplayableClause : public OpenMPClause {
public:
  OpenMPReplayableClause() : OpenMPClause(OMPC_replayable) {}
  std::string toString() override;
};

// threadset clause
class OpenMPThreadsetClause : public OpenMPClause {
public:
  OpenMPThreadsetClause() : OpenMPClause(OMPC_threadset) {}
  std::string toString() override;
};

// indirect clause
class OpenMPIndirectClause : public OpenMPClause {
public:
  OpenMPIndirectClause() : OpenMPClause(OMPC_indirect) {}
  std::string toString() override;
};

// local clause
class OpenMPLocalClause : public OpenMPClause {
public:
  OpenMPLocalClause() : OpenMPClause(OMPC_local) {}
  std::string toString() override;
};

// init_complete clause
class OpenMPInitCompleteClause : public OpenMPClause {
public:
  OpenMPInitCompleteClause() : OpenMPClause(OMPC_init_complete) {}
  std::string toString() override;
};

// safesync clause
class OpenMPSafesyncClause : public OpenMPClause {
public:
  OpenMPSafesyncClause() : OpenMPClause(OMPC_safesync) {}
  std::string toString() override;
};

// device_safesync clause
class OpenMPDeviceSafesyncClause : public OpenMPClause {
public:
  OpenMPDeviceSafesyncClause() : OpenMPClause(OMPC_device_safesync) {}
  std::string toString() override;
};

// memscope clause
class OpenMPMemscopeClause : public OpenMPClause {
  OpenMPMemscopeClauseKind scope = OMPC_MEMSCOPE_unknown;

public:
  OpenMPMemscopeClause() : OpenMPClause(OMPC_memscope) {}
  void setScope(OpenMPMemscopeClauseKind value) { scope = value; }
  OpenMPMemscopeClauseKind getScope() const { return scope; }
  std::string toString() override;
};

// looprange clause
class OpenMPLooprangeClause : public OpenMPClause {
public:
  OpenMPLooprangeClause() : OpenMPClause(OMPC_looprange) {}
  std::string toString() override;
};

// permutation clause
class OpenMPPermutationClause : public OpenMPClause {
public:
  OpenMPPermutationClause() : OpenMPClause(OMPC_permutation) {}
  std::string toString() override;
};

// counts clause
class OpenMPCountsClause : public OpenMPClause {
public:
  OpenMPCountsClause() : OpenMPClause(OMPC_counts) {}
  std::string toString() override;
};

// inductor clause
class OpenMPInductorClause : public OpenMPClause {
public:
  OpenMPInductorClause() : OpenMPClause(OMPC_inductor) {}
  std::string toString() override;
};

// collector clause
class OpenMPCollectorClause : public OpenMPClause {
public:
  OpenMPCollectorClause() : OpenMPClause(OMPC_collector) {}
  std::string toString() override;
};

// combiner clause
class OpenMPCombinerClause : public OpenMPClause {
public:
  OpenMPCombinerClause() : OpenMPClause(OMPC_combiner) {}
  std::string toString() override;
};

// no_openmp clause
class OpenMPNoOpenmpClause : public OpenMPClause {
public:
  OpenMPNoOpenmpClause() : OpenMPClause(OMPC_no_openmp) {}
  std::string toString() override;
};

// no_openmp_constructs clause
class OpenMPNoOpenmpConstructsClause : public OpenMPClause {
public:
  OpenMPNoOpenmpConstructsClause() : OpenMPClause(OMPC_no_openmp_constructs) {}
  std::string toString() override;
};

// no_openmp_routines clause
class OpenMPNoOpenmpRoutinesClause : public OpenMPClause {
public:
  OpenMPNoOpenmpRoutinesClause() : OpenMPClause(OMPC_no_openmp_routines) {}
  std::string toString() override;
};

// no_parallelism clause
class OpenMPNoParallelismClause : public OpenMPClause {
public:
  OpenMPNoParallelismClause() : OpenMPClause(OMPC_no_parallelism) {}
  std::string toString() override;
};

// nocontext clause
class OpenMPNocontextClause : public OpenMPClause {
public:
  OpenMPNocontextClause() : OpenMPClause(OMPC_nocontext) {}
  std::string toString() override;
};

// novariants clause
class OpenMPNovariantsClause : public OpenMPClause {
public:
  OpenMPNovariantsClause() : OpenMPClause(OMPC_novariants) {}
  std::string toString() override;
};

// enter clause
class OpenMPEnterClause : public OpenMPClause {
public:
  OpenMPEnterClause() : OpenMPClause(OMPC_enter) {}
  std::string toString() override;
};

// use clause
class OpenMPUseClause : public OpenMPClause {
public:
  OpenMPUseClause() : OpenMPClause(OMPC_use) {}
  std::string toString() override;
};

// holds clause
class OpenMPHoldsClause : public OpenMPClause {
public:
  OpenMPHoldsClause() : OpenMPClause(OMPC_holds) {}
  std::string toString() override;
};

#endif // OMPPARSER_OPENMPAST_H
