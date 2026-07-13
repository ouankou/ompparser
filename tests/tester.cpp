/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <OpenMPIR.h>
#include <OpenMPParser.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>

void output(OpenMPDirective *);
std::string test(OpenMPDirective *);
int openFile(std::ifstream &, const char *);

ompparser::BaseLanguage convertLanguage(OpenMPBaseLang language) {
  switch (language) {
  case Lang_C:
    return ompparser::BaseLanguage::C;
  case Lang_Cplusplus:
    return ompparser::BaseLanguage::CXX;
  case Lang_Fortran:
    return ompparser::BaseLanguage::Fortran;
  case Lang_unknown:
    break;
  }
  return ompparser::BaseLanguage::C;
}

void output(OpenMPDirective *node) {
  if (!node)
    return;

  std::string unparsing_string = node->generatePragmaString();
  std::cout << unparsing_string << "\n";
  node->generateDOT();
}

std::string test(OpenMPDirective *node) {
  if (!node)
    return {};

  return node->generatePragmaString();
}

int openFile(std::ifstream &file, const char *filename) {
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(filename);
  } catch (const std::ifstream::failure &) {
    std::cerr << "Exception caused by opening the given file\n";
    return -1;
  }

  return 0;
}

int main(int argc, const char *argv[]) {
  const char *filename = nullptr;
  int result = -1;
  OpenMPBaseLang default_base_lang = Lang_C;
  bool allow_extensions = false;

  for (int arg_idx = 1; arg_idx < argc; ++arg_idx) {
    const std::string arg = argv[arg_idx];
    if (arg == "--lang=c") {
      default_base_lang = Lang_C;
    } else if (arg == "--lang=c++" || arg == "--lang=cpp" ||
               arg == "--lang=cxx") {
      default_base_lang = Lang_Cplusplus;
    } else if (arg == "--lang=fortran") {
      default_base_lang = Lang_Fortran;
    } else if (arg == "--allow-extensions") {
      allow_extensions = true;
    } else if (filename == nullptr) {
      filename = argv[arg_idx];
    } else {
      std::cout << "Unexpected testing argument: " << arg << "\n";
      return -1;
    }
  }

  std::ifstream input_file;

  if (filename != nullptr)
    result = openFile(input_file, filename);

  if (result != 0) {
    std::cout << "No testing file is available.\n";
    return -1;
  }

  std::string input_pragma;
  std::string output_pragma;
  std::string validation_string;
  int total_amount = 0;
  int passed_amount = 0;
  int failed_amount = 0;
  int invalid_amount = 0;
  int expected_invalid_amount = 0;
  int line_no = 0;
  int current_pragma_line_no = 1;

  char current_char = input_file.peek();
  std::string current_line;
  std::regex fortran_regex("^[!cC*][$][Oo][Mm][Pp]([Xx])?");
  bool expect_invalid_next = false;
  OpenMPBaseLang base_lang = default_base_lang;

  std::string filename_string = std::string(filename);
  filename_string = filename_string.substr(filename_string.rfind("/") + 1);

  while (!input_file.eof()) {
    line_no += 1;
    switch (current_char) {
    case '\n':
      input_file.seekg(1, std::ios_base::cur);
      break;
    default:
      std::getline(input_file, current_line);
      current_line = std::regex_replace(current_line, std::regex("^\\s+"),
                                        std::string(""));
      if (current_line.rfind("EXPECT:", 0) == 0) {
        std::string expectation = current_line.substr(7);
        const auto first = expectation.find_first_not_of(" \t");
        if (first != std::string::npos)
          expectation.erase(0, first);
        const auto last = expectation.find_last_not_of(" \t\r\n");
        if (last != std::string::npos)
          expectation.erase(last + 1);
        std::string lowered = expectation;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        expect_invalid_next =
            (lowered == "invalid" || lowered == "fail" || lowered == "failure");
        current_char = input_file.peek();
        continue;
      }
      const bool is_fortran_directive =
          std::regex_search(current_line, fortran_regex);
      if (is_fortran_directive) {
        base_lang = Lang_Fortran;
      } else {
        base_lang = default_base_lang;
      }
      if ((current_line.size() >= 7 &&
           current_line.compare(0, 7, "#pragma") == 0) ||
          is_fortran_directive) {
        total_amount += 1;
        if (input_pragma.size()) {
          std::cout << "======================================\n";
          std::cout << "Line: " << current_pragma_line_no << "\n";
          std::cout << "GIVEN INPUT: " << input_pragma << "\n";
          std::cout << "GENERATED OUTPUT: " << output_pragma << "\n";
          std::cout << "Missing the output for validation.\n";
          std::cout << "======================================\n";
          invalid_amount += 1;
        }
        current_pragma_line_no = line_no;
        input_pragma = current_line;
        output_pragma.clear();
        bool generated_output = false;
        const bool is_expected_invalid_case = expect_invalid_next;
        ompparser::ParseOptions options;
        options.language = convertLanguage(base_lang);
        if (allow_extensions ||
            filename_string.find("ompx") != std::string::npos)
          options.extensions = ompparser::ExtensionPolicy::AllowRegistered;
        ompparser::ParseResult parse_result =
            ompparser::parseDirective(input_pragma, options);
        if (parse_result.success()) {
          ompparser::UnparseResult unparse_result =
              ompparser::unparse(*parse_result.directive);
          if (unparse_result.success())
            output_pragma = std::move(unparse_result.text);
        } else if (!is_expected_invalid_case) {
          for (const ompparser::Diagnostic &diagnostic :
               parse_result.diagnostics)
            std::cout << "PARSE DIAGNOSTIC: " << diagnostic.message << "\n";
        }
        generated_output = !output_pragma.empty();
        if (is_expected_invalid_case) {
          if (generated_output) {
            std::cout << "======================================\n";
            std::cout << "Line: " << current_pragma_line_no << "\n";
            std::cout << "EXPECTED INVALID INPUT WAS ACCEPTED: " << input_pragma
                      << "\n";
            std::cout << "PARSER OUTPUT: " << output_pragma << "\n";
            std::cout << "======================================\n";
          }
          if (generated_output)
            failed_amount += 1;
          else
            passed_amount += 1;
          expected_invalid_amount += 1;
          input_pragma.clear();
          validation_string.clear();
          output_pragma.clear();
          expect_invalid_next = false;
          current_char = input_file.peek();
          continue;
        }
        expect_invalid_next = false;
      } else if (current_line.rfind("PASS:", 0) == 0) {
        validation_string = current_line.substr(5);
        const auto first_val = validation_string.find_first_not_of(" \t");
        if (first_val != std::string::npos)
          validation_string.erase(0, first_val);
        else
          validation_string.clear();
        if (input_pragma.size() == 0) {
          validation_string.clear();
        } else if (output_pragma != validation_string) {
          std::cout << "======================================\n";
          std::cout << "Line: " << current_pragma_line_no << "\n";
          std::cout << "FAILED INPUT: " << input_pragma << "\n";
          std::cout << "WRONG OUTPUT: " << output_pragma << "\n";
          std::cout << "EXPECTED OUTPUT: " << validation_string << "\n";
          std::cout << "======================================\n";
          failed_amount += 1;
        } else {
          passed_amount += 1;
        }
        input_pragma.clear();
        validation_string.clear();
        expect_invalid_next = false;
      }
    }
    current_char = input_file.peek();
  }
  if (expect_invalid_next) {
    std::cout << "Dangling EXPECT: invalid without an input directive.\n";
    invalid_amount += 1;
  }

  if (input_pragma.size()) {
    std::cout << "======================================\n";
    std::cout << "Line: " << current_pragma_line_no << "\n";
    std::cout << "GIVEN INPUT: " << input_pragma << "\n";
    std::cout << "GENERATED OUTPUT: " << output_pragma << "\n";
    std::cout << "Missing the output for validation.\n";
    std::cout << "======================================\n";
    invalid_amount += 1;
  }
  input_pragma.clear();

  std::cout << "=================== SUMMARY ===================\n";
  std::cout << "TOTAL TESTS  : " << total_amount << "\n";
  std::cout << "PASSED TESTS : " << passed_amount << "\n";
  std::cout << "FAILED TESTS : " << failed_amount << "\n";
  std::cout << "INVALID TESTS: " << invalid_amount << "\n";
  std::cout << "EXPECTED INVALIDS: " << expected_invalid_amount << "\n";

  const bool has_failures = failed_amount > 0 || invalid_amount > 0;
  return has_failures ? 1 : 0;
}
