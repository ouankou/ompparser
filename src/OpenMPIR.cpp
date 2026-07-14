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
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdarg.h>
#include <utility>

extern bool clause_separator_comma;

namespace {

OpenMPExprParseCallback gOpenMPExprParseCallback = nullptr;
void *gOpenMPExprParseUserData = nullptr;
OpenMPExprParseMode gOpenMPExprParseMode = OMP_EXPR_PARSE_none;

[[noreturn]] void failDuplicateClause(OpenMPDirectiveKind directive_kind,
                                      OpenMPClauseKind clause_kind) {
  std::cerr << "OMPPARSER_SEMANTIC[duplicate-clause]: directive "
            << directive_kind << " repeats singleton clause " << clause_kind
            << "\n";
  std::abort();
}

[[noreturn]] void failDuplicateClauseExpression(OpenMPClauseKind clause_kind,
                                                const char *expression) {
  std::cerr << "OMPPARSER_SEMANTIC[duplicate-clause-expression]: clause "
            << clause_kind << " repeats expression '"
            << (expression != nullptr ? expression : "<null>") << "'\n";
  std::abort();
}

bool isExplicitSingletonClause(OpenMPClauseKind kind) {
  switch (kind) {
  case OMPC_simdlen:
  case OMPC_safelen:
  case OMPC_seq_cst:
  case OMPC_acq_rel:
  case OMPC_release:
  case OMPC_acquire:
  case OMPC_relaxed:
  case OMPC_hint:
    return true;
  default:
    return false;
  }
}

} // namespace

void openmpConfigureExprParseCallback(OpenMPExprParseCallback callback,
                                      void *user_data) {
  gOpenMPExprParseCallback = callback;
  gOpenMPExprParseUserData = user_data;
}

void openmpResetExprParseCallback() {
  gOpenMPExprParseCallback = nullptr;
  gOpenMPExprParseUserData = nullptr;
  gOpenMPExprParseMode = OMP_EXPR_PARSE_none;
}

void openmpSetExprParseMode(OpenMPExprParseMode mode) {
  gOpenMPExprParseMode = mode;
}

const void *openmpParseExpressionNode(OpenMPDirectiveKind directive_kind,
                                      OpenMPClauseKind clause_kind,
                                      OpenMPExprParseMode parse_mode,
                                      const char *expression) {
  if (expression == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[expression-callback]: expression is "
                 "null\n";
    std::abort();
  }
  if (gOpenMPExprParseCallback == nullptr) {
    return nullptr;
  }
  const OpenMPExprParseMode effective_mode =
      parse_mode != OMP_EXPR_PARSE_none ? parse_mode : gOpenMPExprParseMode;
  return gOpenMPExprParseCallback(directive_kind, clause_kind, effective_mode,
                                  expression, gOpenMPExprParseUserData);
}

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

bool isOpenMPIdentifierSpelling(const std::string &spelling) {
  if (spelling.empty() ||
      !(std::isalpha(static_cast<unsigned char>(spelling.front())) ||
        spelling.front() == '_')) {
    return false;
  }
  return std::all_of(spelling.begin() + 1, spelling.end(),
                     [](unsigned char character) {
                       return std::isalnum(character) || character == '_';
                     });
}

bool isOpenMPStringPropertySpelling(const std::string &spelling) {
  if (spelling.size() < 3 ||
      (spelling.front() != '"' && spelling.front() != '\'') ||
      spelling.back() != spelling.front()) {
    return false;
  }

  bool escaped = false;
  for (std::size_t index = 1; index + 1 < spelling.size(); ++index) {
    const char character = spelling[index];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (character == '\\') {
      escaped = true;
      continue;
    }
    if (character == spelling.front()) {
      return false;
    }
  }
  return !escaped;
}

std::string normalizeOpenMPNamePropertyIdentity(const std::string &spelling) {
  if (isOpenMPIdentifierSpelling(spelling)) {
    return spelling;
  }
  if (isOpenMPStringPropertySpelling(spelling)) {
    // OpenMP treats an identifier and a string literal containing the same
    // character sequence as the same name-list property value.
    return spelling.substr(1, spelling.size() - 2);
  }
  std::cerr << "OMPPARSER_SEMANTIC[variant-name-property]: property '"
            << spelling
            << "' is neither an OpenMP identifier nor a string literal\n";
  std::abort();
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
                                      std::string &array_section_expression,
                                      std::string &dist_data_arguments) {
  const std::string trimmed_expression = trimWhitespace(expression);
  array_section_expression = trimmed_expression;
  dist_data_arguments.clear();
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

  array_section_expression = base_expression;
  dist_data_arguments =
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

  switch (clause_kind) {
  case OMPC_if:
  case OMPC_num_threads:
  case OMPC_num_teams:
  case OMPC_thread_limit:
  case OMPC_collapse:
  case OMPC_ordered:
  case OMPC_partial:
  case OMPC_nowait:
  case OMPC_schedule:
  case OMPC_dist_schedule:
  case OMPC_safelen:
  case OMPC_simdlen:
  case OMPC_final:
  case OMPC_priority:
  case OMPC_detach:
  case OMPC_grainsize:
  case OMPC_num_tasks:
  case OMPC_device:
  case OMPC_sizes:
  case OMPC_hint:
  case OMPC_filter:
  case OMPC_nocontext:
  case OMPC_novariants:
  case OMPC_looprange:
  case OMPC_counts:
  case OMPC_graph_id:
    return OMP_EXPR_PARSE_expression;
  default:
    break;
  }

  return parse_mode;
}

} // namespace

void OpenMPApplyClause::addTransformation(OpenMPApplyTransformKind kind,
                                          const std::string &argument,
                                          OpenMPClauseSeparator sep) {
  ApplyTransform t;
  t.kind = kind;
  t.argument = trimWhitespace(argument);
  t.separator = sep;
  switch (kind) {
  case OMPC_APPLY_TRANSFORM_unroll_partial:
  case OMPC_APPLY_TRANSFORM_tile_sizes:
    if (t.argument.empty()) {
      fprintf(stderr, "OMP_PARSER_INVARIANT[apply]: transformation requires an "
                      "expression argument\n");
      std::abort();
    }
    addAuxiliaryLangExpr(t.argument);
    break;
  case OMPC_APPLY_TRANSFORM_user:
    if (t.argument.empty()) {
      fprintf(stderr,
              "OMP_PARSER_INVARIANT[apply]: named transformation is empty\n");
      std::abort();
    }
    break;
  case OMPC_APPLY_TRANSFORM_unroll:
  case OMPC_APPLY_TRANSFORM_unroll_full:
  case OMPC_APPLY_TRANSFORM_reverse:
  case OMPC_APPLY_TRANSFORM_interchange:
  case OMPC_APPLY_TRANSFORM_nothing:
    if (!t.argument.empty()) {
      fprintf(stderr,
              "OMP_PARSER_INVARIANT[apply]: argumentless transformation "
              "carries an expression\n");
      std::abort();
    }
    break;
  case OMPC_APPLY_TRANSFORM_apply:
  case OMPC_APPLY_TRANSFORM_unknown:
  default:
    fprintf(stderr,
            "OMP_PARSER_INVARIANT[apply]: invalid direct transformation "
            "kind\n");
    std::abort();
  }
  transforms.push_back(std::move(t));
}

