/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

//===--- OpenMPKinds.h - OpenMP enums ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines some OpenMP-specific enums and functions.
///
//===----------------------------------------------------------------------===//

#ifndef __OPENMPKINDS_H__
#define __OPENMPKINDS_H__

/// OpenMP directives.
enum OpenMPDirectiveKind {
#define OPENMP_DIRECTIVE(Name) OMPD_##Name,
#define OPENMP_DIRECTIVE_EXT(Name, Str) OMPD_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DIRECTIVE
#undef OPENMP_DIRECTIVE_EXT
};

/// OpenMP clauses.
enum OpenMPClauseKind {
#define OPENMP_CLAUSE(Name, Class) OMPC_##Name,
#define OPENMP_CLAUSE_EXT(Name, Class, Str) OPENMP_CLAUSE(Name, Class)
#include "OpenMPKinds.def"
#undef OPENMP_CLAUSE_EXT
#undef OPENMP_CLAUSE
};

// Separator used between clauses when reconstructing pragmas.
enum OpenMPClauseSeparator { OMPC_CLAUSE_SEP_space, OMPC_CLAUSE_SEP_comma };

// context selector set for 'when' clause.
enum OpenMPWhenClauseSelectorSet {
#define OPENMP_WHEN_SELECTOR_SET(Name) OMPC_WHEN_SELECTOR_SET_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_WHEN_SELECTOR_SET
};

// context selector for 'when' clause.
enum OpenMPWhenClauseSelectorParameter {
#define OPENMP_WHEN_SELECTOR_PARAMETER(Name)                                   \
  OMPC_WHEN_SELECTOR_PARAMETER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_WHEN_SELECTOR_PARAMETER
};

// context selector for 'when' clause.
enum OpenMPClauseContextKind {
#define OPENMP_CONTEXT_KIND(Name) OMPC_CONTEXT_KIND_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_CONTEXT_KIND
};

// context selector for 'when' clause.
enum OpenMPClauseContextVendor {
#define OPENMP_CONTEXT_VENDOR(Name) OMPC_CONTEXT_VENDOR_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_CONTEXT_VENDOR
};

enum OpenMPImplementationExprKind {
  OMPC_IMPL_EXPR_unknown,
  OMPC_IMPL_EXPR_user,
  OMPC_IMPL_EXPR_requires
};

// Preserve ordering of selector sets in variant constructs.
enum OpenMPContextSelectorSequenceKind {
  OMPC_SELECTOR_user,
  OMPC_SELECTOR_construct,
  OMPC_SELECTOR_device,
  OMPC_SELECTOR_target_device,
  OMPC_SELECTOR_implementation
};

enum OpenMPInitClauseKind {
  OMPC_INIT_KIND_target,
  OMPC_INIT_KIND_targetsync,
  OMPC_INIT_KIND_unknown
};

enum OpenMPAdjustArgsModifier {
  OMPC_ADJUST_ARGS_need_device_ptr,
  OMPC_ADJUST_ARGS_unknown
};

enum OpenMPAppendArgsModifier { OMPC_APPEND_ARGS_unknown };

enum OpenMPIteratorKind { OMP_ITER_unknown, OMP_ITER_iterator };

enum OpenMPApplyTransformKind {
  OMPC_APPLY_TRANSFORM_unroll,
  OMPC_APPLY_TRANSFORM_unroll_partial,
  OMPC_APPLY_TRANSFORM_unroll_full,
  OMPC_APPLY_TRANSFORM_reverse,
  OMPC_APPLY_TRANSFORM_interchange,
  OMPC_APPLY_TRANSFORM_nothing,
  OMPC_APPLY_TRANSFORM_tile_sizes,
  OMPC_APPLY_TRANSFORM_apply,
  OMPC_APPLY_TRANSFORM_unknown
};

// OpenMP attributes for 'if' clause.
enum OpenMPIfClauseModifier {
#define OPENMP_IF_MODIFIER(Name) OMPC_IF_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_IF_MODIFIER
};

/// OpenMP attributes for 'default' clause.
enum OpenMPDefaultClauseKind {
#define OPENMP_DEFAULT_KIND(Name) OMPC_DEFAULT_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEFAULT_KIND
};

/// modifiers for 'order' clause.
enum OpenMPOrderClauseModifier {
#define OPENMP_ORDER_MODIFIER(Name) OMPC_ORDER_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_ORDER_MODIFIER
};

/// OpenMP attributes for 'order' clause.
enum OpenMPOrderClauseKind {
#define OPENMP_ORDER_KIND(Name) OMPC_ORDER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_ORDER_KIND
};

