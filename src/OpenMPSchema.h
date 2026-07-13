/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#ifndef OMPPARSER_OPENMPSCHEMA_H
#define OMPPARSER_OPENMPSCHEMA_H

#include "OpenMPKinds.h"

namespace ompparser {

enum class ClauseCardinality { Repeatable, Unique };

const char *getClauseName(OpenMPClauseKind kind);
const char *getDirectiveName(OpenMPDirectiveKind kind);
ClauseCardinality getClauseCardinality(OpenMPClauseKind kind);
bool clauseRequiresExpressionList(OpenMPClauseKind kind);
bool isClauseAllowedOnDirective(OpenMPDirectiveKind directive,
                                OpenMPClauseKind clause);

} // namespace ompparser

#endif // OMPPARSER_OPENMPSCHEMA_H
