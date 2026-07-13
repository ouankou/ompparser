/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#ifndef OMPPARSER_OPENMPPARSERINTERNAL_H
#define OMPPARSER_OPENMPPARSERINTERNAL_H

#include "OpenMPIR.h"
#include "OpenMPParser.h"

OpenMPDirective *parseOpenMP(const char *input);
void setLang(OpenMPBaseLang language);

namespace ompparser::detail {

void beginDiagnostics();
void reportDiagnostic(DiagnosticCode code, const std::string &message,
                      int line = 0, int column = 0);
bool hasErrorDiagnostics();
std::vector<Diagnostic> takeDiagnostics();

} // namespace ompparser::detail

#endif // OMPPARSER_OPENMPPARSERINTERNAL_H
