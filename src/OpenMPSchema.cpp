/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#include "OpenMPSchema.h"

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace {

template <std::size_t Size>
constexpr bool isStrictlyIncreasing(const uint32_t (&values)[Size]) {
  for (std::size_t index = 1; index < Size; ++index) {
    if (values[index - 1] >= values[index]) {
      return false;
    }
  }
  return true;
}

} // namespace

namespace ompparser {

const char *getClauseName(OpenMPClauseKind kind) {
  switch (kind) {
#define OPENMP_CLAUSE(Name, Class)                                             \
  case OMPC_##Name:                                                            \
    return #Name;
#define OPENMP_CLAUSE_EXT(Name, Class, Spelling) OPENMP_CLAUSE(Name, Class)
#include "OpenMPKinds.def"
#undef OPENMP_CLAUSE_EXT
#undef OPENMP_CLAUSE
  }
  return "unknown";
}

const char *getDirectiveName(OpenMPDirectiveKind kind) {
  switch (kind) {
#define OPENMP_DIRECTIVE(Name)                                                 \
  case OMPD_##Name:                                                            \
    return #Name;
#define OPENMP_DIRECTIVE_EXT(Name, Spelling) OPENMP_DIRECTIVE(Name)
#include "OpenMPKinds.def"
#undef OPENMP_DIRECTIVE_EXT
#undef OPENMP_DIRECTIVE
  }
  return "unknown";
}

bool isClauseAllowedOnDirective(OpenMPDirectiveKind directive,
                                OpenMPClauseKind clause) {
  constexpr uint32_t ClauseCount = static_cast<uint32_t>(OMPC_unknown) + 1;
  static constexpr uint32_t AllowedDirectiveClauses[] = {
#define OPENMP_ALLOWED_CLAUSE(Directive, Clause)                               \
  static_cast<uint32_t>(OMPD_##Directive) * ClauseCount +                      \
      static_cast<uint32_t>(OMPC_##Clause),
#include "OpenMPApplicability.def"
#undef OPENMP_ALLOWED_CLAUSE
  };
  static_assert(isStrictlyIncreasing(AllowedDirectiveClauses),
                "OpenMPApplicability.def must follow enum order and contain "
                "no duplicate entries");

  const uint32_t key = static_cast<uint32_t>(directive) * ClauseCount +
                       static_cast<uint32_t>(clause);
  return std::binary_search(std::begin(AllowedDirectiveClauses),
                            std::end(AllowedDirectiveClauses), key);
}

ClauseCardinality getClauseCardinality(OpenMPClauseKind kind) {
  switch (kind) {
#define OPENMP_UNIQUE_CLAUSE(Name)                                             \
  case OMPC_##Name:                                                            \
    return ClauseCardinality::Unique;
#include "OpenMPSchema.def"
#undef OPENMP_UNIQUE_CLAUSE
  default:
    return ClauseCardinality::Repeatable;
  }
}

bool clauseRequiresExpressionList(OpenMPClauseKind kind) {
  switch (kind) {
  case OMPC_if:
  case OMPC_num_threads:
  case OMPC_private:
  case OMPC_firstprivate:
  case OMPC_shared:
  case OMPC_copyin:
  case OMPC_align:
  case OMPC_reduction:
  case OMPC_allocate:
  case OMPC_num_teams:
  case OMPC_thread_limit:
  case OMPC_lastprivate:
  case OMPC_collapse:
  case OMPC_linear:
  case OMPC_safelen:
  case OMPC_simdlen:
  case OMPC_aligned:
  case OMPC_nontemporal:
  case OMPC_uniform:
  case OMPC_inclusive:
  case OMPC_exclusive:
  case OMPC_copyprivate:
  case OMPC_initializer:
  case OMPC_final:
  case OMPC_in_reduction:
  case OMPC_priority:
  case OMPC_affinity:
  case OMPC_detach:
  case OMPC_grainsize:
  case OMPC_num_tasks:
  case OMPC_device:
  case OMPC_map:
  case OMPC_use_device_ptr:
  case OMPC_use_device_addr:
  case OMPC_is_device_ptr:
  case OMPC_has_device_addr:
  case OMPC_interop:
  case OMPC_to:
  case OMPC_from:
  case OMPC_link:
  case OMPC_task_reduction:
  case OMPC_hint:
  case OMPC_filter:
  case OMPC_message:
  case OMPC_holds:
  case OMPC_graph_id:
  case OMPC_threadset:
  case OMPC_local:
  case OMPC_looprange:
  case OMPC_permutation:
  case OMPC_counts:
  case OMPC_inductor:
  case OMPC_collector:
  case OMPC_combiner:
  case OMPC_nocontext:
  case OMPC_novariants:
  case OMPC_enter:
  case OMPC_use:
  case OMPC_sizes:
    return true;
  default:
    return false;
  }
}

} // namespace ompparser