void OpenMPApplyClause::addNestedApply(OpenMPApplyClause *nested,
                                       OpenMPClauseSeparator sep) {
  if (nested == nullptr) {
    fprintf(stderr,
            "OMP_PARSER_INVARIANT[apply]: nested apply transformation is "
            "null\n");
    std::abort();
  }
  ApplyTransform t;
  t.kind = OMPC_APPLY_TRANSFORM_apply;
  t.nested_apply.reset(nested);
  t.separator = sep;
  auxiliaryExpressionNodes.insert(auxiliaryExpressionNodes.end(),
                                  nested->auxiliaryExpressionNodes.begin(),
                                  nested->auxiliaryExpressionNodes.end());
  transforms.push_back(std::move(t));
}

OpenMPClause *
OpenMPDirective::registerClause(std::unique_ptr<OpenMPClause> clause) {
  if (clause == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[register-clause]: clause is null\n";
    std::abort();
  }
  clause->setDirectiveKind(this->kind);
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
    std::cerr << "OMPPARSER_INVARIANT[declare-reduction-type]: type name is "
                 "null\n";
    std::abort();
  }
  std::string cleaned = normalizeRawExpression(_typename_list);
  if (cleaned.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[declare-reduction-type]: type name is "
                 "empty\n";
    std::abort();
  }
  auto owned_value = std::make_unique<char[]>(cleaned.size() + 1);
  std::memcpy(owned_value.get(), cleaned.c_str(), cleaned.size() + 1);
  const char *stored_expression = owned_value.get();
  typename_list.push_back(stored_expression);
  typename_storage.push_back(std::move(owned_value));
}

void OpenMPInitializerClause::setUserDefinedPriv(char *_priv) {
  user_defined_priv = normalizeRawExpression(_priv);
  if (user_defined_priv.empty()) {
    std::cerr << "OMPPARSER_SEMANTIC[initializer]: initializer expression is "
                 "empty\n";
    std::abort();
  }

  const auto is_identifier_character = [](unsigned char character) {
    return std::isalnum(character) || character == '_';
  };
  const auto contains_identifier = [&](const std::string &identifier,
                                       std::size_t begin) {
    std::size_t position = user_defined_priv.find(identifier, begin);
    while (position != std::string::npos) {
      const bool valid_left =
          position == 0 || !is_identifier_character(static_cast<unsigned char>(
                               user_defined_priv[position - 1]));
      const std::size_t end = position + identifier.size();
      const bool valid_right =
          end == user_defined_priv.size() ||
          !is_identifier_character(
              static_cast<unsigned char>(user_defined_priv[end]));
      if (valid_left && valid_right)
        return true;
      position = user_defined_priv.find(identifier, position + 1);
    }
    return false;
  };

  constexpr const char *PrivateIdentifier = "omp_priv";
  constexpr std::size_t PrivateIdentifierLength = 8;
  if (user_defined_priv.compare(0, PrivateIdentifierLength,
                                PrivateIdentifier) == 0 &&
      (user_defined_priv.size() == PrivateIdentifierLength ||
       !is_identifier_character(static_cast<unsigned char>(
           user_defined_priv[PrivateIdentifierLength])))) {
    const std::size_t operation =
        user_defined_priv.find_first_not_of(" \t\r\n", PrivateIdentifierLength);
    if (operation != std::string::npos &&
        (user_defined_priv[operation] == '=' ||
         user_defined_priv[operation] == '('))
      return;
  }

  const std::size_t call_open = user_defined_priv.find('(');
  if (call_open != std::string::npos && call_open > 0 &&
      user_defined_priv.back() == ')' &&
      contains_identifier(PrivateIdentifier, call_open + 1))
    return;

  std::cerr << "OMPPARSER_SEMANTIC[initializer]: initializer must assign or "
               "construct omp_priv, or call a function with omp_priv\n";
  std::abort();
}

void OpenMPDeclareMapperDirective::setUserDefinedIdentifier(
    std::string _user_defined_identifier) {
  if (!user_defined_identifier.empty() ||
      user_defined_identifier_node != nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[declare-mapper-identifier]: identifier "
                 "was assigned more than once\n";
    std::abort();
  }
  user_defined_identifier = trimWhitespace(_user_defined_identifier);
  if (user_defined_identifier.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[declare-mapper-identifier]: identifier "
                 "is empty\n";
    std::abort();
  }
  user_defined_identifier_node = openmpParseExpressionNode(
      OMPD_declare_mapper, OMPC_unknown, OMP_EXPR_PARSE_verbatim,
      user_defined_identifier.c_str());
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
                               OpenMPClauseSeparator sep, int line, int col,
                               OpenMPExprParseMode parse_mode) {
  if (expression == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[clause-expression]: clause " << kind
              << " received a null expression\n";
    std::abort();
  }
  std::string normalized = normalizeClauseExpression(this->kind, expression);
  if (normalized.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[clause-expression]: clause " << kind
              << " received an empty expression\n";
    std::abort();
  }
  // Since the size of expression vector is supposed to be small, brute force is
  // used here.
  // Skip deduplication if duplicates are allowed (e.g., for sizes(4, 4))
  if (!allow_duplicates) {
    for (unsigned int i = 0; i < this->expressions.size(); i++) {
      if (!strcmp(expressions.at(i), normalized.c_str())) {
        failDuplicateClauseExpression(kind, normalized.c_str());
      };
    };
  }
  const OpenMPExprParseMode effective_parse_mode =
      resolveClauseExpressionParseMode(this->kind, parse_mode, normalized);
  const void *expression_node =
      openmpParseExpressionNode(this->directive_kind, this->kind,
                                effective_parse_mode, normalized.c_str());
  size_t length = normalized.size();
  auto owned_value = std::make_unique<char[]>(length + 1);
  std::memcpy(owned_value.get(), normalized.c_str(), length + 1);
  const char *stored_expression = owned_value.get();
  expressions.push_back(stored_expression);
  expressionNodes.push_back(expression_node);
  expression_separators.push_back(sep);
  owned_expressions.push_back(std::move(owned_value));
  locations.push_back(SourceLocation(line, col));
};

