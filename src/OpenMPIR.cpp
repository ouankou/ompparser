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
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
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

bool equalsIgnoringAsciiCase(const std::string &left, const char *right) {
  const size_t right_size = std::strlen(right);
  return left.size() == right_size &&
         std::equal(
             left.begin(), left.end(), right,
             [](unsigned char left_character, unsigned char right_character) {
               return std::tolower(left_character) ==
                      std::tolower(right_character);
             });
}

void requireAtomicDefaultMemOrder(OpenMPAtomicDefaultMemOrderClauseKind kind) {
  switch (kind) {
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_seq_cst:
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acq_rel:
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acquire:
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_release:
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_relaxed:
    return;
  case OMPC_ATOMIC_DEFAULT_MEM_ORDER_unknown:
    break;
  }
  throw std::invalid_argument("invalid atomic default memory order");
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

ompparser::HostFragment
makeHostFragment(const char *spelling, ompparser::HostFragmentRole role,
                 OpenMPExprParseMode parse_mode = OMP_EXPR_PARSE_none) {
  ompparser::HostFragment fragment;
  if (spelling == nullptr) {
    return fragment;
  }
  fragment.spelling = spelling;
  fragment.role = role;
  if (parse_mode == OMP_EXPR_PARSE_none) {
    switch (role) {
    case ompparser::HostFragmentRole::Variable:
      parse_mode = OMP_EXPR_PARSE_variable_list;
      break;
    case ompparser::HostFragmentRole::Locator:
      parse_mode = OMP_EXPR_PARSE_array_section;
      break;
    case ompparser::HostFragmentRole::Verbatim:
      parse_mode = OMP_EXPR_PARSE_verbatim;
      break;
    case ompparser::HostFragmentRole::Expression:
    case ompparser::HostFragmentRole::Condition:
    case ompparser::HostFragmentRole::Type:
    case ompparser::HostFragmentRole::Declarator:
    case ompparser::HostFragmentRole::Initializer:
      parse_mode = OMP_EXPR_PARSE_expression;
      break;
    }
  }
  fragment.parse_mode = parse_mode;
  const bool has_source_range =
      openmpGetLexemeSourceRange(spelling, fragment.range);
  if (role != ompparser::HostFragmentRole::Expression ||
      parse_mode == OMP_EXPR_PARSE_openmp_context_name) {
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

void assignSubfragmentRange(ompparser::HostFragment &fragment,
                            const std::string &source, std::size_t begin,
                            std::size_t end,
                            const ompparser::SourceRange &source_range,
                            bool has_source_range) {
  if (!has_source_range || begin > end || end > source.size()) {
    return;
  }
  fragment.range.begin = source_range.begin;
  advanceSourcePosition(fragment.range.begin, source, 0, begin);
  fragment.range.end = fragment.range.begin;
  advanceSourcePosition(fragment.range.end, source, begin, end);
}

} // namespace

namespace {

bool isIdentifierChar(char ch) {
  const unsigned char uch = static_cast<unsigned char>(ch);
  return std::isalnum(uch) != 0 || ch == '_';
}

struct ParsedQuotedLiteral {
  std::string::size_type content_begin = 0;
  std::string::size_type content_end = 0;
  std::string::size_type end = 0;
  bool raw = false;
};

bool parseCxxQuotedLiteralAt(const std::string &text,
                             std::string::size_type begin,
                             ParsedQuotedLiteral *literal) {
  if (literal == nullptr || begin >= text.size() ||
      (begin > 0 && isIdentifierChar(text[begin - 1]))) {
    return false;
  }

  std::string::size_type quote = begin;
  if (text.compare(quote, 2, "u8") == 0) {
    quote += 2;
  } else if (text[quote] == 'u' || text[quote] == 'U' || text[quote] == 'L') {
    ++quote;
  }

  bool raw = false;
  if (quote < text.size() && text[quote] == 'R') {
    raw = true;
    ++quote;
  }
  if (quote >= text.size() ||
      (raw ? text[quote] != '"' : text[quote] != '"' && text[quote] != '\'')) {
    return false;
  }

  if (raw) {
    const std::string::size_type delimiter_begin = quote + 1;
    const std::string::size_type open = text.find('(', delimiter_begin);
    if (open == std::string::npos || open - delimiter_begin > 16) {
      return false;
    }
    for (std::string::size_type index = delimiter_begin; index < open;
         ++index) {
      const unsigned char character = static_cast<unsigned char>(text[index]);
      if (std::isspace(character) || text[index] == '(' || text[index] == ')' ||
          text[index] == '\\') {
        return false;
      }
    }
    const std::string delimiter =
        text.substr(delimiter_begin, open - delimiter_begin);
    const std::string terminator = ")" + delimiter + "\"";
    const std::string::size_type close = text.find(terminator, open + 1);
    if (close == std::string::npos) {
      return false;
    }
    literal->content_begin = open + 1;
    literal->content_end = close;
    literal->end = close + terminator.size();
    literal->raw = true;
    return true;
  }

  const char delimiter = text[quote];
  bool escaped = false;
  for (std::string::size_type index = quote + 1; index < text.size(); ++index) {
    const char character = text[index];
    if (escaped) {
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == delimiter) {
      literal->content_begin = quote + 1;
      literal->content_end = index;
      literal->end = index + 1;
      return true;
    }
  }
  return false;
}

bool parseFortranQuotedLiteralAt(const std::string &text,
                                 std::string::size_type begin,
                                 ParsedQuotedLiteral *literal) {
  if (literal == nullptr || begin >= text.size() ||
      (text[begin] != '\'' && text[begin] != '"')) {
    return false;
  }

  const char delimiter = text[begin];
  for (std::string::size_type index = begin + 1; index < text.size(); ++index) {
    if (text[index] != delimiter) {
      continue;
    }
    if (index + 1 < text.size() && text[index + 1] == delimiter) {
      ++index;
      continue;
    }
    literal->content_begin = begin + 1;
    literal->content_end = index;
    literal->end = index + 1;
    literal->raw = false;
    return true;
  }
  return false;
}

std::vector<bool> quotedLiteralCharacters(const std::string &text) {
  std::vector<bool> quoted(text.size(), false);
  for (std::string::size_type index = 0; index < text.size();) {
    ParsedQuotedLiteral literal;
    if (!parseCxxQuotedLiteralAt(text, index, &literal)) {
      ++index;
      continue;
    }
    std::fill(quoted.begin() + index, quoted.begin() + literal.end, true);
    index = literal.end;
  }
  return quoted;
}

std::vector<std::string> splitTopLevelCommaSeparated(const std::string &text) {
  std::vector<std::string> parts;
  std::string::size_type part_begin = 0;
  int paren_depth = 0;
  int bracket_depth = 0;
  int brace_depth = 0;

  for (std::string::size_type index = 0; index < text.size(); ++index) {
    ParsedQuotedLiteral literal;
    if (parseCxxQuotedLiteralAt(text, index, &literal)) {
      index = literal.end - 1;
      continue;
    }
    const char ch = text[index];
    if (ch == '\'' || ch == '"') {
      return {};
    }
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
  for (std::string::size_type index = 0; index < trimmed_expression.size();
       ++index) {
    ParsedQuotedLiteral literal;
    if (parseCxxQuotedLiteralAt(trimmed_expression, index, &literal)) {
      index = literal.end - 1;
      continue;
    }
    const char ch = trimmed_expression[index];
    if (ch == '\'' || ch == '"') {
      return false;
    }
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
  const std::vector<bool> quoted = quotedLiteralCharacters(trimmed_expression);
  for (std::string::size_type i = trimmed_expression.size(); i-- > 0;) {
    if (quoted[i]) {
      continue;
    }
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

struct IteratorDelimiterState {
  int parentheses = 0;
  int brackets = 0;
  int braces = 0;
  int angles = 0;
  char quote = '\0';
  bool escaped = false;
  bool raw_string = false;
  bool raw_string_body_started = false;
  std::string raw_string_delimiter;
  std::string raw_string_terminator;

  bool structuralTopLevel() const {
    return parentheses == 0 && brackets == 0 && braces == 0;
  }
  bool fullyTopLevel() const { return structuralTopLevel() && angles == 0; }
  bool balanced() const {
    return fullyTopLevel() && quote == '\0' && !escaped && !raw_string;
  }
  void consumeQuoted(const std::string &text, std::size_t index) {
    const char character = text[index];
    if (quote == '\0') {
      if (character == '\'' || character == '"') {
        quote = character;
        raw_string = character == '"' && index > 0 && text[index - 1] == 'R';
        raw_string_body_started = false;
        raw_string_delimiter.clear();
        raw_string_terminator.clear();
      }
      return;
    }
    if (raw_string) {
      if (!raw_string_body_started) {
        if (character == '(') {
          raw_string_body_started = true;
          raw_string_terminator = ")" + raw_string_delimiter + "\"";
        } else {
          raw_string_delimiter.push_back(character);
        }
      } else if (index + 1 >= raw_string_terminator.size() &&
                 text.compare(index + 1 - raw_string_terminator.size(),
                              raw_string_terminator.size(),
                              raw_string_terminator) == 0) {
        quote = '\0';
        raw_string = false;
        raw_string_body_started = false;
        raw_string_delimiter.clear();
        raw_string_terminator.clear();
      }
      return;
    }
    if (escaped) {
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == quote) {
      quote = '\0';
    }
  }
};

bool consumeIteratorDelimiter(IteratorDelimiterState &state,
                              const std::string &text, std::size_t index,
                              bool track_angles, std::string &error) {
  const char character = text[index];
  if (state.quote != '\0' || character == '\'' || character == '"') {
    state.consumeQuoted(text, index);
    return true;
  }
  switch (character) {
  case '(':
    ++state.parentheses;
    break;
  case ')':
    if (--state.parentheses < 0) {
      error = "unmatched ')' delimiter";
      return false;
    }
    break;
  case '[':
    ++state.brackets;
    break;
  case ']':
    if (--state.brackets < 0) {
      error = "unmatched ']' delimiter";
      return false;
    }
    break;
  case '{':
    ++state.braces;
    break;
  case '}':
    if (--state.braces < 0) {
      error = "unmatched '}' delimiter";
      return false;
    }
    break;
  case '<':
    if (track_angles && state.structuralTopLevel()) {
      ++state.angles;
    }
    break;
  case '>':
    if (track_angles && state.structuralTopLevel() && state.angles > 0) {
      --state.angles;
    }
    break;
  default:
    break;
  }
  return true;
}

bool remainderBeginsIterator(const std::string &text, std::size_t begin,
                             std::string &error) {
  IteratorDelimiterState state;
  for (std::size_t index = begin; index < text.size(); ++index) {
    const char character = text[index];
    if (!consumeIteratorDelimiter(state, text, index, true, error)) {
      return false;
    }
    if (state.quote != '\0') {
      continue;
    }
    if (character == '=' && state.fullyTopLevel()) {
      return !trimWhitespace(text, begin, index - begin).empty();
    }
    const bool scope_colon =
        character == ':' &&
        ((index > begin && text[index - 1] == ':') ||
         (index + 1 < text.size() && text[index + 1] == ':'));
    if (((character == ':' && !scope_colon) || character == ',') &&
        state.fullyTopLevel()) {
      return false;
    }
  }
  if (!state.balanced()) {
    error = "unbalanced delimiters after iterator separator";
  }
  return false;
}

bool splitIteratorItems(const std::string &text,
                        std::vector<std::string> &items, std::string &error) {
  IteratorDelimiterState state;
  std::size_t item_begin = 0;
  bool saw_assignment = false;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char character = text[index];
    if (!consumeIteratorDelimiter(state, text, index, !saw_assignment, error)) {
      return false;
    }
    if (state.quote != '\0') {
      continue;
    }
    if (character == '=' && state.fullyTopLevel()) {
      saw_assignment = true;
    } else if (character == ',' && state.fullyTopLevel() && saw_assignment) {
      std::string remainder_error;
      if (remainderBeginsIterator(text, index + 1, remainder_error)) {
        const std::string item =
            trimWhitespace(text, item_begin, index - item_begin);
        if (item.empty()) {
          error = "iterator list contains an empty item";
          return false;
        }
        items.push_back(item);
        item_begin = index + 1;
        saw_assignment = false;
        state = {};
      } else if (!remainder_error.empty()) {
        error = std::move(remainder_error);
        return false;
      }
    }
  }
  if (!state.balanced()) {
    error = "iterator list has unbalanced delimiters";
    return false;
  }
  if (!saw_assignment) {
    error = "iterator item has no assignment";
    return false;
  }
  const std::string item = trimWhitespace(text, item_begin, text.size());
  if (item.empty()) {
    error = "iterator list has no final item";
    return false;
  }
  items.push_back(item);
  return true;
}

std::size_t findIteratorAssignment(const std::string &item,
                                   std::string &error) {
  IteratorDelimiterState state;
  for (std::size_t index = 0; index < item.size(); ++index) {
    const char character = item[index];
    if (!consumeIteratorDelimiter(state, item, index, true, error)) {
      return std::string::npos;
    }
    if (state.quote == '\0' && character == '=' && state.fullyTopLevel()) {
      return index;
    }
  }
  error = "iterator item has no top-level assignment";
  return std::string::npos;
}

bool splitIteratorRange(const std::string &range,
                        std::vector<std::string> &fields, std::string &error) {
  IteratorDelimiterState state;
  std::size_t field_begin = 0;
  unsigned ternary_depth = 0;
  for (std::size_t index = 0; index < range.size(); ++index) {
    const char character = range[index];
    if (!consumeIteratorDelimiter(state, range, index, false, error)) {
      return false;
    }
    if (state.quote != '\0') {
      continue;
    }
    if (character == '?' && state.structuralTopLevel()) {
      ++ternary_depth;
      continue;
    }
    const bool scope_colon =
        character == ':' &&
        ((index > 0 && range[index - 1] == ':') ||
         (index + 1 < range.size() && range[index + 1] == ':'));
    if (character == ':' && !scope_colon && state.structuralTopLevel()) {
      if (ternary_depth > 0) {
        --ternary_depth;
      } else {
        const std::string field =
            trimWhitespace(range, field_begin, index - field_begin);
        if (field.empty()) {
          error = "iterator range contains an empty field";
          return false;
        }
        fields.push_back(field);
        field_begin = index + 1;
      }
    }
  }
  if (!state.balanced() || ternary_depth != 0) {
    error = "iterator range has unbalanced delimiters";
    return false;
  }
  const std::string field =
      trimWhitespace(range, field_begin, range.size() - field_begin);
  if (field.empty()) {
    error = "iterator range has an empty final field";
    return false;
  }
  fields.push_back(field);
  if (fields.size() < 2 || fields.size() > 3) {
    error = "iterator range must contain begin:end[:step]";
    return false;
  }
  return true;
}

bool isOpenMPIdentifierSpelling(const std::string &spelling) {
  return !spelling.empty() &&
         (std::isalpha(static_cast<unsigned char>(spelling.front())) ||
          spelling.front() == '_') &&
         std::all_of(spelling.begin() + 1, spelling.end(),
                     [](unsigned char character) {
                       return std::isalnum(character) || character == '_';
                     });
}

int hexadecimalDigitValue(char character) {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

bool appendUtf8Scalar(std::uint32_t value, std::string &output) {
  if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
    return false;
  }
  if (value <= 0x7f) {
    output.push_back(static_cast<char>(value));
  } else if (value <= 0x7ff) {
    output.push_back(static_cast<char>(0xc0 | (value >> 6)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else if (value <= 0xffff) {
    output.push_back(static_cast<char>(0xe0 | (value >> 12)));
    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  } else {
    output.push_back(static_cast<char>(0xf0 | (value >> 18)));
    output.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
    output.push_back(static_cast<char>(0x80 | (value & 0x3f)));
  }
  return true;
}

std::optional<std::string>
decodeCxxStringLiteral(const std::string &spelling,
                       const ParsedQuotedLiteral &literal) {
  if (literal.raw) {
    return spelling.substr(literal.content_begin,
                           literal.content_end - literal.content_begin);
  }
  if (literal.content_begin == 0 ||
      spelling[literal.content_begin - 1] != '"') {
    return std::nullopt;
  }

  std::string identity;
  identity.reserve(literal.content_end - literal.content_begin);
  for (std::size_t index = literal.content_begin;
       index < literal.content_end;) {
    const char character = spelling[index++];
    if (character != '\\') {
      identity.push_back(character);
      continue;
    }
    if (index == literal.content_end) {
      return std::nullopt;
    }

    const char escaped = spelling[index++];
    switch (escaped) {
    case '\'':
    case '"':
    case '?':
    case '\\':
      identity.push_back(escaped);
      continue;
    case 'a':
      identity.push_back('\a');
      continue;
    case 'b':
      identity.push_back('\b');
      continue;
    case 'f':
      identity.push_back('\f');
      continue;
    case 'n':
      identity.push_back('\n');
      continue;
    case 'r':
      identity.push_back('\r');
      continue;
    case 't':
      identity.push_back('\t');
      continue;
    case 'v':
      identity.push_back('\v');
      continue;
    case '\n':
      continue;
    case '\r':
      if (index < literal.content_end && spelling[index] == '\n') {
        ++index;
      }
      continue;
    default:
      break;
    }

    if (escaped >= '0' && escaped <= '7') {
      std::uint32_t value = static_cast<std::uint32_t>(escaped - '0');
      for (int digit = 1; digit < 3 && index < literal.content_end &&
                          spelling[index] >= '0' && spelling[index] <= '7';
           ++digit) {
        value = value * 8 + static_cast<std::uint32_t>(spelling[index] - '0');
        ++index;
      }
      if (value > 0xff) {
        return std::nullopt;
      }
      identity.push_back(static_cast<char>(value));
      continue;
    }

    if (escaped == 'x') {
      std::uint32_t value = 0;
      std::size_t digits = 0;
      while (index < literal.content_end) {
        const int digit = hexadecimalDigitValue(spelling[index]);
        if (digit < 0) {
          break;
        }
        if (value > (0xffu - static_cast<std::uint32_t>(digit)) / 16u) {
          return std::nullopt;
        }
        value = value * 16u + static_cast<std::uint32_t>(digit);
        ++index;
        ++digits;
      }
      if (digits == 0) {
        return std::nullopt;
      }
      identity.push_back(static_cast<char>(value));
      continue;
    }

    if (escaped == 'u' || escaped == 'U') {
      const std::size_t required_digits = escaped == 'u' ? 4 : 8;
      if (required_digits > literal.content_end - index) {
        return std::nullopt;
      }
      std::uint32_t value = 0;
      for (std::size_t digit_index = 0; digit_index < required_digits;
           ++digit_index) {
        const int digit = hexadecimalDigitValue(spelling[index++]);
        if (digit < 0 || value > (0x10ffffu - digit) / 16u) {
          return std::nullopt;
        }
        value = value * 16u + static_cast<std::uint32_t>(digit);
      }
      if (!appendUtf8Scalar(value, identity)) {
        return std::nullopt;
      }
      continue;
    }

    return std::nullopt;
  }
  return identity;
}

std::optional<std::string>
normalizeOpenMPNamePropertyIdentity(const std::string &spelling,
                                    OpenMPBaseLang language) {
  if (isOpenMPIdentifierSpelling(spelling)) {
    return spelling;
  }
  ParsedQuotedLiteral literal;
  if (language == Lang_Fortran &&
      parseFortranQuotedLiteralAt(spelling, 0, &literal) &&
      literal.end == spelling.size()) {
    std::string identity;
    identity.reserve(literal.content_end - literal.content_begin);
    const char delimiter = spelling.front();
    for (std::size_t index = literal.content_begin; index < literal.content_end;
         ++index) {
      identity.push_back(spelling[index]);
      if (spelling[index] == delimiter && index + 1 < literal.content_end &&
          spelling[index + 1] == delimiter) {
        ++index;
      }
    }
    return identity;
  }
  if ((language == Lang_C || language == Lang_Cplusplus) &&
      parseCxxQuotedLiteralAt(spelling, 0, &literal) &&
      literal.end == spelling.size() &&
      (language == Lang_Cplusplus || !literal.raw)) {
    return decodeCxxStringLiteral(spelling, literal);
  }
  return std::nullopt;
}

const char *contextKindIdentity(OpenMPClauseContextKind kind) {
  switch (kind) {
  case OMPC_CONTEXT_KIND_host:
    return "host";
  case OMPC_CONTEXT_KIND_nohost:
    return "nohost";
  case OMPC_CONTEXT_KIND_any:
    return "any";
  case OMPC_CONTEXT_KIND_cpu:
    return "cpu";
  case OMPC_CONTEXT_KIND_gpu:
    return "gpu";
  case OMPC_CONTEXT_KIND_fpga:
    return "fpga";
  case OMPC_CONTEXT_KIND_unknown:
    return nullptr;
  }
  return nullptr;
}

const char *contextVendorIdentity(OpenMPClauseContextVendor vendor) {
  switch (vendor) {
  case OMPC_CONTEXT_VENDOR_amd:
    return "amd";
  case OMPC_CONTEXT_VENDOR_arm:
    return "arm";
  case OMPC_CONTEXT_VENDOR_bsc:
    return "bsc";
  case OMPC_CONTEXT_VENDOR_cray:
    return "cray";
  case OMPC_CONTEXT_VENDOR_fujitsu:
    return "fujitsu";
  case OMPC_CONTEXT_VENDOR_gnu:
    return "gnu";
  case OMPC_CONTEXT_VENDOR_ibm:
    return "ibm";
  case OMPC_CONTEXT_VENDOR_intel:
    return "intel";
  case OMPC_CONTEXT_VENDOR_llvm:
    return "llvm";
  case OMPC_CONTEXT_VENDOR_nvidia:
    return "nvidia";
  case OMPC_CONTEXT_VENDOR_pgi:
    return "pgi";
  case OMPC_CONTEXT_VENDOR_ti:
    return "ti";
  case OMPC_CONTEXT_VENDOR_user:
    return "user";
  case OMPC_CONTEXT_VENDOR_unknown:
    return "unknown";
  case OMPC_CONTEXT_VENDOR_unspecified:
    return nullptr;
  }
  return nullptr;
}

bool isKnownContextKindIdentity(const std::string &identity) {
  static constexpr const char *Names[] = {"host", "nohost", "any",
                                          "cpu",  "gpu",    "fpga"};
  return std::find(std::begin(Names), std::end(Names), identity) !=
         std::end(Names);
}

bool isKnownContextVendorIdentity(const std::string &identity) {
  static constexpr const char *Names[] = {
      "amd",   "arm",  "bsc",    "cray", "fujitsu", "gnu",  "ibm",
      "intel", "llvm", "nvidia", "pgi",  "ti",      "user", "unknown"};
  return std::find(std::begin(Names), std::end(Names), identity) !=
         std::end(Names);
}

std::size_t findTopLevelMapperColon(const std::string &text,
                                    std::string &error) {
  IteratorDelimiterState state;
  std::size_t mapper_colon = std::string::npos;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char character = text[index];
    if (!consumeIteratorDelimiter(state, text, index, true, error)) {
      return std::string::npos;
    }
    if (state.quote != '\0') {
      continue;
    }
    const bool scope_colon =
        character == ':' &&
        ((index > 0 && text[index - 1] == ':') ||
         (index + 1 < text.size() && text[index + 1] == ':'));
    if (character == ':' && !scope_colon && state.fullyTopLevel()) {
      if (mapper_colon != std::string::npos) {
        error = "declare mapper has more than one top-level identifier colon";
        return std::string::npos;
      }
      mapper_colon = index;
    }
  }
  if (!state.balanced()) {
    error = "declare mapper specification has unbalanced delimiters";
    return std::string::npos;
  }
  return mapper_colon;
}

} // namespace

bool parseOpenMPIteratorDefinitions(const char *spelling,
                                    std::vector<OpenMPIterator> &result,
                                    std::string &error) {
  result.clear();
  error.clear();
  if (spelling == nullptr) {
    error = "iterator specifier list is null";
    return false;
  }
  const std::string text(spelling);
  ompparser::SourceRange source_range;
  const bool has_source_range =
      openmpGetLexemeSourceRange(spelling, source_range);
  std::vector<std::string> items;
  if (!splitIteratorItems(text, items, error)) {
    return false;
  }
  std::size_t item_search_begin = 0;
  for (const std::string &item : items) {
    const std::size_t item_begin = text.find(item, item_search_begin);
    if (item_begin == std::string::npos) {
      error = "iterator item is not a source-faithful substring";
      result.clear();
      return false;
    }
    item_search_begin = item_begin + item.size();
    const std::size_t assignment = findIteratorAssignment(item, error);
    if (assignment == std::string::npos) {
      result.clear();
      return false;
    }
    const std::string declaration = trimWhitespace(item, 0, assignment);
    const std::string range =
        trimWhitespace(item, assignment + 1, item.size() - assignment - 1);
    std::size_t name_end = declaration.size();
    while (name_end > 0 && std::isspace(static_cast<unsigned char>(
                               declaration[name_end - 1]))) {
      --name_end;
    }
    std::size_t name_begin = name_end;
    while (name_begin > 0) {
      const unsigned char character =
          static_cast<unsigned char>(declaration[name_begin - 1]);
      if (!std::isalnum(character) && character != '_') {
        break;
      }
      --name_begin;
    }
    const std::string name =
        declaration.substr(name_begin, name_end - name_begin);
    if (name.empty() ||
        !(std::isalpha(static_cast<unsigned char>(name.front())) ||
          name.front() == '_') ||
        !std::all_of(name.begin() + 1, name.end(), [](unsigned char value) {
          return std::isalnum(value) || value == '_';
        })) {
      error = "iterator declaration has no final identifier";
      result.clear();
      return false;
    }
    const std::string qualifier = trimWhitespace(declaration, 0, name_begin);
    std::vector<std::string> fields;
    if (!splitIteratorRange(range, fields, error)) {
      result.clear();
      return false;
    }
    OpenMPIterator iterator;
    iterator.set(qualifier, name, fields[0], fields[1],
                 fields.size() == 3 ? fields[2] : std::string());
    const std::size_t declaration_begin = item.find(declaration);
    const std::size_t range_begin = item.find(range, assignment + 1);
    if (declaration_begin == std::string::npos ||
        range_begin == std::string::npos) {
      error = "iterator fields are not source-faithful substrings";
      result.clear();
      return false;
    }
    const std::size_t name_offset = declaration_begin + name_begin;
    if (!qualifier.empty()) {
      const std::size_t qualifier_offset = declaration.find(qualifier);
      if (qualifier_offset == std::string::npos) {
        error = "iterator qualifier is not a source-faithful substring";
        result.clear();
        return false;
      }
      assignSubfragmentRange(iterator.qualifier, text,
                             item_begin + declaration_begin + qualifier_offset,
                             item_begin + declaration_begin + qualifier_offset +
                                 qualifier.size(),
                             source_range, has_source_range);
    }
    assignSubfragmentRange(iterator.variable, text, item_begin + name_offset,
                           item_begin + name_offset + name.size(), source_range,
                           has_source_range);
    std::size_t field_search_begin = 0;
    ompparser::HostFragment *fragments[] = {&iterator.begin, &iterator.end,
                                            &iterator.step};
    for (std::size_t field_index = 0; field_index < fields.size();
         ++field_index) {
      const std::size_t field_begin =
          range.find(fields[field_index], field_search_begin);
      if (field_begin == std::string::npos) {
        error = "iterator range field is not a source-faithful substring";
        result.clear();
        return false;
      }
      assignSubfragmentRange(
          *fragments[field_index], text, item_begin + range_begin + field_begin,
          item_begin + range_begin + field_begin + fields[field_index].size(),
          source_range, has_source_range);
      field_search_begin = field_begin + fields[field_index].size();
    }
    result.push_back(std::move(iterator));
  }
  return true;
}

void OpenMPApplyClause::setLabel(const char *value) {
  label = makeHostFragment(value, ompparser::HostFragmentRole::Verbatim,
                           OMP_EXPR_PARSE_openmp_syntax);
}

void OpenMPApplyClause::addTransformation(OpenMPApplyTransformKind kind,
                                          const char *argument,
                                          OpenMPClauseSeparator sep) {
  ApplyTransform t;
  t.kind = kind;
  t.argument = makeHostFragment(argument,
                                kind == OMPC_APPLY_TRANSFORM_unknown
                                    ? ompparser::HostFragmentRole::Verbatim
                                    : ompparser::HostFragmentRole::Expression,
                                kind == OMPC_APPLY_TRANSFORM_unknown
                                    ? OMP_EXPR_PARSE_openmp_syntax
                                    : OMP_EXPR_PARSE_expression);
  t.separator = sep;
  transforms.push_back(std::move(t));
}

void OpenMPApplyClause::addNestedApply(OpenMPApplyClause *nested,
                                       OpenMPClauseSeparator sep) {
  if (nested == nullptr) {
    throw std::invalid_argument("nested apply transformation is null");
  }
  ApplyTransform t;
  t.kind = OMPC_APPLY_TRANSFORM_apply;
  t.nested_apply.reset(nested);
  t.separator = sep;
  transforms.push_back(std::move(t));
}

OpenMPClause *
OpenMPDirective::registerClause(std::unique_ptr<OpenMPClause> clause) {
  if (clause == nullptr) {
    throw std::invalid_argument("cannot register a null clause");
  }
  clause->setDirectiveKind(this->kind);
  clause->setBaseLang(this->lang);
  OpenMPClause *raw_ptr = clause.get();
  clause_storage.push_back(std::move(clause));
  return raw_ptr;
}

void OpenMPDirective::setBaseLang(OpenMPBaseLang value) {
  lang = value;
  for (const std::unique_ptr<OpenMPClause> &clause : clause_storage) {
    if (clause == nullptr) {
      throw std::logic_error("cannot assign a base language to a null clause");
    }
    clause->setBaseLang(value);
  }
}

void OpenMPDirective::adoptClausesFrom(OpenMPDirective &source) {
  if (lang != Lang_unknown && source.lang != Lang_unknown &&
      lang != source.lang) {
    throw std::logic_error(
        "cannot adopt clauses from a directive in another base language");
  }
  for (auto &entry : source.clauses) {
    auto &destination = clauses[entry.first];
    destination.insert(destination.end(), entry.second.begin(),
                       entry.second.end());
  }
  source.clauses.clear();

  for (OpenMPClause *clause : source.clauses_in_original_order) {
    if (clause == nullptr) {
      throw std::logic_error("cannot adopt a null clause-order entry");
    }
    if (std::find(clauses_in_original_order.begin(),
                  clauses_in_original_order.end(),
                  clause) != clauses_in_original_order.end()) {
      throw std::logic_error("cannot adopt a duplicate clause-order entry");
    }
    clause->setClausePosition(
        static_cast<int>(clauses_in_original_order.size()));
    clauses_in_original_order.push_back(clause);
  }
  source.clauses_in_original_order.clear();

  for (std::unique_ptr<OpenMPClause> &clause : source.clause_storage) {
    if (clause == nullptr) {
      throw std::logic_error("cannot adopt a null owned clause");
    }
    clause->setBaseLang(lang != Lang_unknown ? lang : source.lang);
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
    errors.insert(errors.end(), clause->getConstructionErrors().begin(),
                  clause->getConstructionErrors().end());
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
  proc_name = makeHostFragment(name, ompparser::HostFragmentRole::Expression);
}

void OpenMPDeclareTargetDirective::addExtendedList(const char *item) {
  if (item != nullptr) {
    extended_list.push_back(
        makeHostFragment(item, ompparser::HostFragmentRole::Locator));
  }
}

void OpenMPFlushDirective::addFlushList(const char *item) {
  if (item != nullptr) {
    flush_list.push_back(
        makeHostFragment(item, ompparser::HostFragmentRole::Variable));
  }
}

void OpenMPDepobjDirective::addDepobj(const char *item) {
  depobj = makeHostFragment(item, ompparser::HostFragmentRole::Expression);
}

void OpenMPDependClause::addDependenceVector(const char *dependence) {
  dependence_vector =
      makeHostFragment(dependence, ompparser::HostFragmentRole::Expression);
}

void OpenMPReductionClause::setUserDefinedIdentifier(const char *identifier) {
  user_defined_identifier =
      makeHostFragment(identifier, ompparser::HostFragmentRole::Declarator,
                       OMP_EXPR_PARSE_openmp_syntax);
}

void OpenMPReductionClause::setUserDefinedIdentifierSourceRange(
    const ompparser::SourceRange &source_range) {
  if (user_defined_identifier.spelling.empty() ||
      source_range.end.offset <= source_range.begin.offset) {
    throw std::invalid_argument(
        "reduction identifier requires a nonempty exact source range");
  }
  user_defined_identifier.range = source_range;
}

void OpenMPIfClause::setUserDefinedModifier(const char *modifier) {
  user_defined_modifier =
      makeHostFragment(modifier, ompparser::HostFragmentRole::Declarator);
}

void OpenMPInReductionClause::setUserDefinedIdentifier(const char *identifier) {
  user_defined_identifier =
      makeHostFragment(identifier, ompparser::HostFragmentRole::Declarator,
                       OMP_EXPR_PARSE_openmp_syntax);
}

void OpenMPTaskReductionClause::setUserDefinedIdentifier(
    const char *identifier) {
  user_defined_identifier =
      makeHostFragment(identifier, ompparser::HostFragmentRole::Declarator,
                       OMP_EXPR_PARSE_openmp_syntax);
}

void OpenMPAllocateClause::setUserDefinedAllocator(const char *_allocator) {
  if (_allocator == nullptr || *_allocator == '\0' ||
      !user_defined_allocator.spelling.empty()) {
    construction_errors.push_back(
        "allocate clause allocator is empty or was assigned twice");
    return;
  }
  user_defined_allocator =
      makeHostFragment(_allocator, ompparser::HostFragmentRole::Expression);
  user_defined_allocator.clause_kind = kind;
}

void OpenMPAllocateClause::setAllocatorModifier(const char *allocator) {
  if (allocator == nullptr || *allocator == '\0' ||
      this->allocator != OMPC_ALLOCATE_ALLOCATOR_unspecified ||
      !user_defined_allocator.spelling.empty() ||
      (!modifier_order.empty() &&
       std::find(modifier_order.begin(), modifier_order.end(),
                 ModifierKind::Allocator) != modifier_order.end())) {
    construction_errors.push_back(
        "allocate clause has duplicate or mixed allocator syntax");
    return;
  }
  setUserDefinedAllocator(allocator);
  if (user_defined_allocator.spelling.empty()) {
    construction_errors.push_back(
        "allocate allocator modifier has no typed allocator payload");
    return;
  }
  this->allocator = OMPC_ALLOCATE_ALLOCATOR_user;
  modifier_order.push_back(ModifierKind::Allocator);
}

void OpenMPAllocateClause::setAlignModifier(const char *value) {
  const bool has_legacy_allocator =
      allocator != OMPC_ALLOCATE_ALLOCATOR_unspecified &&
      allocator != OMPC_ALLOCATE_ALLOCATOR_user;
  if (value == nullptr || *value == '\0' || !alignment.spelling.empty() ||
      has_legacy_allocator) {
    construction_errors.push_back(
        "allocate clause alignment is empty, was assigned twice, or was "
        "combined with exclusive legacy allocator syntax");
    return;
  }
  alignment = makeHostFragment(value, ompparser::HostFragmentRole::Expression);
  alignment.clause_kind = kind;
  modifier_order.push_back(ModifierKind::Align);
}

bool OpenMPAllocateClause::usesAllocatorModifierSyntax() const {
  return std::find(modifier_order.begin(), modifier_order.end(),
                   ModifierKind::Allocator) != modifier_order.end();
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
      makeHostFragment(_identifier, ompparser::HostFragmentRole::Declarator,
                       OMP_EXPR_PARSE_verbatim);
}

void OpenMPFromClause::setMapperIdentifier(const char *_identifier) {
  if (_identifier == nullptr) {
    mapper_identifier = {};
    return;
  }
  mapper_identifier =
      makeHostFragment(_identifier, ompparser::HostFragmentRole::Declarator,
                       OMP_EXPR_PARSE_verbatim);
}

void OpenMPMapClause::setMapperIdentifier(const char *_identifier) {
  if (_identifier == nullptr) {
    mapper_identifier = {};
    return;
  }
  mapper_identifier =
      makeHostFragment(_identifier, ompparser::HostFragmentRole::Declarator,
                       OMP_EXPR_PARSE_verbatim);
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
      _user_defined_identifier, ompparser::HostFragmentRole::Declarator,
      OMP_EXPR_PARSE_openmp_declare_mapper_identifier);
}

void OpenMPDeclareMapperDirective::setDeclareMapperType(
    const char *_declare_mapper_type) {
  type =
      makeHostFragment(_declare_mapper_type, ompparser::HostFragmentRole::Type,
                       OMP_EXPR_PARSE_openmp_declare_mapper_type);
}

void OpenMPDeclareMapperDirective::setDeclareMapperVar(
    const char *_declare_mapper_variable) {
  var = makeHostFragment(_declare_mapper_variable,
                         ompparser::HostFragmentRole::Variable,
                         OMP_EXPR_PARSE_openmp_declare_mapper_variable);
}

bool OpenMPDeclareMapperDirective::setSpecification(
    const char *specification, OpenMPBaseLang language,
    OpenMPBaseLang &resolved_language, std::string &error) {
  resolved_language = Lang_unknown;
  error.clear();
  if (specification == nullptr ||
      identifier != OMPD_DECLARE_MAPPER_IDENTIFIER_unspecified ||
      identifier_explicit || !user_defined_identifier.spelling.empty() ||
      !type.spelling.empty() || !var.spelling.empty()) {
    error = "declare mapper specification is null or was assigned twice";
    return false;
  }
  const std::string source(specification);
  const std::size_t text_begin = source.find_first_not_of(" \t\n\r\f\v");
  const std::size_t text_end = source.find_last_not_of(" \t\n\r\f\v");
  const std::string text =
      text_begin == std::string::npos
          ? std::string()
          : source.substr(text_begin, text_end - text_begin + 1);
  if (text.empty()) {
    error = "declare mapper specification is empty";
    return false;
  }

  const std::size_t mapper_colon = findTopLevelMapperColon(text, error);
  if (!error.empty()) {
    return false;
  }
  OpenMPDeclareMapperDirectiveIdentifier parsed_identifier =
      OMPD_DECLARE_MAPPER_IDENTIFIER_default;
  bool parsed_identifier_explicit = false;
  std::string parsed_user_identifier;
  std::string declaration = text;
  std::size_t declaration_begin = 0;
  std::size_t identifier_begin = std::string::npos;
  if (mapper_colon != std::string::npos) {
    parsed_identifier_explicit = true;
    const std::string mapper_identifier = trimWhitespace(text, 0, mapper_colon);
    declaration =
        trimWhitespace(text, mapper_colon + 1, text.size() - mapper_colon - 1);
    identifier_begin = text.find(mapper_identifier);
    declaration_begin = text.find(declaration, mapper_colon + 1);
    const bool is_default_identifier =
        mapper_identifier == "default" ||
        (language == Lang_Fortran &&
         equalsIgnoringAsciiCase(mapper_identifier, "default"));
    if (is_default_identifier) {
      parsed_identifier = OMPD_DECLARE_MAPPER_IDENTIFIER_default;
    } else if (isOpenMPIdentifierSpelling(mapper_identifier)) {
      parsed_identifier = OMPD_DECLARE_MAPPER_IDENTIFIER_user;
      parsed_user_identifier = mapper_identifier;
    } else {
      error = "declare mapper identifier is not one identifier";
      return false;
    }
  }
  if (declaration.empty()) {
    error = "declare mapper type and variable are empty";
    return false;
  }

  std::string parsed_type;
  std::string parsed_variable;
  bool parsed_type_var_space = false;
  std::size_t parsed_type_begin = std::string::npos;
  std::size_t parsed_variable_begin = std::string::npos;
  if (language == Lang_Fortran) {
    const std::size_t separator = declaration.find("::");
    if (separator == std::string::npos ||
        declaration.find("::", separator + 2) != std::string::npos) {
      error = "Fortran declare mapper requires one 'type :: variable' "
              "separator";
      return false;
    }
    parsed_type = trimWhitespace(declaration, 0, separator);
    parsed_variable = trimWhitespace(declaration, separator + 2,
                                     declaration.size() - separator - 2);
    parsed_type_begin = declaration.find(parsed_type);
    parsed_variable_begin = declaration.find(parsed_variable, separator + 2);
    parsed_type_var_space = true;
  } else if (language == Lang_C || language == Lang_Cplusplus) {
    std::size_t variable_end = declaration.size();
    while (variable_end > 0 && std::isspace(static_cast<unsigned char>(
                                   declaration[variable_end - 1]))) {
      --variable_end;
    }
    std::size_t variable_begin = variable_end;
    while (variable_begin > 0) {
      const unsigned char character =
          static_cast<unsigned char>(declaration[variable_begin - 1]);
      if (!std::isalnum(character) && character != '_') {
        break;
      }
      --variable_begin;
    }
    parsed_variable =
        declaration.substr(variable_begin, variable_end - variable_begin);
    parsed_type = trimWhitespace(declaration, 0, variable_begin);
    parsed_type_begin = declaration.find(parsed_type);
    parsed_variable_begin = variable_begin;
    parsed_type_var_space =
        variable_begin > 0 && std::isspace(static_cast<unsigned char>(
                                  declaration[variable_begin - 1]));
  } else {
    error = "declare mapper has no exact base language";
    return false;
  }
  if (parsed_type.empty() || !isOpenMPIdentifierSpelling(parsed_variable)) {
    error = "declare mapper does not have an exact type and variable";
    return false;
  }
  if ((mapper_colon != std::string::npos &&
       (identifier_begin == std::string::npos ||
        declaration_begin == std::string::npos)) ||
      parsed_type_begin == std::string::npos ||
      parsed_variable_begin == std::string::npos) {
    error = "declare mapper fields are not source-faithful substrings";
    return false;
  }

  identifier = parsed_identifier;
  identifier_explicit = parsed_identifier_explicit;
  ompparser::SourceRange source_range;
  const bool has_source_range =
      openmpGetLexemeSourceRange(specification, source_range);
  if (!parsed_user_identifier.empty()) {
    setUserDefinedIdentifier(parsed_user_identifier.c_str());
    assignSubfragmentRange(
        user_defined_identifier, source, text_begin + identifier_begin,
        text_begin + identifier_begin + parsed_user_identifier.size(),
        source_range, has_source_range);
  }
  setDeclareMapperType(parsed_type.c_str());
  setDeclareMapperVar(parsed_variable.c_str());
  assignSubfragmentRange(
      type, source, text_begin + declaration_begin + parsed_type_begin,
      text_begin + declaration_begin + parsed_type_begin + parsed_type.size(),
      source_range, has_source_range);
  assignSubfragmentRange(var, source,
                         text_begin + declaration_begin + parsed_variable_begin,
                         text_begin + declaration_begin +
                             parsed_variable_begin + parsed_variable.size(),
                         source_range, has_source_range);
  type_var_has_space = parsed_type_var_space;
  resolved_language = language;
  return true;
}

void OpenMPVariantClause::recordVariantError(const std::string &message) {
  construction_errors.push_back(message);
}

OpenMPVariantClause::TraitSetSelector *
OpenMPVariantClause::activeTraitSet(const char *operation) {
  if (!active_trait_set || *active_trait_set >= trait_sets.size()) {
    recordVariantError(std::string(operation) +
                       " requires an active context selector set");
    return nullptr;
  }
  return &trait_sets[*active_trait_set];
}

OpenMPVariantClause::TraitSelector *
OpenMPVariantClause::activeTraitSelector(const char *operation) {
  TraitSetSelector *set = activeTraitSet(operation);
  if (set == nullptr || !active_trait_selector ||
      *active_trait_selector >= set->selectors.size()) {
    recordVariantError(std::string(operation) +
                       " requires an active trait selector");
    return nullptr;
  }
  return &set->selectors[*active_trait_selector];
}

void OpenMPVariantClause::beginTraitSet(
    OpenMPContextSelectorSequenceKind kind) {
  if (active_trait_set) {
    recordVariantError("context selector sets cannot be nested");
    return;
  }
  TraitSetSelector set;
  set.kind = kind;
  trait_sets.push_back(std::move(set));
  active_trait_set = trait_sets.size() - 1;
  active_trait_selector.reset();
}

void OpenMPVariantClause::endTraitSet() {
  if (!active_trait_set) {
    recordVariantError("context selector set terminator has no matching set");
    return;
  }
  if (active_trait_selector) {
    recordVariantError("context selector set ended inside a trait selector");
    active_trait_selector.reset();
  }
  active_trait_set.reset();
}

void OpenMPVariantClause::beginTraitSelector(
    OpenMPContextTraitSelectorKind kind, const char *score,
    const char *implementation_defined_name) {
  TraitSetSelector *set = activeTraitSet("trait selector");
  if (set == nullptr) {
    return;
  }
  if (active_trait_selector) {
    recordVariantError("trait selectors cannot be nested");
    return;
  }
  TraitSelector selector;
  selector.kind = kind;
  selector.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression,
                       OMP_EXPR_PARSE_constant_integer);
  selector.score.clause_kind = getKind();
  if (implementation_defined_name != nullptr) {
    selector.implementation_defined_name =
        trimWhitespace(implementation_defined_name);
  }
  set->selectors.push_back(std::move(selector));
  active_trait_selector = set->selectors.size() - 1;
}

void OpenMPVariantClause::addExpressionProperty(
    const char *expression, OpenMPExprParseMode parse_mode) {
  TraitSelector *selector = activeTraitSelector("trait property");
  if (selector == nullptr || expression == nullptr) {
    if (expression == nullptr) {
      recordVariantError("trait expression property is null");
    }
    return;
  }
  TraitProperty property;
  const ompparser::HostFragmentRole role =
      parse_mode == OMP_EXPR_PARSE_verbatim
          ? ompparser::HostFragmentRole::Verbatim
          : ompparser::HostFragmentRole::Expression;
  property.fragment = makeHostFragment(expression, role, parse_mode);
  property.fragment.clause_kind = getKind();
  selector->properties.push_back(std::move(property));
}

void OpenMPVariantClause::addContextKindProperty(OpenMPClauseContextKind kind) {
  TraitSelector *selector = activeTraitSelector("kind property");
  if (selector == nullptr) {
    return;
  }
  TraitProperty property;
  property.context_kind = kind;
  selector->properties.push_back(std::move(property));
}

void OpenMPVariantClause::addContextVendorProperty(
    OpenMPClauseContextVendor vendor) {
  TraitSelector *selector = activeTraitSelector("vendor property");
  if (selector == nullptr) {
    return;
  }
  TraitProperty property;
  property.context_vendor = vendor;
  selector->properties.push_back(std::move(property));
}

void OpenMPVariantClause::addAtomicDefaultMemOrderProperty(
    OpenMPAtomicDefaultMemOrderClauseKind kind) {
  requireAtomicDefaultMemOrder(kind);
  TraitSelector *selector =
      activeTraitSelector("atomic_default_mem_order property");
  if (selector == nullptr) {
    return;
  }
  TraitProperty property;
  property.atomic_default_mem_order = kind;
  selector->properties.push_back(std::move(property));
}

void OpenMPVariantClause::addRequiresProperty(OpenMPClauseKind kind,
                                              const char *required_expression) {
  TraitSelector *selector = activeTraitSelector("requires property");
  if (selector == nullptr) {
    return;
  }

  std::unique_ptr<OpenMPClause> requirement;
  if (kind == OMPC_device_safesync) {
    requirement = std::make_unique<OpenMPDeviceSafesyncClause>();
  } else {
    switch (kind) {
    case OMPC_reverse_offload:
    case OMPC_unified_address:
    case OMPC_unified_shared_memory:
    case OMPC_dynamic_allocators:
    case OMPC_self_maps:
      requirement = std::make_unique<OpenMPClause>(kind);
      break;
    default:
      recordVariantError("requires property has an invalid clause kind");
      return;
    }
  }
  requirement->setDirectiveKind(OMPD_requires);
  if (required_expression != nullptr) {
    requirement->addLangExpr(required_expression, OMPC_CLAUSE_SEP_space, 0, 0,
                             OMP_EXPR_PARSE_expression);
  }

  TraitProperty property;
  property.requirement = std::move(requirement);
  selector->properties.push_back(std::move(property));
}

void OpenMPVariantClause::addRequiresAtomicDefaultMemOrderProperty(
    OpenMPAtomicDefaultMemOrderClauseKind kind) {
  requireAtomicDefaultMemOrder(kind);
  TraitSelector *selector = activeTraitSelector("requires property");
  if (selector == nullptr) {
    return;
  }
  TraitProperty property;
  property.requirement =
      std::make_unique<OpenMPAtomicDefaultMemOrderClause>(kind);
  property.requirement->setDirectiveKind(OMPD_requires);
  selector->properties.push_back(std::move(property));
}

void OpenMPVariantClause::addRequiresExtensionProperty(const char *identifier) {
  TraitSelector *selector = activeTraitSelector("requires property");
  if (selector == nullptr || identifier == nullptr) {
    if (identifier == nullptr) {
      recordVariantError("implementation-defined requires property is null");
    }
    return;
  }
  auto requirement =
      std::make_unique<OpenMPExtImplementationDefinedRequirementClause>();
  requirement->setDirectiveKind(OMPD_requires);
  requirement->setImplementationDefinedRequirement(identifier);

  TraitProperty property;
  property.requirement = std::move(requirement);
  selector->properties.push_back(std::move(property));
}

void OpenMPVariantClause::endTraitSelector() {
  if (activeTraitSelector("trait selector terminator") == nullptr) {
    return;
  }
  active_trait_selector.reset();
}

void OpenMPVariantClause::addConstructDirective(
    const char *score, std::unique_ptr<OpenMPDirective> construct_directive) {
  TraitSetSelector *set = activeTraitSet("construct selector");
  if (set == nullptr) {
    return;
  }
  TraitSelector selector;
  selector.kind = OMPC_TRAIT_construct;
  selector.score =
      makeHostFragment(score, ompparser::HostFragmentRole::Expression,
                       OMP_EXPR_PARSE_constant_integer);
  selector.score.clause_kind = getKind();
  selector.construct_directive = std::move(construct_directive);
  set->selectors.push_back(std::move(selector));
}

void OpenMPVariantClause::visitHostFragments(
    const ompparser::HostFragmentVisitor &visitor) {
  for (TraitSetSelector &set : trait_sets) {
    for (TraitSelector &selector : set.selectors) {
      if (!selector.score.spelling.empty()) {
        visitOwnedHostFragment(visitor, selector.score);
      }
      for (TraitProperty &property : selector.properties) {
        if (!property.fragment.spelling.empty()) {
          visitOwnedHostFragment(visitor, property.fragment);
        }
        if (property.requirement != nullptr) {
          property.requirement->visitHostFragments(visitor);
        }
      }
      if (selector.construct_directive != nullptr) {
        selector.construct_directive->visitHostFragments(visitor);
      }
    }
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
  if (active_trait_set || active_trait_selector) {
    errors.push_back("context selector construction was not completed");
  }
  if (getKind() != OMPC_otherwise && trait_sets.empty()) {
    errors.push_back("variant clause requires a context selector set");
  }

  std::vector<OpenMPContextSelectorSequenceKind> seen_sets;
  for (const TraitSetSelector &set : trait_sets) {
    if (std::find(seen_sets.begin(), seen_sets.end(), set.kind) !=
        seen_sets.end()) {
      errors.push_back("duplicate trait-set selector");
    }
    seen_sets.push_back(set.kind);
    if (set.selectors.empty()) {
      errors.push_back("trait-set selector is empty");
      continue;
    }

    std::vector<std::pair<OpenMPContextTraitSelectorKind, std::string>>
        seen_selectors;
    bool set_contains_any = false;
    for (const TraitSelector &selector : set.selectors) {
      bool legal = false;
      switch (selector.kind) {
      case OMPC_TRAIT_condition:
        legal = set.kind == OMPC_SELECTOR_user;
        break;
      case OMPC_TRAIT_construct:
        legal = set.kind == OMPC_SELECTOR_construct;
        break;
      case OMPC_TRAIT_kind:
      case OMPC_TRAIT_arch:
      case OMPC_TRAIT_isa:
        legal = set.kind == OMPC_SELECTOR_device ||
                set.kind == OMPC_SELECTOR_target_device;
        break;
      case OMPC_TRAIT_device_num:
      case OMPC_TRAIT_uid:
        legal = set.kind == OMPC_SELECTOR_target_device;
        break;
      case OMPC_TRAIT_vendor:
      case OMPC_TRAIT_extension:
      case OMPC_TRAIT_requires:
      case OMPC_TRAIT_atomic_default_mem_order:
      case OMPC_TRAIT_implementation_user:
        legal = set.kind == OMPC_SELECTOR_implementation;
        break;
      }
      if (!legal) {
        errors.push_back("trait selector is in the wrong selector set");
      }

      const std::string identity =
          selector.kind == OMPC_TRAIT_construct
              ? (selector.construct_directive
                     ? std::to_string(selector.construct_directive->getKind())
                     : std::string("null"))
          : selector.kind == OMPC_TRAIT_implementation_user
              ? selector.implementation_defined_name
              : std::string();
      const auto key = std::make_pair(selector.kind, identity);
      if (std::find(seen_selectors.begin(), seen_selectors.end(), key) !=
          seen_selectors.end()) {
        errors.push_back("duplicate trait selector");
      }
      seen_selectors.push_back(key);

      if ((set.kind == OMPC_SELECTOR_construct ||
           set.kind == OMPC_SELECTOR_device ||
           set.kind == OMPC_SELECTOR_target_device) &&
          !selector.score.spelling.empty()) {
        errors.push_back("trait score is prohibited in this selector set");
      }
      if (selector.kind == OMPC_TRAIT_implementation_user &&
          selector.implementation_defined_name.empty()) {
        errors.push_back("implementation-defined selector name is empty");
      }

      const std::size_t property_count = selector.properties.size();
      const bool exactly_one =
          selector.kind == OMPC_TRAIT_condition ||
          selector.kind == OMPC_TRAIT_device_num ||
          selector.kind == OMPC_TRAIT_uid ||
          selector.kind == OMPC_TRAIT_atomic_default_mem_order;
      const bool at_least_one = selector.kind == OMPC_TRAIT_kind ||
                                selector.kind == OMPC_TRAIT_arch ||
                                selector.kind == OMPC_TRAIT_isa ||
                                selector.kind == OMPC_TRAIT_vendor ||
                                selector.kind == OMPC_TRAIT_extension ||
                                selector.kind == OMPC_TRAIT_requires;
      if ((exactly_one && property_count != 1) ||
          (at_least_one && property_count == 0) ||
          (selector.kind == OMPC_TRAIT_construct && property_count != 0) ||
          (selector.kind == OMPC_TRAIT_implementation_user &&
           !selector.score.spelling.empty() && property_count == 0)) {
        errors.push_back("trait selector has invalid property cardinality");
      }
      if (selector.kind == OMPC_TRAIT_construct &&
          selector.construct_directive == nullptr) {
        errors.push_back("construct selector contains a null directive");
      }

      std::vector<std::string> seen_properties;
      for (const TraitProperty &property : selector.properties) {
        const bool expression = !property.fragment.spelling.empty();
        const bool context_kind = property.context_kind.has_value();
        const bool context_vendor = property.context_vendor.has_value();
        const bool atomic_order = property.atomic_default_mem_order.has_value();
        const bool requirement = property.requirement != nullptr;
        if (static_cast<int>(expression) + static_cast<int>(context_kind) +
                static_cast<int>(context_vendor) +
                static_cast<int>(atomic_order) +
                static_cast<int>(requirement) !=
            1) {
          errors.push_back("trait property does not have one typed payload");
          continue;
        }

        std::string property_identity;
        if (expression) {
          const OpenMPExprParseMode expected_mode =
              selector.kind == OMPC_TRAIT_condition ||
                      selector.kind == OMPC_TRAIT_device_num
                  ? OMP_EXPR_PARSE_expression
              : selector.kind == OMPC_TRAIT_kind ||
                      selector.kind == OMPC_TRAIT_arch ||
                      selector.kind == OMPC_TRAIT_isa ||
                      selector.kind == OMPC_TRAIT_uid ||
                      selector.kind == OMPC_TRAIT_vendor ||
                      selector.kind == OMPC_TRAIT_extension
                  ? OMP_EXPR_PARSE_openmp_context_name
                  : OMP_EXPR_PARSE_verbatim;
          if (property.fragment.parse_mode != expected_mode) {
            errors.push_back("trait property has the wrong host parse mode");
          }
          const bool name_list_property = selector.kind == OMPC_TRAIT_kind ||
                                          selector.kind == OMPC_TRAIT_arch ||
                                          selector.kind == OMPC_TRAIT_isa ||
                                          selector.kind == OMPC_TRAIT_uid ||
                                          selector.kind == OMPC_TRAIT_vendor ||
                                          selector.kind == OMPC_TRAIT_extension;
          if (name_list_property) {
            const std::optional<std::string> identity =
                normalizeOpenMPNamePropertyIdentity(property.fragment.spelling,
                                                    getBaseLang());
            if (!identity) {
              errors.push_back(
                  "name-list trait property is not an identifier or literal");
              continue;
            }
            property_identity = *identity;
            if (selector.kind == OMPC_TRAIT_kind) {
              if (!isKnownContextKindIdentity(property_identity)) {
                errors.push_back("kind trait property is not a defined kind");
              }
              set_contains_any = set_contains_any || property_identity == "any";
            } else if (selector.kind == OMPC_TRAIT_vendor &&
                       !isKnownContextVendorIdentity(property_identity)) {
              errors.push_back("vendor trait property is not a defined vendor");
            }
          } else {
            property_identity = property.fragment.spelling;
          }
        } else if (context_kind) {
          if (selector.kind != OMPC_TRAIT_kind) {
            errors.push_back("context kind is attached to the wrong selector");
          }
          const char *identity = contextKindIdentity(*property.context_kind);
          if (identity == nullptr) {
            errors.push_back("context kind property has an invalid value");
            continue;
          }
          property_identity = identity;
          set_contains_any = set_contains_any ||
                             *property.context_kind == OMPC_CONTEXT_KIND_any;
        } else if (context_vendor) {
          if (selector.kind != OMPC_TRAIT_vendor) {
            errors.push_back("vendor is attached to the wrong selector");
          }
          const char *identity =
              contextVendorIdentity(*property.context_vendor);
          if (identity == nullptr) {
            errors.push_back("vendor property has an invalid value");
            continue;
          }
          property_identity = identity;
        } else if (atomic_order) {
          property_identity =
              "order:" + std::to_string(*property.atomic_default_mem_order);
          if (selector.kind != OMPC_TRAIT_atomic_default_mem_order) {
            errors.push_back(
                "atomic memory order is attached to the wrong selector");
          }
        } else {
          if (selector.kind != OMPC_TRAIT_requires) {
            errors.push_back("requirement is attached to the wrong selector");
          }
          const OpenMPClauseKind requirement_kind =
              property.requirement->OpenMPClause::getKind();
          switch (requirement_kind) {
          case OMPC_reverse_offload:
          case OMPC_unified_address:
          case OMPC_unified_shared_memory:
          case OMPC_dynamic_allocators:
          case OMPC_self_maps:
          case OMPC_device_safesync:
          case OMPC_atomic_default_mem_order:
          case OMPC_ext_implementation_defined_requirement:
            break;
          default:
            errors.push_back("requires property has an invalid clause kind");
          }
          if (property.requirement->getDirectiveKind() != OMPD_requires) {
            errors.push_back(
                "requires property has the wrong directive ownership");
          }
          errors.insert(errors.end(),
                        property.requirement->getConstructionErrors().begin(),
                        property.requirement->getConstructionErrors().end());

          const auto &requirement_expressions =
              property.requirement->getExpressionItems();
          if (requirement_kind == OMPC_atomic_default_mem_order) {
            const auto *atomic =
                dynamic_cast<const OpenMPAtomicDefaultMemOrderClause *>(
                    property.requirement.get());
            if (atomic == nullptr || !requirement_expressions.empty()) {
              errors.push_back(
                  "atomic_default_mem_order requirement is malformed");
            } else {
              switch (atomic->getKind()) {
              case OMPC_ATOMIC_DEFAULT_MEM_ORDER_seq_cst:
              case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acq_rel:
              case OMPC_ATOMIC_DEFAULT_MEM_ORDER_acquire:
              case OMPC_ATOMIC_DEFAULT_MEM_ORDER_release:
              case OMPC_ATOMIC_DEFAULT_MEM_ORDER_relaxed:
                break;
              default:
                errors.push_back(
                    "atomic_default_mem_order requirement has an invalid "
                    "memory order");
              }
            }
          } else if (requirement_kind ==
                     OMPC_ext_implementation_defined_requirement) {
            const auto *extension = dynamic_cast<
                const OpenMPExtImplementationDefinedRequirementClause *>(
                property.requirement.get());
            if (extension == nullptr ||
                extension->getImplementationDefinedRequirement().empty() ||
                !requirement_expressions.empty()) {
              errors.push_back(
                  "implementation-defined requirement is malformed");
            }
          } else {
            if (requirement_expressions.size() > 1) {
              errors.push_back(
                  "requirement has more than one required expression");
            }
            for (const OpenMPExpressionItem &item : requirement_expressions) {
              if (item.fragment.spelling.empty() ||
                  item.fragment.parse_mode != OMP_EXPR_PARSE_expression) {
                errors.push_back(
                    "requirement has an invalid required expression");
              }
            }
          }
          property_identity = "requirement:" + std::to_string(requirement_kind);
          if (requirement_kind == OMPC_ext_implementation_defined_requirement) {
            const auto *extension = dynamic_cast<
                const OpenMPExtImplementationDefinedRequirementClause *>(
                property.requirement.get());
            if (extension != nullptr) {
              property_identity +=
                  ":" + extension->getImplementationDefinedRequirement();
            }
          }
        }
        if (std::find(seen_properties.begin(), seen_properties.end(),
                      property_identity) != seen_properties.end()) {
          errors.push_back("duplicate trait property");
        }
        seen_properties.push_back(std::move(property_identity));
      }
    }
    if (set_contains_any && (set.selectors.size() != 1 ||
                             set.selectors.front().properties.size() != 1)) {
      errors.push_back("kind(any) excludes all other selector properties");
    }
  }
  return errors.size() == initial_error_count;
}

void OpenMPClause::addLangExpr(const char *expression,
                               OpenMPClauseSeparator sep, int line, int col,
                               OpenMPExprParseMode parse_mode) {
  if (expression == nullptr) {
    throw std::invalid_argument("cannot add a null host expression");
  }
  std::string spelling(expression);
  const OpenMPExprParseMode effective_parse_mode =
      resolveClauseExpressionParseMode(this->kind, parse_mode, spelling);
  expressions.emplace_back(spelling, sep, effective_parse_mode);
  OpenMPExpressionItem &item = expressions.back();
  item.fragment.clause_kind = kind;
  item.fragment.parse_mode = effective_parse_mode;
  switch (effective_parse_mode) {
  case OMP_EXPR_PARSE_constant_integer:
  case OMP_EXPR_PARSE_openmp_iterator_type:
  case OMP_EXPR_PARSE_openmp_iterator_name:
  case OMP_EXPR_PARSE_openmp_declare_mapper_identifier:
  case OMP_EXPR_PARSE_openmp_declare_mapper_type:
  case OMP_EXPR_PARSE_openmp_declare_mapper_variable:
  case OMP_EXPR_PARSE_openmp_context_name:
    item.fragment.role = ompparser::HostFragmentRole::Expression;
    break;
  case OMP_EXPR_PARSE_variable_list:
    item.fragment.role = ompparser::HostFragmentRole::Variable;
    break;
  case OMP_EXPR_PARSE_array_section:
    item.fragment.role = ompparser::HostFragmentRole::Locator;
    break;
  case OMP_EXPR_PARSE_openmp_source:
  case OMP_EXPR_PARSE_openmp_syntax:
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
        makeHostFragment(label, ompparser::HostFragmentRole::Verbatim,
                         OMP_EXPR_PARSE_openmp_syntax);
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

void OpenMPInductionClause::visitSpecificationItems(
    const SpecificationItemVisitor &visitor) const {
  if (!visitor) {
    throw std::invalid_argument(
        "induction specification visitor must be callable");
  }
  for (const ItemRef &item : sequence) {
    switch (item.kind) {
    case ItemStep:
      if (item.index != 0 || step_expression.spelling.empty()) {
        throw std::logic_error("invalid induction step sequence reference");
      }
      visitor(SpecificationItemKind::Step, nullptr, step_expression);
      break;
    case ItemBinding:
      if (item.index >= bindings.size() ||
          bindings[item.index].label.spelling.empty() ||
          bindings[item.index].expression.spelling.empty()) {
        throw std::logic_error("invalid induction binding sequence reference");
      }
      visitor(SpecificationItemKind::Binding, &bindings[item.index].label,
              bindings[item.index].expression);
      break;
    case ItemPassthrough:
      if (item.index >= passthrough_items.size() ||
          passthrough_items[item.index].spelling.empty()) {
        throw std::logic_error(
            "invalid induction expression sequence reference");
      }
      visitor(SpecificationItemKind::Expression, nullptr,
              passthrough_items[item.index]);
      break;
    }
  }
}

void OpenMPInductionClause::visitHostFragments(
    const ompparser::HostFragmentVisitor &visitor) {
  for (const ItemRef &item : sequence) {
    switch (item.kind) {
    case ItemStep:
      if (item.index != 0 || step_expression.spelling.empty()) {
        throw std::logic_error("invalid induction step sequence reference");
      }
      visitOwnedHostFragment(visitor, step_expression);
      break;
    case ItemBinding:
      if (item.index >= bindings.size() ||
          bindings[item.index].label.spelling.empty() ||
          bindings[item.index].expression.spelling.empty()) {
        throw std::logic_error("invalid induction binding sequence reference");
      }
      visitOwnedHostFragment(visitor, bindings[item.index].label);
      visitOwnedHostFragment(visitor, bindings[item.index].expression);
      break;
    case ItemPassthrough:
      if (item.index >= passthrough_items.size() ||
          passthrough_items[item.index].spelling.empty()) {
        throw std::logic_error(
            "invalid induction expression sequence reference");
      }
      visitOwnedHostFragment(visitor, passthrough_items[item.index]);
      break;
    }
  }
  OpenMPClause::visitHostFragments(visitor);
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
      makeHostFragment(specification, ompparser::HostFragmentRole::Verbatim,
                       OMP_EXPR_PARSE_openmp_source);
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitModifierList::addPreferType(const std::string &specification) {
  OpenMPInitModifier modifier;
  modifier.category = OpenMPInitModifierCategory::PreferType;
  modifier.argument.spelling = specification;
  modifier.argument.role = ompparser::HostFragmentRole::Verbatim;
  modifier.argument.parse_mode = OMP_EXPR_PARSE_openmp_source;
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
  modifier.argument.parse_mode = OMP_EXPR_PARSE_array_section;
  modifiers.push_back(std::move(modifier));
}

void OpenMPInitClause::setOperand(const char *value) {
  operand = makeHostFragment(value, ompparser::HostFragmentRole::Variable);
}

void OpenMPInitClause::setOperand(const std::string &value) {
  operand = {};
  operand.spelling = value;
  operand.role = ompparser::HostFragmentRole::Variable;
  operand.parse_mode = OMP_EXPR_PARSE_variable_list;
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
  enum class ExpectedArgument { Integer, String };
  std::vector<ExpectedArgument> expected_arguments;
  expected_arguments.reserve(6);
  auto requirePreflightInteger = [&](std::size_t index) -> int {
    if (index >= arguments.size()) {
      throw std::invalid_argument(
          "missing integer clause-construction argument");
    }
    const int *value = std::get_if<int>(&arguments[index]);
    if (value == nullptr) {
      throw std::invalid_argument(
          "mistyped integer clause-construction argument");
    }
    return *value;
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
  case OMPC_ext_implementation_defined_requirement:
  case OMPC_match:
  case OMPC_uses_allocators:
  case OMPC_aligned:
  case OMPC_num_threads:
  case OMPC_when:
  case OMPC_otherwise:
    break;
  case OMPC_fail:
  case OMPC_severity:
  case OMPC_at:
  case OMPC_order:
  case OMPC_proc_bind:
  case OMPC_bind:
  case OMPC_lastprivate:
  case OMPC_linear:
  case OMPC_dist_schedule:
  case OMPC_device:
  case OMPC_atomic_default_mem_order:
  case OMPC_depobj_update:
  case OMPC_doacross:
  case OMPC_affinity:
  case OMPC_grainsize:
  case OMPC_num_tasks:
  case OMPC_to:
  case OMPC_from:
  case OMPC_device_type:
    expected_arguments = {ExpectedArgument::Integer};
    break;
  case OMPC_default:
    expected_arguments = {ExpectedArgument::Integer};
    if (arguments.size() == 2) {
      expected_arguments.push_back(ExpectedArgument::Integer);
    }
    break;
  case OMPC_if:
    expected_arguments = {ExpectedArgument::Integer};
    if (requirePreflightInteger(0) == OMPC_IF_MODIFIER_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_schedule:
    expected_arguments = {ExpectedArgument::Integer, ExpectedArgument::Integer,
                          ExpectedArgument::Integer};
    if (requirePreflightInteger(2) == OMPC_SCHEDULE_KIND_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_initializer:
    expected_arguments = {ExpectedArgument::Integer};
    if (requirePreflightInteger(0) == OMPC_INITIALIZER_PRIV_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_allocate:
    expected_arguments = {ExpectedArgument::Integer};
    if (requirePreflightInteger(0) == OMPC_ALLOCATE_ALLOCATOR_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_allocator:
    expected_arguments = {ExpectedArgument::Integer};
    if (requirePreflightInteger(0) == OMPC_ALLOCATOR_ALLOCATOR_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_in_reduction:
    expected_arguments = {ExpectedArgument::Integer};
    if (requirePreflightInteger(0) == OMPC_IN_REDUCTION_IDENTIFIER_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_task_reduction:
    expected_arguments = {ExpectedArgument::Integer};
    if (requirePreflightInteger(0) == OMPC_TASK_REDUCTION_IDENTIFIER_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_depend:
  case OMPC_defaultmap:
    expected_arguments = {ExpectedArgument::Integer, ExpectedArgument::Integer};
    break;
  case OMPC_reduction:
    expected_arguments = {ExpectedArgument::Integer, ExpectedArgument::Integer};
    if (requirePreflightInteger(1) == OMPC_REDUCTION_IDENTIFIER_user) {
      expected_arguments.push_back(ExpectedArgument::String);
    }
    break;
  case OMPC_map:
    expected_arguments = {ExpectedArgument::Integer, ExpectedArgument::Integer,
                          ExpectedArgument::Integer, ExpectedArgument::Integer,
                          ExpectedArgument::Integer};
    break;
  default:
    throw std::invalid_argument("unsupported OpenMP clause kind");
  }

  if (arguments.size() != expected_arguments.size()) {
    throw std::invalid_argument(arguments.size() < expected_arguments.size()
                                    ? "missing clause-construction arguments"
                                    : "too many clause-construction arguments");
  }
  for (std::size_t index = 0; index < expected_arguments.size(); ++index) {
    const bool valid =
        expected_arguments[index] == ExpectedArgument::Integer
            ? std::holds_alternative<int>(arguments[index])
            : std::holds_alternative<std::string>(arguments[index]);
    if (!valid) {
      throw std::invalid_argument("mistyped clause-construction argument");
    }
  }

  std::vector<OpenMPClause *> *current_clauses = getClauses(kind);
  OpenMPClause *new_clause = NULL;
  std::size_t argument_index = 0;

  auto nextIntegerArgument = [&]() {
    if (argument_index >= arguments.size()) {
      throw std::invalid_argument(
          "missing integer clause-construction argument");
    }
    const ClauseArgument &argument = arguments[argument_index++];
    const int *value = std::get_if<int>(&argument);
    if (value == nullptr) {
      throw std::invalid_argument(
          "mistyped integer clause-construction argument");
    }
    return *value;
  };
  auto nextStringArgument = [&]() -> const char * {
    if (argument_index >= arguments.size()) {
      throw std::invalid_argument(
          "missing string clause-construction argument");
    }
    const ClauseArgument &argument = arguments[argument_index++];
    const std::string *value = std::get_if<std::string>(&argument);
    if (value == nullptr) {
      throw std::invalid_argument(
          "mistyped string clause-construction argument");
    }
    return value->empty() ? nullptr : value->c_str();
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
    if (!arguments.empty()) {
      throw std::invalid_argument("too many clause-construction arguments");
    }
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
    const char *user_defined_identifier = nullptr;
    if (identifier == OMPC_REDUCTION_IDENTIFIER_user) {
      user_defined_identifier = nextStringArgument();
    }
    new_clause = OpenMPReductionClause::addReductionClause(
        this, modifier, identifier, user_defined_identifier);
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
    new_clause = OpenMPMapClause::addMapClause(this, modifier1, modifier2,
                                               modifier3, type, ref_modifier);
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
    throw std::invalid_argument("unsupported OpenMP clause kind");
  }
  };

  if (argument_index != arguments.size()) {
    throw std::invalid_argument("too many clause-construction arguments");
  }

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
    OpenMPMapClauseType type, OpenMPMapClauseRefModifier ref_modifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_map);
  OpenMPClause *new_clause = NULL;
  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(std::make_unique<OpenMPMapClause>(
        modifier1, modifier2, modifier3, type, ref_modifier));
    current_clauses->push_back(new_clause);
  } else {
    new_clause = directive->registerClause(std::make_unique<OpenMPMapClause>(
        modifier1, modifier2, modifier3, type, ref_modifier));
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
    const char *user_defined_identifier) {

  std::vector<OpenMPClause *> *current_clauses =
      directive->getClauses(OMPC_reduction);
  OpenMPClause *new_clause = NULL;

  if (current_clauses->size() == 0) {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPReductionClause>(modifier, identifier));
    if (identifier == OMPC_REDUCTION_IDENTIFIER_user &&
        user_defined_identifier) {
      ((OpenMPReductionClause *)new_clause)
          ->setUserDefinedIdentifier(user_defined_identifier);
    };
    current_clauses->push_back(new_clause);
  } else {
    new_clause = directive->registerClause(
        std::make_unique<OpenMPReductionClause>(modifier, identifier));
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
