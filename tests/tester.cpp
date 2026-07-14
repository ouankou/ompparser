/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <OpenMPIR.h>
#include <algorithm>
#include <cctype>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <string>
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

void output(OpenMPDirective *);
std::string test(OpenMPDirective *);
int openFile(std::ifstream &, const char *);

struct DeathTestResult {
  bool aborted = false;
  std::string diagnostic;
};

DeathTestResult expectInvalidDirectiveDeath(const std::string &input,
                                            OpenMPBaseLang base_lang) {
  int diagnostic_pipe[2];
  if (pipe(diagnostic_pipe) != 0) {
    std::cerr << "Failed to create expected-invalid diagnostic pipe\n";
    return {};
  }

  const pid_t child = fork();
  if (child < 0) {
    close(diagnostic_pipe[0]);
    close(diagnostic_pipe[1]);
    std::cerr << "Failed to fork expected-invalid parser process\n";
    return {};
  }

  if (child == 0) {
    (void)prctl(PR_SET_DUMPABLE, 0);
    close(diagnostic_pipe[0]);
    if (dup2(diagnostic_pipe[1], STDERR_FILENO) < 0) {
      _exit(125);
    }
    close(diagnostic_pipe[1]);
    OpenMPParseOptions options;
    options.base_lang = base_lang;
    auto directive = parseOpenMP(input.c_str(), options);
    (void)directive->generatePragmaString();
    _exit(0);
  }

  close(diagnostic_pipe[1]);
  std::string diagnostic;
  char buffer[512];
  ssize_t count = 0;
  while ((count = read(diagnostic_pipe[0], buffer, sizeof(buffer))) > 0) {
    diagnostic.append(buffer, static_cast<std::size_t>(count));
  }
  close(diagnostic_pipe[0]);

  int status = 0;
  if (waitpid(child, &status, 0) != child) {
    std::cerr << "Failed to wait for expected-invalid parser process\n";
    return {};
  }

  const bool has_single_diagnostic =
      diagnostic.rfind("OMPPARSER_", 0) == 0 &&
      std::count(diagnostic.begin(), diagnostic.end(), '\n') == 1;
  return {WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT &&
              has_single_diagnostic,
          diagnostic};
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

  for (int arg_idx = 1; arg_idx < argc; ++arg_idx) {
    const std::string arg = argv[arg_idx];
    if (arg == "--lang=c") {
      default_base_lang = Lang_C;
    } else if (arg == "--lang=c++" || arg == "--lang=cpp" ||
               arg == "--lang=cxx") {
      default_base_lang = Lang_Cplusplus;
    } else if (arg == "--lang=fortran") {
      default_base_lang = Lang_Fortran;
    } else if (filename == nullptr) {
      filename = argv[arg_idx];
    } else {
      std::cout << "Unexpected testing argument: " << arg << "\n";
      return -1;
    }
  }

  std::ifstream input_file;
  std::ofstream output_file;

  if (filename != nullptr)
    result = openFile(input_file, filename);

  if (result != 0) {
    std::cout << "No testing file is available.\n";
    return -1;
  }

  // open the result table file in /tmp to avoid cluttering source tree
  output_file.open("/tmp/ompparser_results.md", std::ios_base::app);
  if (!output_file.is_open()) {
    std::cout << "No output file is available.\n";
    return -1;
  }

  std::string input_pragma;
  std::string output_pragma;
  std::string validation_string;
  std::string pass = "true";
  std::map<std::string, std::string> processed_data;
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
  const std::string invalid_marker = "invalid test without paired validation.";
  bool expect_invalid_next = false;
  bool expect_invalid_block = false;
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
      if (current_line.find(invalid_marker) != std::string::npos) {
        const auto comment_pos = current_line.find("//");
        if (comment_pos != std::string::npos) {
          std::string trimmed = current_line.substr(0, comment_pos);
          while (!trimmed.empty() &&
                 std::isspace(static_cast<unsigned char>(trimmed.back())))
            trimmed.pop_back();
          if (trimmed.empty()) {
            expect_invalid_block = true;
            current_char = input_file.peek();
            continue;
          }
          current_line = trimmed;
        }
        expect_invalid_next = true;
      }
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
        auto search_pragma = processed_data.find(input_pragma);
        const bool has_cached_output = search_pragma != processed_data.end();
        bool generated_output = false;
        const bool is_expected_invalid_case =
            expect_invalid_next || expect_invalid_block;
        if (is_expected_invalid_case) {
          const DeathTestResult death =
              expectInvalidDirectiveDeath(input_pragma, base_lang);
          output_pragma.clear();
          generated_output = false;
          pass = death.aborted ? "true" : "false";
          output_file << filename_string.c_str() << " | ` "
                      << input_pragma.c_str() << " ` | ` expected hard error ` | "
                      << pass.c_str() << "\n";
          if (!death.aborted) {
            std::cout << "======================================\n";
            std::cout << "Line: " << current_pragma_line_no << "\n";
            std::cout << "EXPECTED HARD FAILURE: " << input_pragma << "\n";
            std::cout << "DIAGNOSTIC: " << death.diagnostic << "\n";
            std::cout << "======================================\n";
          }
        } else if (has_cached_output) {
          output_pragma = search_pragma->second;
          generated_output = output_pragma.size() != 0;
          pass = generated_output ? "true" : "false";
        } else {
          OpenMPParseOptions options;
          options.base_lang = base_lang;
          auto openMPAST = parseOpenMP(input_pragma.c_str(), options);
          output_pragma = test(openMPAST.get());
          generated_output = output_pragma.size() != 0;
          if (!generated_output)
            pass = "false";
          output_file << filename_string.c_str() << " | ` "
                      << input_pragma.c_str() << " ` | ` "
                      << output_pragma.c_str() << " ` | " << pass.c_str()
                      << "\n";
          processed_data[input_pragma] = output_pragma;
        }
        if (is_expected_invalid_case) {
          if (pass == "true") {
            passed_amount += 1;
            expected_invalid_amount += 1;
          } else {
            failed_amount += 1;
          }
          input_pragma.clear();
          validation_string.clear();
          output_pragma.clear();
          pass = "true";
          expect_invalid_next = false;
          break;
        }
        pass = "true";
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
        expect_invalid_block = false;
      }
    }
    current_char = input_file.peek();
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