void OpenMPNowaitClause::addLangExpr(const char *expression,
                                     OpenMPClauseSeparator sep, int line,
                                     int col, OpenMPExprParseMode parse_mode) {
  if (!expressions.empty() || !expressionNodes.empty() ||
      !expression_separators.empty() || !locations.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[nowait-expression]: nowait accepts at "
                 "most one expression\n";
    std::abort();
  }
  OpenMPClause::addLangExpr(expression, sep, line, col, parse_mode);
  if (expressions.size() != 1 || expressionNodes.size() != 1 ||
      expression_separators.size() != 1 || locations.size() != 1) {
    std::cerr << "OMPPARSER_INVARIANT[nowait-expression]: expression storage "
                 "cardinality diverged\n";
    std::abort();
  }
}

OpenMPVariantClause::TraitSetSelector &
OpenMPVariantClause::requireActiveTraitSet(
    OpenMPContextSelectorSequenceKind expected, const char *invariant_name) {
  if (!active_trait_set.has_value() || *active_trait_set >= trait_sets.size()) {
    std::cerr << "OMPPARSER_INVARIANT[" << invariant_name
              << "]: selector has no active trait set\n";
    std::abort();
  }
  TraitSetSelector &set = trait_sets[*active_trait_set];
  if (set.kind != expected) {
    std::cerr << "OMPPARSER_INVARIANT[" << invariant_name
              << "]: selector is not legal in trait set "
              << static_cast<int>(set.kind) << '\n';
    std::abort();
  }
  return set;
}

OpenMPVariantClause::TraitSelector &
OpenMPVariantClause::requireActiveTraitSelector(const char *invariant_name) {
  if (!active_trait_set.has_value() || *active_trait_set >= trait_sets.size() ||
      !active_trait_selector.has_value() ||
      *active_trait_selector >=
          trait_sets[*active_trait_set].selectors.size()) {
    std::cerr << "OMPPARSER_INVARIANT[" << invariant_name
              << "]: no active trait selector\n";
    std::abort();
  }
  return trait_sets[*active_trait_set].selectors[*active_trait_selector];
}

void OpenMPVariantClause::requireUniqueSelector(
    OpenMPContextTraitSelectorKind kind, const std::string &identity,
    const char *invariant_name) const {
  if (!active_trait_set.has_value() || *active_trait_set >= trait_sets.size()) {
    std::cerr << "OMPPARSER_INVARIANT[" << invariant_name
              << "]: duplicate check has no active trait set\n";
    std::abort();
  }
  for (const TraitSelector &selector :
       trait_sets[*active_trait_set].selectors) {
    if (selector.kind != kind) {
      continue;
    }
    bool same_identity = true;
    if (kind == OMPC_TRAIT_construct) {
      same_identity =
          selector.construct_directive != nullptr &&
          std::to_string(selector.construct_directive->getKind()) == identity;
    } else if (kind == OMPC_TRAIT_implementation_user) {
      same_identity = selector.implementation_defined_name == identity;
    }
    if (same_identity) {
      std::cerr << "OMPPARSER_INVARIANT[" << invariant_name
                << "]: duplicate trait selector\n";
      std::abort();
    }
  }
}

void OpenMPVariantClause::beginTraitSelector(
    OpenMPContextTraitSelectorKind kind, const char *score,
    const char *implementation_defined_name) {
  if (!active_trait_set.has_value() || *active_trait_set >= trait_sets.size()) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-selector]: selector has no "
                 "active trait set\n";
    std::abort();
  }
  if (active_trait_selector.has_value()) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-selector]: nested trait "
                 "selector\n";
    std::abort();
  }
  if (score == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[variant-score]: score pointer is null\n";
    std::abort();
  }

  const OpenMPContextSelectorSequenceKind set_kind =
      trait_sets[*active_trait_set].kind;
  bool legal = false;
  switch (kind) {
  case OMPC_TRAIT_condition:
    legal = set_kind == OMPC_SELECTOR_user;
    break;
  case OMPC_TRAIT_construct:
    legal = set_kind == OMPC_SELECTOR_construct;
    break;
  case OMPC_TRAIT_kind:
  case OMPC_TRAIT_arch:
  case OMPC_TRAIT_isa:
    legal = set_kind == OMPC_SELECTOR_device ||
            set_kind == OMPC_SELECTOR_target_device;
    break;
  case OMPC_TRAIT_device_num:
  case OMPC_TRAIT_uid:
    legal = set_kind == OMPC_SELECTOR_target_device;
    break;
  case OMPC_TRAIT_vendor:
  case OMPC_TRAIT_extension:
  case OMPC_TRAIT_requires:
  case OMPC_TRAIT_atomic_default_mem_order:
  case OMPC_TRAIT_implementation_user:
    legal = set_kind == OMPC_SELECTOR_implementation;
    break;
  }
  if (!legal || kind == OMPC_TRAIT_construct) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-selector]: selector is not "
                 "legal for property-list construction in trait set "
              << static_cast<int>(set_kind) << '\n';
    std::abort();
  }

  const std::string normalized_score = trimWhitespace(score);
  if (!normalized_score.empty() && (set_kind == OMPC_SELECTOR_construct ||
                                    set_kind == OMPC_SELECTOR_device ||
                                    set_kind == OMPC_SELECTOR_target_device)) {
    std::cerr << "OMPPARSER_INVARIANT[variant-score]: OpenMP prohibits a score "
                 "in this trait selector set\n";
    std::abort();
  }

  std::string identity;
  if (kind == OMPC_TRAIT_implementation_user) {
    identity = implementation_defined_name == nullptr
                   ? std::string()
                   : trimWhitespace(implementation_defined_name);
    if (identity.empty()) {
      std::cerr << "OMPPARSER_INVARIANT[implementation-selector]: custom "
                   "selector name is empty\n";
      std::abort();
    }
  } else if (implementation_defined_name != nullptr &&
             !trimWhitespace(implementation_defined_name).empty()) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-selector]: built-in "
                 "selector received a custom name\n";
    std::abort();
  }
  requireUniqueSelector(kind, identity, "variant-trait-selector");

  TraitSelector selector;
  selector.kind = kind;
  selector.score = normalized_score;
  selector.implementation_defined_name = identity;
  selector.score_node =
      addAuxiliaryLangExpr(selector.score, OMP_EXPR_PARSE_constant_integer);
  trait_sets[*active_trait_set].selectors.push_back(std::move(selector));
  active_trait_selector = trait_sets[*active_trait_set].selectors.size() - 1;
}