/// OpenMP attributes for 'proc_bind' clause.
enum OpenMPProcBindClauseKind {
#define OPENMP_PROC_BIND_KIND(Name) OMPC_PROC_BIND_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_PROC_BIND_KIND
};

/// OpenMP attributes for 'Allocate' clause.
enum OpenMPAllocateClauseAllocator {
#define OPENMP_ALLOCATE_ALLOCATOR_KIND(Name) OMPC_ALLOCATE_ALLOCATOR_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_ALLOCATE_ALLOCATOR_KIND
};

/// OpenMP attributes for 'Allocator' clause.
enum OpenMPAllocatorClauseAllocator {
#define OPENMP_ALLOCATOR_ALLOCATOR_KIND(Name) OMPC_ALLOCATOR_ALLOCATOR_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_ALLOCATOR_ALLOCATOR_KIND
};

/// modifiers for 'reduction' clause.
enum OpenMPReductionClauseModifier {
#define OPENMP_REDUCTION_MODIFIER(Name) OMPC_REDUCTION_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_REDUCTION_MODIFIER
};

/// identifiers for 'reduction' clause.
enum OpenMPReductionClauseIdentifier {
#define OPENMP_REDUCTION_IDENTIFIER(Name) OMPC_REDUCTION_IDENTIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_REDUCTION_IDENTIFIER
};

/// modifiers for 'lastprivate' clause.
enum OpenMPLastprivateClauseModifier {
#define OPENMP_LASTPRIVATE_MODIFIER(Name) OMPC_LASTPRIVATE_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_LASTPRIVATE_MODIFIER
};

/// step for 'linear' clause.
enum OpenMPLinearClauseStep {
#define OPENMP_LINEAR_STEP(Name) OMPC_LINEAR_Step_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_LINEAR_STEP
};

/// modifiers for 'linear' clause.
enum OpenMPLinearClauseModifier {
#define OPENMP_LINEAR_MODIFIER(Name) OMPC_LINEAR_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_LINEAR_MODIFIER
};

/// modifiers for 'schedule' clause.
enum OpenMPScheduleClauseModifier {
#define OPENMP_SCHEDULE_MODIFIER(Name) OMPC_SCHEDULE_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_SCHEDULE_MODIFIER
};

/// OpenMP attributes for 'schedule' clause.
enum OpenMPScheduleClauseKind {
#define OPENMP_SCHEDULE_KIND(Name) OMPC_SCHEDULE_KIND_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_SCHEDULE_KIND
};

/// OpenMP attributes for 'dist_schedule' clause.
enum OpenMPDistScheduleClauseKind {
#define OPENMP_DIST_SCHEDULE_KIND(Name) OMPC_DIST_SCHEDULE_KIND_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DIST_SCHEDULE_KIND
};

/// OpenMP attributes for 'grainsize' clause modifier (OpenMP 5.1).
enum OpenMPGrainsizeClauseModifier {
#define OPENMP_GRAINSIZE_MODIFIER(Name) OMPC_GRAINSIZE_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_GRAINSIZE_MODIFIER
};

/// OpenMP attributes for 'num_tasks' clause modifier (OpenMP 5.1).
enum OpenMPNumTasksClauseModifier {
#define OPENMP_NUM_TASKS_MODIFIER(Name) OMPC_NUM_TASKS_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_NUM_TASKS_MODIFIER
};

/// OpenMP attributes for 'bind' clause.
enum OpenMPBindClauseBinding {
#define OPENMP_BIND_BINDING(Name) OMPC_BIND_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_BIND_BINDING
};

/// omp_priv for 'initializer' clause.
enum OpenMPInitializerClausePriv {
#define OPENMP_INITIALIZER_PRIV(Name) OMPC_INITIALIZER_PRIV_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_INITIALIZER_PRIV
};

/// OpenMP attributes for 'atomic_default_mem_order' clause.
enum OpenMPAtomicDefaultMemOrderClauseKind {
#define OPENMP_ATOMIC_DEFAULT_MEM_ORDER_KIND(Name)                             \
  OMPC_ATOMIC_DEFAULT_MEM_ORDER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_ATOMIC_DEFAULT_MEM_ORDER_KIND
};

/// OpenMP attributes for 'UsesAllocators' clause.
enum OpenMPUsesAllocatorsClauseAllocator {
#define OPENMP_USESALLOCATORS_ALLOCATOR_KIND(Name)                             \
  OMPC_USESALLOCATORS_ALLOCATOR_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_USESALLOCATORS_ALLOCATOR_KIND
};

/// modifiers for 'device' clause.
enum OpenMPDeviceClauseModifier {
#define OPENMP_DEVICE_MODIFIER(Name) OMPC_DEVICE_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEVICE_MODIFIER
};

