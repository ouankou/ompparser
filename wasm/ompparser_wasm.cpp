/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "OpenMPIR.h"
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

std::vector<std::string> ExtractPragmas(const std::string &input) {
  std::vector<std::string> pragmas;
  std::istringstream stream(input);
  std::string current_line;
  std::regex c_regex(
      "^([[:blank:]]*#pragma)([[:blank:]]+)(omp)[[:blank:]]+(.*)");
  std::regex fortran_regex(
      "^([[:blank:]]*[!cC*]\\$omp&?)([[:blank:]]*)(.*)",
      std::regex_constants::icase);
  std::regex comment_regex("[/][*]([^*]|[*][^/])*[*][/]");
  std::regex line_comment_regex("//.*$");
  std::regex continue_regex("([\\\\]+[[:blank:]]*$)");

  while (std::getline(stream, current_line)) {
    StripTrailingCarriageReturn(current_line);

    current_line = std::regex_replace(current_line, comment_regex, "");

    if (std::regex_match(current_line, c_regex)) {
      std::string input_pragma;
      current_line = std::regex_replace(current_line, line_comment_regex, "");

      while (std::regex_search(current_line, continue_regex)) {
        current_line = std::regex_replace(current_line, continue_regex, "");
        input_pragma += current_line;
        if (!std::getline(stream, current_line)) {
          current_line.clear();
          break;
        }
        StripTrailingCarriageReturn(current_line);
        current_line = std::regex_replace(current_line, comment_regex, "");
        current_line = std::regex_replace(current_line, line_comment_regex, "");
      }
      input_pragma += current_line;
      pragmas.push_back(input_pragma);
      continue;
    }

    if (std::regex_match(current_line, fortran_regex)) {
      std::smatch fortran_match;
      std::string combined_body;
      std::string sentinel;
      std::string spacing;

      std::regex_match(current_line, fortran_match, fortran_regex);
      sentinel = fortran_match[1].str();
      spacing = fortran_match[2].str();
      combined_body = fortran_match[3].str();

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
        continuation_line = std::regex_replace(continuation_line, comment_regex,
                                               "");

        std::smatch continuation_match;
        if (!std::regex_match(continuation_line, continuation_match,
                              fortran_regex)) {
          stream.clear();
          stream.seekg(next_pos);
          break;
        }

        std::string continuation_body = continuation_match[3].str();
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
    OpenMPDirective *openmp_ast = parseOpenMP(pragma.c_str(), nullptr);
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
