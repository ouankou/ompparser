/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "OpenMPIR.h"
#include "OpenMPParserInternal.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <utility>

extern thread_local bool clause_separator_comma;

namespace {

std::string trimWhitespace(const std::string &text) {
  const char *whitespace = " \t\n\r\f\v";
  const size_t begin = text.find_first_not_of(whitespace);
  if (begin == std::string::npos) {
    return std::string();
  }
  const size_t end = text.find_last_not_of(whitespace);
  return text.substr(begin, end - begin + 1);
}

std::string trimWhitespace(const std::string &text, size_t pos, size_t count) {
  if (pos >= text.size()) {
    return std::string();
  }
  const char *whitespace = " \t\n\r\f\v";
  size_t end_pos = pos + count;
  if (end_pos > text.size() || end_pos < pos) {
    end_pos = text.size();
  }
  size_t begin = text.find_first_not_of(whitespace, pos);
  if (begin == std::string::npos || begin >= end_pos) {
    return std::string();
  }
  size_t end = text.find_last_not_of(whitespace, end_pos - 1);
  return text.substr(begin, end - begin + 1);
}

void advanceSourcePosition(ompparser::SourcePosition &position,
                           const std::string &text, size_t begin, size_t end) {
  end = std::min(end, text.size());
  begin = std::min(begin, end);
  for (size_t index = begin; index < end; ++index) {
    ++position.offset;
    if (text[index] == '\n') {
      ++position.line;
      position.column = 1;
    } else {
      ++position.column;
    }
  }
}

void setFragmentSourceSubrange(ompparser::HostFragment &fragment,
                               const ompparser::SourceRange &source_range,
                               const std::string &source, size_t begin) {
  if (begin > source.size() ||
      fragment.spelling.size() > source.size() - begin ||
      source.compare(begin, fragment.spelling.size(), fragment.spelling) != 0) {
    return;
  }

  fragment.range.begin = source_range.begin;
  advanceSourcePosition(fragment.range.begin, source, 0, begin);
  fragment.range.end = fragment.range.begin;
  advanceSourcePosition(fragment.range.end, source, begin,
                        begin + fragment.spelling.size());
}

void trimHostFragment(ompparser::HostFragment &fragment,
                      bool has_source_range) {
  const std::string whitespace = " \t\n\r\f\v";
  const size_t begin = fragment.spelling.find_first_not_of(whitespace);
  const size_t end = fragment.spelling.find_last_not_of(whitespace);
  const size_t trimmed_begin =
      begin == std::string::npos ? fragment.spelling.size() : begin;
  const size_t trimmed_size =
      end == std::string::npos ? 0 : end - trimmed_begin + 1;

  if (has_source_range) {
    advanceSourcePosition(fragment.range.begin, fragment.spelling, 0,
                          trimmed_begin);
    fragment.range.end = fragment.range.begin;
    advanceSourcePosition(fragment.range.end, fragment.spelling, trimmed_begin,
                          trimmed_begin + trimmed_size);
  }
  fragment.spelling = fragment.spelling.substr(trimmed_begin, trimmed_size);
}

ompparser::HostFragment makeHostFragment(const char *spelling,
                                         ompparser::HostFragmentRole role) {
  ompparser::HostFragment fragment;
  if (spelling == nullptr) {
    return fragment;
  }
  fragment.spelling = spelling;
  fragment.role = role;
  const bool has_source_range =
      openmpGetLexemeSourceRange(spelling, fragment.range);
  if (role != ompparser::HostFragmentRole::Expression) {
    trimHostFragment(fragment, has_source_range);
  }
  if (!has_source_range) {
    fragment.range.begin.line =
        static_cast<uint32_t>(std::max(openmpGetCurrentTokenLine(), 0));
    fragment.range.begin.column =
        static_cast<uint32_t>(std::max(openmpGetCurrentTokenColumn(), 0));
    fragment.range.end = fragment.range.begin;
    fragment.range.end.column +=
        static_cast<uint32_t>(fragment.spelling.size());
  }
  return fragment;
}

} // namespace

namespace {

bool isIdentifierChar(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalnum(uch) != 0 || ch == '_';
}

std::vector<std::string> splitTopLevelCommaSeparated(const std::string &text) {
  std::vector<std::string> parts;
  std::string::size_type part_begin = 0;
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;

  for (std::string::size_type index = 0; index < text.size(); ++index) {
    const char ch = text[index];
    switch (ch) {
    case '(':
      ++paren_depth;
      break;
    case ')':
      --paren_depth;
      if (paren_depth < 0) {
        return {};
      }
      break;
    case '[':
      ++bracket_depth;
      break;
    case ']':
      --bracket_depth;
      if (bracket_depth < 0) {
        return {};
      }
      break;
    case '{':
      ++brace_depth;
      break;
    case '}':
      --brace_depth;
      if (brace_depth < 0) {
        return {};
      }
      break;
    case ',':
      if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
        parts.push_back(text.substr(part_begin, index - part_begin));
        part_begin = index + 1;
      }
      break;
    default:
      break;
    }
  }

  if (paren_depth != 0 || bracket_depth != 0 || brace_depth != 0) {
    return {};
  }

  parts.push_back(text.substr(part_begin));
  return parts;
}

bool hasIncompleteTrailingOperator(const std::string &expression) {
  if (expression.empty()) {
    return false;
  }

  const std::string::size_type end = expression.size() - 1;
  const char trailing = expression[end];
  switch (trailing) {
  case '+':
    return !(end > 0 && expression[end - 1] == '+');
  case '-':
    return !(end > 0 && expression[end - 1] == '-');
  case '.':
  case '*':
  case '/':
  case '%':
  case '&':
  case '|':
  case '^':
  case '!':
  case '~':
  case '=':
  case '<':
  case '>':
  case '?':
  case ':':
    return true;
  default:
    return false;
  }
}

bool isValidDistDataBaseExpression(const std::string &expression) {
  const std::string trimmed_expression = trimWhitespace(expression);
  if (trimmed_expression.empty()) {
    return false;
  }

  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  for (const char ch : trimmed_expression) {
    switch (ch) {
    case '(':
      ++paren_depth;
      break;
    case ')':
      --paren_depth;
      if (paren_depth < 0) {
        return false;
      }
      break;
    case '[':
      ++bracket_depth;
      break;
    case ']':
      --bracket_depth;
      if (bracket_depth < 0) {
        return false;
      }
      break;
    case '{':
      ++brace_depth;
      break;
    case '}':
      --brace_depth;
      if (brace_depth < 0) {
        return false;
      }
      break;
    default:
      break;
    }
  }

  if (paren_depth != 0 || bracket_depth != 0 || brace_depth != 0) {
    return false;
  }

  const char trailing = trimmed_expression.back();
  if (trailing == ',' || trailing == '(' || trailing == '[' ||
      trailing == '{') {
    return false;
  }
  if (hasIncompleteTrailingOperator(trimmed_expression)) {
    return false;
  }

  return true;
}

bool splitMapExpressionDistDataSuffix(const std::string &expression,
                                      std::string *array_section_expression,
                                      std::string *dist_data_arguments) {
  if (array_section_expression == nullptr || dist_data_arguments == nullptr) {
    return false;
  }

  const std::string trimmed_expression = trimWhitespace(expression);
  *array_section_expression = trimmed_expression;
  dist_data_arguments->clear();
  if (trimmed_expression.empty() || trimmed_expression.back() != ')') {
    return false;
  }

  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;
  std::string::size_type open_paren_pos = std::string::npos;
  for (std::string::size_type i = trimmed_expression.size(); i-- > 0;) {
    const char ch = trimmed_expression[i];
    switch (ch) {
    case ')':
      ++paren_depth;
      break;
    case '(':
      --paren_depth;
      if (paren_depth < 0) {
        return false;
      }
      if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
        open_paren_pos = i;
      }
      break;
    case ']':
      ++bracket_depth;
      break;
    case '[':
      --bracket_depth;
      if (bracket_depth < 0) {
        return false;
      }
      break;
    case '}':
      ++brace_depth;
      break;
    case '{':
      --brace_depth;
      if (brace_depth < 0) {
        return false;
      }
      break;
    default:
      break;
    }

    if (open_paren_pos != std::string::npos) {
      break;
    }
  }

  if (open_paren_pos == std::string::npos || paren_depth != 0 ||
      bracket_depth != 0 || brace_depth != 0) {
    return false;
  }

  const std::string prefix =
      trimWhitespace(trimmed_expression, 0, open_paren_pos);
  if (prefix.empty()) {
    return false;
  }

  std::string::size_type token_end = prefix.size();
  while (token_end > 0 &&
         std::isspace(static_cast<unsigned char>(prefix[token_end - 1]))) {
    --token_end;
  }
  std::string::size_type token_begin = token_end;
  while (token_begin > 0 && isIdentifierChar(prefix[token_begin - 1])) {
    --token_begin;
  }
  if (token_begin == token_end) {
    return false;
  }
  if (token_begin > 0) {
    const char boundary = prefix[token_begin - 1];
    if (boundary == '.' || boundary == '>' || boundary == ':') {
      return false;
    }
  }

  std::string token = prefix.substr(token_begin, token_end - token_begin);
  std::transform(
      token.begin(), token.end(), token.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (token != "dist_data") {
    return false;
  }

  const std::string base_expression = trimWhitespace(prefix, 0, token_begin);
  if (!isValidDistDataBaseExpression(base_expression)) {
    return false;
  }

  *array_section_expression = base_expression;
  *dist_data_arguments =
      trimWhitespace(trimmed_expression, open_paren_pos + 1,
                     trimmed_expression.size() - open_paren_pos - 2);
  return true;
}

