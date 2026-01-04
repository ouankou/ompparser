/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "OpenMPIR.h"
#include <cctype>
#include <emscripten/bind.h>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace {

enum WasmLangMode {
  LangMode_Auto = 0,
  LangMode_C = 1,
  LangMode_Cplusplus = 2,
  LangMode_Fortran = 3
};

static const std::regex &GetFortranRegex() {
  static const std::regex kFortranRegex("^[[:blank:]]*[!cC*]\\$omp",
                                        std::regex_constants::icase);
  return kFortranRegex;
}

bool IsFortranDirective(const std::string &line) {
  return std::regex_search(line, GetFortranRegex());
}

OpenMPBaseLang ResolveLang(int lang_mode, int lang_hint) {
  auto to_base_lang = [](int value, OpenMPBaseLang fallback) {
    switch (value) {
    case LangMode_C:
      return Lang_C;
    case LangMode_Cplusplus:
      return Lang_Cplusplus;
    case LangMode_Fortran:
      return Lang_Fortran;
    default:
      return fallback;
    }
  };

  if (lang_mode == LangMode_Auto) {
    return to_base_lang(lang_hint, Lang_C);
  }

  return to_base_lang(lang_mode, Lang_C);
}

void StripTrailingCarriageReturn(std::string &line) {
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
}

std::string StripBlockComments(const std::string &line, bool &in_block_comment,
                               bool &pending_space) {
  std::string result;
  size_t pos = 0;

  while (pos < line.size()) {
    if (in_block_comment) {
      size_t end = line.find("*/", pos);
      if (end == std::string::npos)
        return result;
      in_block_comment = false;
      pos = end + 2;
      if (pending_space && pos < line.size()) {
        char next_char = line[pos];
        if (!std::isspace(static_cast<unsigned char>(next_char)))
          result.push_back(' ');
      }
      pending_space = false;
      continue;
    }

    size_t start = line.find("/*", pos);
    if (start == std::string::npos) {
      result.append(line.substr(pos));
      break;
    }

    result.append(line.substr(pos, start - pos));
    pending_space = !result.empty() &&
                    !std::isspace(static_cast<unsigned char>(result.back()));
    in_block_comment = true;
    pos = start + 2;
  }

  return result;
}

std::string StripFortranInlineComment(const std::string &text) {
  bool in_single_quote = false;
  bool in_double_quote = false;

  for (size_t i = 0; i < text.size(); ++i) {
    char current = text[i];

    if (current == '\'' && !in_double_quote) {
      if (in_single_quote) {
        if (i + 1 < text.size() && text[i + 1] == '\'') {
          ++i;
        } else {
          in_single_quote = false;
        }
      } else {
        in_single_quote = true;
      }
      continue;
    }

    if (current == '"' && !in_single_quote) {
      if (in_double_quote) {
        if (i + 1 < text.size() && text[i + 1] == '"') {
          ++i;
        } else {
          in_double_quote = false;
        }
      } else {
        in_double_quote = true;
      }
      continue;
    }

    if (current == '!' && !in_single_quote && !in_double_quote)
      return text.substr(0, i);
  }

  return text;
}

std::string TrimLeadingWhitespace(const std::string &text) {
  size_t start = text.find_first_not_of(" \t");
  if (start == std::string::npos)
    return "";
  return text.substr(start);
}

