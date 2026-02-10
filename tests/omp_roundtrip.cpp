/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include <OpenMPIR.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>

void output(std::vector<OpenMPDirective *> *);
void savePragmaList(std::vector<OpenMPDirective *> *, const char *);
int openFile(std::ifstream &, const char *);
extern std::unique_ptr<std::vector<std::string>> preProcessCManaged(
    std::ifstream &);

void output(std::vector<OpenMPDirective *> *omp_ast_list) {

  if (omp_ast_list != NULL) {
    for (unsigned int i = 0; i < omp_ast_list->size(); i++) {
      if (omp_ast_list->at(i) != NULL) {
        std::cout << omp_ast_list->at(i)->generatePragmaString() << std::endl;
      } else {
        std::cout << "NULL" << std::endl;
      };
    };
  };
}

void savePragmaList(std::vector<OpenMPDirective *> *omp_ast_list,
                    const char *filename) {

  std::string output_filename = std::string(filename) + ".pragmas";
  std::ofstream output_file(output_filename.c_str(), std::ofstream::trunc);

  if (omp_ast_list != NULL) {
    for (unsigned int i = 0; i < omp_ast_list->size(); i++) {
      if (omp_ast_list->at(i) != NULL) {
        output_file << omp_ast_list->at(i)->generatePragmaString() << std::endl;
      } else {
        output_file << "NULL" << std::endl;
      };
    };
  };

  output_file.close();
}

int openFile(std::ifstream &file, const char *filename) {
  file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
  try {
    file.open(filename);
  } catch (std::ifstream::failure const &) {
    std::cerr << "Exception caused by opening the given file\n";
    return -1;
  }

  return 0;
}

int main(int argc, const char *argv[]) {
  const char *filename = NULL;
  bool normalize_clauses = true; // Default: normalization enabled
  int result;
  unsigned int i;
  auto omp_ast_list = std::make_unique<std::vector<OpenMPDirective *>>();
  OpenMPDirective *omp_ast = NULL;
  auto omp_directive_list = std::make_unique<std::vector<std::string>>();

  // Parse command line arguments
  for (int arg_idx = 1; arg_idx < argc; arg_idx++) {
    if (strcmp(argv[arg_idx], "--no-normalize") == 0) {
      normalize_clauses = false;
    } else if (filename == NULL) {
      filename = argv[arg_idx];
    }
  }

  std::ifstream input_file;

  if (filename == NULL) {
    std::cout << "No specific testing file is provided, use the default "
                 "PARALLEL testing instead.\n";
    filename = "../tests/parallel.txt";
  };
  result = openFile(input_file, filename);
  if (result) {
    std::cout << "No testing file is available.\n";
    return -1;
  };

  std::unique_ptr<std::vector<std::string>> omp_pragmas =
      preProcessCManaged(input_file);

  // Set normalization flag globally before parsing
  setNormalizeClauses(normalize_clauses);

  // Detect language from file extension
  std::string filename_str(filename);
  OpenMPBaseLang default_lang = Lang_C;
  if (filename_str.length() >= 4) {
    std::string ext = filename_str.substr(filename_str.length() - 4);
    if (ext == ".cpp" || ext == ".cxx" || ext == ".c++" ||
        (filename_str.length() >= 3 &&
         filename_str.substr(filename_str.length() - 3) == ".cc")) {
      default_lang = Lang_Cplusplus;
    } else if (ext == ".f90" || ext == ".F90" || ext == ".f95" || ext == ".F95" ||
               (filename_str.length() >= 2 &&
                (filename_str.substr(filename_str.length() - 2) == ".f" ||
                 filename_str.substr(filename_str.length() - 2) == ".F"))) {
      default_lang = Lang_Fortran;
    }
  }

  // Regex to detect Fortran directives (case-insensitive for $omp/$OMP)
  std::regex fortran_regex("^[[:blank:]]*[!cC\\*]\\$omp", std::regex_constants::icase);

  // parse the preprocessed inputs
  for (i = 0; i < omp_pragmas->size(); i++) {
    // Detect if this is a Fortran directive and set language accordingly
    if (std::regex_search(omp_pragmas->at(i), fortran_regex)) {
      setLang(Lang_Fortran);
    } else {
      setLang(default_lang);
    }

    omp_ast = parseOpenMP(omp_pragmas->at(i).c_str(), nullptr, nullptr);
    omp_ast_list->push_back(omp_ast);
    if (omp_ast != NULL) {
      omp_directive_list->push_back(omp_ast->toString());
    } else {
      omp_directive_list->push_back("");
    };
  };

  std::cout << "=================== SUMMARY ===================\n";
  std::cout << "TOTAL OPENMP PRAGMAS: " << omp_pragmas->size() << "\n";

  output(omp_ast_list.get());

  savePragmaList(omp_ast_list.get(), filename);

  // Clean up allocated directives
  for (OpenMPDirective *directive : *omp_ast_list) {
    delete directive;
  }

  return 0;
}