bool hasRawStringPrefixAt(const std::string &text, std::string::size_type index,
                          std::string::size_type *delimiter_begin) {
  if (index >= text.size()) {
    return false;
  }

  if (index > 0 && isIdentifierChar(text[index - 1])) {
    return false;
  }

  if (text[index] == 'R' && index + 1 < text.size() && text[index + 1] == '"') {
    *delimiter_begin = index + 2;
    return true;
  }

  if (index + 2 < text.size() && text[index + 2] == '"' &&
      (text[index] == 'u' || text[index] == 'U' || text[index] == 'L') &&
      text[index + 1] == 'R') {
    *delimiter_begin = index + 3;
    return true;
  }

  if (index + 3 < text.size() && text.compare(index, 4, "u8R\"") == 0) {
    *delimiter_begin = index + 4;
    return true;
  }

  return false;
}

bool skipRawStringLiteral(const std::string &text,
                          std::string::size_type &index) {
  std::string::size_type delimiter_begin = 0;
  if (!hasRawStringPrefixAt(text, index, &delimiter_begin)) {
    return false;
  }

  std::string::size_type open_paren = delimiter_begin;
  while (open_paren < text.size() && text[open_paren] != '(') {
    ++open_paren;
  }

  if (open_paren >= text.size()) {
    return false;
  }

  const std::string delimiter =
      text.substr(delimiter_begin, open_paren - delimiter_begin);
  if (delimiter.size() > 16) {
    return false;
  }
  for (char delimiter_char : delimiter) {
    if (delimiter_char == '(' || delimiter_char == ')' ||
        delimiter_char == '\\' ||
        std::isspace(static_cast<unsigned char>(delimiter_char)) != 0) {
      return false;
    }
  }

  const std::string end_marker = ")" + delimiter + "\"";
  const std::string::size_type end_pos = text.find(end_marker, open_paren + 1);
  if (end_pos == std::string::npos) {
    index = text.size();
    return true;
  }

  index = end_pos + end_marker.size();
  return true;
}

bool skipQuotedLiteral(const std::string &text, std::string::size_type &index) {
  if (index >= text.size()) {
    return false;
  }

  if (skipRawStringLiteral(text, index)) {
    return true;
  }

  const char quote = text[index];
  if (quote != '\'' && quote != '"') {
    return false;
  }

  ++index;
  while (index < text.size()) {
    if (text[index] == '\\') {
      index += (index + 1 < text.size()) ? 2 : 1;
      continue;
    }
    if (text[index] == quote) {
      ++index;
      break;
    }
    ++index;
  }

  return true;
}

bool skipComment(const std::string &text, std::string::size_type &index) {
  if (index + 1 >= text.size() || text[index] != '/') {
    return false;
  }

  const char next = text[index + 1];
  if (next == '/') {
    const std::string::size_type end_pos = text.find('\n', index + 2);
    index = end_pos == std::string::npos ? text.size() : end_pos;
    return true;
  }

  if (next == '*') {
    const std::string::size_type end_pos = text.find("*/", index + 2);
    index = end_pos == std::string::npos ? text.size() : end_pos + 2;
    return true;
  }

  return false;
}

bool isArraySectionDesignator(const std::string &expression_text) {
  int bracket_depth = 0;
  int paren_depth = 0;
  int question_mark_depth = 0;
  bool saw_array_section_colon = false;
  bool saw_square_bracket = false;

  std::string::size_type i = 0;
  while (i < expression_text.size()) {
    if (skipQuotedLiteral(expression_text, i)) {
      continue;
    }
    if (skipComment(expression_text, i)) {
      continue;
    }

    const char ch = expression_text[i];

    if (ch == '[') {
      saw_square_bracket = true;
      ++bracket_depth;
      ++i;
      continue;
    }
    if (ch == ']') {
      if (bracket_depth <= 0) {
        return false;
      }
      --bracket_depth;
      ++i;
      continue;
    }
    if (ch == '(') {
      ++paren_depth;
      ++i;
      continue;
    }
    if (ch == ')') {
      if (paren_depth <= 0) {
        return false;
      }
      --paren_depth;
      ++i;
      continue;
    }

    const bool in_bracket_scope = bracket_depth > 0;
    const bool in_fortran_paren_scope =
        !saw_square_bracket && bracket_depth == 0 && paren_depth > 0;
    if (!in_bracket_scope && !in_fortran_paren_scope) {
      if (std::isspace(static_cast<unsigned char>(ch)) != 0 ||
          isIdentifierChar(ch) || ch == '.') {
        ++i;
        continue;
      }

      if (ch == ':' && i + 1 < expression_text.size() &&
          expression_text[i + 1] == ':') {
        i += 2;
        continue;
      }

      if (ch == '-' && i + 1 < expression_text.size() &&
          expression_text[i + 1] == '>') {
        i += 2;
        continue;
      }

      if (!saw_square_bracket && ch == '%') {
        ++i;
        continue;
      }

      return false;
    }

    if (ch == '?') {
      ++question_mark_depth;
      ++i;
      continue;
    }

    if (ch == ':') {
      if (i + 1 < expression_text.size() && expression_text[i + 1] == ':') {
        i += 2;
        continue;
      }

      if (question_mark_depth > 0) {
        --question_mark_depth;
      } else {
        saw_array_section_colon = true;
      }
      ++i;
      continue;
    }

    ++i;
  }

  if (bracket_depth != 0 || paren_depth != 0 || question_mark_depth != 0) {
    return false;
  }

  return saw_array_section_colon;
}

OpenMPExprParseMode
resolveClauseExpressionParseMode(OpenMPClauseKind clause_kind,
                                 OpenMPExprParseMode parse_mode,
                                 const std::string &normalized_expression) {
  if (parse_mode != OMP_EXPR_PARSE_variable_list) {
    return parse_mode;
  }

  if (clause_kind == OMPC_depend || clause_kind == OMPC_affinity) {
    return isArraySectionDesignator(normalized_expression)
               ? OMP_EXPR_PARSE_array_section
               : OMP_EXPR_PARSE_expression;
  }

  return parse_mode;
}

} // namespace

void OpenMPApplyClause::setLabel(const char *value) {
  label = makeHostFragment(value, ompparser::HostFragmentRole::Declarator);
}

void OpenMPApplyClause::addTransformation(OpenMPApplyTransformKind kind,
                                          const char *argument,
                                          OpenMPClauseSeparator sep) {
  ApplyTransform t;
  t.kind = kind;
  t.argument =
      makeHostFragment(argument, ompparser::HostFragmentRole::Expression);
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
  if (clause != nullptr) {
    clause->setDirectiveKind(this->kind);
  }
  OpenMPClause *raw_ptr = clause.get();
  clause_storage.push_back(std::move(clause));
  return raw_ptr;
}

void OpenMPDirective::adoptClausesFrom(OpenMPDirective &source) {
  for (auto &entry : source.clauses) {
    auto &destination = clauses[entry.first];
    destination.insert(destination.end(), entry.second.begin(),
                       entry.second.end());
  }
  source.clauses.clear();

  for (OpenMPClause *clause : source.clauses_in_original_order) {
    if (clause == nullptr ||
        std::find(clauses_in_original_order.begin(),
                  clauses_in_original_order.end(),
                  clause) != clauses_in_original_order.end()) {
      continue;
    }
    clause->setClausePosition(
        static_cast<int>(clauses_in_original_order.size()));
    clauses_in_original_order.push_back(clause);
  }
  source.clauses_in_original_order.clear();

  for (std::unique_ptr<OpenMPClause> &clause : source.clause_storage) {
    clause_storage.push_back(std::move(clause));
  }
  source.clause_storage.clear();
}