void OpenMPVariantClause::addExpressionProperty(
    const char *expression, OpenMPExprParseMode parse_mode) {
  TraitSelector &selector =
      requireActiveTraitSelector("variant-trait-property");
  if (expression == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-property]: expression is "
                 "null\n";
    std::abort();
  }
  const std::string spelling = trimWhitespace(expression);
  if (spelling.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-property]: expression is "
                 "empty\n";
    std::abort();
  }
  const OpenMPExprParseMode required_mode =
      selector.kind == OMPC_TRAIT_condition ||
              selector.kind == OMPC_TRAIT_device_num
          ? OMP_EXPR_PARSE_expression
          : OMP_EXPR_PARSE_verbatim;
  if (parse_mode != required_mode || selector.kind == OMPC_TRAIT_kind ||
      selector.kind == OMPC_TRAIT_vendor ||
      selector.kind == OMPC_TRAIT_atomic_default_mem_order ||
      selector.kind == OMPC_TRAIT_construct) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-property]: expression "
                 "payload has the wrong selector kind or parse mode\n";
    std::abort();
  }
  const bool name_list_property =
      selector.kind == OMPC_TRAIT_arch || selector.kind == OMPC_TRAIT_isa ||
      selector.kind == OMPC_TRAIT_uid || selector.kind == OMPC_TRAIT_extension;
  const std::string identity =
      name_list_property ? normalizeOpenMPNamePropertyIdentity(spelling)
                         : spelling;
  for (const TraitProperty &property : selector.properties) {
    const std::string existing_identity =
        name_list_property
            ? normalizeOpenMPNamePropertyIdentity(property.spelling)
            : property.spelling;
    if (existing_identity == identity) {
      std::cerr << "OMPPARSER_INVARIANT[variant-trait-property]: duplicate "
                   "trait property\n";
      std::abort();
    }
  }
  TraitProperty property;
  property.spelling = spelling;
  property.parse_mode = parse_mode;
  property.node = addAuxiliaryLangExpr(property.spelling, parse_mode);
  selector.properties.push_back(std::move(property));
}

void OpenMPVariantClause::addContextKindProperty(OpenMPClauseContextKind kind) {
  TraitSelector &selector = requireActiveTraitSelector("device-kind-property");
  if (selector.kind != OMPC_TRAIT_kind || kind == OMPC_CONTEXT_KIND_unknown) {
    std::cerr << "OMPPARSER_INVARIANT[device-kind-property]: invalid kind "
                 "property\n";
    std::abort();
  }
  for (const TraitProperty &property : selector.properties) {
    if (property.context_kind == kind) {
      std::cerr << "OMPPARSER_INVARIANT[device-kind-property]: duplicate kind "
                   "property\n";
      std::abort();
    }
  }
  TraitProperty property;
  property.context_kind = kind;
  selector.properties.push_back(std::move(property));
}

void OpenMPVariantClause::addContextVendorProperty(
    OpenMPClauseContextVendor vendor) {
  TraitSelector &selector = requireActiveTraitSelector("vendor-property");
  if (selector.kind != OMPC_TRAIT_vendor ||
      vendor == OMPC_CONTEXT_VENDOR_unspecified) {
    std::cerr << "OMPPARSER_INVARIANT[vendor-property]: invalid vendor "
                 "property\n";
    std::abort();
  }
  for (const TraitProperty &property : selector.properties) {
    if (property.context_vendor == vendor) {
      std::cerr << "OMPPARSER_INVARIANT[vendor-property]: duplicate vendor "
                   "property\n";
      std::abort();
    }
  }
  TraitProperty property;
  property.context_vendor = vendor;
  selector.properties.push_back(std::move(property));
}

void OpenMPVariantClause::addAtomicDefaultMemOrderProperty(
    OpenMPAtomicDefaultMemOrderClauseKind kind) {
  TraitSelector &selector =
      requireActiveTraitSelector("atomic-default-mem-order-property");
  if (selector.kind != OMPC_TRAIT_atomic_default_mem_order ||
      kind == OMPC_ATOMIC_DEFAULT_MEM_ORDER_unknown) {
    std::cerr << "OMPPARSER_INVARIANT[atomic-default-mem-order-property]: "
                 "invalid property\n";
    std::abort();
  }
  if (!selector.properties.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[atomic-default-mem-order-property]: "
                 "duplicate property\n";
    std::abort();
  }
  TraitProperty property;
  property.atomic_default_mem_order = kind;
  selector.properties.push_back(std::move(property));
}

void OpenMPVariantClause::requireSelectorPropertyCardinality(
    const TraitSelector &selector) const {
  const std::size_t count = selector.properties.size();
  const bool exactly_one = selector.kind == OMPC_TRAIT_condition ||
                           selector.kind == OMPC_TRAIT_device_num ||
                           selector.kind == OMPC_TRAIT_uid ||
                           selector.kind == OMPC_TRAIT_atomic_default_mem_order;
  const bool at_least_one =
      selector.kind == OMPC_TRAIT_kind || selector.kind == OMPC_TRAIT_arch ||
      selector.kind == OMPC_TRAIT_isa || selector.kind == OMPC_TRAIT_vendor ||
      selector.kind == OMPC_TRAIT_extension ||
      selector.kind == OMPC_TRAIT_requires;
  if ((exactly_one && count != 1) || (at_least_one && count == 0)) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-property]: selector has "
                 "invalid property cardinality\n";
    std::abort();
  }
  if (selector.kind == OMPC_TRAIT_kind) {
    bool has_any = false;
    for (const TraitProperty &property : selector.properties) {
      has_any = has_any || property.context_kind == OMPC_CONTEXT_KIND_any;
    }
    if (has_any && count != 1) {
      std::cerr << "OMPPARSER_INVARIANT[device-kind-property]: kind(any) must "
                   "be the only property in its selector\n";
      std::abort();
    }
  }
  if (selector.kind == OMPC_TRAIT_implementation_user &&
      !selector.score.empty() && selector.properties.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[implementation-selector]: a scored "
                 "implementation-defined selector requires a property\n";
    std::abort();
  }
}

void OpenMPVariantClause::endTraitSelector() {
  TraitSelector &selector =
      requireActiveTraitSelector("variant-trait-selector");
  requireSelectorPropertyCardinality(selector);
  active_trait_selector.reset();
}

void OpenMPVariantClause::addConstructDirective(
    const char *score, std::unique_ptr<OpenMPDirective> construct_directive) {
  requireActiveTraitSet(OMPC_SELECTOR_construct, "construct-selector");
  if (active_trait_selector.has_value() || score == nullptr || *score != '\0' ||
      construct_directive == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[construct-selector]: malformed "
                 "construct selector\n";
    std::abort();
  }
  requireUniqueSelector(OMPC_TRAIT_construct,
                        std::to_string(construct_directive->getKind()),
                        "construct-selector");
  TraitSelector selector;
  selector.kind = OMPC_TRAIT_construct;
  selector.construct_directive = std::move(construct_directive);
  trait_sets[*active_trait_set].selectors.push_back(std::move(selector));
}

void OpenMPVariantClause::beginTraitSet(
    OpenMPContextSelectorSequenceKind kind) {
  switch (kind) {
  case OMPC_SELECTOR_user:
  case OMPC_SELECTOR_construct:
  case OMPC_SELECTOR_device:
  case OMPC_SELECTOR_target_device:
  case OMPC_SELECTOR_implementation:
    break;
  default:
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-set]: invalid trait set "
                 "kind\n";
    std::abort();
  }
  if (active_trait_set.has_value()) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-set]: nested trait set\n";
    std::abort();
  }
  for (const TraitSetSelector &set : trait_sets) {
    if (set.kind == kind) {
      std::cerr << "OMPPARSER_INVARIANT[variant-trait-set]: duplicate trait "
                   "set\n";
      std::abort();
    }
  }
  TraitSetSelector set;
  set.kind = kind;
  trait_sets.push_back(std::move(set));
  active_trait_set = trait_sets.size() - 1;
  active_trait_selector.reset();
}