std::vector<std::string> ExtractPragmas(const std::string &input) {
  std::vector<std::string> pragmas;
  std::istringstream stream(input);
  std::string current_line;
  std::regex c_regex(
      "^([[:blank:]]*#[[:blank:]]*pragma)([[:blank:]]+)(omp)[[:blank:]]+(.*)");
  std::regex fortran_regex(
      "^([[:blank:]]*[!cC*]\\$omp&?)([[:blank:]]*)(.*)",
      std::regex_constants::icase);
  std::regex line_comment_regex("//.*$");
  std::regex continue_regex("([\\\\]+[[:blank:]]*$)");
  bool in_block_comment = false;
  bool pending_space = false;

  while (std::getline(stream, current_line)) {
    StripTrailingCarriageReturn(current_line);
    std::string stripped_line =
        StripBlockComments(current_line, in_block_comment, pending_space);
    if (stripped_line.empty())
      continue;

    if (std::regex_match(stripped_line, c_regex)) {
      std::string input_pragma;
      stripped_line =
          std::regex_replace(stripped_line, line_comment_regex, "");

      while (std::regex_search(stripped_line, continue_regex)) {
        stripped_line =
            std::regex_replace(stripped_line, continue_regex, "");
        input_pragma += stripped_line;
        if (!std::getline(stream, current_line)) {
          stripped_line.clear();
          break;
        }
        StripTrailingCarriageReturn(current_line);
        stripped_line =
            StripBlockComments(current_line, in_block_comment, pending_space);
        stripped_line =
            std::regex_replace(stripped_line, line_comment_regex, "");
      }
      input_pragma += stripped_line;
      pragmas.push_back(input_pragma);
      continue;
    }

    if (std::regex_match(stripped_line, fortran_regex)) {
      std::smatch fortran_match;
      std::string combined_body;
      std::string sentinel;
      std::string spacing;

      std::regex_match(stripped_line, fortran_match, fortran_regex);
      sentinel = fortran_match[1].str();
      spacing = fortran_match[2].str();
      combined_body = StripFortranInlineComment(fortran_match[3].str());

      auto strip_trailing_ampersand = [](std::string &text) -> bool {
        size_t end = text.find_last_not_of(" \t");
        if (end == std::string::npos) {
          text.clear();
          return false;
        }
        bool has_ampersand = text[end] == '&';
        if (has_ampersand) {
          text.erase(end);
          end = text.find_last_not_of(" \t");
          if (end != std::string::npos)
            text.erase(end + 1);
          else
            text.clear();
        }
        return has_ampersand;
      };

      bool continue_line = strip_trailing_ampersand(combined_body);

      while (continue_line) {
        std::streampos next_pos = stream.tellg();
        std::string continuation_line;
        if (!std::getline(stream, continuation_line))
          break;
        StripTrailingCarriageReturn(continuation_line);
        continuation_line =
            StripBlockComments(continuation_line, in_block_comment,
                               pending_space);

        std::smatch continuation_match;
        if (!std::regex_match(continuation_line, continuation_match,
                              fortran_regex)) {
          stream.clear();
          stream.seekg(next_pos);
          break;
        }

        std::string continuation_body = continuation_match[3].str();
        continuation_body = StripFortranInlineComment(continuation_body);
        combined_body += " " + continuation_body;
        continue_line = strip_trailing_ampersand(combined_body);
      }

      pragmas.push_back(sentinel + spacing + combined_body);
    }
  }

  return pragmas;
}

std::string ParseAndUnparse(const std::string &input, int lang_mode,
                            int lang_hint) {
  std::vector<std::string> pragmas = ExtractPragmas(input);
  std::ostringstream output;
  const bool auto_lang = (lang_mode == LangMode_Auto);
  OpenMPBaseLang default_lang = ResolveLang(lang_mode, lang_hint);

  setNormalizeClauses(true);

  bool first = true;
  for (const auto &pragma : pragmas) {
    OpenMPBaseLang lang = default_lang;
    if (auto_lang && IsFortranDirective(pragma))
      lang = Lang_Fortran;

    setLang(lang);
    std::string parse_input = pragma;
    if (lang == Lang_Fortran)
      parse_input = TrimLeadingWhitespace(pragma);
    OpenMPDirective *openmp_ast = parseOpenMP(parse_input.c_str(), nullptr);
    if (!openmp_ast)
      continue;

    std::string line = openmp_ast->generatePragmaString();
    delete openmp_ast;

    if (line.empty())
      continue;

    if (!first)
      output << "\n";
    output << line;
    first = false;
  }

  return output.str();
}

} // namespace

EMSCRIPTEN_BINDINGS(ompparser_wasm_module) {
  emscripten::function("parseAndUnparse", &ParseAndUnparse);
}