bool OpenMPDirective::validateInvariants(
    std::vector<std::string> &errors) const {
  errors.insert(errors.end(), construction_errors.begin(),
                construction_errors.end());
  std::vector<const OpenMPClause *> owned;
  owned.reserve(clause_storage.size());
  for (const auto &clause : clause_storage) {
    if (!clause) {
      errors.push_back("directive owns a null clause");
      continue;
    }
    if (std::find(owned.begin(), owned.end(), clause.get()) != owned.end()) {
      errors.push_back("directive owns the same clause more than once");
      continue;
    }
    owned.push_back(clause.get());
  }

  for (const auto &entry : clauses) {
    std::vector<const OpenMPClause *> indexed;
    for (const OpenMPClause *clause : entry.second) {
      if (!clause) {
        errors.push_back("clause index contains a null entry");
        continue;
      }
      if (clause->getKind() != entry.first) {
        errors.push_back("clause index kind does not match clause payload");
      }
      if (std::find(owned.begin(), owned.end(), clause) == owned.end()) {
        errors.push_back("clause index references an unowned clause");
      }
      if (std::find(indexed.begin(), indexed.end(), clause) != indexed.end()) {
        errors.push_back("clause index contains a duplicate pointer");
      }
      indexed.push_back(clause);
    }
  }

  std::vector<const OpenMPClause *> ordered;
  for (std::size_t index = 0; index < clauses_in_original_order.size();
       ++index) {
    const OpenMPClause *clause = clauses_in_original_order[index];
    if (!clause) {
      errors.push_back("source-order sequence contains a null clause");
      continue;
    }
    if (std::find(owned.begin(), owned.end(), clause) == owned.end()) {
      errors.push_back("source-order sequence references an unowned clause");
    }
    if (std::find(ordered.begin(), ordered.end(), clause) != ordered.end()) {
      errors.push_back("source-order sequence contains a duplicate pointer");
    }
    if (clause->getClausePosition() != static_cast<int>(index)) {
      errors.push_back("clause position does not match source-order index");
    }
    const auto kind_iter = clauses.find(clause->getKind());
    if (kind_iter == clauses.end() ||
        std::find(kind_iter->second.begin(), kind_iter->second.end(), clause) ==
            kind_iter->second.end()) {
      errors.push_back("source-order clause is absent from its kind index");
    }
    ordered.push_back(clause);
  }

  for (const OpenMPClause *clause : owned) {
    if (std::find(ordered.begin(), ordered.end(), clause) == ordered.end()) {
      errors.push_back("owned clause is absent from source order");
    }
  }

  return errors.empty();
}

void OpenMPDeclareReductionDirective::setCombiner(const char *_combiner) {
  combiner =
      makeHostFragment(_combiner, ompparser::HostFragmentRole::Expression);
  ompparser::SourceRange source_range;
  const bool has_source_range =
      _combiner != nullptr &&
      openmpGetLexemeSourceRange(_combiner, source_range);
  trimHostFragment(combiner, has_source_range);
}