void OpenMPVariantClause::endTraitSet() {
  if (!active_trait_set.has_value() || *active_trait_set >= trait_sets.size() ||
      active_trait_selector.has_value()) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-set]: no active trait set "
                 "to end\n";
    std::abort();
  }
  if (trait_sets[*active_trait_set].selectors.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[variant-trait-set]: trait set is empty\n";
    std::abort();
  }
  const TraitSetSelector &set = trait_sets[*active_trait_set];
  if (set.kind == OMPC_SELECTOR_device ||
      set.kind == OMPC_SELECTOR_target_device) {
    for (const TraitSelector &selector : set.selectors) {
      if (selector.kind != OMPC_TRAIT_kind) {
        continue;
      }
      for (const TraitProperty &property : selector.properties) {
        if (property.context_kind == OMPC_CONTEXT_KIND_any &&
            set.selectors.size() != 1) {
          std::cerr << "OMPPARSER_INVARIANT[device-kind-property]: kind(any) "
                       "prohibits every other property in its selector set\n";
          std::abort();
        }
      }
    }
  }
  active_trait_set.reset();
}

void OpenMPFirstprivateClause::addLangExpr(const char *expression,
                                           OpenMPClauseSeparator sep, int line,
                                           int col,
                                           OpenMPExprParseMode parse_mode) {
  if (expression == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[firstprivate-expression]: expression "
                 "is null\n";
    std::abort();
  }
  std::string normalized = normalizeClauseExpression(this->kind, expression);
  if (normalized.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[firstprivate-expression]: expression "
                 "is empty\n";
    std::abort();
  }
  if (!allow_duplicates) {
    for (size_t i = 0; i < this->expressions.size(); ++i) {
      const bool same_saved =
          i < saved_statuses.size() && saved_statuses[i] == current_saved_state;
      const bool existing_has_modifier = expressionHasDirectiveNameModifier(i);
      const bool same_modifier =
          existing_has_modifier == current_has_directive_name_modifier &&
          (!current_has_directive_name_modifier ||
           getExpressionDirectiveNameModifier(i) ==
               current_directive_name_modifier);
      if (!strcmp(expressions.at(i), normalized.c_str()) && same_saved &&
          same_modifier) {
        failDuplicateClauseExpression(kind, normalized.c_str());
      }
    }
  }

  const OpenMPExprParseMode effective_parse_mode =
      resolveClauseExpressionParseMode(this->kind, parse_mode, normalized);
  const void *expression_node =
      openmpParseExpressionNode(this->directive_kind, this->kind,
                                effective_parse_mode, normalized.c_str());
  size_t length = normalized.size();
  auto owned_value = std::make_unique<char[]>(length + 1);
  std::memcpy(owned_value.get(), normalized.c_str(), length + 1);
  const char *stored_expression = owned_value.get();
  expressions.push_back(stored_expression);
  expressionNodes.push_back(expression_node);
  expression_separators.push_back(sep);
  owned_expressions.push_back(std::move(owned_value));
  locations.push_back(SourceLocation(line, col));
  saved_statuses.push_back(current_saved_state);
  directive_name_modifier_statuses.push_back(
      current_has_directive_name_modifier);
  directive_name_modifiers.push_back(current_has_directive_name_modifier
                                         ? current_directive_name_modifier
                                         : OMPD_unknown);
};

void OpenMPInductionClause::addStepExpression(const char *expression) {
  if (expression == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[induction-step]: expression is null\n";
    std::abort();
  }
  std::string cleaned = trimWhitespace(std::string(expression));
  std::string normalized =
      normalizeClauseExpression(OMPC_induction, cleaned.c_str());
  if (normalized.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[induction-step]: expression is empty\n";
    std::abort();
  }

  if (step_expression.empty()) {
    step_expression = std::move(normalized);
    addAuxiliaryLangExpr(step_expression);
    sequence.push_back({ItemStep, 0});
    return;
  }

  passthrough_items.push_back(std::move(normalized));
  addAuxiliaryLangExpr(passthrough_items.back());
  sequence.push_back({ItemPassthrough, passthrough_items.size() - 1});
}

void OpenMPInductionClause::addBinding(const char *label,
                                       const char *expression) {
  if (label == nullptr || expression == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[induction-binding]: label or expression "
                 "is null\n";
    std::abort();
  }
  Binding binding;
  binding.label = trimWhitespace(std::string(label));
  std::string cleaned = trimWhitespace(std::string(expression));
  binding.expression =
      normalizeClauseExpression(OMPC_induction, cleaned.c_str());
  if (binding.label.empty() || binding.expression.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[induction-binding]: label or expression "
                 "is empty\n";
    std::abort();
  }
  bindings.push_back(std::move(binding));
  addAuxiliaryLangExpr(bindings.back().expression);
  sequence.push_back({ItemBinding, bindings.size() - 1});
}

void OpenMPInductionClause::addPassthroughItem(const char *expression) {
  if (expression == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[induction-item]: expression is null\n";
    std::abort();
  }
  std::string cleaned = trimWhitespace(std::string(expression));
  std::string normalized =
      normalizeClauseExpression(OMPC_induction, cleaned.c_str());
  if (normalized.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[induction-item]: expression is empty\n";
    std::abort();
  }
  passthrough_items.push_back(std::move(normalized));
  addAuxiliaryLangExpr(passthrough_items.back());
  sequence.push_back({ItemPassthrough, passthrough_items.size() - 1});
}

