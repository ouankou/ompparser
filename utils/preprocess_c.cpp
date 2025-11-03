/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <fstream>
#include <iostream>
#include <memory>
#include <regex>

std::unique_ptr<std::vector<std::string>> preProcessCManaged(std::ifstream &);
std::vector<std::string> *preProcessC(std::ifstream &);

std::unique_ptr<std::vector<std::string>>
preProcessCManaged(std::ifstream &input_file) {

  std::string input_pragma;
  auto omp_pragmas = std::make_unique<std::vector<std::string>>();

  char current_char = input_file.peek();
  std::string current_line;
  std::regex c_regex(
      "^([[:blank:]]*#pragma)([[:blank:]]+)(omp)[[:blank:]]+(.*)");
  std::regex fortran_regex(
      "^([[:blank:]]*[!cC*]\\$omp)([[:blank:]]+)(.*)",
      std::regex_constants::icase);
  std::regex comment_regex("[/][*]([^*]|[*][^/])*[*][/]");
  std::regex continue_regex("([\\\\]+[[:blank:]]*$)");

  while (!input_file.eof()) {
    switch (current_char) {
    case '\n':
      input_file.seekg(1, std::ios_base::cur);
      break;
    default:
      std::getline(input_file, current_line);
      // remove the inline comments
      current_line = std::regex_replace(current_line, comment_regex, "");
      input_pragma = "";
      if (std::regex_match(current_line, c_regex)) {
        // combine continuous lines if necessary
        while (std::regex_search(current_line, continue_regex)) {
          // remove the slash part at the end
          current_line = std::regex_replace(current_line, continue_regex, "");
          // add the current line to the pragma string
          input_pragma += current_line;
          // get the next line
          std::getline(input_file, current_line);
          // remove the inline comments of next line
          current_line = std::regex_replace(current_line, comment_regex, "");
        };
        input_pragma += current_line;
        omp_pragmas->push_back(input_pragma);
      } else if (std::regex_match(current_line, fortran_regex)) {
        // Fortran directive found
        input_pragma = current_line;
        omp_pragmas->push_back(input_pragma);
      }
    };
    current_char = input_file.peek();
  };

  return omp_pragmas;
}

std::vector<std::string> *preProcessC(std::ifstream &input_file) {
  return preProcessCManaged(input_file).release();
}