void OpenMPDeclareVariantDirective::setVariantFuncID(const char *identifier) {
  variant_func_id =
      makeHostFragment(identifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPAllocateDirective::addAllocateList(const char *item) {
  if (item != nullptr) {
    allocate_list.push_back(
        makeHostFragment(item, ompparser::HostFragmentRole::Variable));
  }
}

void OpenMPThreadprivateDirective::addThreadprivateList(const char *item) {
  if (item != nullptr) {
    threadprivate_list.push_back(
        makeHostFragment(item, ompparser::HostFragmentRole::Variable));
  }
}

void OpenMPGroupprivateDirective::addGroupprivateList(const char *item) {
  if (item != nullptr) {
    groupprivate_list.push_back(
        makeHostFragment(item, ompparser::HostFragmentRole::Variable));
  }
}

void OpenMPDeclareSimdDirective::addProcName(const char *name) {
  proc_name = makeHostFragment(name, ompparser::HostFragmentRole::Declarator);
}

void OpenMPDeclareTargetDirective::addExtendedList(const char *item) {
  if (item != nullptr) {
    extended_list.push_back(
        makeHostFragment(item, ompparser::HostFragmentRole::Variable));
  }
}

void OpenMPFlushDirective::addFlushList(const char *item) {
  if (item != nullptr) {
    flush_list.push_back(
        makeHostFragment(item, ompparser::HostFragmentRole::Variable));
  }
}

void OpenMPDepobjDirective::addDepobj(const char *item) {
  depobj = makeHostFragment(item, ompparser::HostFragmentRole::Variable);
}

void OpenMPDependClause::addDependenceVector(const char *dependence) {
  dependence_vector =
      makeHostFragment(dependence, ompparser::HostFragmentRole::Expression);
}

void OpenMPReductionClause::setUserDefinedIdentifier(const char *identifier) {
  user_defined_identifier =
      makeHostFragment(identifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPReductionClause::setUserDefinedModifier(const char *modifier) {
  user_defined_modifier =
      makeHostFragment(modifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPIfClause::setUserDefinedModifier(const char *modifier) {
  user_defined_modifier =
      makeHostFragment(modifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPInReductionClause::setUserDefinedIdentifier(const char *identifier) {
  user_defined_identifier =
      makeHostFragment(identifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPTaskReductionClause::setUserDefinedIdentifier(
    const char *identifier) {
  user_defined_identifier =
      makeHostFragment(identifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPAllocateClause::setUserDefinedAllocator(const char *_allocator) {
  if (_allocator == nullptr) {
    return;
  }
  const std::string value(_allocator);
  if (user_defined_allocator.spelling.empty()) {
    user_defined_allocator =
        makeHostFragment(_allocator, ompparser::HostFragmentRole::Expression);
    return;
  }
  const auto existing = std::find_if(
      extra_allocator_parameters.begin(), extra_allocator_parameters.end(),
      [&value](const ompparser::HostFragment &parameter) {
        return parameter.spelling == value;
      });
  if (value != user_defined_allocator.spelling &&
      existing == extra_allocator_parameters.end()) {
    addExtraAllocatorParameter(_allocator);
  }
}

void OpenMPAllocateClause::addExtraAllocatorParameter(const char *parameter) {
  if (parameter != nullptr) {
    extra_allocator_parameters.push_back(
        makeHostFragment(parameter, ompparser::HostFragmentRole::Expression));
  }
}

void OpenMPAllocatorClause::setUserDefinedAllocator(const char *allocator) {
  user_defined_allocator =
      makeHostFragment(allocator, ompparser::HostFragmentRole::Expression);
}

void OpenMPLinearClause::setUserDefinedStep(const char *step) {
  user_defined_step =
      makeHostFragment(step, ompparser::HostFragmentRole::Expression);
}

void OpenMPAlignedClause::setUserDefinedAlignment(const char *alignment) {
  user_defined_alignment =
      makeHostFragment(alignment, ompparser::HostFragmentRole::Expression);
}

void OpenMPDistScheduleClause::setChunkSize(const char *size) {
  chunk_size = makeHostFragment(size, ompparser::HostFragmentRole::Expression);
}

void OpenMPScheduleClause::setUserDefinedKind(const char *kind) {
  user_defined_kind =
      makeHostFragment(kind, ompparser::HostFragmentRole::Declarator);
}

void OpenMPScheduleClause::setChunkSize(const char *size) {
  chunk_size = makeHostFragment(size, ompparser::HostFragmentRole::Expression);
}

void OpenMPToClause::setMapperIdentifier(const char *_identifier) {
  if (_identifier == nullptr) {
    mapper_identifier = {};
    return;
  }
  mapper_identifier =
      makeHostFragment(_identifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPFromClause::setMapperIdentifier(const char *_identifier) {
  if (_identifier == nullptr) {
    mapper_identifier = {};
    return;
  }
  mapper_identifier =
      makeHostFragment(_identifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPDeclareReductionDirective::addTypenameList(
    const char *_typename_list) {
  if (_typename_list == nullptr) {
    return;
  }
  typename_list.push_back(
      makeHostFragment(_typename_list, ompparser::HostFragmentRole::Type));
}

void OpenMPInitializerClause::setUserDefinedPriv(const char *_priv) {
  if (_priv != nullptr && expressions.empty()) {
    addLangExpr(_priv, OMPC_CLAUSE_SEP_space, 0, 0, OMP_EXPR_PARSE_expression);
  }
}

void OpenMPDeclareMapperDirective::setUserDefinedIdentifier(
    const char *_user_defined_identifier) {
  user_defined_identifier = makeHostFragment(
      _user_defined_identifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPDeclareMapperDirective::setDeclareMapperType(
    const char *_declare_mapper_type) {
  type =
      makeHostFragment(_declare_mapper_type, ompparser::HostFragmentRole::Type);
}

void OpenMPDeclareMapperDirective::setDeclareMapperVar(
    const char *_declare_mapper_variable) {
  var = makeHostFragment(_declare_mapper_variable,
                         ompparser::HostFragmentRole::Variable);
}

void OpenMPVariantClause::setUserCondition(const char *score,
                                           const char *condition_expression) {
  ++user_condition_count;
  user_condition_expression.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  user_condition_expression.expression = makeHostFragment(
      condition_expression, ompparser::HostFragmentRole::Expression);
}

void OpenMPVariantClause::addConstructDirectiveImpl(
    const char *score, OpenMPDirective *construct_directive) {
  ScoredConstruct construct;
  construct.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  construct.directive = construct_directive;
  construct_directives.push_back(std::move(construct));
}

void OpenMPVariantClause::setArchExpression(const char *score,
                                            const char *expression) {
  DeviceSelectorData &selector =
      getDeviceSelectorData(is_target_device_selector);
  ++selector.arch_expression_count;
  selector.arch_expression.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  selector.arch_expression.expression =
      makeHostFragment(expression, ompparser::HostFragmentRole::Verbatim);
}

void OpenMPVariantClause::setIsaExpression(const char *score,
                                           const char *expression) {
  DeviceSelectorData &selector =
      getDeviceSelectorData(is_target_device_selector);
  ++selector.isa_expression_count;
  selector.isa_expression.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  selector.isa_expression.expression =
      makeHostFragment(expression, ompparser::HostFragmentRole::Verbatim);
}

void OpenMPVariantClause::setContextKind(const char *score,
                                         OpenMPClauseContextKind context_kind) {
  DeviceSelectorData &selector =
      getDeviceSelectorData(is_target_device_selector);
  ++selector.context_kind_count;
  selector.context_kind_name.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  selector.context_kind_name.kind = context_kind;
}

void OpenMPVariantClause::setDeviceNumExpression(const char *score,
                                                 const char *expression) {
  DeviceSelectorData &selector =
      getDeviceSelectorData(is_target_device_selector);
  ++selector.device_num_expression_count;
  selector.device_num_expression.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  selector.device_num_expression.expression =
      makeHostFragment(expression, ompparser::HostFragmentRole::Expression);
}

void OpenMPVariantClause::setExtensionExpression(const char *score,
                                                 const char *expression) {
  ++extension_expression_count;
  extension_expression.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  extension_expression.expression =
      makeHostFragment(expression, ompparser::HostFragmentRole::Verbatim);
}

void OpenMPVariantClause::setImplementationKind(
    const char *score, OpenMPClauseContextVendor context_vendor) {
  ++implementation_kind_count;
  context_vendor_name.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  context_vendor_name.vendor = context_vendor;
}

void OpenMPVariantClause::setImplementationRequiresExpression(
    const char *score, const char *expression) {
  ++implementation_expression_count;
  implementation_user_defined_expression.kind = OMPC_IMPL_EXPR_requires;
  implementation_user_defined_expression.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  implementation_user_defined_expression.expression =
      makeHostFragment(expression, ompparser::HostFragmentRole::Verbatim);
}

void OpenMPVariantClause::setImplementationUserExpression(
    const char *score, const char *expression) {
  ++implementation_expression_count;
  implementation_user_defined_expression.kind = OMPC_IMPL_EXPR_user;
  implementation_user_defined_expression.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression);
  implementation_user_defined_expression.expression =
      makeHostFragment(expression, ompparser::HostFragmentRole::Verbatim);
}

void OpenMPVariantClause::visitHostFragments(
    const ompparser::HostFragmentVisitor &visitor) {
  auto visit_scored_expression = [&visitor](ScoredExpression &expression) {
    if (!expression.score.spelling.empty()) {
      visitor(expression.score);
    }
    if (!expression.expression.spelling.empty()) {
      visitor(expression.expression);
    }
  };
  visit_scored_expression(user_condition_expression);
  for (ScoredConstruct &construct : construct_directives) {
    if (!construct.score.spelling.empty()) {
      visitor(construct.score);
    }
    if (construct.directive != nullptr) {
      construct.directive->visitHostFragments(visitor);
    }
  }
  auto visit_device_selector = [&](DeviceSelectorData &selector) {
    visit_scored_expression(selector.arch_expression);
    visit_scored_expression(selector.isa_expression);
    if (!selector.context_kind_name.score.spelling.empty()) {
      visitor(selector.context_kind_name.score);
    }
    visit_scored_expression(selector.device_num_expression);
  };
  visit_device_selector(device_selector);
  visit_device_selector(target_device_selector);
  visit_scored_expression(extension_expression);
  if (!context_vendor_name.score.spelling.empty()) {
    visitor(context_vendor_name.score);
  }
  if (!implementation_user_defined_expression.score.spelling.empty()) {
    visitor(implementation_user_defined_expression.score);
  }
  if (!implementation_user_defined_expression.expression.spelling.empty()) {
    visitor(implementation_user_defined_expression.expression);
  }
  OpenMPClause::visitHostFragments(visitor);
}

void OpenMPWhenClause::visitHostFragments(
    const ompparser::HostFragmentVisitor &visitor) {
  OpenMPVariantClause::visitHostFragments(visitor);
  if (variant_directive != nullptr) {
    variant_directive->visitHostFragments(visitor);
  }
}

void OpenMPOtherwiseClause::visitHostFragments(
    const ompparser::HostFragmentVisitor &visitor) {
  OpenMPVariantClause::visitHostFragments(visitor);
  if (variant_directive != nullptr) {
    variant_directive->visitHostFragments(visitor);
  }
}

void OpenMPDefaultClause::visitHostFragments(
    const ompparser::HostFragmentVisitor &visitor) {
  OpenMPClause::visitHostFragments(visitor);
  if (variant_directive != nullptr) {
    variant_directive->visitHostFragments(visitor);
  }
}

bool OpenMPVariantClause::validateSelectorInvariants(
    std::vector<std::string> &errors) const {
  const std::size_t initial_error_count = errors.size();
  auto require_unique = [&errors](std::size_t count, const char *name) {
    if (count > 1) {
      errors.push_back(std::string("duplicate '") + name + "' trait selector");
    }
  };
  require_unique(user_condition_count, "condition");
  require_unique(extension_expression_count, "extension");
  require_unique(implementation_kind_count, "vendor");
  require_unique(implementation_expression_count, "implementation-defined");

  std::vector<OpenMPContextSelectorSequenceKind> selector_sets;
  for (OpenMPContextSelectorSequenceKind selector : selector_order) {
    if (std::find(selector_sets.begin(), selector_sets.end(), selector) !=
        selector_sets.end()) {
      errors.push_back("duplicate trait-set selector");
    } else {
      selector_sets.push_back(selector);
    }
  }

  std::vector<OpenMPDirectiveKind> construct_kinds;
  for (const ScoredConstruct &construct : construct_directives) {
    if (!construct.score.spelling.empty()) {
      errors.push_back(
          "trait scores are not permitted in the construct selector set");
    }
    if (construct.directive == nullptr) {
      errors.push_back("construct selector contains a null directive");
      continue;
    }
    const OpenMPDirectiveKind kind = construct.directive->getKind();
    if (std::find(construct_kinds.begin(), construct_kinds.end(), kind) !=
        construct_kinds.end()) {
      errors.push_back("duplicate construct trait selector");
    } else {
      construct_kinds.push_back(kind);
    }
  }

  auto validate_device_selector = [&](const DeviceSelectorData &selector,
                                      bool is_target_device) {
    require_unique(selector.arch_expression_count, "arch");
    require_unique(selector.isa_expression_count, "isa");
    require_unique(selector.context_kind_count, "kind");
    require_unique(selector.device_num_expression_count, "device_num");
    if (!selector.arch_expression.score.spelling.empty() ||
        !selector.isa_expression.score.spelling.empty() ||
        !selector.context_kind_name.score.spelling.empty() ||
        !selector.device_num_expression.score.spelling.empty()) {
      errors.push_back(
          "trait scores are not permitted in device selector sets");
    }
    if (selector.context_kind_name.kind == OMPC_CONTEXT_KIND_any &&
        (!selector.arch_expression.expression.spelling.empty() ||
         !selector.isa_expression.expression.spelling.empty() ||
         !selector.device_num_expression.expression.spelling.empty())) {
      errors.push_back(
          "kind(any) excludes other properties in the same selector set");
    }
    if (!is_target_device && selector.device_num_expression_count != 0) {
      errors.push_back(
          "device_num is only permitted in the target_device selector set");
    }
  };
  validate_device_selector(device_selector, false);
  validate_device_selector(target_device_selector, true);
  return errors.size() == initial_error_count;
}

void OpenMPClause::addLangExpr(const char *expression,
                               OpenMPClauseSeparator sep, int line, int col,
                               OpenMPExprParseMode parse_mode) {
  if (expression == nullptr) {
    return;
  }
  std::string spelling(expression);
  const OpenMPExprParseMode effective_parse_mode =
      resolveClauseExpressionParseMode(this->kind, parse_mode, spelling);
  expressions.emplace_back(spelling, sep, effective_parse_mode);
  OpenMPExpressionItem &item = expressions.back();
  switch (effective_parse_mode) {
  case OMP_EXPR_PARSE_variable_list:
    item.fragment.role = ompparser::HostFragmentRole::Variable;
    break;
  case OMP_EXPR_PARSE_array_section:
    item.fragment.role = ompparser::HostFragmentRole::Locator;
    break;
  case OMP_EXPR_PARSE_verbatim:
    item.fragment.role = ompparser::HostFragmentRole::Verbatim;
    break;
  case OMP_EXPR_PARSE_expression:
  case OMP_EXPR_PARSE_none:
    item.fragment.role = ompparser::HostFragmentRole::Expression;
    break;
  }
  SourceLocation location(line, col);
  if (!openmpGetLexemeSourceRange(expression, item.fragment.range)) {
    item.fragment.range.begin.line =
        static_cast<uint32_t>(std::max(location.getLine(), 0));
    item.fragment.range.begin.column =
        static_cast<uint32_t>(std::max(location.getColumn(), 0));
    item.fragment.range.end = item.fragment.range.begin;
    item.fragment.range.end.column +=
        static_cast<uint32_t>(item.fragment.spelling.size());
  }
};

std::vector<const char *> *OpenMPClause::getExpressions() {
  legacy_expression_view.clear();
  legacy_expression_view.reserve(expressions.size());
  for (const OpenMPExpressionItem &item : expressions) {
    legacy_expression_view.push_back(item.fragment.spelling.c_str());
  }
  return &legacy_expression_view;
}

const std::vector<const char *> *OpenMPClause::getExpressions() const {
  legacy_expression_view.clear();
  legacy_expression_view.reserve(expressions.size());
  for (const OpenMPExpressionItem &item : expressions) {
    legacy_expression_view.push_back(item.fragment.spelling.c_str());
  }
  return &legacy_expression_view;
}

void OpenMPInductionClause::addStepExpression(const char *expression) {
  if (expression == nullptr) {
    return;
  }
  if (step_expression.spelling.empty()) {
    step_expression =
        makeHostFragment(expression, ompparser::HostFragmentRole::Expression);
    sequence.push_back({ItemStep, 0});
    return;
  }

  passthrough_items.push_back(
      makeHostFragment(expression, ompparser::HostFragmentRole::Expression));
  sequence.push_back({ItemPassthrough, passthrough_items.size() - 1});
}

void OpenMPInductionClause::addBinding(const char *label,
                                       const char *expression) {
  if (expression == nullptr) {
    return;
  }
  Binding binding;
  if (label != nullptr) {
    binding.label =
        makeHostFragment(label, ompparser::HostFragmentRole::Declarator);
  }
  binding.expression =
      makeHostFragment(expression, ompparser::HostFragmentRole::Expression);
  bindings.push_back(std::move(binding));
  sequence.push_back({ItemBinding, bindings.size() - 1});
}

void OpenMPInductionClause::addPassthroughItem(const char *expression) {
  if (expression == nullptr) {
    return;
  }
  passthrough_items.push_back(
      makeHostFragment(expression, ompparser::HostFragmentRole::Expression));
  sequence.push_back({ItemPassthrough, passthrough_items.size() - 1});
}

void OpenMPMapClause::addItem(const char *expr, OpenMPClauseSeparator sep) {
  if (expr == nullptr) {
    return;
  }

  ompparser::SourceRange source_range;
  const bool has_source_range = openmpGetLexemeSourceRange(expr, source_range);
  addItemWithRange(expr, sep, has_source_range ? &source_range : nullptr);
}

void OpenMPMapClause::addItem(const std::string &expr,
                              OpenMPClauseSeparator sep) {
  addItemWithRange(expr, sep, nullptr);
}

void OpenMPMapClause::addItemWithRange(
    const std::string &expr, OpenMPClauseSeparator sep,
    const ompparser::SourceRange *source_range) {
  std::string array_section_expression;
  std::string dist_data_arguments;
  bool has_dist_data = splitMapExpressionDistDataSuffix(
      expr, &array_section_expression, &dist_data_arguments);

  const std::string trimmed_expression = trimWhitespace(expr);
  const std::string parsed_expression =
      has_dist_data ? array_section_expression : trimmed_expression;
  addLangExpr(parsed_expression.c_str(), sep, 0, 0,
              OMP_EXPR_PARSE_array_section);
  const std::string::size_type locator_begin = expr.find(parsed_expression);
  if (source_range != nullptr && locator_begin != std::string::npos) {
    setFragmentSourceSubrange(expressions.back().fragment, *source_range, expr,
                              locator_begin);
  }

  std::vector<DistDataPolicy> parsed_policies;
  if (has_dist_data) {
    const std::vector<std::string> policy_texts =
        splitTopLevelCommaSeparated(dist_data_arguments);
    if (policy_texts.empty()) {
      parsed_policies.emplace_back();
    }
    std::string::size_type policy_search_begin =
        locator_begin == std::string::npos
            ? 0
            : locator_begin + parsed_expression.size();
    for (const std::string &raw_policy : policy_texts) {
      const std::string policy_text = trimWhitespace(raw_policy);
      if (policy_text.empty()) {
        parsed_policies.emplace_back();
        continue;
      }

      std::string::size_type policy_begin = std::string::npos;
      if (source_range != nullptr) {
        policy_begin = expr.find(policy_text, policy_search_begin);
        if (policy_begin != std::string::npos) {
          policy_search_begin = policy_begin + policy_text.size();
        }
      }

      DistDataPolicy policy;
      std::string policy_name = policy_text;
      std::string policy_argument;
      const std::string::size_type open_pos = policy_text.find('(');
      if (open_pos != std::string::npos) {
        int depth = 0;
        std::string::size_type close_pos = std::string::npos;
        for (std::string::size_type index = open_pos;
             index < policy_text.size(); ++index) {
          const char ch = policy_text[index];
          if (ch == '(') {
            ++depth;
          } else if (ch == ')') {
            --depth;
            if (depth == 0) {
              close_pos = index;
              break;
            }
          }
        }
        if (close_pos == std::string::npos ||
            trimWhitespace(policy_text, close_pos + 1,
                           policy_text.size() - close_pos - 1)
                    .size() != 0) {
          parsed_policies.push_back(std::move(policy));
          continue;
        }
        policy_name = trimWhitespace(policy_text, 0, open_pos);
        policy_argument =
            trimWhitespace(policy_text, open_pos + 1, close_pos - open_pos - 1);
      }

      std::string normalized_name = policy_name;
      std::transform(normalized_name.begin(), normalized_name.end(),
                     normalized_name.begin(), [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                     });

      if (normalized_name == "duplicate") {
        policy.kind = DIST_DATA_duplicate;
      } else if (normalized_name == "block") {
        policy.kind = DIST_DATA_block;
      } else if (normalized_name == "cyclic") {
        policy.kind = DIST_DATA_cyclic;
      }

      policy.argument.spelling = policy_argument;
      policy.argument.role = ompparser::HostFragmentRole::Expression;
      if (!policy_argument.empty()) {
        policy.argument.semantic.reset();
        if (source_range != nullptr && policy_begin != std::string::npos) {
          const std::string::size_type argument_begin =
              policy_text.find(policy_argument, open_pos + 1);
          if (argument_begin != std::string::npos) {
            setFragmentSourceSubrange(policy.argument, *source_range, expr,
                                      policy_begin + argument_begin);
          }
        }
      }

      parsed_policies.push_back(std::move(policy));
    }
  }

  dist_data_policies.push_back(std::move(parsed_policies));
}

void OpenMPAdjustArgsClause::addArgument(const char *arg) {
  arguments.push_back(
      makeHostFragment(arg, ompparser::HostFragmentRole::Variable));
}

std::string OpenMPAdjustArgsClause::toString() {
  std::string result = "adjust_args(";
  std::string modifier_string;
  switch (modifier) {
  case OMPC_ADJUST_ARGS_need_device_addr:
    modifier_string = "need_device_addr";
    break;
  case OMPC_ADJUST_ARGS_need_device_ptr:
    modifier_string = "need_device_ptr";
    break;
  case OMPC_ADJUST_ARGS_nothing:
    modifier_string = "nothing";
    break;
  case OMPC_ADJUST_ARGS_unknown:
  default:
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
    result += arguments[i].spelling;
  }

  result += ")";
  result += " ";
  return result;
}

void OpenMPAppendArgsClause::addInteropOperation() {
  operations.push_back({OMPC_APPEND_ARGS_interop, {}});
}

OpenMPInitModifierList *OpenMPAppendArgsClause::getCurrentOperationModifiers() {
  return operations.empty() ? nullptr : &operations.back().modifiers;
}

std::string OpenMPAppendArgsClause::toString() {
  std::string result = "append_args(";
  for (size_t i = 0; i < operations.size(); ++i) {
    if (i > 0) {
      result += ", ";
    }
    if (operations[i].kind == OMPC_APPEND_ARGS_interop) {
      result += "interop(" + operations[i].modifiers.toString() + ")";
    }
  }
  return result + ") ";
}

void OpenMPUsesAllocatorsClause::addUsesAllocatorsAllocatorSequence(
    OpenMPUsesAllocatorsClauseAllocator allocator, const char *traits_array,
    const char *user_allocator) {
  auto usesAllocatorsAllocator = std::make_unique<usesAllocatorParameter>(
      allocator,
      makeHostFragment(traits_array, ompparser::HostFragmentRole::Expression),
      makeHostFragment(user_allocator, ompparser::HostFragmentRole::Variable));
  usesAllocatorsAllocatorSequenceView.push_back(usesAllocatorsAllocator.get());
  usesAllocatorsAllocatorSequenceStorage.push_back(
      std::move(usesAllocatorsAllocator));
}

void OpenMPInitModifierList::addInteropType(OpenMPInitClauseKind value) {
  OpenMPInitModifier modifier;
  modifier.category = OpenMPInitModifierCategory::InteropType;
  modifier.interop_type = value;
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitModifierList::addDirectiveName(OpenMPDirectiveKind value) {
  OpenMPInitModifier modifier;
  modifier.category = OpenMPInitModifierCategory::DirectiveName;
  modifier.directive_name = value;
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitModifierList::addPreferType(const char *specification) {
  OpenMPInitModifier modifier;
  modifier.category = OpenMPInitModifierCategory::PreferType;
  modifier.argument =
      makeHostFragment(specification, ompparser::HostFragmentRole::Verbatim);
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitModifierList::addPreferType(const std::string &specification) {
  OpenMPInitModifier modifier;
  modifier.category = OpenMPInitModifierCategory::PreferType;
  modifier.argument.spelling = specification;
  modifier.argument.role = ompparser::HostFragmentRole::Verbatim;
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitModifierList::addDepinfo(OpenMPDependClauseType type,
                                        const char *locator) {
  OpenMPInitModifier modifier;
  modifier.category = OpenMPInitModifierCategory::Depinfo;
  modifier.dependence_type = type;
  modifier.argument =
      makeHostFragment(locator, ompparser::HostFragmentRole::Locator);
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitModifierList::addDepinfo(OpenMPDependClauseType type,
                                        const std::string &locator) {
  OpenMPInitModifier modifier;
  modifier.category = OpenMPInitModifierCategory::Depinfo;
  modifier.dependence_type = type;
  modifier.argument.spelling = locator;
  modifier.argument.role = ompparser::HostFragmentRole::Locator;
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitClause::setOperand(const char *value) {
  operand = makeHostFragment(value, ompparser::HostFragmentRole::Variable);
}

void OpenMPInitClause::setOperand(const std::string &value) {
  operand = {};
  operand.spelling = value;
  operand.role = ompparser::HostFragmentRole::Variable;
}

std::string OpenMPInitModifierList::toString() const {
  std::string result;
  for (const OpenMPInitModifier &modifier : modifiers) {
    std::string text;
    switch (modifier.category) {
    case OpenMPInitModifierCategory::InteropType:
      if (modifier.interop_type == OMPC_INIT_KIND_target) {
        text = "target";
      } else if (modifier.interop_type == OMPC_INIT_KIND_targetsync) {
        text = "targetsync";
      }
      break;
    case OpenMPInitModifierCategory::DirectiveName:
      if (modifier.directive_name == OMPD_depobj) {
        text = "depobj";
      } else if (modifier.directive_name == OMPD_interop) {
        text = "interop";
      }
      break;
    case OpenMPInitModifierCategory::PreferType:
      text = "prefer_type(" + modifier.argument.spelling + ")";
      break;
    case OpenMPInitModifierCategory::Depinfo:
      switch (modifier.dependence_type) {
      case OMPC_DEPENDENCE_TYPE_in:
        text = "in";
        break;
      case OMPC_DEPENDENCE_TYPE_out:
        text = "out";
        break;
      case OMPC_DEPENDENCE_TYPE_inout:
        text = "inout";
        break;
      case OMPC_DEPENDENCE_TYPE_inoutset:
        text = "inoutset";
        break;
      case OMPC_DEPENDENCE_TYPE_mutexinoutset:
        text = "mutexinoutset";
        break;
      default:
        break;
      }
      if (!text.empty()) {
        text += "(" + modifier.argument.spelling + ")";
      }
      break;
    }
    if (text.empty()) {
      continue;
    }
    if (!result.empty()) {
      result += ", ";
    }
    result += text;
  }
  return result;
}

std::string OpenMPInitClause::toString() {
  std::string result = "init(";
  const std::string modifier_text = modifiers.toString();
  if (!modifier_text.empty()) {
    result += modifier_text + ": ";
  }
  result += operand.spelling;
  return result + ") ";
}

/**
 *
 * @param kind
 * Clause arguments arrive through a checked int/string variant boundary. The
 * grammar still uses integer semantic values, but malformed argument counts or
 * types now produce diagnostics instead of variadic undefined behavior.
 * @return
 */
OpenMPClause *OpenMPDirective::addOpenMPClauseWithArguments(
    OpenMPClauseKind kind, const std::vector<ClauseArgument> &arguments) {
  std::vector<OpenMPClause *> *current_clauses = getClauses(kind);
  OpenMPClause *new_clause = NULL;
  std::size_t argument_index = 0;

  auto nextIntegerArgument = [&]() {
    if (argument_index < arguments.size()) {
      if (const int *value = std::get_if<int>(&arguments[argument_index++])) {
        return *value;
      }
    } else {
      ++argument_index;
    }
    construction_errors.push_back(
        "missing or mistyped integer clause-construction argument");
    return 0;
  };
  auto nextStringArgument = [&]() -> const char * {
    if (argument_index < arguments.size()) {
      if (const std::string *value =
              std::get_if<std::string>(&arguments[argument_index++])) {
        return value->empty() ? nullptr : value->c_str();
      }
    } else {
      ++argument_index;
    }
    construction_errors.push_back(
        "missing or mistyped string clause-construction argument");
    return nullptr;
  };

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
  case OMPC_graph_id:
  case OMPC_graph_reset:
  case OMPC_replayable:
  case OMPC_indirect:
  case OMPC_transparent:
  case OMPC_threadset:
  case OMPC_safesync:
  case OMPC_device_safesync:
  case OMPC_memscope:
  case OMPC_local:
  case OMPC_init:
  case OMPC_init_complete:
  case OMPC_use:
  case OMPC_interop:

  {
    new_clause = registerClause(makeClause(kind));
    current_clauses->push_back(new_clause);
    break;
  }
  case OMPC_fail: {
    OpenMPFailClauseMemoryOrder memory_order =
        (OpenMPFailClauseMemoryOrder)nextIntegerArgument();
    new_clause =
        registerClause(std::make_unique<OpenMPFailClause>(memory_order));
    current_clauses->push_back(new_clause);
    break;
  }

  case OMPC_severity: {
    OpenMPSeverityClauseKind severity_kind =
        (OpenMPSeverityClauseKind)nextIntegerArgument();
    new_clause =
        registerClause(std::make_unique<OpenMPSeverityClause>(severity_kind));
    current_clauses->push_back(new_clause);
    break;
  }

  case OMPC_at: {
    OpenMPAtClauseKind at_kind = (OpenMPAtClauseKind)nextIntegerArgument();
    new_clause = registerClause(std::make_unique<OpenMPAtClause>(at_kind));
    current_clauses->push_back(new_clause);
    break;
  }
  case OMPC_if: {
    OpenMPIfClauseModifier modifier =
        (OpenMPIfClauseModifier)nextIntegerArgument();
    const char *user_defined_modifier = NULL;
    if (modifier == OMPC_IF_MODIFIER_user)
      user_defined_modifier = nextStringArgument();
    new_clause =
        OpenMPIfClause::addIfClause(this, modifier, user_defined_modifier);
    break;
  }
  case OMPC_default: {
    OpenMPDefaultClauseKind default_kind =
        (OpenMPDefaultClauseKind)nextIntegerArgument();
    OpenMPDefaultmapClauseCategory category =
        OMPC_DEFAULTMAP_CATEGORY_unspecified;
    if (argument_index < arguments.size()) {
      category = (OpenMPDefaultmapClauseCategory)nextIntegerArgument();
    }
    new_clause =
        OpenMPDefaultClause::addDefaultClause(this, default_kind, category);
    break;
  }
  case OMPC_order: {
    OpenMPOrderClauseKind order_kind =
        (OpenMPOrderClauseKind)nextIntegerArgument();
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
        (OpenMPReductionClauseModifier)nextIntegerArgument();
    OpenMPReductionClauseIdentifier identifier =
        (OpenMPReductionClauseIdentifier)nextIntegerArgument();
    const char *user_defined_modifier = nextStringArgument();
    const char *user_defined_identifier = nextStringArgument();
    new_clause = OpenMPReductionClause::addReductionClause(
        this, modifier, identifier, user_defined_modifier,
        user_defined_identifier);
    break;
  }
  case OMPC_proc_bind: {
    OpenMPProcBindClauseKind proc_bind_kind =
        (OpenMPProcBindClauseKind)nextIntegerArgument();
    new_clause = OpenMPProcBindClause::addProcBindClause(this, proc_bind_kind);
    break;
  }
  case OMPC_uses_allocators: {
    new_clause = OpenMPUsesAllocatorsClause::addUsesAllocatorsClause(this);
    break;
  }
  case OMPC_bind: {
    OpenMPBindClauseBinding bind_binding =
        (OpenMPBindClauseBinding)nextIntegerArgument();
    new_clause = OpenMPBindClause::addBindClause(this, bind_binding);
    break;
  }

  case OMPC_lastprivate: {
    OpenMPLastprivateClauseModifier modifier =
        (OpenMPLastprivateClauseModifier)nextIntegerArgument();
    new_clause = OpenMPLastprivateClause::addLastprivateClause(this, modifier);
    break;
  }

  case OMPC_linear: {
    OpenMPLinearClauseModifier modifier =
        (OpenMPLinearClauseModifier)nextIntegerArgument();
    new_clause = OpenMPLinearClause::addLinearClause(this, modifier);
    break;
  }
  case OMPC_aligned: {
    new_clause = OpenMPAlignedClause::addAlignedClause(this);
    break;
  }
  case OMPC_dist_schedule: {
    OpenMPDistScheduleClauseKind dist_schedule_kind =
        (OpenMPDistScheduleClauseKind)nextIntegerArgument();
    new_clause = OpenMPDistScheduleClause::addDistScheduleClause(
        this, dist_schedule_kind);
    break;
  }
  case OMPC_schedule: {
    OpenMPScheduleClauseModifier modifier1 =
        (OpenMPScheduleClauseModifier)nextIntegerArgument();
    OpenMPScheduleClauseModifier modifier2 =
        (OpenMPScheduleClauseModifier)nextIntegerArgument();
    OpenMPScheduleClauseKind schedule_kind =
        (OpenMPScheduleClauseKind)nextIntegerArgument();
    const char *user_defined_kind = NULL;
    if (schedule_kind == OMPC_SCHEDULE_KIND_user)
      user_defined_kind = nextStringArgument();
    new_clause = OpenMPScheduleClause::addScheduleClause(
        this, modifier1, modifier2, schedule_kind, user_defined_kind);

    break;
  }
  case OMPC_device: {
    OpenMPDeviceClauseModifier modifier =
        (OpenMPDeviceClauseModifier)nextIntegerArgument();
    new_clause = OpenMPDeviceClause::addDeviceClause(this, modifier);
    break;
  }

  case OMPC_initializer: {
    OpenMPInitializerClausePriv priv =
        (OpenMPInitializerClausePriv)nextIntegerArgument();
    const char *user_defined_priv = NULL;
    if (priv == OMPC_INITIALIZER_PRIV_user)
      user_defined_priv = nextStringArgument();
    new_clause = OpenMPInitializerClause::addInitializerClause(
        this, priv, user_defined_priv);
    break;
  }
  case OMPC_allocate: {
    OpenMPAllocateClauseAllocator allocator =
        (OpenMPAllocateClauseAllocator)nextIntegerArgument();
    const char *user_defined_allocator = NULL;
    if (allocator == OMPC_ALLOCATE_ALLOCATOR_user)
      user_defined_allocator = nextStringArgument();
    new_clause = OpenMPAllocateClause::addAllocateClause(
        this, allocator, user_defined_allocator);
    break;
  }
  case OMPC_allocator: {
    OpenMPAllocatorClauseAllocator allocator =
        (OpenMPAllocatorClauseAllocator)nextIntegerArgument();
    const char *user_defined_allocator = NULL;
    if (allocator == OMPC_ALLOCATOR_ALLOCATOR_user)
      user_defined_allocator = nextStringArgument();
    new_clause = OpenMPAllocatorClause::addAllocatorClause(
        this, allocator, user_defined_allocator);

    break;
  }
  case OMPC_atomic_default_mem_order: {
    OpenMPAtomicDefaultMemOrderClauseKind atomic_default_mem_order_kind =
        (OpenMPAtomicDefaultMemOrderClauseKind)nextIntegerArgument();
    new_clause =
        OpenMPAtomicDefaultMemOrderClause::addAtomicDefaultMemOrderClause(
            this, atomic_default_mem_order_kind);

    break;
  }
  case OMPC_in_reduction: {
    OpenMPInReductionClauseIdentifier identifier =
        (OpenMPInReductionClauseIdentifier)nextIntegerArgument();
    const char *user_defined_identifier = NULL;
    if (identifier == OMPC_IN_REDUCTION_IDENTIFIER_user)
      user_defined_identifier = nextStringArgument();
    new_clause = OpenMPInReductionClause::addInReductionClause(
        this, identifier, user_defined_identifier);
    break;
  }
  case OMPC_depobj_update: {
    OpenMPDepobjUpdateClauseDependeceType type =
        (OpenMPDepobjUpdateClauseDependeceType)nextIntegerArgument();
    new_clause = OpenMPDepobjUpdateClause::addDepobjUpdateClause(this, type);
    break;
  }
  case OMPC_depend: {
    OpenMPDependClauseModifier modifier =
        (OpenMPDependClauseModifier)nextIntegerArgument();
    OpenMPDependClauseType type = (OpenMPDependClauseType)nextIntegerArgument();
    new_clause = OpenMPDependClause::addDependClause(this, modifier, type);
    break;
  }
  case OMPC_doacross: {
    OpenMPDoacrossClauseType type =
        (OpenMPDoacrossClauseType)nextIntegerArgument();
    std::vector<OpenMPClause *> *current_clauses = getClauses(OMPC_doacross);
    new_clause = registerClause(std::make_unique<OpenMPDoacrossClause>(type));
    current_clauses->push_back(new_clause);
    break;
  }
  case OMPC_affinity: {
    OpenMPAffinityClauseModifier modifier =
        (OpenMPAffinityClauseModifier)nextIntegerArgument();
    new_clause = OpenMPAffinityClause::addAffinityClause(this, modifier);
    break;
  }
  case OMPC_grainsize: {
    OpenMPGrainsizeClauseModifier modifier =
        (OpenMPGrainsizeClauseModifier)nextIntegerArgument();
    std::vector<OpenMPClause *> *current_clauses = getClauses(OMPC_grainsize);
    new_clause =
        registerClause(std::make_unique<OpenMPGrainsizeClause>(modifier));
    current_clauses->push_back(new_clause);
    break;
  }
  case OMPC_num_tasks: {
    OpenMPNumTasksClauseModifier modifier =
        (OpenMPNumTasksClauseModifier)nextIntegerArgument();
    std::vector<OpenMPClause *> *current_clauses = getClauses(OMPC_num_tasks);
    new_clause =
        registerClause(std::make_unique<OpenMPNumTasksClause>(modifier));
    current_clauses->push_back(new_clause);
    break;
  }
  case OMPC_to: {
    OpenMPToClauseKind to_kind = (OpenMPToClauseKind)nextIntegerArgument();
    new_clause = OpenMPToClause::addToClause(this, to_kind);
    break;
  }
  case OMPC_from: {
    OpenMPFromClauseKind from_kind =
        (OpenMPFromClauseKind)nextIntegerArgument();
    new_clause = OpenMPFromClause::addFromClause(this, from_kind);
    break;
  }

  case OMPC_device_type: {
    OpenMPDeviceTypeClauseKind device_type_kind =
        (OpenMPDeviceTypeClauseKind)nextIntegerArgument();
    new_clause =
        OpenMPDeviceTypeClause::addDeviceTypeClause(this, device_type_kind);
    break;
  }

  case OMPC_defaultmap: {
    OpenMPDefaultmapClauseBehavior behavior =
        (OpenMPDefaultmapClauseBehavior)nextIntegerArgument();
    OpenMPDefaultmapClauseCategory category =
        (OpenMPDefaultmapClauseCategory)nextIntegerArgument();
    new_clause =
        OpenMPDefaultmapClause::addDefaultmapClause(this, behavior, category);
    break;
  }
  case OMPC_task_reduction: {
    OpenMPTaskReductionClauseIdentifier identifier =
        (OpenMPTaskReductionClauseIdentifier)nextIntegerArgument();
    const char *user_defined_identifier = NULL;
    if (identifier == OMPC_TASK_REDUCTION_IDENTIFIER_user)
      user_defined_identifier = nextStringArgument();
    new_clause = OpenMPTaskReductionClause::addTaskReductionClause(
        this, identifier, user_defined_identifier);
    break;
  }
  case OMPC_map: {
    OpenMPMapClauseModifier modifier1 =
        (OpenMPMapClauseModifier)nextIntegerArgument();
    OpenMPMapClauseModifier modifier2 =
        (OpenMPMapClauseModifier)nextIntegerArgument();
    OpenMPMapClauseModifier modifier3 =
        (OpenMPMapClauseModifier)nextIntegerArgument();
    OpenMPMapClauseType type = (OpenMPMapClauseType)nextIntegerArgument();
    OpenMPMapClauseRefModifier ref_modifier =
        (OpenMPMapClauseRefModifier)nextIntegerArgument();
    const char *mapper_argument = nextStringArgument();
    std::string mapper_identifier =
        mapper_argument != nullptr ? mapper_argument : "";
    new_clause =
        OpenMPMapClause::addMapClause(this, modifier1, modifier2, modifier3,
                                      type, ref_modifier, mapper_identifier);
    break;
  }
  case OMPC_num_threads: {
    new_clause = registerClause(std::make_unique<OpenMPNumThreadsClause>());
    current_clauses->push_back(new_clause);
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
    new_clause = directive->registerClause(
        std::make_unique<OpenMPMapClause>(modifier1, modifier2, modifier3, type,
                                          ref_modifier, mapper_identifier));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPTaskReductionClause::addTaskReductionClause(
    OpenMPDirective *directive, OpenMPTaskReductionClauseIdentifier identifier,
    const char *user_defined_identifier) {

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
  OpenMPClause *new_clause = directive->registerClause(
      std::make_unique<OpenMPProcBindClause>(proc_bind_kind));
  current_clauses->push_back(new_clause);
  return new_clause;
};

OpenMPClause *
OpenMPBindClause::addBindClause(OpenMPDirective *directive,
                                OpenMPBindClauseBinding bind_binding) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_bind);
  OpenMPClause *new_clause = directive->registerClause(
      std::make_unique<OpenMPBindClause>(bind_binding));
  current_clauses->push_back(new_clause);

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

OpenMPClause *OpenMPReductionClause::addReductionClause(
    OpenMPDirective *directive, OpenMPReductionClauseModifier modifier,
    OpenMPReductionClauseIdentifier identifier,
    const char *user_defined_modifier, const char *user_defined_identifier) {

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
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDepobjUpdateClause>(type));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPInReductionClause::addInReductionClause(
    OpenMPDirective *directive, OpenMPInReductionClauseIdentifier identifier,
    const char *user_defined_identifier) {
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
  OpenMPClause *new_clause = directive->registerClause(
      std::make_unique<OpenMPAtomicDefaultMemOrderClause>(
          atomic_default_mem_order_kind));
  current_clauses->push_back(new_clause);
  return new_clause;
};

OpenMPClause *OpenMPAllocatorClause::addAllocatorClause(
    OpenMPDirective *directive, OpenMPAllocatorClauseAllocator allocator,
    const char *user_defined_allocator) {
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
                                        const char *user_defined_allocator) {
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
                                              const char *user_defined_priv) {
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
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDeviceClause>(modifier));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPScheduleClause::addScheduleClause(
    OpenMPDirective *directive, OpenMPScheduleClauseModifier modifier1,
    OpenMPScheduleClauseModifier modifier2,
    OpenMPScheduleClauseKind schedule_kind, const char *user_defined_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_schedule);
  OpenMPClause *new_clause = NULL;

  new_clause = directive->registerClause(std::make_unique<OpenMPScheduleClause>(
      modifier1, modifier2, schedule_kind));
  if (schedule_kind == OMPC_SCHEDULE_KIND_user)
    ((OpenMPScheduleClause *)new_clause)->setUserDefinedKind(user_defined_kind);
  current_clauses->push_back(new_clause);
  return new_clause;
};

OpenMPClause *OpenMPDistScheduleClause::addDistScheduleClause(
    OpenMPDirective *directive,
    OpenMPDistScheduleClauseKind dist_schedule_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_dist_schedule);
  OpenMPClause *new_clause = NULL;

  new_clause = directive->registerClause(
      std::make_unique<OpenMPDistScheduleClause>(dist_schedule_kind));
  current_clauses->push_back(new_clause);
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
    new_clause = directive->registerClause(
        std::make_unique<OpenMPLastprivateClause>(modifier));
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPIfClause::addIfClause(OpenMPDirective *directive,
                                          OpenMPIfClauseModifier modifier,
                                          const char *user_defined_modifier) {

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
                                      OpenMPDefaultClauseKind default_kind,
                                      OpenMPDefaultmapClauseCategory category) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_default);
  OpenMPClause *new_clause = NULL;

  new_clause = directive->registerClause(
      std::make_unique<OpenMPDefaultClause>(default_kind, category));
  current_clauses->push_back(new_clause);

  return new_clause;
};

OpenMPClause *
OpenMPOrderClause::addOrderClause(OpenMPDirective *directive,
                                  OpenMPOrderClauseModifier order_modifier,
                                  OpenMPOrderClauseKind order_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_order);
  OpenMPClause *new_clause = directive->registerClause(
      std::make_unique<OpenMPOrderClause>(order_modifier, order_kind));
  current_clauses->push_back(new_clause);
  new_clause->setClausePosition(
      static_cast<int>(directive->getClausesInOriginalOrder()->size()));
  directive->getClausesInOriginalOrder()->push_back(new_clause);

  return new_clause;
};

OpenMPClause *
OpenMPOrderClause::addOrderClause(OpenMPDirective *directive,
                                  OpenMPOrderClauseKind order_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_order);
  OpenMPClause *new_clause = directive->registerClause(
      std::make_unique<OpenMPOrderClause>(order_kind));
  current_clauses->push_back(new_clause);
  new_clause->setClausePosition(
      static_cast<int>(directive->getClausesInOriginalOrder()->size()));
  directive->getClausesInOriginalOrder()->push_back(new_clause);

  return new_clause;
};

OpenMPClause *
OpenMPAlignedClause::addAlignedClause(OpenMPDirective *directive) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_aligned);
  OpenMPClause *new_clause =
      directive->registerClause(std::make_unique<OpenMPAlignedClause>());
  current_clauses->push_back(new_clause);

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
  OpenMPClause *new_clause =
      directive->registerClause(std::make_unique<OpenMPMatchClause>());
  current_clauses->push_back(new_clause);
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

std::string OpenMPGraphResetClause::toString() {
  std::string str = expressionToString();
  if (str.empty())
    return "graph_reset ";
  return "graph_reset(" + str + ") ";
}

std::string OpenMPTransparentClause::toString() {
  std::string str = expressionToString();
  if (str.empty())
    return "transparent ";
  return "transparent(" + str + ") ";
}

std::string OpenMPReplayableClause::toString() {
  const std::string expression = expressionToString();
  return expression.empty() ? "replayable " : "replayable(" + expression + ") ";
}

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

std::string OpenMPInitCompleteClause::toString() {
  const std::string expression = expressionToString();
  return expression.empty() ? "init_complete "
                            : "init_complete(" + expression + ") ";
}

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