void OpenMPMapClause::addItem(const std::string &expr,
                              OpenMPClauseSeparator sep) {
  std::string array_section_expression;
  std::string dist_data_arguments;
  bool has_dist_data = splitMapExpressionDistDataSuffix(
      expr, array_section_expression, dist_data_arguments);

  const std::string trimmed_expression = trimWhitespace(expr);
  if (trimmed_expression.empty()) {
    std::cerr << "OMPPARSER_INVARIANT[map-item]: map item is empty\n";
    std::abort();
  }
  std::string item_expression = trimmed_expression;
  if (has_dist_data) {
    item_expression =
        array_section_expression + " dist_data(" + dist_data_arguments + ")";
  }
  items.push_back(OpenMPExpressionItem{item_expression, sep});

  const std::string parsed_expression =
      has_dist_data ? array_section_expression : trimmed_expression;
  addLangExpr(parsed_expression.c_str(), sep, 0, 0,
              OMP_EXPR_PARSE_array_section);

  std::vector<DistDataPolicy> parsed_policies;
  if (has_dist_data) {
    const std::vector<std::string> policy_texts =
        splitTopLevelCommaSeparated(dist_data_arguments);
    for (const std::string &raw_policy : policy_texts) {
      const std::string policy_text = trimWhitespace(raw_policy);
      if (policy_text.empty()) {
        std::cerr << "OMPPARSER_SEMANTIC[dist-data-policy]: policy is empty\n";
        std::abort();
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
          std::cerr
              << "OMPPARSER_SEMANTIC[dist-data-policy]: malformed policy '"
              << policy_text << "'\n";
          std::abort();
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
      } else {
        std::cerr
            << "OMPPARSER_SEMANTIC[dist-data-policy]: unsupported policy '"
            << policy_name << "'\n";
        std::abort();
      }

      policy.argument = policy_argument;
      if (!policy_argument.empty()) {
        policy.argument_node = openmpParseExpressionNode(
            this->directive_kind, this->kind, OMP_EXPR_PARSE_expression,
            policy_argument.c_str());
      }

      parsed_policies.push_back(std::move(policy));
    }
  }

  dist_data_policies.push_back(std::move(parsed_policies));
}

void OpenMPAdjustArgsClause::addArgument(const std::string &arg) {
  std::string cleaned =
      normalizeClauseExpression(OMPC_adjust_args, arg.c_str());
  arguments.push_back(cleaned);
  addAuxiliaryLangExpr(cleaned);
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
  addAuxiliaryLangExpr(cleaned);
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
  addAuxiliaryLangExpr(traits_cleaned);
  addAuxiliaryLangExpr(user_cleaned);
  auto usesAllocatorsAllocator = std::make_unique<usesAllocatorParameter>(
      _allocator, traits_cleaned, user_cleaned);
  usesAllocatorsAllocatorSequenceView.push_back(usesAllocatorsAllocator.get());
  usesAllocatorsAllocatorSequenceStorage.push_back(
      std::move(usesAllocatorsAllocator));
}

void OpenMPInitClause::addInteropType(OpenMPInitClauseKind value) {
  if (value == OMPC_INIT_KIND_unknown) {
    fprintf(stderr,
            "OMP_PARSER_INVARIANT[init]: unknown interop type modifier\n");
    std::abort();
  }
  for (OpenMPInitClauseKind existing : interop_types) {
    if (existing == value) {
      fprintf(stderr,
              "OMP_PARSER_INVARIANT[init]: duplicate interop type modifier\n");
      std::abort();
    }
  }
  interop_types.push_back(value);
  modifier_sequence.push_back({ModifierInteropType, interop_types.size() - 1});
}

void OpenMPInitClause::addInteropType(const std::string &raw_type) {
  const std::string trimmed_type = trimWhitespace(raw_type);
  if (trimmed_type.empty()) {
    fprintf(stderr, "OMP_PARSER_INVARIANT[init]: empty interop type name\n");
    std::abort();
  }
  for (const std::string &existing : raw_interop_types) {
    if (existing == trimmed_type) {
      fprintf(stderr,
              "OMP_PARSER_INVARIANT[init]: duplicate interop type name\n");
      std::abort();
    }
  }
  raw_interop_types.push_back(trimmed_type);
  modifier_sequence.push_back(
      {ModifierRawInteropType, raw_interop_types.size() - 1});
}

void OpenMPInitClause::setDirectiveNameModifier(OpenMPDirectiveKind value) {
  if (has_directive_name_modifier ||
      (value != OMPD_depobj && value != OMPD_interop)) {
    fprintf(stderr,
            "OMP_PARSER_INVARIANT[init]: invalid or duplicate directive-name "
            "modifier\n");
    std::abort();
  }
  has_directive_name_modifier = true;
  directive_name_modifier = value;
  modifier_sequence.push_back({ModifierDirectiveName, 0});
}

void OpenMPInitClause::setPreferType(const std::string &spec) {
  const std::string trimmed_spec = trimWhitespace(spec);
  if (has_prefer_type || trimmed_spec.empty()) {
    fprintf(stderr,
            "OMP_PARSER_INVARIANT[init]: empty or duplicate prefer_type "
            "modifier\n");
    std::abort();
  }
  has_prefer_type = true;
  prefer_type_spec = trimmed_spec;
  addAuxiliaryLangExpr(prefer_type_spec);
  modifier_sequence.push_back({ModifierPreferType, 0});
}

void OpenMPInitClause::setDepinfo(OpenMPDependClauseType type,
                                  const std::string &locator) {
  const std::string trimmed_locator = trimWhitespace(locator);
  if (has_depinfo || type == OMPC_DEPENDENCE_TYPE_unknown ||
      trimmed_locator.empty()) {
    fprintf(stderr, "OMP_PARSER_INVARIANT[init]: invalid or duplicate depinfo "
                    "modifier\n");
    std::abort();
  }
  has_depinfo = true;
  depinfo_type = type;
  depinfo_locator = trimmed_locator;
  addAuxiliaryLangExpr(depinfo_locator);
  modifier_sequence.push_back({ModifierDepinfo, 0});
}