enum OpenMPInReductionClauseIdentifier {
#define OPENMP_IN_REDUCTION_IDENTIFIER(Name)                                   \
  OMPC_IN_REDUCTION_IDENTIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_IN_REDUCTION_IDENTIFIER
};

enum OpenMPDependClauseModifier {
#define OPENMP_DEPEND_MODIFIER(Name) OMPC_DEPEND_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEPEND_MODIFIER
};

enum OpenMPDeclareMapperDirectiveIdentifier {
#define OPENMP_DECLARE_MAPPER_IDENTIFIER(Name)                                 \
  OMPD_DECLARE_MAPPER_IDENTIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DECLARE_MAPPER_IDENTIFIER
};

enum OpenMPDependClauseType {
#define OPENMP_DEPENDENCE_TYPE(Name) OMPC_DEPENDENCE_TYPE_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEPENDENCE_TYPE
};

enum OpenMPAffinityClauseModifier {
#define OPENMP_AFFINITY_MODIFIER(Name) OMPC_AFFINITY_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_AFFINITY_MODIFIER
};

enum OpenMPToClauseKind {
#define OPENMP_TO_KIND(Name) OMPC_TO_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_TO_KIND
};

enum OpenMPFromClauseKind {
#define OPENMP_FROM_KIND(Name) OMPC_FROM_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_FROM_KIND
};

enum OpenMPDefaultmapClauseBehavior {
#define OPENMP_DEFAULTMAP_BEHAVIOR(Name) OMPC_DEFAULTMAP_BEHAVIOR_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEFAULTMAP_BEHAVIOR
};

enum OpenMPDefaultmapClauseCategory {
#define OPENMP_DEFAULTMAP_CATEGORY(Name) OMPC_DEFAULTMAP_CATEGORY_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEFAULTMAP_CATEGORY
};

enum OpenMPDeviceTypeClauseKind {
#define OPENMP_DEVICE_TYPE_KIND(Name) OMPC_DEVICE_TYPE_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEVICE_TYPE_KIND
};

enum OpenMPMapIteratorKind {
  OMPC_MAP_ITERATOR_unknown,
  OMPC_MAP_ITERATOR_iterator
};

/// modifiers for 'map' clause.
enum OpenMPMapClauseModifier {
#define OPENMP_MAP_MODIFIER(Name) OMPC_MAP_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_MAP_MODIFIER
};

enum OpenMPMapClauseRefModifier {
#define OPENMP_MAP_REF_MODIFIER(Name) OMPC_MAP_REF_MODIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_MAP_REF_MODIFIER
};

enum OpenMPMapClauseType {
#define OPENMP_MAP_TYPE(Name) OMPC_MAP_TYPE_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_MAP_TYPE
};

enum OpenMPTaskReductionClauseIdentifier {
#define OPENMP_TASK_REDUCTION_IDENTIFIER(Name)                                 \
  OMPC_TASK_REDUCTION_IDENTIFIER_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_TASK_REDUCTION_IDENTIFIER
};

enum OpenMPDepobjUpdateClauseDependeceType {
#define OPENMP_DEPOBJ_UPDATE_DEPENDENCE_TYPE(Name)                             \
  OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DEPOBJ_UPDATE_DEPENDENCE_TYPE
};

/// OpenMP attributes for 'doacross' clause (OpenMP 5.2)
enum OpenMPDoacrossClauseType {
#define OPENMP_DOACROSS_TYPE(Name) OMPC_DOACROSS_TYPE_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_DOACROSS_TYPE
};

/// OpenMP attributes for 'at' clause (OpenMP 5.1 - error directive)
enum OpenMPAtClauseKind {
#define OPENMP_AT_KIND(Name) OMPC_AT_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_AT_KIND
};

/// OpenMP attributes for 'severity' clause (OpenMP 5.1 - error directive)
enum OpenMPSeverityClauseKind {
#define OPENMP_SEVERITY_KIND(Name) OMPC_SEVERITY_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_SEVERITY_KIND
};

/// OpenMP attributes for 'fail' clause (OpenMP 5.1 - atomic)
enum OpenMPFailClauseMemoryOrder {
#define OPENMP_FAIL_MEMORY_ORDER(Name) OMPC_FAIL_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_FAIL_MEMORY_ORDER
};

/// OpenMP attributes for 'memscope' clause (OpenMP 6.0)
enum OpenMPMemscopeClauseKind {
#define OPENMP_MEMSCOPE_KIND(Name) OMPC_MEMSCOPE_##Name,
#include "OpenMPKinds.def"
#undef OPENMP_MEMSCOPE_KIND
};

#endif