void OpenMPInitClause::setOperand(const std::string &value) {
  const std::string trimmed_operand = trimWhitespace(value);
  if (!operand.empty() || trimmed_operand.empty()) {
    fprintf(stderr, "OMP_PARSER_INVARIANT[init]: empty or duplicate operand\n");
    std::abort();
  }
  operand = normalizeClauseExpression(OMPC_init, trimmed_operand.c_str());
  addLangExpr(operand.c_str(), OMPC_CLAUSE_SEP_space, 0, 0,
              OMP_EXPR_PARSE_expression);
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

  for (const ModifierRef &modifier : modifier_sequence) {
    switch (modifier.kind) {
    case ModifierDirectiveName:
      appendModifier(directive_name_modifier == OMPD_depobj ? "depobj"
                                                            : "interop");
      break;
    case ModifierPreferType:
      appendModifier("prefer_type(" + prefer_type_spec + ")");
      break;
    case ModifierDepinfo: {
      std::string keyword;
      switch (depinfo_type) {
      case OMPC_DEPENDENCE_TYPE_in:
        keyword = "in";
        break;
      case OMPC_DEPENDENCE_TYPE_out:
        keyword = "out";
        break;
      case OMPC_DEPENDENCE_TYPE_inout:
        keyword = "inout";
        break;
      case OMPC_DEPENDENCE_TYPE_inoutset:
        keyword = "inoutset";
        break;
      case OMPC_DEPENDENCE_TYPE_mutexinoutset:
        keyword = "mutexinoutset";
        break;
      default:
        std::abort();
      }
      appendModifier(keyword + "(" + depinfo_locator + ")");
      break;
    }
    case ModifierInteropType:
      if (modifier.index >= interop_types.size()) {
        std::abort();
      }
      appendModifier(interop_types[modifier.index] == OMPC_INIT_KIND_target
                         ? "target"
                         : "targetsync");
      break;
    case ModifierRawInteropType:
      if (modifier.index >= raw_interop_types.size()) {
        std::abort();
      }
      appendModifier(raw_interop_types[modifier.index]);
      break;
    default:
      std::abort();
    }
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
  std::vector<OpenMPClause *> *current_clauses = getOrCreateClauses(kind);
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
    if (clause_kind == OMPC_nowait) {
      return std::make_unique<OpenMPNowaitClause>();
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
    if (current_clauses->size() == 0) {
      new_clause = registerClause(makeClause(kind));
      current_clauses->push_back(new_clause);
    } else if (isExplicitSingletonClause(kind)) {
      failDuplicateClause(this->getKind(), kind);
    } else if (this->getNormalizeClauses()) {
      new_clause = current_clauses->at(0);
    } else {
      new_clause = registerClause(makeClause(kind));
      current_clauses->push_back(new_clause);
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
      failDuplicateClause(this->getKind(), kind);
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
      failDuplicateClause(this->getKind(), kind);
    }
    break;
  }

  case OMPC_at: {
    OpenMPAtClauseKind at_kind = (OpenMPAtClauseKind)va_arg(args, int);
    if (current_clauses->size() == 0) {
      new_clause = registerClause(std::make_unique<OpenMPAtClause>(at_kind));
      current_clauses->push_back(new_clause);
    } else {
      failDuplicateClause(this->getKind(), kind);
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
    std::vector<OpenMPClause *> *current_clauses =
        getOrCreateClauses(OMPC_doacross);
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
    std::vector<OpenMPClause *> *current_clauses =
        getOrCreateClauses(OMPC_grainsize);
    if (current_clauses->size() == 0) {
      new_clause =
          registerClause(std::make_unique<OpenMPGrainsizeClause>(modifier));
      current_clauses->push_back(new_clause);
    } else {
      failDuplicateClause(this->getKind(), kind);
    }
    break;
  }
  case OMPC_num_tasks: {
    OpenMPNumTasksClauseModifier modifier =
        (OpenMPNumTasksClauseModifier)va_arg(args, int);
    std::vector<OpenMPClause *> *current_clauses =
        getOrCreateClauses(OMPC_num_tasks);
    if (current_clauses->size() == 0) {
      new_clause =
          registerClause(std::make_unique<OpenMPNumTasksClause>(modifier));
      current_clauses->push_back(new_clause);
    } else {
      failDuplicateClause(this->getKind(), kind);
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
      failDuplicateClause(this->getKind(), kind);
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
    std::cerr << "OMPPARSER_INVARIANT[clause-kind]: directive "
              << this->getKind() << " cannot construct clause " << kind << "\n";
    std::abort();
  }
  };

  if (new_clause == nullptr) {
    std::cerr << "OMPPARSER_INVARIANT[clause-construction]: directive "
              << this->getKind() << " produced no clause for kind " << kind
              << "\n";
    std::abort();
  }
  if (clause_separator_comma) {
    new_clause->setPrecedingSeparator(OMPC_CLAUSE_SEP_comma);
  } else {
    new_clause->setPrecedingSeparator(OMPC_CLAUSE_SEP_space);
  }
  clause_separator_comma = false;

  va_end(args);
  if (new_clause->getClausePosition() == -1) {
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
      directive->getOrCreateClauses(OMPC_map);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPMapClause>(modifier1, modifier2, modifier3, type,
                                          ref_modifier, mapper_identifier));
    static_cast<OpenMPMapClause *>(new_clause)->captureMapperIdentifierNode();
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
    static_cast<OpenMPMapClause *>(new_clause)->captureMapperIdentifierNode();
    current_clauses->push_back(new_clause);
  }
  return new_clause;
};

OpenMPClause *OpenMPTaskReductionClause::addTaskReductionClause(
    OpenMPDirective *directive, OpenMPTaskReductionClauseIdentifier identifier,
    char *user_defined_identifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_task_reduction);
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
      directive->getOrCreateClauses(OMPC_defaultmap);
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
      directive->getOrCreateClauses(OMPC_device_type);
  OpenMPClause *new_clause = NULL;
  new_clause = directive->registerClause(
      std::make_unique<OpenMPDeviceTypeClause>(device_type_kind));
  current_clauses->push_back(new_clause);

  return new_clause;
};

OpenMPClause *OpenMPProcBindClause::addProcBindClause(
    OpenMPDirective *directive, OpenMPProcBindClauseKind proc_bind_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_proc_bind);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPProcBindClause>(proc_bind_kind));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    failDuplicateClause(directive->getKind(), OMPC_proc_bind);
  };
  return new_clause;
};

OpenMPClause *
OpenMPBindClause::addBindClause(OpenMPDirective *directive,
                                OpenMPBindClauseBinding bind_binding) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_bind);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPBindClause>(bind_binding));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    failDuplicateClause(directive->getKind(), OMPC_bind);
  };

  return new_clause;
};

OpenMPClause *
OpenMPLinearClause::addLinearClause(OpenMPDirective *directive,
                                    OpenMPLinearClauseModifier modifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_linear);
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

  std::vector<OpenMPClause *> *current_clauses = directive->getOrCreateClauses(
      OMPC_ext_implementation_defined_requirement);
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

  std::vector<OpenMPClause *> *current_clauses = directive->getOrCreateClauses(
      OMPC_ext_implementation_defined_requirement);

  for (std::vector<OpenMPClause *>::iterator it = current_clauses->begin();
       it != current_clauses->end() - 1; it++) {
    if (((OpenMPExtImplementationDefinedRequirementClause *)(*it))
            ->getImplementationDefinedRequirement() ==
        ((OpenMPExtImplementationDefinedRequirementClause *)current_clause)
            ->getImplementationDefinedRequirement()) {
      failDuplicateClause(directive->getKind(),
                          OMPC_ext_implementation_defined_requirement);
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
      directive->getOrCreateClauses(OMPC_linear);

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
      const auto &current_locations =
          static_cast<OpenMPLinearClause *>(current_clause)
              ->getExpressionLocations();

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
            failDuplicateClauseExpression(OMPC_linear, *it_expr_current);
          }
        }
        if (!not_normalize) {
          OpenMPClauseSeparator sep = OMPC_CLAUSE_SEP_space;
          if (idx < current_separators.size()) {
            sep = current_separators[idx];
          }

          int line = 0;
          int col = 0;
          if (idx < current_locations.size()) {
            line = current_locations[idx].getLine();
            col = current_locations[idx].getColumn();
          }

          (*it)->addLangExpr(*it_expr_current, sep, line, col,
                             OMP_EXPR_PARSE_variable_list);
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
      directive->getOrCreateClauses(OMPC_reduction);
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
      directive->getOrCreateClauses(OMPC_from);
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

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_to);
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
      directive->getOrCreateClauses(OMPC_affinity);
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
      directive->getOrCreateClauses(OMPC_depend);
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
      directive->getOrCreateClauses(OMPC_depend);

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
          const auto &current_locations =
              static_cast<OpenMPDependClause *>(current_clause)
                  ->getExpressionLocations();
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
                failDuplicateClauseExpression(OMPC_depend, *it_expr_current);
              }
            }
            if (para_merge == true) {
              const OpenMPClauseSeparator sep =
                  (idx < current_separators.size())
                      ? (has_existing && current_separators[idx] ==
                                             OMPC_CLAUSE_SEP_space
                             ? OMPC_CLAUSE_SEP_comma
                             : current_separators[idx])
                      : OMPC_CLAUSE_SEP_comma;

              int line = 0;
              int col = 0;
              if (idx < current_locations.size()) {
                line = current_locations[idx].getLine();
                col = current_locations[idx].getColumn();
              }

              (*it)->addLangExpr(*it_expr_current, sep, line, col,
                                 OMP_EXPR_PARSE_variable_list);
            }
          }
          current_clauses->pop_back();
          directive->getClausesInOriginalOrder()->pop_back();
          break;
        }
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
      const auto &current_locations =
          static_cast<OpenMPDependClause *>(current_clause)
              ->getExpressionLocations();
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
            failDuplicateClauseExpression(OMPC_depend, *it_expr_current);
          }
        }
        if (para_merge == true) {
          const OpenMPClauseSeparator sep =
              (idx < current_separators.size())
                  ? (has_existing &&
                             current_separators[idx] == OMPC_CLAUSE_SEP_space
                         ? OMPC_CLAUSE_SEP_comma
                         : current_separators[idx])
                  : OMPC_CLAUSE_SEP_comma;

          int line = 0;
          int col = 0;
          if (idx < current_locations.size()) {
            line = current_locations[idx].getLine();
            col = current_locations[idx].getColumn();
          }

          (*it)->addLangExpr(*it_expr_current, sep, line, col,
                             OMP_EXPR_PARSE_variable_list);
        }
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
      directive->getOrCreateClauses(OMPC_depobj_update);
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
      directive->getOrCreateClauses(OMPC_in_reduction);
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
      directive->getOrCreateClauses(OMPC_atomic_default_mem_order);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAtomicDefaultMemOrderClause>(
            atomic_default_mem_order_kind));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    failDuplicateClause(directive->getKind(), OMPC_atomic_default_mem_order);
  }
  return new_clause;
};

OpenMPClause *OpenMPAllocatorClause::addAllocatorClause(
    OpenMPDirective *directive, OpenMPAllocatorClauseAllocator allocator,
    char *user_defined_allocator) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_allocator);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPAllocatorClause>(allocator));
    if (allocator == OMPC_ALLOCATOR_ALLOCATOR_user)
      ((OpenMPAllocatorClause *)new_clause)
          ->setUserDefinedAllocator(user_defined_allocator);
    current_clauses->push_back(new_clause);
  } else {
    failDuplicateClause(directive->getKind(), OMPC_allocator);
  }
  return new_clause;
};

OpenMPClause *
OpenMPAllocateClause::addAllocateClause(OpenMPDirective *directive,
                                        OpenMPAllocateClauseAllocator allocator,
                                        char *user_defined_allocator) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_allocate);
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
      directive->getOrCreateClauses(OMPC_initializer);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPInitializerClause>(priv));
    if (priv == OMPC_INITIALIZER_PRIV_user)
      ((OpenMPInitializerClause *)new_clause)
          ->setUserDefinedPriv(user_defined_priv);
    current_clauses->push_back(new_clause);
  } else {
    failDuplicateClause(directive->getKind(), OMPC_initializer);
  }
  return new_clause;
};

OpenMPClause *
OpenMPDeviceClause::addDeviceClause(OpenMPDirective *directive,
                                    OpenMPDeviceClauseModifier modifier) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_device);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDeviceClause>(modifier));
    current_clauses->push_back(new_clause);
  } else {
    failDuplicateClause(directive->getKind(), OMPC_device);
  }
  return new_clause;
};

OpenMPClause *OpenMPScheduleClause::addScheduleClause(
    OpenMPDirective *directive, OpenMPScheduleClauseModifier modifier1,
    OpenMPScheduleClauseModifier modifier2,
    OpenMPScheduleClauseKind schedule_kind, char *user_defined_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_schedule);
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
    failDuplicateClause(directive->getKind(), OMPC_schedule);
  }
  return new_clause;
};

OpenMPClause *OpenMPDistScheduleClause::addDistScheduleClause(
    OpenMPDirective *directive,
    OpenMPDistScheduleClauseKind dist_schedule_kind) {
  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_dist_schedule);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDistScheduleClause>(dist_schedule_kind));
    current_clauses->push_back(new_clause);
  } else {
    failDuplicateClause(directive->getKind(), OMPC_dist_schedule);
  }
  return new_clause;
};

OpenMPClause *OpenMPLastprivateClause::addLastprivateClause(
    OpenMPDirective *directive, OpenMPLastprivateClauseModifier modifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_lastprivate);
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

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_if);
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
    for (OpenMPClause *clause : *current_clauses) {
      auto *if_clause = static_cast<OpenMPIfClause *>(clause);
      const std::string current_user_modifier =
          user_defined_modifier != nullptr ? std::string(user_defined_modifier)
                                           : std::string();
      if (if_clause->getModifier() == modifier &&
          if_clause->getUserDefinedModifier() == current_user_modifier) {
        failDuplicateClause(directive->getKind(), OMPC_if);
      }
    }
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
      directive->getOrCreateClauses(OMPC_default);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPDefaultClause>(default_kind));
    current_clauses->push_back(new_clause);
  } else { /* could be an error since if clause may only appear once */
    failDuplicateClause(directive->getKind(), OMPC_default);
  };

  return new_clause;
};

OpenMPClause *
OpenMPOrderClause::addOrderClause(OpenMPDirective *directive,
                                  OpenMPOrderClauseModifier order_modifier,
                                  OpenMPOrderClauseKind order_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_order);
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
    failDuplicateClause(directive->getKind(), OMPC_order);
  };

  return new_clause;
};

OpenMPClause *
OpenMPOrderClause::addOrderClause(OpenMPDirective *directive,
                                  OpenMPOrderClauseKind order_kind) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_order);
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
    failDuplicateClause(directive->getKind(), OMPC_order);
  };

  return new_clause;
};

OpenMPClause *
OpenMPAlignedClause::addAlignedClause(OpenMPDirective *directive) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getOrCreateClauses(OMPC_aligned);
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
      directive->getOrCreateClauses(OMPC_when);
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
      directive->getOrCreateClauses(OMPC_otherwise);
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
      directive->getOrCreateClauses(OMPC_match);
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
      directive->getOrCreateClauses(OMPC_uses_allocators);
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
