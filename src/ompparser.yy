/*
 * Copyright (c) 2018-2025, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

/* OpenMP C/C++/Fortran Grammar */

%define api.prefix {openmp_}
%defines
%define parse.error verbose

%{

#include "OpenMPIR.h"

#include <cctype>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

/*the scanner function*/
extern int openmp_lex(); 

/*A customized initialization function for the scanner, str is the string to be scanned.*/
extern void openmp_lexer_init(const char *str);

/* Standalone ompparser */
extern void start_lexer(const char* input);
extern void end_lexer(void);
extern "C" void openmp_reset_lexer_flags();
extern "C" bool openmp_consume_compact_parallel_do();
extern "C" bool openmp_consume_declare_target_underscore();
extern "C" bool openmp_consume_compact_enddo();

//The directive/clause that are being parsed
static OpenMPDirective *current_directive = nullptr;
static OpenMPClause *current_clause = nullptr;
static OpenMPDirective *current_parent_directive = nullptr;
static OpenMPClause *current_parent_clause = nullptr;
static std::vector<OpenMPApplyClause *> apply_clause_stack;
static std::vector<OpenMPClauseSeparator> apply_clause_separator_stack;
static int firstParameter = 0;
static int secondParameter = 0;
static int thirdParameter = 0;
static OpenMPUsesAllocatorsClauseAllocator usesAllocator;
static const char *firstStringParameter = "";
static const char *secondStringParameter = "";
static const char *reduction_modifier_expression = nullptr;
OpenMPClauseSeparator current_expr_separator = OMPC_CLAUSE_SEP_space;
OpenMPClauseSeparator current_apply_transform_separator = OMPC_CLAUSE_SEP_comma;
static int map_ref_modifier_parameter = OMPC_MAP_REF_MODIFIER_unspecified;
static std::vector<const char *> iterator_definition;
static std::vector<const char *> depend_iterator_definition;
static std::vector<std::vector<const char *>> depend_iterators_definition_class;
static std::vector<const char *> map_iterator_args;
static std::vector<const char *> tofrom_iterator_args;
static inline bool hasMapIteratorModifier() {
  return firstParameter == OMPC_MAP_MODIFIER_iterator ||
         secondParameter == OMPC_MAP_MODIFIER_iterator ||
         thirdParameter == OMPC_MAP_MODIFIER_iterator;
}

static void addMapIteratorDefinition(OpenMPClause *clause,
                                     std::vector<const char *> *args) {
  if (clause == nullptr || args == nullptr) {
    if (args != nullptr) {
      args->clear();
    }
    return;
  }

  auto *map_clause = static_cast<OpenMPMapClause *>(clause);
  map_clause->clearIterators();
  if (args->size() < 3) {
    args->clear();
    return;
  }
  std::string qualifier;
  std::string var((*args)[0]);
  std::string begin((*args)[1]);
  std::string end((*args)[2]);
  std::string step;
  if (args->size() > 3) {
    step = std::string((*args)[3]);
  }

  map_clause->addIterator(qualifier, var, begin, end, step);
  args->clear();
}

static void addToFromIteratorDefinition(OpenMPClause *clause,
                                        std::vector<const char *> *args) {
  if (clause == nullptr || args == nullptr) {
    if (args != nullptr) {
      args->clear();
    }
    return;
  }
  if (args->size() < 3) {
    args->clear();
    return;
  }

  std::string qualifier;
  std::string var((*args)[0]);
  std::string begin((*args)[1]);
  std::string end((*args)[2]);
  std::string step;
  if (args->size() > 3) {
    step = std::string((*args)[3]);
  }

  if (clause->getKind() == OMPC_to) {
    auto *to_clause = static_cast<OpenMPToClause *>(clause);
    to_clause->clearIterators();
    to_clause->addIterator(qualifier, var, begin, end, step);
  } else if (clause->getKind() == OMPC_from) {
    auto *from_clause = static_cast<OpenMPFromClause *>(clause);
    from_clause->clearIterators();
    from_clause->addIterator(qualifier, var, begin, end, step);
  }
  args->clear();
}

static const char *trait_score = "";
/* Treat the entire expression as a string for now */
extern void openmp_parse_expr();
extern void openmp_begin_type_string();
extern void openmp_begin_raw_expression();
static int openmp_error(const char *);
void *(*exprParse)(const char *) = nullptr;

bool b_within_variable_list =
    false; // a flag to indicate if the program is now processing a list of
           // variables

/* used for language setting and detecting*/
OpenMPBaseLang user_set_lang = Lang_unknown;
OpenMPBaseLang auto_lang;
void setLang(OpenMPBaseLang _lang) { user_set_lang = _lang; };

/* used for clause normalization control */
bool normalize_clauses_global = true;
void setNormalizeClauses(bool normalize) { normalize_clauses_global = normalize; };

// Track whether the next clause should be preceded by a comma (to preserve Fortran spacing)
bool clause_separator_comma = false;
static std::string current_pragma_raw;

%}

%locations

/* The %union declaration specifies the entire collection of possible data types for semantic values.
these names are used in the %token and %type declarations to pick one of the types for a terminal or nonterminal symbol
corresponding C type is union name defaults to YYSTYPE.
*/

%union {  int itype;
          double ftype;
          const char* stype;
          void* ptype; /* For expressions or variables */
        }


%token  OMP PARALLEL FOR DO DECLARE DISTRIBUTE LOOP SCAN SECTIONS SECTION SINGLE CANCEL TASKGROUP CANCELLATION POINT THREAD VARIANT THREADPRIVATE METADIRECTIVE MAPPER
        IF NUM_THREADS DEFAULT PRIVATE FIRSTPRIVATE SAVED SHARED COPYIN REDUCTION PROC_BIND ALLOCATE SIMD TASK LASTPRIVATE WHEN MATCH PARTIAL FULL
        LINEAR SCHEDULE COLLAPSE NOWAIT ORDER ORDERED MODIFIER_CONDITIONAL MODIFIER_MONOTONIC MODIFIER_NONMONOTONIC STATIC DYNAMIC GUIDED AUTO RUNTIME MODOFIER_VAL MODOFIER_REF MODOFIER_UVAL MODIFIER_SIMD
        SAFELEN SIMDLEN ALIGNED ALIGN NONTEMPORAL UNIFORM INBRANCH NOTINBRANCH DIST_SCHEDULE BIND INCLUSIVE EXCLUSIVE COPYPRIVATE ALLOCATOR INITIALIZER OMP_PRIV IDENTIFIER_DEFAULT WORKSHARE/*YAYING*/
        NONE MASTER PRIMARY CLOSE SPREAD MODIFIER_INSCAN MODIFIER_TASK MODIFIER_DEFAULT 
        PLUS MINUS STAR BITAND BITOR BITXOR LOGAND LOGOR EQV NEQV MAX MIN
        DEFAULT_MEM_ALLOC LARGE_CAP_MEM_ALLOC CONST_MEM_ALLOC HIGH_BW_MEM_ALLOC LOW_LAT_MEM_ALLOC CGROUP_MEM_ALLOC
        PTEAM_MEM_ALLOC THREAD_MEM_ALLOC
        TEAMS
        NUM_TEAMS THREAD_LIMIT DOUBLE_COLON
        END USER CONSTRUCT DEVICE IMPLEMENTATION CONDITION SCORE VENDOR
        KIND HOST NOHOST ANY CPU GPU FPGA ISA ARCH EXTENSION
        AMD ARM BSC CRAY FUJITSU GNU IBM INTEL LLVM NVIDIA PGI TI UNKNOWN
        FINAL UNTIED MERGEABLE IN_REDUCTION DEPEND PRIORITY AFFINITY DETACH MODIFIER_ITERATOR DEPOBJ FINAL_CLAUSE IN INOUT INOUTSET MUTEXINOUTSET OUT
	        TASKLOOP GRAINSIZE NUM_TASKS NOGROUP TASKYIELD REQUIRES REVERSE_OFFLOAD UNIFIED_ADDRESS UNIFIED_SHARED_MEMORY ATOMIC_DEFAULT_MEM_ORDER DYNAMIC_ALLOCATORS SELF_MAPS SEQ_CST ACQ_REL RELAXED UNROLL TILE
	        USE_DEVICE_PTR USE_DEVICE_ADDR TARGET TARGETSYNC TARGET_DATA_COMPOSITE DATA ENTER EXIT ANCESTOR DEVICE_NUM IS_DEVICE_PTR HAS_DEVICE_ADDR SIZES STRICT
        DEFAULTMAP BEHAVIOR_ALLOC BEHAVIOR_TO BEHAVIOR_FROM BEHAVIOR_TOFROM BEHAVIOR_FIRSTPRIVATE BEHAVIOR_NONE BEHAVIOR_DEFAULT BEHAVIOR_PRESENT CATEGORY_SCALAR CATEGORY_AGGREGATE CATEGORY_POINTER CATEGORY_ALLOCATABLE CATEGORY_ALL UPDATE TO FROM TO_MAPPER TO_ITERATOR FROM_MAPPER FROM_ITERATOR USES_ALLOCATORS TRAITS
 LINK DEVICE_TYPE TARGET_DEVICE MAP MAP_MODIFIER_ALWAYS MAP_MODIFIER_CLOSE MAP_MODIFIER_PRESENT MAP_MODIFIER_SELF MAP_MODIFIER_MAPPER MAP_MODIFIER_ITERATOR MAP_REF_MODIFIER_REF_PTEE MAP_REF_MODIFIER_REF_PTR MAP_REF_MODIFIER_REF_PTR_PTEE MAP_TYPE_TO MAP_TYPE_FROM MAP_TYPE_TOFROM MAP_TYPE_STORAGE MAP_TYPE_ALLOC MAP_TYPE_RELEASE MAP_TYPE_DELETE MAP_TYPE_PRESENT MAP_TYPE_SELF EXT_ BARRIER TASKWAIT FLUSH RELEASE ACQUIRE ATOMIC READ WRITE CAPTURE HINT CRITICAL SOURCE SINK DESTROY THREADS
        CONCURRENT REPRODUCIBLE UNCONSTRAINED
        LESSOREQUAL MOREOREQUAL NOTEQUAL
        ERROR_DIR NOTHING MASKED SCOPE INTEROP ASSUME ASSUMES BEGIN_DIR
        ALLOCATORS TASKGRAPH TASK_ITERATION DISPATCH GROUPPRIVATE WORKDISTRIBUTE FUSE INTERCHANGE REVERSE SPLIT STRIPE INDUCTION
        FILTER COMPARE FAIL WEAK AT SEVERITY MESSAGE COMPILATION EXECUTION FATAL WARNING
        DOACROSS ABSENT PRESENT CONTAINS HOLDS OTHERWISE
	        GRAPH_ID GRAPH_RESET TRANSPARENT REPLAYABLE THREADSET INDIRECT LOCAL INIT PREFER_TYPE INIT_COMPLETE SAFESYNC DEVICE_SAFESYNC MEMSCOPE
	        LOOPRANGE PERMUTATION COUNTS INDUCTOR COLLECTOR COMBINER NEED_DEVICE_PTR ADJUST_ARGS APPEND_ARGS APPLY STEP
	        NO_OPENMP NO_OPENMP_CONSTRUCTS NO_OPENMP_ROUTINES NO_PARALLELISM NOCONTEXT NOVARIANTS USE ALL CGROUP
%token <itype> ICONSTANT
%token <stype> EXPRESSION ID_EXPRESSION EXPR_STRING VAR_STRING TASK_REDUCTION ALLOCATOR_IDENTIFIER
/* associativity and precedence */
%left '<' '>' '='
%left '+' '-'
%left '*' '/' '%'
%nonassoc LOWER_THAN_LOOP
%nonassoc LOOP
%nonassoc LOWER_THAN_PAREN
%nonassoc ')'
%nonassoc LOWER_THAN_COLON
%nonassoc ':'

%type <stype> expression
%type <itype> init_depinfo_kind
%type <itype> directive_name

/* start point for the parsing */
%start openmp_directive

%%

/* lang-dependent expression is only used in clause, at this point, the current_clause object should already be created. */
expression : EXPR_STRING {
             current_clause->addLangExpr($1, current_expr_separator);
             $$ = $1;
           }
variable :   EXPR_STRING {
                if (current_clause != nullptr) {
                  switch (current_clause->getKind()) {
                  case OMPC_reduction: {
                    auto *red_clause =
                        static_cast<OpenMPReductionClause *>(current_clause);
                    red_clause->addOperand($1, current_expr_separator);
                    break;
                  }
                  case OMPC_in_reduction: {
                    auto *in_red_clause =
                        static_cast<OpenMPInReductionClause *>(current_clause);
                    in_red_clause->addOperand($1, current_expr_separator);
                    break;
                  }
                  case OMPC_task_reduction: {
                    auto *task_red_clause =
                        static_cast<OpenMPTaskReductionClause *>(current_clause);
                    task_red_clause->addOperand($1, current_expr_separator);
                    break;
                  }
                  case OMPC_inclusive:
                  case OMPC_exclusive: {
                    auto *scan_clause =
                        static_cast<OpenMPScanClause *>(current_clause);
                    scan_clause->addOperand($1, current_expr_separator);
                    break;
                  }
                  case OMPC_order: {
                    auto *ord_clause =
                        static_cast<OpenMPOrderClause *>(current_clause);
                    ord_clause->addOperand($1, current_expr_separator);
                    break;
                  }
                  default:
                    current_clause->addLangExpr($1, current_expr_separator);
                  }
                }
                current_expr_separator = OMPC_CLAUSE_SEP_space;
             } /* we use expression for variable so far */
           | UPDATE { current_clause->addLangExpr("update"); }
           | DEVICE { current_clause->addLangExpr("device"); }
           | HOST { current_clause->addLangExpr("host"); }
           | NOHOST { current_clause->addLangExpr("nohost"); }
           | TARGET { current_clause->addLangExpr("target"); }
           | DISPATCH { current_clause->addLangExpr("dispatch"); }
           | USER { current_clause->addLangExpr("user"); }
           | CONSTRUCT { current_clause->addLangExpr("construct"); }
           | IMPLEMENTATION { current_clause->addLangExpr("implementation"); }
           | DEVICE_TYPE { current_clause->addLangExpr("device_type"); }
           | KIND { current_clause->addLangExpr("kind"); }
           | ARCH { current_clause->addLangExpr("arch"); }
           | ISA { current_clause->addLangExpr("isa"); }
           | CONDITION { current_clause->addLangExpr("condition"); }
           | DYNAMIC_ALLOCATORS { current_clause->addLangExpr("dynamic_allocators"); }
           | REVERSE_OFFLOAD { current_clause->addLangExpr("reverse_offload"); }
           | SELF_MAPS { current_clause->addLangExpr("self_maps"); }
           | ATOMIC_DEFAULT_MEM_ORDER { current_clause->addLangExpr("atomic_default_mem_order"); }
           | ALLOCATOR { current_clause->addLangExpr("allocator"); }
           | USES_ALLOCATORS { current_clause->addLangExpr("uses_allocators"); }
           | DATA { current_clause->addLangExpr("data"); }
           | ANY { current_clause->addLangExpr("any"); }
           | MAP { current_clause->addLangExpr("map"); }
           | TO { current_clause->addLangExpr("to"); }
           | FROM { current_clause->addLangExpr("from"); }
           | LINK { current_clause->addLangExpr("link"); }
           | ENTER { current_clause->addLangExpr("enter"); }
           | PRIVATE { current_clause->addLangExpr("private"); }
           | SHARED { current_clause->addLangExpr("shared"); }
           | REDUCTION { current_clause->addLangExpr("reduction"); }
           | ALL { current_clause->addLangExpr("all"); }
           | CGROUP { current_clause->addLangExpr("cgroup"); }

/* For absent/contains clauses that take directive names as arguments */
directive_name : PARALLEL { $$ = OMPD_parallel; }
               | FOR { $$ = OMPD_for; }
               | DO { $$ = OMPD_do; }
               | SIMD { $$ = OMPD_simd; }
               | TARGET { $$ = OMPD_target; }
               | TEAMS { $$ = OMPD_teams; }
               | DISTRIBUTE { $$ = OMPD_distribute; }
               | TASK { $$ = OMPD_task; }
               | TASKLOOP { $$ = OMPD_taskloop; }
               | SECTIONS { $$ = OMPD_sections; }
               | SECTION { $$ = OMPD_section; }
               | SINGLE { $$ = OMPD_single; }
               | MASTER { $$ = OMPD_master; }
               | MASKED { $$ = OMPD_masked; }
               | CRITICAL { $$ = OMPD_critical; }
               | BARRIER { $$ = OMPD_barrier; }
               | TASKWAIT { $$ = OMPD_taskwait; }
               | TASKGROUP { $$ = OMPD_taskgroup; }
               | ATOMIC { $$ = OMPD_atomic; }
               | FLUSH { $$ = OMPD_flush; }
               | ORDERED { $$ = OMPD_ordered; }
               | SCAN { $$ = OMPD_scan; }
               | SCOPE { $$ = OMPD_scope; }
               | LOOP { $$ = OMPD_loop; }
               | WORKSHARE { $$ = OMPD_workshare; }
               | CANCEL { $$ = OMPD_cancel; }
               | METADIRECTIVE { $$ = OMPD_metadirective; }
               ;

directive_name_list : directive_name {
                        if (current_clause) {
                             if (current_clause->getKind() == OMPC_absent) {
                                  ((OpenMPAbsentClause*)current_clause)->addDirective((OpenMPDirectiveKind)$1);
                             } else if (current_clause->getKind() == OMPC_contains) {
                                  ((OpenMPContainsClause*)current_clause)->addDirective((OpenMPDirectiveKind)$1);
                             }
                        }
                    }
                    | directive_name_list ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } directive_name {
                        if (current_clause) {
                             if (current_clause->getKind() == OMPC_absent) {
                                  ((OpenMPAbsentClause*)current_clause)->addDirective((OpenMPDirectiveKind)$4);
                             } else if (current_clause->getKind() == OMPC_contains) {
                                  ((OpenMPContainsClause*)current_clause)->addDirective((OpenMPDirectiveKind)$4);
                             }
                        }
                    }
                    ;

/*expr_list : expression
        | expr_list ',' expression
        ;
*/
var_list : variable
        | var_list ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } variable
        ;

openmp_directive : parallel_directive
                 | metadirective_directive
                 | declare_variant_directive
                 | begin_declare_variant_directive
                 | end_declare_variant_directive
                 | for_directive
                 | do_directive
                 | simd_directive
                 | teams_directive
                 | for_simd_directive
                 | do_simd_directive
                 | parallel_for_simd_directive
                 | parallel_do_simd_directive
                 | declare_simd_directive
                 | declare_simd_fortran_directive
                 | distribute_directive
                 | distribute_simd_directive
	                 | distribute_parallel_for_directive
	                 | distribute_parallel_do_directive
	                 | distribute_parallel_loop_directive
	                 | distribute_parallel_for_simd_directive
	                 | distribute_parallel_do_simd_directive
	                 | distribute_parallel_loop_simd_directive
                 | parallel_for_directive
	                 | parallel_do_directive
	                 | parallel_loop_directive
	                 | parallel_loop_simd_directive
	                 | parallel_sections_directive
                 | parallel_single_directive
                 | parallel_workshare_directive
                 | parallel_master_directive
                 | master_taskloop_directive
                 | master_taskloop_simd_directive
                 | parallel_master_taskloop_directive
                 | parallel_master_taskloop_simd_directive
                 | loop_directive
                 | scan_directive
                 | sections_directive
                 | section_directive
                 | single_directive
                 | workshare_directive
                 | cancel_directive
//                 | cancel_fortran_directive
                 | cancellation_point_directive
//                 | cancellation_point_fortran_directive
                 | allocate_directive
                 | task_directive
                 | taskloop_directive
                 | taskloop_simd_directive
                 | taskyield_directive
                 | requires_directive
                 | target_data_directive
                 | target_data_composite_directive
	                 | target_enter_data_directive
	                 | target_exit_data_directive
	                 | target_directive
	                 | target_loop_directive
	                 | target_loop_simd_directive
	                 | target_update_directive
                 | declare_target_directive
                 | end_declare_target_directive
                 | master_directive
                 | threadprivate_directive
                 | declare_reduction_directive
                 | declare_mapper_directive
                 | end_directive
                 | barrier_directive
                 | taskwait_directive
                 | taskgroup_directive
                 | flush_directive
                 | atomic_directive
                 | critical_directive
                 | depobj_directive
                 | ordered_directive
                 | teams_distribute_directive
                 | teams_distribute_simd_directive
	                 | teams_distribute_parallel_for_directive
	                 | teams_distribute_parallel_for_simd_directive
	                 | teams_distribute_parallel_loop_directive
	                 | teams_distribute_parallel_loop_simd_directive
	                 | teams_loop_directive
	                 | teams_loop_simd_directive
                 | target_parallel_directive
                 | target_parallel_for_directive
	                 | target_parallel_for_simd_directive
	                 | target_parallel_loop_directive
	                 | target_parallel_loop_simd_directive
	                 | target_simd_directive
                 | target_teams_directive
                 | target_teams_distribute_directive
	                 | target_teams_distribute_simd_directive
	                 | target_teams_loop_directive
	                 | target_teams_loop_simd_directive
	                 | target_teams_distribute_parallel_for_directive
	                 | target_teams_distribute_parallel_for_simd_directive
	                 | target_teams_distribute_parallel_loop_directive
	                 | target_teams_distribute_parallel_loop_simd_directive
                 | teams_distribute_parallel_do_directive
                 | teams_distribute_parallel_do_simd_directive
                 | target_parallel_do_directive
                 | target_parallel_do_simd_directive
                 | target_teams_distribute_parallel_do_directive
                 | target_teams_distribute_parallel_do_simd_directive
                 | unroll_directive
                 | tile_directive
                 | error_directive
                 | nothing_directive
                 | masked_directive
                 | masked_taskloop_directive
                 | masked_taskloop_simd_directive
                 | parallel_masked_directive
                 | parallel_masked_taskloop_directive
                 | parallel_masked_taskloop_simd_directive
                 | scope_directive
                 | interop_directive
                 | assume_directive
                 | end_assume_directive
                 | assumes_directive
                 | begin_assumes_directive
                 | end_assumes_directive
                 | begin_metadirective_directive
                 | begin_declare_target_directive
                 | allocators_directive
                 | taskgraph_directive
                 | task_iteration_directive
                 | dispatch_directive
                 | groupprivate_directive
                 | workdistribute_directive
                 | fuse_directive
                 | interchange_directive
                 | reverse_directive
                 | split_directive
                 | stripe_directive
                 | declare_induction_directive
                 ;

variant_directive : parallel_do_directive
                  | parallel_do_simd_directive
	                  | parallel_for_directive
	                  | parallel_for_simd_directive
	                  | parallel_loop_directive
	                  | parallel_loop_simd_directive
	                  | parallel_directive
                  | teams_distribute_directive
                  | teams_distribute_simd_directive
                  | teams_distribute_parallel_do_directive
                  | teams_distribute_parallel_do_simd_directive
	                  | teams_distribute_parallel_for_directive
	                  | teams_distribute_parallel_for_simd_directive
	                  | teams_distribute_parallel_loop_directive
	                  | teams_distribute_parallel_loop_simd_directive
	                  | teams_loop_directive
	                  | teams_loop_simd_directive
	                  | teams_directive
                  | metadirective_directive
                  | declare_variant_directive
                  | for_directive
                  | do_directive
                  | for_simd_directive
                  | do_simd_directive
                  | simd_directive
                  | declare_simd_directive
                  | distribute_directive
                  | distribute_simd_directive
	                  | distribute_parallel_for_directive
	                  | distribute_parallel_for_simd_directive
	                  | distribute_parallel_do_directive
	                  | distribute_parallel_do_simd_directive
	                  | distribute_parallel_loop_directive
	                  | distribute_parallel_loop_simd_directive
                  | loop_directive
                  | scan_directive
                  | sections_directive
                  | section_directive
                  | error_directive
                  | single_directive
                  | task_directive
                  | taskwait_directive
                  | cancel_directive
                  | cancellation_point_directive
                  | allocate_directive
                  | master_directive
                  | masked_directive
                  | nothing_directive
                  | target_parallel_do_directive
                  | target_parallel_do_simd_directive
                  | target_parallel_for_directive
	                  | target_parallel_for_simd_directive
	                  | target_parallel_loop_directive
	                  | target_parallel_loop_simd_directive
	                  | target_parallel_directive
                  | target_simd_directive
                  | target_teams_distribute_directive
                  | target_teams_distribute_simd_directive
                  | target_teams_distribute_parallel_do_directive
                  | target_teams_distribute_parallel_do_simd_directive
	                  | target_teams_distribute_parallel_for_directive
	                  | target_teams_distribute_parallel_for_simd_directive
	                  | target_teams_distribute_parallel_loop_directive
	                  | target_teams_distribute_parallel_loop_simd_directive
	                  | target_teams_loop_directive
	                  | target_teams_loop_simd_directive
	                  | target_teams_directive
	                  | target_loop_directive
	                  | target_loop_simd_directive
	                  | target_directive
	                  ;

fortran_paired_directive : parallel_directive
                         | parallel_sections_directive
                         | parallel_single_directive
                         | do_paired_directive
                         | metadirective_directive
                         | begin_metadirective_directive
                         | master_directive
                         | masked_directive
                         | masked_taskloop_directive
                         | masked_taskloop_simd_directive
                         | parallel_masked_directive
                         | parallel_masked_taskloop_directive
                         | parallel_masked_taskloop_simd_directive
                         | teams_directive
                         | section_directive
                         | sections_paired_directive
                         | simd_directive
                         | do_simd_paired_directive
                         | distribute_directive
                         | distribute_simd_directive
                         | distribute_parallel_do_directive
                         | distribute_parallel_do_simd_directive
                         | parallel_do_directive
                         | parallel_loop_directive
                         | parallel_workshare_directive
                         | parallel_do_simd_directive
                         | parallel_master_directive
                         | master_taskloop_directive
                         | master_taskloop_simd_directive
                         | parallel_master_taskloop_directive
                         | parallel_master_taskloop_simd_directive
                         | loop_directive
                         | single_paired_directive
                         | workshare_paired_directive
                         | task_directive
                         | taskloop_directive
                         | taskloop_simd_directive
                         | target_directive
                         | target_data_directive
                         | target_data_composite_directive
                         | critical_directive
                         | taskgroup_directive
                         | atomic_directive
                         | scope_directive
                         | ordered_directive
                         | teams_distribute_directive
                         | teams_distribute_simd_directive
                         | teams_distribute_parallel_do_directive
                         | teams_distribute_parallel_do_simd_directive
                         | teams_loop_directive
                         | target_parallel_directive
                         | target_parallel_do_directive
                         | target_parallel_do_simd_directive
                         | target_parallel_loop_directive
                         | target_simd_directive
                         | target_teams_directive
                         | target_teams_distribute_directive
                         | target_teams_distribute_simd_directive
                         | target_teams_loop_directive
                         | target_teams_distribute_parallel_do_directive
                         | target_teams_distribute_parallel_do_simd_directive
                         | unroll_directive
                         | tile_directive
                         ;

end_directive : END { current_directive = new OpenMPEndDirective();
                current_parent_directive = current_directive;
                current_parent_clause = current_clause;
                ((OpenMPEndDirective*)current_directive)
                    ->setUseCompactEndDo(openmp_consume_compact_enddo());
              } end_clause_seq {
                ((OpenMPEndDirective*)current_parent_directive)->setPairedDirective(current_directive);
                current_directive = current_parent_directive;
                current_clause = current_parent_clause;
                current_parent_directive = nullptr;
                current_parent_clause = nullptr;
              }
              ;

end_clause_seq : fortran_paired_directive
               ;

metadirective_directive : METADIRECTIVE { current_directive = new OpenMPDirective(OMPD_metadirective); }
                          metadirective_clause_optseq
                        ;

metadirective_clause_optseq : /* empty */
                            | metadirective_clause_seq
                            ;

metadirective_clause_seq : metadirective_clause
                         | metadirective_clause_seq metadirective_clause
                         | metadirective_clause_seq ',' { clause_separator_comma = true; } metadirective_clause
                         ;

metadirective_clause : when_clause
                     | default_variant_clause
                     | otherwise_clause
                     ;

when_clause : WHEN { current_clause = current_directive->addOpenMPClause(OMPC_when); }
                '(' context_selector_specification ':' {
                current_parent_directive = current_directive;
                current_parent_clause = current_clause;
                } when_variant_directive {
                current_directive = current_parent_directive;
                current_clause = current_parent_clause;
                current_parent_directive = nullptr;
                current_parent_clause = nullptr;
                } ')'
            ;

when_variant_directive : variant_directive {((OpenMPWhenClause*)current_parent_clause)->setVariantDirective(current_directive); }
                | { ; }
                ;

context_selector_specification : trait_set_selector
                | context_selector_specification trait_set_selector
                | context_selector_specification ',' trait_set_selector
                ;

trait_set_selector : trait_set_selector_name { } '=' '{' trait_selector_list {
                        if (current_parent_clause) {
                            current_directive = current_parent_directive;
                            current_clause = current_parent_clause;
                            current_parent_directive = nullptr;
                            current_parent_clause = nullptr;
                        };
                     } '}'
                   ;

trait_set_selector_name : USER { ((OpenMPVariantClause*)current_clause)->addSelectorKind(OMPC_SELECTOR_user); }
                | CONSTRUCT { ((OpenMPVariantClause*)current_clause)->addSelectorKind(OMPC_SELECTOR_construct); current_parent_directive = current_directive;
                    current_parent_clause = current_clause; }
                | DEVICE { ((OpenMPVariantClause*)current_clause)->setIsTargetDeviceSelector(false); ((OpenMPVariantClause*)current_clause)->addSelectorKind(OMPC_SELECTOR_device); }
                | TARGET_DEVICE { ((OpenMPVariantClause*)current_clause)->setIsTargetDeviceSelector(true); ((OpenMPVariantClause*)current_clause)->addSelectorKind(OMPC_SELECTOR_target_device); }
                | IMPLEMENTATION { ((OpenMPVariantClause*)current_clause)->addSelectorKind(OMPC_SELECTOR_implementation); }
                ;

trait_selector_list : trait_selector { trait_score = ""; }
                | trait_selector_list trait_selector { trait_score = ""; }
                | trait_selector_list ',' trait_selector { trait_score = ""; }
                ;

trait_selector : condition_selector
                | construct_selector {
                    ((OpenMPVariantClause*)current_parent_clause)->addConstructDirective(trait_score, current_directive);
                }
                | device_selector
                | target_device_selector
                | implementation_selector
                ;

condition_selector : CONDITION '(' trait_score EXPR_STRING { ((OpenMPVariantClause*)current_clause)->setUserCondition(trait_score, $4); } ')'
                ;

device_selector : context_kind
                | context_isa
                | context_arch
                ;

target_device_selector : context_device_num
                       ;

context_kind : KIND '(' trait_score context_kind_name ')'
             ;

context_device_num : DEVICE_NUM '(' trait_score EXPR_STRING { ((OpenMPVariantClause*)current_clause)->setDeviceNumExpression(trait_score, $4); } ')'
                   ;

context_kind_name : HOST { ((OpenMPVariantClause*)current_clause)->setContextKind(trait_score, OMPC_CONTEXT_KIND_host); }
                  | NOHOST { ((OpenMPVariantClause*)current_clause)->setContextKind(trait_score, OMPC_CONTEXT_KIND_nohost); }
                  | ANY { ((OpenMPVariantClause*)current_clause)->setContextKind(trait_score, OMPC_CONTEXT_KIND_any); }
                  | CPU { ((OpenMPVariantClause*)current_clause)->setContextKind(trait_score, OMPC_CONTEXT_KIND_cpu); }
                  | GPU { ((OpenMPVariantClause*)current_clause)->setContextKind(trait_score, OMPC_CONTEXT_KIND_gpu); }
                  | FPGA { ((OpenMPVariantClause*)current_clause)->setContextKind(trait_score, OMPC_CONTEXT_KIND_fpga); }
                  ;

context_isa : ISA '(' trait_score EXPR_STRING { ((OpenMPVariantClause*)current_clause)->setIsaExpression(trait_score, $4); } ')'
            ;

context_arch : ARCH '(' trait_score EXPR_STRING { ((OpenMPVariantClause*)current_clause)->setArchExpression(trait_score, $4); } ')'
             ;

implementation_selector : VENDOR '(' trait_score context_vendor_name ')'
                        | EXTENSION '(' trait_score EXPR_STRING { ((OpenMPVariantClause*)current_clause)->setExtensionExpression(trait_score, $4); } ')'
                        | REQUIRES '(' trait_score EXPR_STRING {
                            ((OpenMPVariantClause*)current_clause)->setImplementationRequiresExpression(trait_score, $4);
                          } ')'
                        | EXPR_STRING { ((OpenMPVariantClause*)current_clause)->setImplementationUserExpression(trait_score, $1); }
                        | EXPR_STRING '(' trait_score ')' { ((OpenMPVariantClause*)current_clause)->setImplementationUserExpression(trait_score, $1); }
                        ;

context_vendor_name : AMD { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_amd); }
                    | ARM { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_arm); }
                    | BSC { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_bsc); }
                    | CRAY { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_cray); }
                    | FUJITSU { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_fujitsu); }
                    | GNU { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_gnu); }
                    | IBM { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_ibm); }
                    | INTEL { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_intel); }
                    | LLVM { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_llvm); }
                    | NVIDIA { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_nvidia); }
                    | PGI { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_pgi); }
                    | TI { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_ti); }
                    | UNKNOWN { ((OpenMPVariantClause*)current_clause)->setImplementationKind(trait_score, OMPC_CONTEXT_VENDOR_unknown); }
                    ;

construct_selector : parallel_selector
                   | dispatch_selector
                   | target_construct_selector
                   | teams_construct_selector
                   | for_construct_selector
                   | do_construct_selector
                   | simd_construct_selector
                   | loop_construct_selector
                   ;

parallel_selector : PARALLEL LOOP parallel_loop_selector_suffix
                | PARALLEL '(' { current_directive = new OpenMPDirective(OMPD_parallel); } parallel_selector_parameter ')'
                | PARALLEL { current_directive = new OpenMPDirective(OMPD_parallel); } %prec LOWER_THAN_LOOP
                ;

parallel_loop_selector_suffix : '(' { current_directive = new OpenMPDirective(OMPD_parallel_loop); } parallel_loop_construct_selector_parameter ')'
                              | { current_directive = new OpenMPDirective(OMPD_parallel_loop); }
                              ;

parallel_loop_construct_selector_parameter : trait_score parallel_loop_clause_optseq
                                           ;

parallel_selector_parameter : trait_score parallel_clause_optseq
                            ;

dispatch_selector : DISPATCH { current_directive = new OpenMPDirective(OMPD_dispatch); }
                  ;

target_construct_selector : TARGET { current_directive = new OpenMPDirective(OMPD_target); }
                          | TARGET '(' { current_directive = new OpenMPDirective(OMPD_target); } target_construct_selector_parameter ')'
                          ;

target_construct_selector_parameter : trait_score target_clause_optseq
                                    ;

teams_construct_selector : TEAMS LOOP teams_loop_selector_suffix
                         | TEAMS '(' { current_directive = new OpenMPDirective(OMPD_teams); } teams_construct_selector_parameter ')'
                         | TEAMS { current_directive = new OpenMPDirective(OMPD_teams); } %prec LOWER_THAN_LOOP
                         ;

teams_loop_selector_suffix : '(' { current_directive = new OpenMPDirective(OMPD_teams_loop); } teams_loop_construct_selector_parameter ')'
                           | { current_directive = new OpenMPDirective(OMPD_teams_loop); }
                           ;

teams_construct_selector_parameter : trait_score teams_clause_optseq
                                   ;

teams_loop_construct_selector_parameter : trait_score teams_loop_clause_optseq
                                        ;

for_construct_selector : FOR { current_directive = new OpenMPDirective(OMPD_for); }
                       | FOR '(' { current_directive = new OpenMPDirective(OMPD_for); } for_construct_selector_parameter ')'
                       ;

for_construct_selector_parameter : trait_score for_clause_optseq
                                 ;

do_construct_selector : DO { current_directive = new OpenMPDirective(OMPD_do); }
                      | DO '(' { current_directive = new OpenMPDirective(OMPD_do); } do_construct_selector_parameter ')'
                      ;

do_construct_selector_parameter : trait_score do_clause_optseq
                                ;

simd_construct_selector : SIMD { current_directive = new OpenMPDirective(OMPD_simd); }
                        | SIMD '(' { current_directive = new OpenMPDirective(OMPD_simd); } simd_construct_selector_parameter ')'
                        ;

simd_construct_selector_parameter : trait_score simd_clause_optseq
                                  ;

loop_construct_selector : LOOP { current_directive = new OpenMPDirective(OMPD_loop); }
                        | LOOP '(' { current_directive = new OpenMPDirective(OMPD_loop); } loop_construct_selector_parameter ')'
                        ;

loop_construct_selector_parameter : trait_score loop_clause_optseq
                                  ;

trait_score : /* empty */
            | SCORE '(' EXPR_STRING { trait_score = $3; } ')' ':'
            ;

declare_variant_directive : DECLARE VARIANT {
                        current_directive = new OpenMPDeclareVariantDirective();
                     } variant_func_id
                     declare_variant_clause_optseq
                   ;

begin_declare_variant_directive : BEGIN_DIR DECLARE VARIANT {
                        current_directive = new OpenMPDirective(OMPD_begin_declare_variant);
                     } declare_variant_clause_optseq
                   ;

end_declare_variant_directive : END DECLARE VARIANT {
                        current_directive = new OpenMPDirective(OMPD_end_declare_variant);
                     }
                   ;

variant_func_id : '(' EXPR_STRING { ((OpenMPDeclareVariantDirective*)current_directive)->setVariantFuncID($2); } ')'
                ;

declare_variant_clause_optseq : /* empty */
                       | declare_variant_clause_seq
                       ;

declare_variant_clause_seq : declare_variant_clause
                           | declare_variant_clause_seq declare_variant_clause
                           | declare_variant_clause_seq ',' { clause_separator_comma = true; } declare_variant_clause
                           ;

declare_variant_clause : match_clause
                       | adjust_args_clause
                       | append_args_clause
                       ;

match_clause : MATCH { current_clause = current_directive->addOpenMPClause(OMPC_match); }
                '(' context_selector_specification ')' { }
             ;

adjust_args_clause : ADJUST_ARGS {
                       current_clause = current_directive->addOpenMPClause(OMPC_adjust_args);
                     } '(' adjust_args_parameter ')' {
                   }
                   ;
adjust_args_parameter : NEED_DEVICE_PTR ':' adjust_args_var_list {
                          auto *adjust_clause =
                              static_cast<OpenMPAdjustArgsClause *>(current_clause);
                          if (adjust_clause != nullptr) {
                            adjust_clause->setModifier(OMPC_ADJUST_ARGS_need_device_ptr);
                          }
                        }
                      | EXPR_STRING ':' adjust_args_var_list {
                          auto *adjust_clause =
                              static_cast<OpenMPAdjustArgsClause *>(current_clause);
                          if (adjust_clause != nullptr) {
                            adjust_clause->setModifier(OMPC_ADJUST_ARGS_unknown,
                                                       std::string($1));
                          }
                        }
                      | adjust_args_var_list
                      ;
adjust_args_var_list : EXPR_STRING {
                        auto *adjust_clause = static_cast<OpenMPAdjustArgsClause *>(current_clause);
                        if (adjust_clause != nullptr) {
                          adjust_clause->addArgument(std::string($1));
                        }
                      }
                     | adjust_args_var_list ',' EXPR_STRING {
                        auto *adjust_clause = static_cast<OpenMPAdjustArgsClause *>(current_clause);
                        if (adjust_clause != nullptr) {
                          adjust_clause->addArgument(std::string($3));
                        }
                      }
                     ;
append_args_clause : APPEND_ARGS {
                       current_clause = current_directive->addOpenMPClause(OMPC_append_args);
                     } '(' append_args_parameter ')' {
                   }
                   ;
append_args_parameter : append_args_var_list
                      | EXPR_STRING ':' append_args_var_list {
                          auto *append_clause = static_cast<OpenMPAppendArgsClause *>(current_clause);
                          if (append_clause != nullptr) {
                            append_clause->setLabel(std::string($1));
                          }
                        }
                      ;
append_args_var_list : EXPR_STRING {
                        auto *append_clause = static_cast<OpenMPAppendArgsClause *>(current_clause);
                        if (append_clause != nullptr) {
                          append_clause->addArgument(std::string($1));
                        }
                      }
                     | append_args_var_list ',' EXPR_STRING {
                        auto *append_clause = static_cast<OpenMPAppendArgsClause *>(current_clause);
                        if (append_clause != nullptr) {
                          append_clause->addArgument(std::string($3));
                        }
                      }
                     ;

parallel_directive : PARALLEL {
                        current_directive = new OpenMPDirective(OMPD_parallel);
                     }
                     parallel_clause_optseq
                   ;
/*xinyao*/
task_directive : TASK {
                        current_directive = new OpenMPDirective(OMPD_task);
                      }
                      task_clause_optseq
               ;
taskloop_directive : TASKLOOP {
                        current_directive = new OpenMPDirective(OMPD_taskloop);
                              }
                     taskloop_clause_optseq
                   ;
taskloop_simd_directive : TASKLOOP SIMD {
                        current_directive = new OpenMPDirective(OMPD_taskloop_simd);
                                         }
                     taskloop_simd_clause_optseq 
                        ;
taskyield_directive : TASKYIELD {
                        current_directive = new OpenMPDirective(OMPD_taskyield);
                     }
                    ;
requires_directive : REQUIRES {
                        current_directive = new OpenMPRequiresDirective();
                     }
                     requires_clause_optseq
                   ;
target_data_directive :  TARGET DATA {
                        current_directive = new OpenMPDirective(OMPD_target_data);
                     }
                     target_data_clause_optseq
                      ;
target_data_composite_directive :  TARGET_DATA_COMPOSITE {
                        current_directive = new OpenMPDirective(OMPD_target_data_composite);
                     }
                     target_data_clause_optseq
                      ;
target_enter_data_directive :  TARGET ENTER DATA {
                        current_directive = new OpenMPDirective(OMPD_target_enter_data);
                     }
                     target_enter_data_clause_optseq 
                            ;
target_exit_data_directive :  TARGET EXIT DATA {
                        current_directive = new OpenMPDirective(OMPD_target_exit_data);
                     }
                     target_exit_data_clause_optseq 
                   ;
target_directive :  TARGET {
                        current_directive = new OpenMPDirective(OMPD_target);
                     }
                     target_clause_optseq 
                   ;
target_update_directive :  TARGET UPDATE{
                        current_directive = new OpenMPDirective(OMPD_target_update);
                     }
                     target_update_clause_optseq 
                   ;
declare_target_directive : DECLARE TARGET {
                        current_directive = new OpenMPDeclareTargetDirective ();
                        current_directive->setDeclareTargetUnderscore(
                            openmp_consume_declare_target_underscore());
                     }
                     declare_target_clause_optseq 
                   ;
flush_directive : FLUSH {
                        current_directive = new OpenMPFlushDirective ();
                        }
                     flush_clause_optseq 
                ;

end_declare_target_directive : END DECLARE TARGET {
                        current_directive = new OpenMPDirective(OMPD_end_declare_target);
                        current_directive->setDeclareTargetUnderscore(
                            openmp_consume_declare_target_underscore());
                                  }
                             ;
master_directive : MASTER {
                        current_directive = new OpenMPDirective(OMPD_master);
                     }
                   ;

barrier_directive : BARRIER {
                        current_directive = new OpenMPDirective(OMPD_barrier);
                             }
                  ;
taskwait_directive : TASKWAIT {
                        current_directive = new OpenMPDirective(OMPD_taskwait);
                     }
                      taskwait_clause_optseq
                   ;
unroll_directive : UNROLL {
                        current_directive = new OpenMPDirective(OMPD_unroll);
                     }
                      unroll_clause_optseq
                   ;
tile_directive : TILE {
                        current_directive = new OpenMPDirective(OMPD_tile);
                     }
                      tile_clause_optseq
                   ;
// OpenMP 5.1 directives
error_directive : ERROR_DIR {
                        current_directive = new OpenMPDirective(OMPD_error);
                     }
                      error_clause_optseq
                   ;
nothing_directive : NOTHING {
                        current_directive = new OpenMPDirective(OMPD_nothing);
                     }
                   ;
masked_directive : MASKED {
                        current_directive = new OpenMPDirective(OMPD_masked);
                     }
                      masked_clause_optseq
                   ;
masked_taskloop_directive : MASKED TASKLOOP {
                        current_directive = new OpenMPDirective(OMPD_masked_taskloop);
                     }
                      masked_taskloop_clause_optseq
                   ;
masked_taskloop_simd_directive : MASKED TASKLOOP SIMD {
                        current_directive = new OpenMPDirective(OMPD_masked_taskloop_simd);
                     }
                      masked_taskloop_simd_clause_optseq
                   ;
parallel_masked_directive : PARALLEL MASKED {
                        current_directive = new OpenMPDirective(OMPD_parallel_masked);
                     }
                      parallel_masked_clause_optseq
                   ;
parallel_masked_taskloop_directive : PARALLEL MASKED TASKLOOP {
                        current_directive = new OpenMPDirective(OMPD_parallel_masked_taskloop);
                     }
                      parallel_masked_taskloop_clause_optseq
                   ;
parallel_masked_taskloop_simd_directive : PARALLEL MASKED TASKLOOP SIMD {
                        current_directive = new OpenMPDirective(OMPD_parallel_masked_taskloop_simd);
                     }
                      parallel_masked_taskloop_simd_clause_optseq
                   ;
scope_directive : SCOPE {
                        current_directive = new OpenMPDirective(OMPD_scope);
                     }
                      scope_clause_optseq
                   ;
interop_directive : INTEROP {
                        current_directive = new OpenMPDirective(OMPD_interop);
                     }
                      interop_clause_optseq
                   ;
// OpenMP 5.2 directives
assume_directive : ASSUME {
                        current_directive = new OpenMPDirective(OMPD_assume);
                     }
                      assume_clause_optseq
                   ;
assumes_directive : ASSUMES {
                        current_directive = new OpenMPDirective(OMPD_assumes);
                     }
                      assumes_clause_optseq
                   ;
begin_assumes_directive : BEGIN_DIR ASSUMES {
                        current_directive = new OpenMPDirective(OMPD_begin_assumes);
                     }
                      begin_assumes_clause_optseq
                   ;
end_assumes_directive : END ASSUMES {
                        current_directive = new OpenMPDirective(OMPD_end_assumes);
                     }
                   ;
end_assume_directive : END ASSUME {
                        current_directive = new OpenMPDirective(OMPD_end_assume);
                     }
                   ;
begin_metadirective_directive : BEGIN_DIR METADIRECTIVE {
                        current_directive = new OpenMPDirective(OMPD_begin_metadirective);
                     }
                      begin_metadirective_clause_optseq
                   ;
begin_declare_target_directive : BEGIN_DIR DECLARE TARGET {
                        current_directive = new OpenMPDirective(OMPD_begin_declare_target);
                        current_directive->setDeclareTargetUnderscore(
                            openmp_consume_declare_target_underscore());
                     }
                   ;
// OpenMP 6.0 directives
allocators_directive : ALLOCATORS {
                        current_directive = new OpenMPDirective(OMPD_allocators);
                     }
                      allocators_clause_optseq
                   ;
taskgraph_directive : TASKGRAPH {
                        current_directive = new OpenMPDirective(OMPD_taskgraph);
                     }
                      taskgraph_clause_optseq
                   ;
task_iteration_directive : TASK_ITERATION {
                        current_directive = new OpenMPDirective(OMPD_task_iteration);
                     }
                      task_iteration_clause_optseq
                   ;
dispatch_directive : DISPATCH {
                        current_directive = new OpenMPDirective(OMPD_dispatch);
                     }
                      dispatch_clause_optseq
                   ;
groupprivate_directive : GROUPPRIVATE {
                        current_directive = new OpenMPGroupprivateDirective();
                     }
                      groupprivate_clause_optseq
                   ;
workdistribute_directive : WORKDISTRIBUTE {
                        current_directive = new OpenMPDirective(OMPD_workdistribute);
                     }
                      workdistribute_clause_optseq
                   ;
fuse_directive : FUSE {
                        current_directive = new OpenMPDirective(OMPD_fuse);
                     }
                      fuse_clause_optseq
                   ;
interchange_directive : INTERCHANGE {
                        current_directive = new OpenMPDirective(OMPD_interchange);
                     }
                      interchange_clause_optseq
                   ;
reverse_directive : REVERSE {
                        current_directive = new OpenMPDirective(OMPD_reverse);
                     }
                      reverse_clause_optseq
                   ;
split_directive : SPLIT {
                        current_directive = new OpenMPDirective(OMPD_split);
                     }
                      split_clause_optseq
                   ;
stripe_directive : STRIPE {
                        current_directive = new OpenMPDirective(OMPD_stripe);
                     }
                      stripe_clause_optseq
                   ;
declare_induction_directive : DECLARE INDUCTION {
                        current_directive = new OpenMPDirective(OMPD_declare_induction);
                        current_clause = current_directive->addOpenMPClause(OMPC_induction);
                     } '(' induction_specification_opt ')' {
                        current_clause = nullptr;
                     }
                     declare_induction_clause_optseq
                   ;
taskgroup_directive : TASKGROUP {
                        current_directive = new OpenMPDirective(OMPD_taskgroup);
                     }
                      taskgroup_clause_optseq
                    ;
critical_directive : CRITICAL {
                        current_directive = new OpenMPCriticalDirective();
                     }
                      critical_clause_optseq
                   ;
depobj_directive : DEPOBJ {
                        current_directive = new OpenMPDepobjDirective ();
                     }
                     depobj_clause_optseq 
                 ;
ordered_directive : ORDERED {
                        current_directive = new OpenMPOrderedDirective ();
                     }
                     ordered_clause_optseq 
                  ;
critical_clause_optseq : /*empty*/
                       | '(' critical_name')'
                       | '(' critical_name')' hint_clause
                       | '(' critical_name')' ',' hint_clause
                       ;
depobj_clause_optseq : '(' depobj ')' depobj_clause
                     ;
depobj : EXPR_STRING { ((OpenMPDepobjDirective*)current_directive)->addDepobj($1); } 
       ;

depobj_clause : depend_depobj_clause
              | init_clause
              | destroy_clause
              | depobj_update_clause
              ;
destroy_clause : DESTROY{current_clause = current_directive->addOpenMPClause(OMPC_destroy); }
               | DESTROY{current_clause = current_directive->addOpenMPClause(OMPC_destroy); } '(' expression ')'
               ;

depobj_update_clause : UPDATE '(' update_dependence_type ')'
                     ;
update_dependence_type : SOURCE { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update, OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_source); }
                       | IN { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update, OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_in); }
                       | OUT { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update, OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_out); }
                       | INOUT { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update, OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_inout); }
                       | INOUTSET { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update, OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_inoutset); }
                       | MUTEXINOUTSET { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update,OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_mutexinoutset); }
                       | DEPOBJ { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update, OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_depobj); }
                       | SINK { current_clause = current_directive->addOpenMPClause(OMPC_depobj_update, OMPC_DEPOBJ_UPDATE_DEPENDENCE_TYPE_sink); }
                       ;

critical_name : EXPR_STRING { ((OpenMPCriticalDirective*)current_directive)->setCriticalName($1); }
              ;
task_clause_optseq : /* empty */
                   | task_clause_seq
                   ;
taskloop_clause_optseq : /* empty */
                       | taskloop_clause_seq
                       ;
taskloop_simd_clause_optseq : /* empty */
                            | taskloop_simd_clause_seq
                            ;
requires_clause_optseq : requires_clause_seq
                       ;
target_data_clause_optseq : /* empty */
                          | target_data_clause_seq
                          ;
target_enter_data_clause_optseq :/* empty */
                                |target_enter_data_clause_seq
                                ;
target_exit_data_clause_optseq :/* empty */
                               |target_exit_data_clause_seq
                               ;
target_clause_optseq :/* empty */
                     |target_clause_seq
                     ;
target_update_clause_optseq :target_update_clause_seq
                            ;
declare_target_clause_optseq : /* empty */
                             | '(' declare_target_extended_list ')'
                             | '(' declare_target_extended_list ')' declare_target_seq
                             | declare_target_seq
                             ;

extended_variable : EXPR_STRING { ((OpenMPDeclareTargetDirective*)current_directive)->addExtendedList($1); }
                  ;
declare_target_extended_list : extended_variable
                             | declare_target_extended_list ',' extended_variable
                             ;
flush_clause_optseq : /* empty */
                    | '(' flush_list ')'
                    | flush_clause_seq
                    ;
flush_list : flush_variable
           | flush_list ',' flush_variable
           ;
flush_variable : EXPR_STRING { ((OpenMPFlushDirective*)current_directive)->addFlushList($1); }
               ;
flush_clause_seq : flush_memory_order_clause
                 | flush_memory_order_clause '(' flush_list ')'
                 ;
flush_memory_order_clause : seq_cst_clause
                          | acq_rel_clause
                          | release_clause
                          | acquire_clause
                          ;

atomic_directive : ATOMIC {
                        current_directive = new OpenMPAtomicDirective ();
                    }
                    atomic_clause_optseq 
                 ;
atomic_clause_optseq : memory_order_clause_seq
                     | memory_order_clause_seq atomic_clause_seq
                     | hint_clause ',' memory_order_clause ',' atomic_clause_seq
                     | memory_order_clause ',' hint_clause ',' atomic_clause_seq
                     | memory_order_clause ','atomic_clause_seq
                     | hint_clause ',' memory_order_clause atomic_clause_seq
                     | memory_order_clause ',' hint_clause atomic_clause_seq
                     | hint_clause ','atomic_clause_seq
                     ;

atomic_clause_seq : compare_clause capture_clause memory_order_clause_seq_after
                  | compare_clause capture_clause ',' memory_order_clause_seq_after
                  | atomic_clause memory_order_clause_seq_after
                  | atomic_clause ',' memory_order_clause_seq_after
                  ;

memory_order_clause_seq : 
                        | memory_order_clause hint_clause
                        | hint_clause memory_order_clause
                        | memory_order_clause
                        | hint_clause
                        ;
memory_order_clause_seq_after :
                              | memory_order_clause hint_clause
                              | hint_clause memory_order_clause
                              | memory_order_clause ',' hint_clause
                              | hint_clause ',' memory_order_clause
                              | memory_order_clause fail_clause
                              | memory_order_clause
                              | hint_clause
                              | fail_clause
                              | memscope_clause
                              | memory_order_clause memscope_clause
                              | memscope_clause memory_order_clause
                              ;
atomic_clause : read_clause
              | write_clause
              | update_clause
              | capture_clause
              | compare_clause
              | weak_clause
              | memscope_clause
              ;

memory_order_clause : seq_cst_clause
                    | acq_rel_clause
                    | release_clause
                    | acquire_clause
                    | relaxed_clause
                    ; 

hint_clause : HINT{ current_clause = current_directive->addOpenMPClause(OMPC_hint);
                     } '(' expression ')' 
            ;
read_clause : READ { current_clause = current_directive->addOpenMPClause(OMPC_read);
                   } 
            ;
write_clause : WRITE { current_clause = current_directive->addOpenMPClause(OMPC_write);
                     } 
             ;
update_clause : UPDATE { current_clause = current_directive->addOpenMPClause(OMPC_update);
                       } 
              ;
capture_clause : CAPTURE { current_clause = current_directive->addOpenMPClause(OMPC_capture);
                         } 
               ;

seq_cst_clause : SEQ_CST { current_clause = current_directive->addOpenMPClause(OMPC_seq_cst); }
               ;
acq_rel_clause : ACQ_REL { current_clause = current_directive->addOpenMPClause(OMPC_acq_rel); }
               ;
release_clause : RELEASE { current_clause = current_directive->addOpenMPClause(OMPC_release); }
               ;
acquire_clause : ACQUIRE { current_clause = current_directive->addOpenMPClause(OMPC_acquire); }
               ;
relaxed_clause : RELAXED { current_clause = current_directive->addOpenMPClause(OMPC_relaxed); }
               ;

// OpenMP 5.1 clause implementations
filter_clause : FILTER { current_clause = current_directive->addOpenMPClause(OMPC_filter); } '(' expression ')'
              ;
at_clause : AT '(' at_kind ')'
          ;
at_kind : COMPILATION { current_clause = current_directive->addOpenMPClause(OMPC_at, OMPC_AT_compilation); }
        | EXECUTION { current_clause = current_directive->addOpenMPClause(OMPC_at, OMPC_AT_execution); }
        ;
severity_clause : SEVERITY '(' severity_kind ')'
                ;
severity_kind : FATAL { current_clause = current_directive->addOpenMPClause(OMPC_severity, OMPC_SEVERITY_fatal); }
              | WARNING { current_clause = current_directive->addOpenMPClause(OMPC_severity, OMPC_SEVERITY_warning); }
              ;
message_clause : MESSAGE { current_clause = current_directive->addOpenMPClause(OMPC_message); } '(' expression ')'
               ;
compare_clause : COMPARE { current_clause = current_directive->addOpenMPClause(OMPC_compare); }
               ;
fail_clause : FAIL '(' fail_memory_order ')'
            ;
fail_memory_order : SEQ_CST { current_clause = current_directive->addOpenMPClause(OMPC_fail, OMPC_FAIL_seq_cst); }
                  | ACQUIRE { current_clause = current_directive->addOpenMPClause(OMPC_fail, OMPC_FAIL_acquire); }
                  | RELAXED { current_clause = current_directive->addOpenMPClause(OMPC_fail, OMPC_FAIL_relaxed); }
                  ;
weak_clause : WEAK { current_clause = current_directive->addOpenMPClause(OMPC_weak); }
            ;

// OpenMP 5.2 clause implementations
doacross_clause : DOACROSS '(' doacross_type ')'
                ;
doacross_type : SOURCE ':' {
                    current_clause = current_directive->addOpenMPClause(OMPC_doacross, OMPC_DOACROSS_TYPE_source);
                    current_expr_separator = OMPC_CLAUSE_SEP_space;
                } doacross_source_arg
              | SINK ':' {
                    current_clause = current_directive->addOpenMPClause(OMPC_doacross, OMPC_DOACROSS_TYPE_sink);
                    auto *doacross_clause = static_cast<OpenMPDoacrossClause *>(current_clause);
                    if (!doacross_clause->getSinkArgs().empty()) {
                      current_expr_separator = OMPC_CLAUSE_SEP_comma;
                    } else {
                      current_expr_separator = OMPC_CLAUSE_SEP_space;
                    }
                } doacross_sink_args
              ;
doacross_source_arg : /* empty */
                    | expression {
                        auto *doacross_clause = static_cast<OpenMPDoacrossClause *>(current_clause);
                        doacross_clause->setSourceExpression($1, current_expr_separator);
                        current_expr_separator = OMPC_CLAUSE_SEP_space;
                      }
                    ;
doacross_sink_args : expression {
                         auto *doacross_clause = static_cast<OpenMPDoacrossClause *>(current_clause);
                         doacross_clause->addSinkArg($1, current_expr_separator);
                         current_expr_separator = OMPC_CLAUSE_SEP_space;
                     }
                   | doacross_sink_args ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } expression {
                         auto *doacross_clause = static_cast<OpenMPDoacrossClause *>(current_clause);
                         doacross_clause->addSinkArg($4, current_expr_separator);
                         current_expr_separator = OMPC_CLAUSE_SEP_space;
                     }
                   ;
absent_clause : ABSENT {
                    current_clause = current_directive->addOpenMPClause(OMPC_absent);
                    current_expr_separator = OMPC_CLAUSE_SEP_space;
              } '(' directive_name_list ')'
              ;
contains_clause : CONTAINS {
                    current_clause = current_directive->addOpenMPClause(OMPC_contains);
                    current_expr_separator = OMPC_CLAUSE_SEP_space;
                } '(' directive_name_list ')'
                ;
holds_clause : HOLDS {
                    current_clause = current_directive->addOpenMPClause(OMPC_holds);
             } '(' expression ')'
             ;
otherwise_clause : OTHERWISE {
                    current_clause = current_directive->addOpenMPClause(OMPC_otherwise);
                    current_parent_directive = current_directive;
                    current_parent_clause = current_clause;
                 } '(' variant_directive {
                    ((OpenMPOtherwiseClause*)current_parent_clause)->setVariantDirective(current_directive);
                    current_directive = current_parent_directive;
                    current_clause = current_parent_clause;
                    current_parent_directive = nullptr;
                    current_parent_clause = nullptr;
                 } ')'
                 ;

// OpenMP 6.0 clause implementations
graph_id_clause : GRAPH_ID { current_clause = current_directive->addOpenMPClause(OMPC_graph_id); } '(' expression ')'
                ;
graph_reset_clause : GRAPH_RESET { current_clause = current_directive->addOpenMPClause(OMPC_graph_reset); }
                   ;
transparent_clause : TRANSPARENT { current_clause = current_directive->addOpenMPClause(OMPC_transparent); } opt_transparent_parens
                   ;

opt_transparent_parens : /* empty */
                       | '(' expression ')'
                       ;
replayable_clause : REPLAYABLE { current_clause = current_directive->addOpenMPClause(OMPC_replayable); }
                  ;
threadset_clause : THREADSET { current_clause = current_directive->addOpenMPClause(OMPC_threadset); } '(' expression ')'
                 ;
init_clause : INIT { current_clause = current_directive->addOpenMPClause(OMPC_init); } '(' init_argument ')'
            ;
init_complete_clause : INIT_COMPLETE { current_clause = current_directive->addOpenMPClause(OMPC_init_complete); }
                     ;
init_argument : init_var
              | init_modifier_list ':' init_var
              ;
init_var : EXPR_STRING {
            auto *init_clause = static_cast<OpenMPInitClause *>(current_clause);
            if (init_clause != nullptr) {
              init_clause->setOperand(std::string($1));
            }
          }
         ;
init_modifier_list : init_modifier
                  | init_modifier_list ',' init_modifier
                  ;
init_modifier : init_interop_type
              | init_depinfo_modifier
              | init_prefer_type_modifier
              | init_directive_name_modifier
              ;
init_interop_type : TARGET {
                     auto *init_clause =
                         static_cast<OpenMPInitClause *>(current_clause);
                     if (init_clause != nullptr) {
                       init_clause->addInteropType(OMPC_INIT_KIND_target);
                     }
                   }
                 | TARGETSYNC {
                     auto *init_clause =
                         static_cast<OpenMPInitClause *>(current_clause);
                     if (init_clause != nullptr) {
                       init_clause->addInteropType(OMPC_INIT_KIND_targetsync);
                     }
                   }
                 | EXPR_STRING {
                     auto *init_clause =
                         static_cast<OpenMPInitClause *>(current_clause);
                     if (init_clause != nullptr) {
                       // Store raw string for unknown/vendor interop types
                       init_clause->addInteropType(std::string($1));
                     }
                   }
                 ;
init_prefer_type_modifier : PREFER_TYPE '(' {
                              openmp_begin_raw_expression();
                            } EXPR_STRING ')' {
                              auto *init_clause =
                                  static_cast<OpenMPInitClause *>(current_clause);
                              if (init_clause != nullptr) {
                                init_clause->setPreferType(std::string($4));
                              }
                            }
                          ;
init_depinfo_modifier : init_depinfo_kind '(' {
                         openmp_begin_raw_expression();
                       } EXPR_STRING ')' {
                         auto *init_clause =
                             static_cast<OpenMPInitClause *>(current_clause);
                         if (init_clause != nullptr) {
                           init_clause->setDepinfo(
                               static_cast<OpenMPDependClauseType>($1),
                               std::string($4));
                         }
                       }
                     ;
init_depinfo_kind : IN { $$ = OMPC_DEPENDENCE_TYPE_in; }
                 | OUT { $$ = OMPC_DEPENDENCE_TYPE_out; }
                 | INOUT { $$ = OMPC_DEPENDENCE_TYPE_inout; }
                 | INOUTSET { $$ = OMPC_DEPENDENCE_TYPE_inoutset; }
                 | MUTEXINOUTSET { $$ = OMPC_DEPENDENCE_TYPE_mutexinoutset; }
                 ;
init_directive_name_modifier : DEPOBJ {
                              auto *init_clause =
                                  static_cast<OpenMPInitClause *>(current_clause);
                              if (init_clause != nullptr) {
                                init_clause->setDirectiveNameModifier(OMPD_depobj);
                              }
                            }
                            | INTEROP {
                              auto *init_clause =
                                  static_cast<OpenMPInitClause *>(current_clause);
                              if (init_clause != nullptr) {
                                init_clause->setDirectiveNameModifier(OMPD_interop);
                              }
                            }
                            ;
use_clause : USE { current_clause = current_directive->addOpenMPClause(OMPC_use); } '(' expression ')'
           ;
novariants_clause : NOVARIANTS {
                      current_clause =
                          current_directive->addOpenMPClause(OMPC_novariants);
                    } '(' novariants_parameter ')'
                  ;
novariants_parameter : expression
                     | DISPATCH ':' expression
                     ;
nocontext_clause : NOCONTEXT {
                   current_clause =
                       current_directive->addOpenMPClause(OMPC_nocontext);
                 } '(' nocontext_parameter ')'
                 ;
nocontext_parameter : expression
                   | DISPATCH ':' expression
                   ;
looprange_clause : LOOPRANGE {
                     current_clause = current_directive->addOpenMPClause(OMPC_looprange);
                   } '(' var_list ')' {
                   }
                 ;
permutation_clause : PERMUTATION {
                       current_clause = current_directive->addOpenMPClause(OMPC_permutation);
                     } '(' var_list ')' {
                   }
                   ;
counts_clause : COUNTS {
                  current_clause = current_directive->addOpenMPClause(OMPC_counts);
                } '(' var_list ')' {
              }
              ;
apply_clause : APPLY {
                 current_clause = current_directive->addOpenMPClause(OMPC_apply);
                 current_apply_transform_separator = OMPC_CLAUSE_SEP_comma;
               } '(' apply_parameter_list ')' {
             }
             ;
apply_parameter_list : apply_parameter
                     | apply_parameter_list ',' {
                         current_apply_transform_separator = OMPC_CLAUSE_SEP_comma;
                       } apply_parameter
                     ;
apply_parameter : apply_transformation
                | EXPR_STRING {
                    auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                    if (apply_clause != nullptr) {
                      std::string label = std::string($1);
                      const char *ws = " \t\n\r\f\v";
                      label.erase(0, label.find_first_not_of(ws));
                      label.erase(label.find_last_not_of(ws) + 1);
                      apply_clause->setLabel(label);
                    }
                  } ':' apply_transformation_seq
                | EXPR_STRING '(' EXPR_STRING ')' apply_parameter_suffix_tail {
                    auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                    if (apply_clause != nullptr) {
                      std::string label = std::string($1) + "(" + std::string($3) + ")";
                      const char *ws = " \t\n\r\f\v";
                      label.erase(0, label.find_first_not_of(ws));
                      label.erase(label.find_last_not_of(ws) + 1);
                      apply_clause->setLabel(label);
                    }
                  }
                ;
apply_parameter_suffix_tail : ':' apply_transformation_seq
                           | /* empty */
                           ;
apply_transformation_seq : apply_transformation
                         | apply_transformation_seq {
                             current_apply_transform_separator =
                                 OMPC_CLAUSE_SEP_space;
                           } apply_transformation
                         ;
apply_transformation : UNROLL {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_unroll, std::string(),
                               current_apply_transform_separator);
                         }
                       }
                     | UNROLL PARTIAL '(' EXPR_STRING ')' {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_unroll_partial,
                               std::string($4),
                               current_apply_transform_separator);
                         }
                       }
                     | UNROLL FULL {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_unroll_full, std::string(),
                               current_apply_transform_separator);
                         }
                       }
                     | REVERSE {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_reverse, std::string(),
                               current_apply_transform_separator);
                         }
                       }
                     | INTERCHANGE {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_interchange, std::string(),
                               current_apply_transform_separator);
                         }
                       }
                     | NOTHING {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_nothing, std::string(),
                               current_apply_transform_separator);
                         }
                       }
                     | TILE SIZES '(' EXPR_STRING ')' {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_tile_sizes, std::string($4),
                               current_apply_transform_separator);
                         }
                       }
                     | nested_apply_transformation
                     | EXPR_STRING {
                         auto *apply_clause = static_cast<OpenMPApplyClause *>(current_clause);
                         if (apply_clause != nullptr) {
                           apply_clause->addTransformation(
                               OMPC_APPLY_TRANSFORM_unknown, std::string($1),
                               current_apply_transform_separator);
                         }
                       }
                     ;
nested_apply_transformation : APPLY {
                         auto *outer_apply_clause =
                             static_cast<OpenMPApplyClause *>(current_clause);
                         apply_clause_stack.push_back(outer_apply_clause);
                         apply_clause_separator_stack.push_back(
                             current_apply_transform_separator);
                         current_clause = new OpenMPApplyClause();
                         current_apply_transform_separator = OMPC_CLAUSE_SEP_comma;
                       } '(' apply_parameter_list ')' {
                         OpenMPApplyClause *nested_apply_clause =
                             static_cast<OpenMPApplyClause *>(current_clause);
                         OpenMPApplyClause *outer_apply_clause = nullptr;
                         OpenMPClauseSeparator outer_sep = OMPC_CLAUSE_SEP_comma;
                         if (!apply_clause_stack.empty()) {
                           outer_apply_clause = apply_clause_stack.back();
                           apply_clause_stack.pop_back();
                         }
                         if (!apply_clause_separator_stack.empty()) {
                           outer_sep = apply_clause_separator_stack.back();
                           apply_clause_separator_stack.pop_back();
                         }
                         current_clause = outer_apply_clause;
                         current_apply_transform_separator = outer_sep;
                         if (outer_apply_clause != nullptr &&
                             nested_apply_clause != nullptr) {
                           outer_apply_clause->addNestedApply(nested_apply_clause,
                                                              outer_sep);
                         } else {
                           delete nested_apply_clause;
                         }
                       }
                     ;
induction_clause : INDUCTION {
                     current_clause =
                         current_directive->addOpenMPClause(OMPC_induction);
                 } '(' induction_specification_opt ')' {
                 }
                 ;
induction_specification_opt : /* empty */
                            | induction_specification_list
                            ;
induction_specification_list : induction_specification_item
                            | induction_specification_list ',' induction_specification_item
                            ;
induction_specification_item : STEP '(' EXPR_STRING ')' {
                         auto *induction_clause =
                             static_cast<OpenMPInductionClause *>(current_clause);
                         if (induction_clause != nullptr) {
                           induction_clause->addStepExpression($3);
                         }
                       }
                     | EXPR_STRING ':' EXPR_STRING {
                         auto *induction_clause =
                             static_cast<OpenMPInductionClause *>(current_clause);
                         if (induction_clause != nullptr) {
                           induction_clause->addBinding($1, $3);
                         }
                       }
                     | EXPR_STRING ':' '(' {
                         openmp_begin_raw_expression();
                       } EXPR_STRING ')' {
                         auto *induction_clause =
                             static_cast<OpenMPInductionClause *>(current_clause);
                         if (induction_clause != nullptr) {
                           std::string binding_expr = "(" + std::string($5) + ")";
                           induction_clause->addBinding($1, binding_expr.c_str());
                         }
                       }
                     | EXPR_STRING {
                         auto *induction_clause =
                             static_cast<OpenMPInductionClause *>(current_clause);
                         if (induction_clause != nullptr) {
                           induction_clause->addPassthroughItem($1);
                         }
                       }
                     ;
inductor_clause : INDUCTOR {
                    current_clause = current_directive->addOpenMPClause(OMPC_inductor);
                  } '(' expression ')' {
                  }
                ;
collector_clause : COLLECTOR {
                     current_clause = current_directive->addOpenMPClause(OMPC_collector);
                   } '(' expression ')' {
                   }
                 ;
combiner_clause : COMBINER {
                      current_clause = current_directive->addOpenMPClause(OMPC_combiner);
                    } '(' {
                      openmp_begin_raw_expression();
                    } EXPR_STRING ')' {
                      current_clause->addLangExpr($5);
                      if (current_directive != nullptr &&
                          current_directive->getKind() == OMPD_declare_reduction) {
                        auto *declare_reduction_directive =
                            static_cast<OpenMPDeclareReductionDirective *>(current_directive);
                        auto *expressions = current_clause->getExpressions();
                        if (expressions != nullptr && !expressions->empty()) {
                          declare_reduction_directive->setCombiner(expressions->back());
                        }
                      }
                    }
                ;
no_openmp_clause : NO_OPENMP { current_clause = current_directive->addOpenMPClause(OMPC_no_openmp); }
                 ;
no_openmp_routines_clause : NO_OPENMP_ROUTINES { current_clause = current_directive->addOpenMPClause(OMPC_no_openmp_routines); }
                          ;
no_openmp_constructs_clause : NO_OPENMP_CONSTRUCTS {
                                current_clause = current_directive->addOpenMPClause(OMPC_no_openmp_constructs);
                            } opt_no_openmp_constructs_parens
                            ;

opt_no_openmp_constructs_parens : /* empty */
                                | '(' expression ')'
                                ;
no_parallelism_clause : NO_PARALLELISM { current_clause = current_directive->addOpenMPClause(OMPC_no_parallelism); }
                      ;

taskwait_clause_optseq : /* empty */
                       | taskwait_clause_seq
                       ;
unroll_clause_optseq : /* empty */
                     | unroll_clause_seq
                     ;
tile_clause_optseq : tile_clause_seq
                   ;
// OpenMP 5.1 clause option sequences
error_clause_optseq : /* empty */
                    | error_clause_seq
                    ;
masked_clause_optseq : /* empty */
                     | masked_clause_seq
                     ;
masked_taskloop_clause_optseq : /* empty */
                              | masked_taskloop_clause_seq
                              ;
masked_taskloop_simd_clause_optseq : /* empty */
                                   | masked_taskloop_simd_clause_seq
                                   ;
parallel_masked_clause_optseq : /* empty */
                              | parallel_masked_clause_seq
                              ;
parallel_masked_taskloop_clause_optseq : /* empty */
                                       | parallel_masked_taskloop_clause_seq
                                       ;
parallel_masked_taskloop_simd_clause_optseq : /* empty */
                                            | parallel_masked_taskloop_simd_clause_seq
                                            ;
scope_clause_optseq : /* empty */
                    | scope_clause_seq
                    ;
interop_clause_optseq : /* empty */
                      | interop_clause_seq
                      ;
// OpenMP 5.2 clause option sequences
assume_clause_optseq : /* empty */
                     | assume_clause_seq
                     ;
assumes_clause_optseq : /* empty */
                      | assumes_clause_seq
                      ;
begin_assumes_clause_optseq : /* empty */
                            | begin_assumes_clause_seq
                            ;
begin_metadirective_clause_optseq : /* empty */
                                  | begin_metadirective_clause_seq
                                  ;
// OpenMP 6.0 clause option sequences
allocators_clause_optseq : /* empty */
                         | allocators_clause_seq
                         ;
taskgraph_clause_optseq : /* empty */
                        | taskgraph_clause_seq
                        ;
task_iteration_clause_optseq : /* empty */
                             | task_iteration_clause_seq
                             ;
dispatch_clause_optseq : /* empty */
                       | dispatch_clause_seq
                       ;
groupprivate_clause_optseq : groupprivate_clause_seq
                           ;
workdistribute_clause_optseq : /* empty */
                             | workdistribute_clause_seq
                             ;
fuse_clause_optseq : /* empty */
                   | fuse_clause_seq
                   ;
interchange_clause_optseq : /* empty */
                          | interchange_clause_seq
                          ;
reverse_clause_optseq : /* empty */
                      | reverse_clause_seq
                      ;
split_clause_optseq : /* empty */
                    | split_clause_seq
                    ;
stripe_clause_optseq : /* empty */
                     | stripe_clause_seq
                     ;
declare_induction_clause_optseq : /* empty */
                                | declare_induction_clause_seq
                                ;
taskgroup_clause_optseq : /* empty */
                        | taskgroup_clause_seq
                        ;

task_clause_seq : task_clause
                | task_clause_seq task_clause
                | task_clause_seq ',' { clause_separator_comma = true; } task_clause
                ;
taskloop_clause_seq : taskloop_clause
                    | taskloop_clause_seq taskloop_clause
                    | taskloop_clause_seq ',' { clause_separator_comma = true; } taskloop_clause
                    ;
taskloop_simd_clause_seq : taskloop_simd_clause
                         | taskloop_simd_clause_seq taskloop_simd_clause
                         | taskloop_simd_clause_seq ',' { clause_separator_comma = true; } taskloop_simd_clause
                         ;
requires_clause_seq : requires_clause
                    | requires_clause_seq requires_clause
                    | requires_clause_seq ',' { clause_separator_comma = true; } requires_clause
                    ;

target_data_clause_seq : target_data_clause
                       | target_data_clause_seq target_data_clause
                       | target_data_clause_seq ',' { clause_separator_comma = true; } target_data_clause
                       ;
target_enter_data_clause_seq : target_enter_data_clause
                             | target_enter_data_clause_seq target_enter_data_clause
                             | target_enter_data_clause_seq ',' { clause_separator_comma = true; } target_enter_data_clause
                             ;
target_exit_data_clause_seq : target_exit_data_clause
                            | target_exit_data_clause_seq target_exit_data_clause
                            | target_exit_data_clause_seq ',' { clause_separator_comma = true; } target_exit_data_clause
                            ;
target_clause_seq : target_clause
                  | target_clause_seq target_clause
                  | target_clause_seq ',' { clause_separator_comma = true; } target_clause
                  ;
target_update_clause_seq : target_update_clause
                         | target_update_clause_seq target_update_clause
                         | target_update_clause_seq ',' { clause_separator_comma = true; } target_update_clause
                         ;
declare_target_seq : declare_target_clause
                   | declare_target_seq declare_target_clause
                   | declare_target_seq ',' { clause_separator_comma = true; } declare_target_clause
                   ;
taskwait_clause_seq : taskwait_clause
                    | taskwait_clause_seq taskwait_clause
                    | taskwait_clause_seq ',' taskwait_clause
                    ;
unroll_clause_seq : unroll_clause
                  | unroll_clause_seq unroll_clause
                  ;
tile_clause_seq : tile_clause
                | tile_clause_seq tile_clause
                ;
// OpenMP 5.1 clause sequences and clauses
error_clause_seq : error_clause
                 | error_clause_seq error_clause
                 | error_clause_seq ',' error_clause
                 ;
error_clause : at_clause
             | severity_clause
             | message_clause
             ;
masked_clause_seq : masked_clause
                  | masked_clause_seq masked_clause
                  | masked_clause_seq ',' masked_clause
                  ;
masked_clause : filter_clause
              | private_clause
              | firstprivate_clause
              | shared_clause
              | reduction_clause
              | allocate_clause
              ;
masked_taskloop_clause_seq : masked_taskloop_clause
                           | masked_taskloop_clause_seq masked_taskloop_clause
                           | masked_taskloop_clause_seq ',' masked_taskloop_clause
                           ;
masked_taskloop_clause : if_taskloop_clause
                       | shared_clause
                       | private_clause
                       | firstprivate_clause
                       | lastprivate_clause
                       | reduction_clause
                       | in_reduction_clause
                       | default_clause
                       | grainsize_clause
                       | num_tasks_clause
                       | collapse_clause
                       | final_clause
                       | priority_clause
                       | untied_clause
                       | mergeable_clause
                       | nogroup_clause
                       | allocate_clause
                       | filter_clause
                       ;
masked_taskloop_simd_clause_seq : masked_taskloop_simd_clause
                                | masked_taskloop_simd_clause_seq masked_taskloop_simd_clause
                                | masked_taskloop_simd_clause_seq ',' masked_taskloop_simd_clause
                                ;
masked_taskloop_simd_clause : if_taskloop_simd_clause
                            | shared_clause
                            | private_clause
                            | firstprivate_clause
                            | lastprivate_clause
                            | reduction_clause
                            | in_reduction_clause
                            | default_clause
                            | grainsize_clause
                            | num_tasks_clause
                            | collapse_clause
                            | final_clause
                            | priority_clause
                            | untied_clause
                            | mergeable_clause
                            | nogroup_clause
                            | safelen_clause
                            | simdlen_clause
                            | linear_clause
                            | aligned_clause
                            | nontemporal_clause
                            | order_clause
                            | allocate_clause
                            | filter_clause
                            ;
parallel_masked_clause_seq : parallel_masked_clause
                           | parallel_masked_clause_seq parallel_masked_clause
                           | parallel_masked_clause_seq ',' parallel_masked_clause
                           ;
parallel_masked_clause : if_parallel_clause
                       | num_threads_clause
                       | default_clause
                       | private_clause
                       | firstprivate_clause
                       | shared_clause
                       | copyin_clause
                       | reduction_clause
                       | proc_bind_clause
                       | allocate_clause
                       | filter_clause
                       ;
parallel_masked_taskloop_clause_seq : parallel_masked_taskloop_clause
                                    | parallel_masked_taskloop_clause_seq parallel_masked_taskloop_clause
                                    | parallel_masked_taskloop_clause_seq ',' parallel_masked_taskloop_clause
                                    ;
parallel_masked_taskloop_clause : if_parallel_taskloop_clause
                                | num_threads_clause
                                | default_clause
                                | private_clause
                                | firstprivate_clause
                                | shared_clause
                                | copyin_clause
                                | reduction_clause
                                | proc_bind_clause
                                | allocate_clause
                                | lastprivate_clause
                                | nowait_clause
                                | grainsize_clause
                                | num_tasks_clause
                                | collapse_clause
                                | final_clause
                                | priority_clause
                                | untied_clause
                                | mergeable_clause
                                | nogroup_clause
                                | filter_clause
                                ;
parallel_masked_taskloop_simd_clause_seq : parallel_masked_taskloop_simd_clause
                                         | parallel_masked_taskloop_simd_clause_seq parallel_masked_taskloop_simd_clause
                                         | parallel_masked_taskloop_simd_clause_seq ',' parallel_masked_taskloop_simd_clause
                                         ;
parallel_masked_taskloop_simd_clause : if_parallel_taskloop_simd_clause
                                     | num_threads_clause
                                     | default_clause
                                     | private_clause
                                     | firstprivate_clause
                                     | shared_clause
                                     | copyin_clause
                                     | reduction_clause
                                     | proc_bind_clause
                                     | allocate_clause
                                     | lastprivate_clause
                                     | nowait_clause
                                     | grainsize_clause
                                     | num_tasks_clause
                                     | collapse_clause
                                     | final_clause
                                     | priority_clause
                                     | untied_clause
                                     | mergeable_clause
                                     | nogroup_clause
                                     | safelen_clause
                                     | simdlen_clause
                                     | linear_clause
                                     | aligned_clause
                                     | nontemporal_clause
                                     | order_clause
                                     | filter_clause
                                     ;
scope_clause_seq : scope_clause
                 | scope_clause_seq scope_clause
                 | scope_clause_seq ',' scope_clause
                 ;
scope_clause : private_clause
             | reduction_clause
             | firstprivate_clause
             | allocate_clause
             | nowait_clause
             ;
interop_clause_seq : interop_clause
                   | interop_clause_seq interop_clause
                   | interop_clause_seq ',' interop_clause
                   ;
interop_clause : init_clause
               | use_clause
               | destroy_clause
               | depend_with_modifier_clause
               | nowait_clause
               | device_clause
               ;
// OpenMP 5.2 clause sequences and clauses
assume_clause_seq : assume_clause
                  | assume_clause_seq assume_clause
                  | assume_clause_seq ',' assume_clause
                  ;
assume_clause : holds_clause
              | absent_clause
              | contains_clause
              | no_openmp_clause
              | no_openmp_routines_clause
              | no_openmp_constructs_clause
              | no_parallelism_clause
              ;
assumes_clause_seq : assumes_clause
                   | assumes_clause_seq assumes_clause
                   | assumes_clause_seq ',' assumes_clause
                   ;
assumes_clause : holds_clause
               | absent_clause
               | contains_clause
               | no_openmp_clause
               | no_openmp_routines_clause
               | no_openmp_constructs_clause
               | no_parallelism_clause
               ;
begin_assumes_clause_seq : begin_assumes_clause
                         | begin_assumes_clause_seq begin_assumes_clause
                         | begin_assumes_clause_seq ',' begin_assumes_clause
                         ;
begin_assumes_clause : holds_clause
                     | absent_clause
                     | contains_clause
                     | no_openmp_clause
                     | no_openmp_routines_clause
                     | no_openmp_constructs_clause
                     | no_parallelism_clause
                     ;
begin_metadirective_clause_seq : begin_metadirective_clause
                               | begin_metadirective_clause_seq begin_metadirective_clause
                               | begin_metadirective_clause_seq ',' begin_metadirective_clause
                               ;
begin_metadirective_clause : when_clause
                           | default_variant_clause
                           | otherwise_clause
                           ;
// OpenMP 6.0 clause sequences and clauses
allocators_clause_seq : allocators_clause
                      | allocators_clause_seq allocators_clause
                      | allocators_clause_seq ',' allocators_clause
                      ;
allocators_clause : allocate_clause
                  | uses_allocators_clause
                  ;
taskgraph_clause_seq : taskgraph_clause
                     | taskgraph_clause_seq taskgraph_clause
                     | taskgraph_clause_seq ',' taskgraph_clause
                     ;
taskgraph_clause : graph_id_clause
                 | graph_reset_clause
                 | transparent_clause
                 | replayable_clause
                 | threadset_clause
                 | if_taskgraph_clause
                 ;
task_iteration_clause_seq : task_iteration_clause
                          | task_iteration_clause_seq task_iteration_clause
                          | task_iteration_clause_seq ',' task_iteration_clause
                          ;
task_iteration_clause : private_clause
                      | firstprivate_clause
                      | shared_clause
                      | reduction_clause
                      | in_reduction_clause
                      | allocate_clause
                      | depend_with_modifier_clause
                      | if_task_iteration_clause
                      ;
dispatch_clause_seq : dispatch_clause
                    | dispatch_clause_seq dispatch_clause
                    | dispatch_clause_seq ',' dispatch_clause
                    ;
dispatch_clause : device_clause
                | is_device_ptr_clause
                | has_device_addr_clause
                | nowait_clause
                | depend_with_modifier_clause
                | novariants_clause
                | nocontext_clause
                | dispatch_interop_clause
                ;
dispatch_interop_clause : INTEROP {
                           current_clause =
                               current_directive->addOpenMPClause(OMPC_interop);
                           if (!current_clause->getExpressions()->empty()) {
                             current_expr_separator = OMPC_CLAUSE_SEP_comma;
                           } else {
                             current_expr_separator = OMPC_CLAUSE_SEP_space;
                           }
                         } '(' dispatch_interop_parameter ')'
                         ;
dispatch_interop_parameter : var_list
                           | DISPATCH ':' var_list
                           ;
groupprivate_clause_seq : '(' groupprivate_var_list ')'
                        | '(' groupprivate_var_list ')' device_type_clause
                        ;
groupprivate_variable : EXPR_STRING { ((OpenMPGroupprivateDirective*)current_directive)->addGroupprivateList($1); }
                      ;
groupprivate_var_list : groupprivate_variable
                      | groupprivate_var_list ',' groupprivate_variable
                      ;
workdistribute_clause_seq : workdistribute_clause
                          | workdistribute_clause_seq workdistribute_clause
                          | workdistribute_clause_seq ',' workdistribute_clause
                          ;
workdistribute_clause : private_clause
                      | firstprivate_clause
                      | lastprivate_clause
                      | reduction_clause
                      | allocate_clause
                      | collapse_clause
                      | order_clause
                      ;
fuse_clause_seq : fuse_clause
                | fuse_clause_seq fuse_clause
                | fuse_clause_seq ',' fuse_clause
                ;
fuse_clause : counts_clause
            | looprange_clause
            | apply_clause
            ;
interchange_clause_seq : interchange_clause
                       | interchange_clause_seq interchange_clause
                       | interchange_clause_seq ',' interchange_clause
                       ;
interchange_clause : permutation_clause
                   ;
reverse_clause_seq : reverse_clause
                   | reverse_clause_seq reverse_clause
                   | reverse_clause_seq ',' reverse_clause
                   ;
reverse_clause : looprange_clause
               ;
split_clause_seq : split_clause
                 | split_clause_seq split_clause
                 | split_clause_seq ',' split_clause
                 ;
split_clause : looprange_clause
             ;
stripe_clause_seq : stripe_clause
                  | stripe_clause_seq stripe_clause
                  | stripe_clause_seq ',' stripe_clause
                  ;
stripe_clause : counts_clause
              | looprange_clause
              ;
declare_induction_clause_seq : declare_induction_clause
                             | declare_induction_clause_seq declare_induction_clause
                             | declare_induction_clause_seq ',' { clause_separator_comma = true; } declare_induction_clause
                             ;
declare_induction_clause : inductor_clause
                         | collector_clause
                         | combiner_clause
                         ;
taskgroup_clause_seq : taskgroup_clause
                     | taskgroup_clause_seq taskgroup_clause
                     | taskgroup_clause_seq ',' taskgroup_clause
                     ;

task_clause : if_task_clause
            | final_clause
            | untied_clause
            | default_clause
            | mergeable_clause
            | private_clause
            | firstprivate_clause
            | shared_clause
            | in_reduction_clause
            | depend_with_modifier_clause
            | priority_clause
            | allocate_clause
            | affinity_clause
            | detach_clause
            | transparent_clause
            | threadset_clause
            ;
taskloop_clause : if_taskloop_clause
                | shared_clause
                | private_clause
                | firstprivate_clause
                | lastprivate_clause
                | reduction_default_only_clause
                | in_reduction_clause
                | default_clause
                | grainsize_clause
                | num_tasks_clause
                | collapse_clause
                | final_clause
                | priority_clause
                | untied_clause
                | mergeable_clause
                | nogroup_clause
                | allocate_clause
                ;
taskloop_simd_clause : if_taskloop_simd_clause
                     | shared_clause
                     | private_clause
                     | firstprivate_clause
                     | lastprivate_clause
                     | reduction_clause 
                     | in_reduction_clause
                     | default_clause
                     | grainsize_clause
                     | num_tasks_clause
                     | collapse_clause
                     | final_clause
                     | priority_clause
                     | untied_clause
                     | mergeable_clause
                     | nogroup_clause
                     | allocate_clause               
                     | safelen_clause
                     | simdlen_clause
                     | linear_clause
                     | aligned_clause
                     | nontemporal_clause
                     | order_clause 
                     ;
requires_clause : reverse_offload_clause
                | unified_address_clause
                | unified_shared_memory_clause
                | atomic_default_mem_order_clause
                | dynamic_allocators_clause
                | self_maps_clause
                | ext_implementation_defined_requirement_clause
                | device_safesync_clause
                ;
target_data_clause : if_target_data_clause
                   | device_clause
                   | map_clause
                   | use_device_ptr_clause
                   | use_device_addr_clause
                   | nogroup_clause
                   | depend_with_modifier_clause
                   ;
target_enter_data_clause: if_target_enter_data_clause
                        | device_clause
                        | map_clause
                        | depend_with_modifier_clause
                        | nowait_clause
                        ;
target_exit_data_clause: if_target_exit_data_clause
                       | device_clause
                       | map_clause
                       | depend_with_modifier_clause
                       | nowait_clause
                       ;
target_clause: if_target_clause
             | device_clause
             | thread_limit_clause
             | private_clause
             | firstprivate_clause
             | in_reduction_clause
             | map_clause
             | is_device_ptr_clause
             | has_device_addr_clause
             | defaultmap_clause
             | nowait_clause
             | allocate_clause
             | depend_with_modifier_clause
             | uses_allocators_clause
             ;
target_update_clause: motion_clause
                    | target_update_other_clause
                    ;
motion_clause: to_clause
             | from_clause
             ;
target_update_other_clause: if_target_update_clause
                          | device_without_modifier_clause
                          | depend_with_modifier_clause
                          | nowait_clause
                          ;
declare_target_clause : to_clause
                      | link_clause
                      | enter_clause
                      | local_clause
                      | device_type_clause
                      | indirect_clause
                      ;
taskwait_clause : depend_with_modifier_clause
                | nowait_clause
                ;
unroll_clause : full_clause
              | partial_clause
              | apply_clause
              ;
tile_clause : sizes_clause
            | apply_clause
            ;
taskgroup_clause : task_reduction_clause
                 | allocate_clause
                 ;
final_clause: FINAL {
                            current_clause = current_directive->addOpenMPClause(OMPC_final);
                         } '(' expression ')'
            ;
untied_clause: UNTIED {
                            current_clause = current_directive->addOpenMPClause(OMPC_untied);
                         } 
             ;
mergeable_clause: MERGEABLE {
                            current_clause = current_directive->addOpenMPClause(OMPC_mergeable);
                         } 
                ;
in_reduction_clause : IN_REDUCTION '(' in_reduction_identifier ':' var_list ')' { }
                    ;
in_reduction_identifier : in_reduction_enum_identifier
                        | EXPR_STRING { current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_user, $1); }
                        ;

in_reduction_enum_identifier :  '+'{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_plus); }
                             | '-'{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_minus); }
                             | '*'{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_mul); }
                             | '&'{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_bitand); }
                             | '|'{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_bitor); }
                             | '^'{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_bitxor); }
                             | LOGAND{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_logand); }
                             | LOGOR{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_logor); }
                             | MAX{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_max); }
                             | MIN{ current_clause = current_directive->addOpenMPClause(OMPC_in_reduction,OMPC_IN_REDUCTION_IDENTIFIER_min); }
                             ;

depend_with_modifier_clause : DEPEND { firstParameter = OMPC_DEPEND_MODIFIER_unspecified; } '(' depend_parameter ':' var_list ')' { ((OpenMPDependClause*)current_clause)->mergeDepend(current_directive, current_clause); }
                            ;

depend_parameter : dependence_type
                 | depend_modifier ',' dependence_type {
                     auto *depend_clause =
                         static_cast<OpenMPDependClause *>(current_clause);
                     if (depend_clause != nullptr) {
                       depend_clause->setDependIteratorsDefinitionClass(
                           depend_iterators_definition_class);
                     }
                     depend_iterators_definition_class.clear();
                   }
                 ;
dependence_type : depend_enum_type 
                ;
depend_modifier : MODIFIER_ITERATOR {
                   depend_iterators_definition_class.clear();
                   depend_iterator_definition.clear();
                   firstParameter = OMPC_DEPEND_MODIFIER_iterator;
                 } '(' depend_iterators_definition ')'
                ;
depend_iterators_definition : depend_iterator_specifier
                            | depend_iterators_definition ',' depend_iterator_specifier
                            ;
depend_iterator_specifier : EXPR_STRING EXPR_STRING {
                            depend_iterator_definition.push_back($1);
                            depend_iterator_definition.push_back($2);
                          } '=' depend_range_specification
                          | EXPR_STRING {
                            depend_iterator_definition.push_back("");
                            depend_iterator_definition.push_back($1);
                          } '=' depend_range_specification
                          ;
depend_range_specification : EXPR_STRING { depend_iterator_definition.push_back($1); }
                             ':' EXPR_STRING { depend_iterator_definition.push_back($4); }
                             depend_range_step
                           ;
depend_range_step : /*empty*/ {
                     depend_iterator_definition.push_back("");
                     depend_iterators_definition_class.push_back(
                         depend_iterator_definition);
                     depend_iterator_definition.clear();
                   }
                  | ':' EXPR_STRING {
                     depend_iterator_definition.push_back($2);
                     depend_iterators_definition_class.push_back(
                         depend_iterator_definition);
                     depend_iterator_definition.clear();
                   }
                  ;
depend_enum_type : IN { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_in);
                        if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; } }
                 | OUT { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_out);
                        if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; } }
                 | INOUT { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_inout);
                        if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; } }
                 | INOUTSET { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_inoutset);
                        if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; } }
                 | MUTEXINOUTSET { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_mutexinoutset);
                        if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; } }
                 | DEPOBJ { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_depobj);
                        if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; } }
                 ;

depend_depobj_clause : DEPEND { firstParameter = OMPC_DEPEND_MODIFIER_unspecified; }'(' dependence_depobj_parameter ')' {
}
                     ;
dependence_depobj_parameter : dependence_depobj_type ':' expression
                            ;
dependence_depobj_type : IN             { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_in); }
                       | OUT            { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_out); }
                       | INOUT          { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_inout); }
                       | INOUTSET       { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_inoutset); }
                       | MUTEXINOUTSET  { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_mutexinoutset); }
                       ;
depend_ordered_clause : DEPEND { firstParameter = OMPC_DEPEND_MODIFIER_unspecified; }'(' dependence_ordered_parameter ')' {
}
                      ;
dependence_ordered_parameter : dependence_ordered_type
                             ;
dependence_ordered_type :  SOURCE { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_source); }
                        | SINK { current_clause = current_directive->addOpenMPClause(OMPC_depend, firstParameter, OMPC_DEPENDENCE_TYPE_sink); } ':' var_list
                        ;

priority_clause: PRIORITY {
                            current_clause = current_directive->addOpenMPClause(OMPC_priority);
                         } '(' expression ')'
               ;

affinity_clause: AFFINITY '(' affinity_parameter ')' ;

affinity_parameter : EXPR_STRING { current_clause = current_directive->addOpenMPClause(OMPC_affinity, OMPC_AFFINITY_MODIFIER_unspecified); current_clause->addLangExpr($1); }
                   | EXPR_STRING ',' { current_clause = current_directive->addOpenMPClause(OMPC_affinity, OMPC_AFFINITY_MODIFIER_unspecified); current_expr_separator = OMPC_CLAUSE_SEP_comma; current_clause->addLangExpr($1, OMPC_CLAUSE_SEP_space); } var_list
                   | affinity_modifier ':' var_list
                   ;

affinity_modifier : MODIFIER_ITERATOR { current_clause = current_directive->addOpenMPClause(OMPC_affinity, OMPC_AFFINITY_MODIFIER_iterator); 
                              }'('iterators_definition')'{}
                  ;
iterators_definition : iterator_specifier
                     | iterators_definition ',' iterator_specifier
                     ;
iterator_specifier : EXPR_STRING EXPR_STRING {
                      iterator_definition.push_back($1);
                      iterator_definition.push_back($2);
                    } '=' range_specification
                   | EXPR_STRING {
                      iterator_definition.push_back("");
                      iterator_definition.push_back($1);
                    } '=' range_specification
                   ;
range_specification : EXPR_STRING { iterator_definition.push_back($1); }
                      ':' EXPR_STRING { iterator_definition.push_back($4); }
                      range_step
                    ;
range_step : /*empty*/ {
             iterator_definition.push_back("");
             auto *affinity_clause =
                 static_cast<OpenMPAffinityClause *>(current_clause);
             if (affinity_clause != nullptr) {
               affinity_clause->addIteratorsDefinitionClass(iterator_definition);
             }
             iterator_definition.clear();
           }
           | ':' EXPR_STRING {
             iterator_definition.push_back($2);
             auto *affinity_clause =
                 static_cast<OpenMPAffinityClause *>(current_clause);
             if (affinity_clause != nullptr) {
               affinity_clause->addIteratorsDefinitionClass(iterator_definition);
             }
             iterator_definition.clear();
           }
           ;

detach_clause: DETACH {
                            current_clause = current_directive->addOpenMPClause(OMPC_detach);
                         } '(' expression ')'
             ;
grainsize_clause: GRAINSIZE { firstParameter = OMPC_GRAINSIZE_MODIFIER_unspecified; } '(' grainsize_parameter ')'
                ;
grainsize_parameter : STRICT ':' {
                        firstParameter = OMPC_GRAINSIZE_MODIFIER_strict;
                        current_clause = current_directive->addOpenMPClause(OMPC_grainsize, firstParameter);
                    } expression
                    | {
                        current_clause = current_directive->addOpenMPClause(OMPC_grainsize, firstParameter);
                    } expression
                    ;
num_tasks_clause: NUM_TASKS { firstParameter = OMPC_NUM_TASKS_MODIFIER_unspecified; } '(' num_tasks_parameter ')'
                ;
num_tasks_parameter : STRICT ':' {
                        firstParameter = OMPC_NUM_TASKS_MODIFIER_strict;
                        current_clause = current_directive->addOpenMPClause(OMPC_num_tasks, firstParameter);
                    } expression
                    | {
                        current_clause = current_directive->addOpenMPClause(OMPC_num_tasks, firstParameter);
                    } expression
                    ;
nogroup_clause: NOGROUP {
                            current_clause = current_directive->addOpenMPClause(OMPC_nogroup);
                         } 
              ;
reverse_offload_clause: REVERSE_OFFLOAD {
                            current_clause = current_directive->addOpenMPClause(OMPC_reverse_offload);
                         } 
                      ;
unified_address_clause: UNIFIED_ADDRESS {
                            current_clause = current_directive->addOpenMPClause(OMPC_unified_address);
                         } 
                      ;
unified_shared_memory_clause: UNIFIED_SHARED_MEMORY {
                            current_clause = current_directive->addOpenMPClause(OMPC_unified_shared_memory);
                         } 
                      ;
atomic_default_mem_order_clause : ATOMIC_DEFAULT_MEM_ORDER '(' atomic_default_mem_order_parameter ')' { } ;

atomic_default_mem_order_parameter : SEQ_CST { current_clause = current_directive->addOpenMPClause(OMPC_atomic_default_mem_order, OMPC_ATOMIC_DEFAULT_MEM_ORDER_seq_cst); }
                                   | ACQ_REL { current_clause = current_directive->addOpenMPClause(OMPC_atomic_default_mem_order, OMPC_ATOMIC_DEFAULT_MEM_ORDER_acq_rel); }
                                   | RELAXED { current_clause = current_directive->addOpenMPClause(OMPC_atomic_default_mem_order, OMPC_ATOMIC_DEFAULT_MEM_ORDER_relaxed); }
                                   ;
dynamic_allocators_clause: DYNAMIC_ALLOCATORS {
                            current_clause = current_directive->addOpenMPClause(OMPC_dynamic_allocators);
                         }
                         ;
self_maps_clause: SELF_MAPS {
                            current_clause = current_directive->addOpenMPClause(OMPC_self_maps);
                         }
                         ;
ext_implementation_defined_requirement_clause: EXT_ EXPR_STRING {
                                               current_clause = current_directive->addOpenMPClause(OMPC_ext_implementation_defined_requirement);
                                               ((OpenMPExtImplementationDefinedRequirementClause*)current_clause)->setImplementationDefinedRequirement($2);
                                               ((OpenMPExtImplementationDefinedRequirementClause*)current_clause)->mergeExtImplementationDefinedRequirement(current_directive, current_clause);
                                             }
                                             ;
device_clause : DEVICE '(' device_parameter ')' ;

device_parameter : EXPR_STRING  { current_clause = current_directive->addOpenMPClause(OMPC_device, OMPC_DEVICE_MODIFIER_unspecified); current_clause->addLangExpr($1); }
                 | EXPR_STRING ',' { current_clause = current_directive->addOpenMPClause(OMPC_device); current_expr_separator = OMPC_CLAUSE_SEP_comma; current_clause->addLangExpr($1, OMPC_CLAUSE_SEP_space); } var_list
                 | device_modifier_parameter ':' var_list
                 ;

device_modifier_parameter : ANCESTOR { current_clause = current_directive->addOpenMPClause(OMPC_device, OMPC_DEVICE_MODIFIER_ancestor); }
                          | DEVICE_NUM { current_clause = current_directive->addOpenMPClause(OMPC_device, OMPC_DEVICE_MODIFIER_device_num); }
                          ;
                          
device_without_modifier_clause : DEVICE '(' device_without_modifier_parameter ')' ;

device_without_modifier_parameter : EXPR_STRING  { current_clause = current_directive->addOpenMPClause(OMPC_device, OMPC_DEVICE_MODIFIER_unspecified); current_clause->addLangExpr($1); }
                                  | EXPR_STRING ',' { current_clause = current_directive->addOpenMPClause(OMPC_device); current_expr_separator = OMPC_CLAUSE_SEP_comma; current_clause->addLangExpr($1, OMPC_CLAUSE_SEP_space); } var_list
                                  ;

use_device_ptr_clause : USE_DEVICE_PTR {
                current_clause = current_directive->addOpenMPClause(OMPC_use_device_ptr);
} '(' var_list ')'
                      ;
            
sizes_clause : SIZES {
                current_clause = current_directive->addOpenMPClause(OMPC_sizes);
} '(' var_list ')'
                      ;

use_device_addr_clause : USE_DEVICE_ADDR {
                current_clause = current_directive->addOpenMPClause(OMPC_use_device_addr);
} '(' var_list ')'
                       ;
is_device_ptr_clause : IS_DEVICE_PTR {
                current_clause = current_directive->addOpenMPClause(OMPC_is_device_ptr);
} '(' var_list ')' {
}
                     ;
                     
has_device_addr_clause : HAS_DEVICE_ADDR {
                current_clause = current_directive->addOpenMPClause(OMPC_has_device_addr);
} '(' var_list ')' {
}
                     ;
defaultmap_clause : DEFAULTMAP{ firstParameter = OMPC_DEFAULTMAP_BEHAVIOR_unspecified; } '('  defaultmap_parameter ')'
                  ;
defaultmap_parameter : defaultmap_behavior { current_clause = current_directive->addOpenMPClause(OMPC_defaultmap, firstParameter,OMPC_DEFAULTMAP_CATEGORY_unspecified); } 
                     | defaultmap_behavior ':' defaultmap_category
                     ;

defaultmap_behavior : BEHAVIOR_ALLOC { firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_alloc; }
                    | BEHAVIOR_TO { firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_to; }
                    | BEHAVIOR_FROM { firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_from; }
                    | BEHAVIOR_TOFROM {firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_tofrom; }
                    | BEHAVIOR_FIRSTPRIVATE { firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_firstprivate; }
                    | BEHAVIOR_NONE { firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_none; }
                    | BEHAVIOR_DEFAULT { firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_default; }
                    | BEHAVIOR_PRESENT { firstParameter=OMPC_DEFAULTMAP_BEHAVIOR_present; }
                    ;
defaultmap_category : CATEGORY_SCALAR { current_clause = current_directive->addOpenMPClause(OMPC_defaultmap, firstParameter,OMPC_DEFAULTMAP_CATEGORY_scalar); }
                    | CATEGORY_AGGREGATE { current_clause = current_directive->addOpenMPClause(OMPC_defaultmap, firstParameter,OMPC_DEFAULTMAP_CATEGORY_aggregate); }
                    | CATEGORY_POINTER { current_clause = current_directive->addOpenMPClause(OMPC_defaultmap,firstParameter,OMPC_DEFAULTMAP_CATEGORY_pointer); }
                    | CATEGORY_ALL { current_clause = current_directive->addOpenMPClause(OMPC_defaultmap,firstParameter,OMPC_DEFAULTMAP_CATEGORY_all); }
                    | CATEGORY_ALLOCATABLE { if (user_set_lang == Lang_Fortran || auto_lang == Lang_Fortran) {current_clause = current_directive->addOpenMPClause(OMPC_defaultmap,firstParameter,OMPC_DEFAULTMAP_CATEGORY_allocatable);} else { yyerror("Defaultmap clause does not support allocatable in C/C++."); YYABORT;} }
                    ;
uses_allocators_clause : USES_ALLOCATORS  { current_clause = current_directive->addOpenMPClause(OMPC_uses_allocators); firstParameter = OMPC_USESALLOCATORS_ALLOCATOR_unspecified; firstStringParameter = ""; secondStringParameter = ""; } '(' uses_allocators_parameter ')' ;
uses_allocators_parameter : allocators_list
                          | allocators_list ',' uses_allocators_parameter
                          ;

allocators_list : TRAITS '(' EXPR_STRING ')' ':' EXPR_STRING {
                    usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_unspecified;
                    firstStringParameter = $3;
                    secondStringParameter = $6;
                    ((OpenMPUsesAllocatorsClause*)current_clause)
                        ->addUsesAllocatorsAllocatorSequence(
                            usesAllocator, firstStringParameter,
                            secondStringParameter);
                  }
                | allocators_list_parameter_enum { firstStringParameter = ""; ((OpenMPUsesAllocatorsClause*)current_clause)->addUsesAllocatorsAllocatorSequence(usesAllocator, firstStringParameter, secondStringParameter); }
                | allocators_list_parameter_enum '(' EXPR_STRING ')' { firstStringParameter = $3; ((OpenMPUsesAllocatorsClause*)current_clause)->addUsesAllocatorsAllocatorSequence(usesAllocator, firstStringParameter, secondStringParameter); }
                | allocators_list_parameter_user { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_user; firstStringParameter = ""; ((OpenMPUsesAllocatorsClause*)current_clause)->addUsesAllocatorsAllocatorSequence(usesAllocator, firstStringParameter, secondStringParameter); }
                | allocators_list_parameter_user '(' EXPR_STRING ')' { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_user; firstStringParameter = $3; ((OpenMPUsesAllocatorsClause*)current_clause)->addUsesAllocatorsAllocatorSequence(usesAllocator, firstStringParameter, secondStringParameter); }
                ;

allocators_list_parameter_enum : DEFAULT_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_default; }
                               | LARGE_CAP_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_large_cap; }
                               | CONST_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_cons_mem; }
                               | HIGH_BW_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_high_bw; }
                               | LOW_LAT_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_low_lat;}
                               | CGROUP_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_cgroup;  }
                               | PTEAM_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_pteam;  }
                               | THREAD_MEM_ALLOC { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_thread; }
                               ;
allocators_list_parameter_user : EXPR_STRING { usesAllocator = OMPC_USESALLOCATORS_ALLOCATOR_unspecified; secondStringParameter = $1; }
                               ;
to_clause: TO { current_expr_separator = OMPC_CLAUSE_SEP_space; } '(' to_parameter ')' ;
to_parameter : EXPR_STRING  { current_clause = current_directive->addOpenMPClause(OMPC_to, OMPC_TO_unspecified); static_cast<OpenMPToClause*>(current_clause)->addItem($1); }
             | EXPR_STRING ',' { current_clause = current_directive->addOpenMPClause(OMPC_to, OMPC_TO_unspecified); current_expr_separator = OMPC_CLAUSE_SEP_comma; static_cast<OpenMPToClause*>(current_clause)->addItem($1); } to_var_list
             | to_mapper ':' to_var_list
             | to_iterator ':' to_var_list
             | PRESENT ':' { current_clause = current_directive->addOpenMPClause(OMPC_to, OMPC_TO_present); } to_var_list
             ;
to_mapper : TO_MAPPER { current_clause = current_directive->addOpenMPClause(OMPC_to, OMPC_TO_mapper);
                              }'('EXPR_STRING')' { ((OpenMPToClause*)current_clause)->setMapperIdentifier($4); }
          ;
to_iterator : TO_ITERATOR { current_clause = current_directive->addOpenMPClause(OMPC_to, OMPC_TO_iterator); tofrom_iterator_args.clear();
                                }'(' to_iterator_args ')' { addToFromIteratorDefinition(current_clause, &tofrom_iterator_args); }
            ;
to_var_list : to_var
            | to_var_list ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } to_var
            ;

to_var : EXPR_STRING {
           auto *to_clause = static_cast<OpenMPToClause *>(current_clause);
           to_clause->addItem($1, current_expr_separator);
           current_expr_separator = OMPC_CLAUSE_SEP_space;
         }
       ;
to_iterator_args : EXPR_STRING { tofrom_iterator_args.push_back($1); } '=' EXPR_STRING { tofrom_iterator_args.push_back($4); } ':' EXPR_STRING { tofrom_iterator_args.push_back($7); } to_iterator_step
                 ;
to_iterator_step : /* empty */
                 | ':' EXPR_STRING { tofrom_iterator_args.push_back($2); }
                 ;

from_clause: FROM { current_expr_separator = OMPC_CLAUSE_SEP_space; } '(' from_parameter ')' ;
from_parameter : EXPR_STRING { current_clause = current_directive->addOpenMPClause(OMPC_from, OMPC_FROM_unspecified); static_cast<OpenMPFromClause*>(current_clause)->addItem($1);  }
               | EXPR_STRING ',' { current_clause = current_directive->addOpenMPClause(OMPC_from, OMPC_FROM_unspecified); current_expr_separator = OMPC_CLAUSE_SEP_comma; static_cast<OpenMPFromClause*>(current_clause)->addItem($1); } from_var_list
               | from_mapper ':' from_var_list
               | from_iterator ':' from_var_list
               | PRESENT ':' { current_clause = current_directive->addOpenMPClause(OMPC_from, OMPC_FROM_present); } from_var_list
               ;
from_mapper : FROM_MAPPER { current_clause = current_directive->addOpenMPClause(OMPC_from, OMPC_FROM_mapper); 
                              }'('EXPR_STRING')' { ((OpenMPFromClause*)current_clause)->setMapperIdentifier($4); }
            ;
from_iterator : FROM_ITERATOR { current_clause = current_directive->addOpenMPClause(OMPC_from, OMPC_FROM_iterator); tofrom_iterator_args.clear();
                                  } '(' to_iterator_args ')' { addToFromIteratorDefinition(current_clause, &tofrom_iterator_args); }
            ;
from_var_list : from_var
              | from_var_list ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } from_var
              ;

from_var : EXPR_STRING {
             auto *from_clause = static_cast<OpenMPFromClause *>(current_clause);
             from_clause->addItem($1, current_expr_separator);
             current_expr_separator = OMPC_CLAUSE_SEP_space;
           }
         ;
link_clause : LINK {
                current_clause = current_directive->addOpenMPClause(OMPC_link);
} '(' var_list ')' {
}
  ;
enter_clause : ENTER {
                current_clause = current_directive->addOpenMPClause(OMPC_enter);
} '(' var_list ')' {
}
  ;
local_clause : LOCAL {
                current_clause = current_directive->addOpenMPClause(OMPC_local);
} '(' var_list ')' {
}
  ;
device_type_clause : DEVICE_TYPE '(' device_type_parameter ')' { } ;

device_type_parameter : HOST { current_clause = current_directive->addOpenMPClause(OMPC_device_type, OMPC_DEVICE_TYPE_host); }
                    | NOHOST { current_clause = current_directive->addOpenMPClause(OMPC_device_type, OMPC_DEVICE_TYPE_nohost); }
                    | ANY { current_clause = current_directive->addOpenMPClause(OMPC_device_type, OMPC_DEVICE_TYPE_any); }
                    ;

map_clause : MAP {
             firstParameter = OMPC_MAP_MODIFIER_unspecified;
             secondParameter = OMPC_MAP_MODIFIER_unspecified;
             thirdParameter = OMPC_MAP_MODIFIER_unspecified;
             map_ref_modifier_parameter = OMPC_MAP_REF_MODIFIER_unspecified;
             current_expr_separator = OMPC_CLAUSE_SEP_space;
           }'(' map_parameter')';

map_parameter : EXPR_STRING {
                 current_clause = current_directive->addOpenMPClause(
                     OMPC_map, firstParameter, secondParameter, thirdParameter,
                     OMPC_MAP_TYPE_unspecified, map_ref_modifier_parameter,
                     firstStringParameter);
                 static_cast<OpenMPMapClause *>(current_clause)->addItem($1);
               }
              | EXPR_STRING ',' {
                  current_clause = current_directive->addOpenMPClause(
                      OMPC_map, firstParameter, secondParameter, thirdParameter,
                      OMPC_MAP_TYPE_unspecified, map_ref_modifier_parameter,
                      firstStringParameter);
                  current_expr_separator = OMPC_CLAUSE_SEP_comma;
                  static_cast<OpenMPMapClause *>(current_clause)->addItem($1);
                } map_var_list
              | map_modifier_type ':' map_var_list
              ;
map_modifier_type : map_modifier_type_no_ref
                  | map_ref_modifier ',' map_modifier_type_no_ref
                  ;
map_modifier_type_no_ref : map_type
                         | map_modifier_iterator ',' map_type
                         | map_modifier1 map_type
                         | map_modifier1 ',' map_type
                         | map_modifier1 ',' map_modifier_parameter1
                         | map_modifier1 map_modifier_parameter1
                         ;
map_ref_modifier : MAP_REF_MODIFIER_REF_PTEE {
                    map_ref_modifier_parameter = OMPC_MAP_REF_MODIFIER_ref_ptee;
                  }
                 | MAP_REF_MODIFIER_REF_PTR {
                    map_ref_modifier_parameter = OMPC_MAP_REF_MODIFIER_ref_ptr;
                  }
                 | MAP_REF_MODIFIER_REF_PTR_PTEE {
                    map_ref_modifier_parameter = OMPC_MAP_REF_MODIFIER_ref_ptr_ptee;
                  }
                 ;
map_modifier_parameter1 : map_modifier2 map_type
                        | map_modifier2 ',' map_type
                        | map_modifier2 map_modifier_parameter2
                        | map_modifier2 ',' map_modifier_parameter2
                        ;
map_modifier_parameter2 : map_modifier3 map_type
                        | map_modifier3 ',' map_type
                        ; 

map_modifier1 : MAP_MODIFIER_ALWAYS { firstParameter = OMPC_MAP_MODIFIER_always; }
              | MAP_MODIFIER_CLOSE  { firstParameter = OMPC_MAP_MODIFIER_close; }
              | MAP_MODIFIER_PRESENT { firstParameter = OMPC_MAP_MODIFIER_present; }
              | MAP_MODIFIER_SELF { firstParameter = OMPC_MAP_MODIFIER_self; }
              | map_modifier_mapper { firstParameter = OMPC_MAP_MODIFIER_mapper; }
              ;
map_modifier2 : MAP_MODIFIER_ALWAYS { if (firstParameter == OMPC_MAP_MODIFIER_always) { yyerror("ALWAYS modifier can appear in the map clause only once\n"); YYABORT; } else { secondParameter = OMPC_MAP_MODIFIER_always; }}
              | MAP_MODIFIER_CLOSE  { if (firstParameter == OMPC_MAP_MODIFIER_close) { yyerror("CLOSE modifier can appear in the map clause only once\n"); YYABORT;} else { secondParameter = OMPC_MAP_MODIFIER_close; }}
              | MAP_MODIFIER_PRESENT { if (firstParameter == OMPC_MAP_MODIFIER_present) { yyerror("PRESENT modifier can appear in the map clause only once\n"); YYABORT;} else { secondParameter = OMPC_MAP_MODIFIER_present; }}
              | MAP_MODIFIER_SELF { if (firstParameter == OMPC_MAP_MODIFIER_self) { yyerror("SELF modifier can appear in the map clause only once\n"); YYABORT;} else { secondParameter = OMPC_MAP_MODIFIER_self; }}
              | map_modifier_mapper { if (firstParameter == OMPC_MAP_MODIFIER_mapper) { yyerror("MAPPER modifier can appear in the map clause only once\n"); YYABORT; } else { secondParameter = OMPC_MAP_MODIFIER_mapper; }}
              ;

map_var_list : map_var
             | map_var_list ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } map_var
             ;

map_var : EXPR_STRING {
             auto *map_clause = static_cast<OpenMPMapClause *>(current_clause);
             map_clause->addItem($1, current_expr_separator);
             current_expr_separator = OMPC_CLAUSE_SEP_space;
          }
        ;
map_modifier3 : MAP_MODIFIER_ALWAYS { if (firstParameter == OMPC_MAP_MODIFIER_always || secondParameter==OMPC_MAP_MODIFIER_always) { yyerror("ALWAYS modifier can appear in the map clause only once\n"); YYABORT; } else { thirdParameter = OMPC_MAP_MODIFIER_always; }}
              | MAP_MODIFIER_CLOSE  { if (firstParameter == OMPC_MAP_MODIFIER_close || secondParameter==OMPC_MAP_MODIFIER_close) { yyerror("CLOSE modifier can appear in the map clause only once\n"); YYABORT; } else { thirdParameter = OMPC_MAP_MODIFIER_close; }}
              | MAP_MODIFIER_PRESENT { if (firstParameter == OMPC_MAP_MODIFIER_present || secondParameter==OMPC_MAP_MODIFIER_present) { yyerror("PRESENT modifier can appear in the map clause only once\n"); YYABORT; } else { thirdParameter = OMPC_MAP_MODIFIER_present; }}
              | MAP_MODIFIER_SELF { if (firstParameter == OMPC_MAP_MODIFIER_self || secondParameter==OMPC_MAP_MODIFIER_self) { yyerror("SELF modifier can appear in the map clause only once\n"); YYABORT; } else { thirdParameter = OMPC_MAP_MODIFIER_self; }}
              | map_modifier_mapper { if (firstParameter == OMPC_MAP_MODIFIER_mapper || secondParameter==OMPC_MAP_MODIFIER_mapper) { yyerror("MAPPER modifier can appear in the map clause only once\n"); YYABORT; } else { thirdParameter = OMPC_MAP_MODIFIER_mapper; }}
              ;
map_type : MAP_TYPE_TO {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_to, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_FROM {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_from, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_TOFROM {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_tofrom, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_STORAGE {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_storage, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_ALLOC {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_alloc, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_RELEASE {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_release, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_DELETE {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_delete, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_PRESENT {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_present, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         | MAP_TYPE_SELF {
             current_clause = current_directive->addOpenMPClause(
                 OMPC_map, firstParameter, secondParameter, thirdParameter,
                 OMPC_MAP_TYPE_self, map_ref_modifier_parameter,
                 firstStringParameter);
             if (hasMapIteratorModifier()) {
               addMapIteratorDefinition(current_clause, &map_iterator_args);
             }
           }
         ;
map_modifier_mapper : MAP_MODIFIER_MAPPER '('EXPR_STRING')' { firstStringParameter = $3; }
                   ;
map_modifier_iterator : MAP_MODIFIER_ITERATOR {
                          firstParameter = OMPC_MAP_MODIFIER_iterator;
                          map_iterator_args.clear();
                        } '(' map_iterator_argument_list ')'
                      ;
map_iterator_argument_list : EXPR_STRING { map_iterator_args.push_back($1); } '=' EXPR_STRING { map_iterator_args.push_back($4); } ':' EXPR_STRING { map_iterator_args.push_back($7); } map_iterator_step
                           ;
map_iterator_step : /* empty */
                  | ':' EXPR_STRING { map_iterator_args.push_back($2); }
                  ;

task_reduction_clause : TASK_REDUCTION '(' task_reduction_identifier ':' var_list ')' {
                      }
                      ;
task_reduction_identifier : task_reduction_enum_identifier
                          | EXPR_STRING { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_user, $1); }
                          ;

task_reduction_enum_identifier : '+' { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_plus); }
                               | '-' { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_minus); }
                               | '*' { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_mul); }
                               | '&' { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_bitand); }
                               | '|' { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_bitor); }
                               | '^' { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_bitxor); }
                               | LOGAND { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_logand); }
                               | LOGOR { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_logor); }
                               | MAX { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_max); }
                               | MIN { current_clause = current_directive->addOpenMPClause(OMPC_task_reduction,OMPC_TASK_REDUCTION_IDENTIFIER_min); }
                               ;
ordered_clause_optseq : /* empty */
                      | ordered_clause_threads_simd_seq
                      | ordered_clause_depend_seq
                      ;
ordered_clause_threads_simd_seq : ordered_clause_threads_simd
                                | ordered_clause_threads_simd_seq ordered_clause_threads_simd
                                | ordered_clause_threads_simd_seq ',' ordered_clause_threads_simd
                                ;
ordered_clause_depend_seq : ordered_clause_depend
                          | ordered_clause_depend_seq ordered_clause_depend
                          | ordered_clause_depend_seq ',' ordered_clause_depend
                          ;
ordered_clause_depend : depend_ordered_clause
                      | doacross_clause
                      ;
ordered_clause_threads_simd : threads_clause
                            | simd_ordered_clause
                            ;
threads_clause : THREADS {
                            current_clause = current_directive->addOpenMPClause(OMPC_threads);
                         } 
               ;
simd_ordered_clause : SIMD {
                            current_clause = current_directive->addOpenMPClause(OMPC_simd);
                         } 
                    ;
teams_distribute_directive : TEAMS DISTRIBUTE {
                        current_directive = new OpenMPDirective(OMPD_teams_distribute);
                                         }
                     teams_distribute_clause_optseq 
                           ;
teams_distribute_clause_optseq : /* empty */
                               | teams_distribute_clause_seq
                               ;
teams_distribute_clause_seq : teams_distribute_clause
                            | teams_distribute_clause_seq teams_distribute_clause
                            | teams_distribute_clause_seq ',' teams_distribute_clause
                            ;
teams_distribute_clause : num_teams_clause
                        | thread_limit_clause
                        | default_clause
                        | private_clause
                        | firstprivate_clause
                        | shared_clause
                        | reduction_default_only_clause
                        | allocate_clause              
                        | lastprivate_distribute_clause
                        | collapse_clause
                        | dist_schedule_clause
                        ;
teams_distribute_simd_directive :  TEAMS DISTRIBUTE SIMD {
                        current_directive = new OpenMPDirective(OMPD_teams_distribute_simd);
                                         }
                     teams_distribute_simd_clause_optseq 
                                ;
teams_distribute_simd_clause_optseq : /* empty */
                                    | teams_distribute_simd_clause_seq
                                    ;
teams_distribute_simd_clause_seq : teams_distribute_simd_clause
                                 | teams_distribute_simd_clause_seq teams_distribute_simd_clause
                                 | teams_distribute_simd_clause_seq ',' teams_distribute_simd_clause
                                 ;
teams_distribute_simd_clause : num_teams_clause
                             | thread_limit_clause
                             | default_clause
                             | private_clause
                             | firstprivate_clause
                             | shared_clause
                             | reduction_clause
                             | allocate_clause
                             | lastprivate_clause
                             | collapse_clause
                             | dist_schedule_clause
                             | if_simd_clause
                             | safelen_clause
                             | simdlen_clause
                             | linear_clause
                             | aligned_clause
                             | nontemporal_clause
                             | order_clause
                             ;
teams_distribute_parallel_for_directive :  TEAMS DISTRIBUTE PARALLEL FOR {
                     current_directive = new OpenMPDirective(OMPD_teams_distribute_parallel_for);
                                         }
                     teams_distribute_parallel_for_clause_optseq 
                                        ;
teams_distribute_parallel_loop_directive : TEAMS DISTRIBUTE PARALLEL LOOP {
                     current_directive =
                         new OpenMPDirective(OMPD_teams_distribute_parallel_loop);
                                         }
                     teams_distribute_parallel_for_clause_optseq
                                        ;
teams_distribute_parallel_for_clause_optseq : /* empty */
                                            | teams_distribute_parallel_for_clause_seq
                                            ;
teams_distribute_parallel_for_clause_seq : teams_distribute_parallel_for_clause
                                         | teams_distribute_parallel_for_clause_seq teams_distribute_parallel_for_clause
                                         | teams_distribute_parallel_for_clause_seq ',' teams_distribute_parallel_for_clause
                                         ;
teams_distribute_parallel_for_clause : num_teams_clause
                                     | thread_limit_clause
                                     | default_clause
                                     | private_clause
                                     | firstprivate_clause
                                     | shared_clause
                                     | reduction_clause
                                     | allocate_clause
                                     | if_parallel_clause
                                     | num_threads_clause                   
                                     | copyin_clause                            
                                     | proc_bind_clause                      
                                     | lastprivate_clause 
                                     | linear_clause
                                     | schedule_clause
                                     | collapse_clause
                                     | ordered_clause
                                     | nowait_clause
                                     | order_clause 
                                     | dist_schedule_clause                                   
                                     ;
teams_distribute_parallel_do_directive :  TEAMS DISTRIBUTE PARALLEL DO {
                     current_directive = new OpenMPDirective(OMPD_teams_distribute_parallel_do);
                     current_directive->setCompactParallelDo(
                         openmp_consume_compact_parallel_do());
                                        }
                     teams_distribute_parallel_do_clause_optseq 
                                       ;
teams_distribute_parallel_do_clause_optseq : /* empty */
                                           | teams_distribute_parallel_do_clause_seq
                                           ;
teams_distribute_parallel_do_clause_seq : teams_distribute_parallel_do_clause
                                        | teams_distribute_parallel_do_clause_seq teams_distribute_parallel_do_clause
                                        | teams_distribute_parallel_do_clause_seq ',' teams_distribute_parallel_do_clause
                                        ;
teams_distribute_parallel_do_clause : num_teams_clause
                                    | thread_limit_clause
                                    | default_clause
                                    | private_clause
                                    | firstprivate_clause
                                    | shared_clause
                                    | reduction_clause
                                    | allocate_clause
                                    | if_parallel_clause
                                    | num_threads_clause                   
                                    | copyin_clause                            
                                    | proc_bind_clause                      
                                    | lastprivate_clause 
                                    | linear_clause
                                    | schedule_clause
                                    | collapse_clause
                                    | ordered_clause
                                    | nowait_clause
                                    | order_clause 
                                    | dist_schedule_clause                                   
                                     ;
teams_distribute_parallel_for_simd_directive : TEAMS DISTRIBUTE PARALLEL FOR SIMD {
                        current_directive = new OpenMPDirective(OMPD_teams_distribute_parallel_for_simd);
                                         }
                       teams_distribute_parallel_for_simd_clause_optseq 
                                             ;
teams_distribute_parallel_loop_simd_directive : TEAMS DISTRIBUTE PARALLEL LOOP SIMD {
                        current_directive = new OpenMPDirective(
                            OMPD_teams_distribute_parallel_loop_simd);
                                         }
                       teams_distribute_parallel_for_simd_clause_optseq
                                             ;
teams_distribute_parallel_for_simd_clause_optseq : /* empty */
                                                 | teams_distribute_parallel_for_simd_clause_seq
                                                 ;
teams_distribute_parallel_for_simd_clause_seq : teams_distribute_parallel_for_simd_clause
                                              | teams_distribute_parallel_for_simd_clause_seq teams_distribute_parallel_for_simd_clause
                                              | teams_distribute_parallel_for_simd_clause_seq ',' teams_distribute_parallel_for_simd_clause
                                              ;
teams_distribute_parallel_for_simd_clause : num_teams_clause
                                          | thread_limit_clause
                                          | default_clause
                                          | private_clause
                                          | firstprivate_clause
                                          | shared_clause
                                          | reduction_clause
                                          | allocate_clause
                                          | if_parallel_simd_clause
                                          | num_threads_clause
                                          | copyin_clause                               
                                          | proc_bind_clause                                  
                                          | lastprivate_clause 
                                          | linear_clause
                                          | schedule_clause
                                          | collapse_clause
                                          | ordered_clause
                                          | nowait_clause
                                          | order_clause 
                                          | dist_schedule_clause
                                          | safelen_clause
                                          | simdlen_clause
                                          | aligned_clause
                                          | nontemporal_clause
                                          ;
teams_distribute_parallel_do_simd_directive : TEAMS DISTRIBUTE PARALLEL DO SIMD {
                        current_directive = new OpenMPDirective(OMPD_teams_distribute_parallel_do_simd);
                                         }
                       teams_distribute_parallel_do_simd_clause_optseq 
                                            ;
teams_distribute_parallel_do_simd_clause_optseq : /* empty */
                                                | teams_distribute_parallel_do_simd_clause_seq
                                                ;
teams_distribute_parallel_do_simd_clause_seq : teams_distribute_parallel_do_simd_clause
                                             | teams_distribute_parallel_do_simd_clause_seq teams_distribute_parallel_do_simd_clause
                                             | teams_distribute_parallel_do_simd_clause_seq ',' teams_distribute_parallel_do_simd_clause
                                             ;
teams_distribute_parallel_do_simd_clause : num_teams_clause
                                         | thread_limit_clause
                                         | default_clause
                                         | private_clause
                                         | firstprivate_clause
                                         | shared_clause
                                         | reduction_clause
                                         | allocate_clause
                                         | if_parallel_simd_clause
                                         | num_threads_clause
                                         | copyin_clause                               
                                         | proc_bind_clause                                  
                                         | lastprivate_clause 
                                         | linear_clause
                                         | schedule_clause
                                         | collapse_clause
                                         | ordered_clause
                                         | nowait_clause
                                         | order_clause 
                                         | dist_schedule_clause
                                         | safelen_clause
                                         | simdlen_clause
                                         | aligned_clause
                                         | nontemporal_clause
                                         ;
teams_loop_directive : TEAMS LOOP{
                        current_directive = new OpenMPDirective(OMPD_teams_loop);
                                         }
                     teams_loop_clause_optseq 
                     ;
teams_loop_simd_directive : TEAMS LOOP SIMD {
                           current_directive =
                               new OpenMPDirective(OMPD_teams_loop_simd);
                         }
                         teams_loop_simd_clause_optseq
                         ;
teams_loop_clause_optseq : /* empty */
                         | teams_loop_clause_seq
                         ;
teams_loop_simd_clause_optseq : /* empty */
                              | teams_loop_simd_clause_seq
                              ;
teams_loop_clause_seq : teams_loop_clause
                      | teams_loop_clause_seq teams_loop_clause
                      | teams_loop_clause_seq ',' { clause_separator_comma = true; } teams_loop_clause
                      ;
teams_loop_simd_clause_seq : teams_loop_simd_clause
                           | teams_loop_simd_clause_seq teams_loop_simd_clause
                           | teams_loop_simd_clause_seq ',' { clause_separator_comma = true; } teams_loop_simd_clause
                           ;
teams_loop_clause : num_teams_clause
                  | thread_limit_clause
                  | default_clause
                  | private_clause
                  | firstprivate_clause
                  | shared_clause
                  | reduction_default_only_clause
                  | allocate_clause
                  | bind_clause
                  | collapse_clause
                  | order_clause
                  | lastprivate_clause
                  ;
teams_loop_simd_clause : num_teams_clause
                      | thread_limit_clause
                      | default_clause
                      | private_clause
                      | firstprivate_clause
                      | shared_clause
                      | reduction_clause
                      | allocate_clause
                      | bind_clause
                      | collapse_clause
                      | order_clause
                      | lastprivate_clause
                      | if_simd_clause
                      | safelen_clause
                      | simdlen_clause
                      | linear_clause
                      | aligned_clause
                      | nontemporal_clause
                      ;
target_parallel_directive : TARGET PARALLEL{
                        current_directive = new OpenMPDirective(OMPD_target_parallel);
                                         }
                     target_parallel_clause_optseq 
                          ;
target_parallel_clause_optseq : /* empty */
                              | target_parallel_clause_seq
                              ;
target_parallel_clause_seq : target_parallel_clause
                           | target_parallel_clause_seq target_parallel_clause
                           | target_parallel_clause_seq ',' target_parallel_clause
                           ;
target_parallel_clause : if_target_parallel_clause
                       | device_clause
                       | private_clause
                       | firstprivate_clause
                       | in_reduction_clause
                       | map_clause
                       | is_device_ptr_clause
                       | defaultmap_clause
                       | nowait_clause
                       | allocate_clause
                       | depend_with_modifier_clause
                       | uses_allocators_clause
                       | num_threads_clause
                       | default_clause
                       | shared_clause
                       | copyin_clause
                       | reduction_clause
                       | proc_bind_clause
                       ;
target_parallel_for_directive : TARGET PARALLEL FOR{
                        current_directive = new OpenMPDirective(OMPD_target_parallel_for);
                                         }
                     target_parallel_for_clause_optseq 
                              ;
target_parallel_for_clause_optseq : /* empty */
                                  | target_parallel_for_clause_seq
                                  ;
target_parallel_for_clause_seq : target_parallel_for_clause
                               | target_parallel_for_clause_seq target_parallel_for_clause
                               | target_parallel_for_clause_seq ',' target_parallel_for_clause
                               ;
target_parallel_for_clause : if_target_parallel_clause
                           | device_clause
                           | private_clause
                           | firstprivate_clause
                           | in_reduction_clause
                           | map_clause
                           | is_device_ptr_clause
                           | defaultmap_clause
                           | nowait_clause
                           | allocate_clause
                           | depend_with_modifier_clause
                           | uses_allocators_clause
                           | num_threads_clause
                           | default_clause
                           | shared_clause
                           | copyin_clause
                           | reduction_clause
                           | proc_bind_clause
                           | lastprivate_clause
                           | linear_clause
                           | schedule_clause
                           | collapse_clause
                           | ordered_clause
                           | order_clause
                           | dist_schedule_clause
                           | induction_clause
                           ;
target_parallel_do_directive : TARGET PARALLEL DO{
                       current_directive = new OpenMPDirective(OMPD_target_parallel_do);
                       current_directive->setCompactParallelDo(
                           openmp_consume_compact_parallel_do());
                     }
                       target_parallel_do_clause_optseq
                     ;
target_parallel_do_clause_optseq : /* empty */
                                 | target_parallel_do_clause_seq
                                 ;
target_parallel_do_clause_seq : target_parallel_do_clause
                              | target_parallel_do_clause_seq target_parallel_do_clause
                              | target_parallel_do_clause_seq ',' target_parallel_do_clause
                              ;
target_parallel_do_clause : if_target_parallel_clause
                          | device_clause
                          | private_clause
                          | firstprivate_clause
                          | in_reduction_clause
                          | map_clause
                          | is_device_ptr_clause
                          | defaultmap_clause
                          | nowait_clause
                          | allocate_clause
                          | depend_with_modifier_clause
                          | uses_allocators_clause
                          | num_threads_clause
                          | default_clause                          
                          | shared_clause
                          | copyin_clause
                          | reduction_clause
                          | proc_bind_clause
                          | lastprivate_clause 
                          | linear_clause
                          | schedule_clause
                          | collapse_clause
                          | ordered_clause
                          | order_clause 
                          | dist_schedule_clause
                          ;
target_parallel_for_simd_directive : TARGET PARALLEL FOR SIMD{
                        current_directive = new OpenMPDirective(OMPD_target_parallel_for_simd);
                                         }
                     target_parallel_for_simd_clause_optseq 
                                   ;
target_parallel_for_simd_clause_optseq : /* empty */
                                       | target_parallel_for_simd_clause_seq
                                       ;
target_parallel_for_simd_clause_seq : target_parallel_for_simd_clause
                                    | target_parallel_for_simd_clause_seq target_parallel_for_simd_clause
                                    | target_parallel_for_simd_clause_seq ',' target_parallel_for_simd_clause
                                    ;
target_parallel_for_simd_clause : if_target_parallel_simd_clause
                                | device_clause
                                | private_clause
                                | firstprivate_clause
                                | in_reduction_clause
                                | map_clause
                                | is_device_ptr_clause
                                | defaultmap_clause
                                | nowait_clause
                                | allocate_clause
                                | depend_with_modifier_clause
                                | uses_allocators_clause
                                | num_threads_clause
                                | default_clause                    
                                | shared_clause
                                | copyin_clause
                                | reduction_clause
                                | proc_bind_clause                       
                                | lastprivate_clause 
                                | linear_clause
                                | schedule_clause
                                | collapse_clause
                                | ordered_clause                        
                                | order_clause
                                | safelen_clause
                                | simdlen_clause
                                | aligned_clause
                                | nontemporal_clause
                                ;
target_parallel_do_simd_directive : TARGET PARALLEL DO SIMD{
                        current_directive = new OpenMPDirective(OMPD_target_parallel_do_simd);
                                         }
                     target_parallel_do_simd_clause_optseq 
                                  ;
target_parallel_do_simd_clause_optseq : /* empty */
                                      | target_parallel_do_simd_clause_seq
                                      ;
target_parallel_do_simd_clause_seq : target_parallel_do_simd_clause
                                   | target_parallel_do_simd_clause_seq target_parallel_do_simd_clause
                                   | target_parallel_do_simd_clause_seq ',' target_parallel_do_simd_clause
                                   ;
target_parallel_do_simd_clause : if_target_parallel_simd_clause
                               | device_clause
                               | private_clause
                               | firstprivate_clause
                               | in_reduction_clause
                               | map_clause
                               | is_device_ptr_clause
                               | defaultmap_clause
                               | nowait_clause
                               | allocate_clause
                               | depend_with_modifier_clause
                               | uses_allocators_clause
                               | num_threads_clause
                               | default_clause                    
                               | shared_clause
                               | copyin_clause
                               | reduction_clause
                               | proc_bind_clause                       
                               | lastprivate_clause 
                               | linear_clause
                               | schedule_clause
                               | collapse_clause
                               | ordered_clause                        
                               | order_clause
                               | safelen_clause
                               | simdlen_clause
                               | aligned_clause
                               | nontemporal_clause
                               ;
target_parallel_loop_directive : TARGET PARALLEL LOOP{
                        current_directive = new OpenMPDirective(OMPD_target_parallel_loop);
                                         }
                     target_parallel_loop_clause_optseq 
                               ;
target_parallel_loop_simd_directive : TARGET PARALLEL LOOP SIMD {
                             current_directive = new OpenMPDirective(
                                 OMPD_target_parallel_loop_simd);
                           }
                           target_parallel_loop_simd_clause_optseq
                           ;
target_parallel_loop_clause_optseq : /* empty */
                                   | target_parallel_loop_clause_seq
                                   ;
target_parallel_loop_simd_clause_optseq : /* empty */
                                       | target_parallel_loop_simd_clause_seq
                                       ;
target_parallel_loop_clause_seq : target_parallel_loop_clause
                                | target_parallel_loop_clause_seq target_parallel_loop_clause
                                | target_parallel_loop_clause_seq ',' { clause_separator_comma = true; } target_parallel_loop_clause
                                ;
target_parallel_loop_simd_clause_seq : target_parallel_loop_simd_clause
                                     | target_parallel_loop_simd_clause_seq target_parallel_loop_simd_clause
                                     | target_parallel_loop_simd_clause_seq ',' { clause_separator_comma = true; } target_parallel_loop_simd_clause
                                     ;
target_parallel_loop_clause : if_target_parallel_clause
                            | device_clause
                            | private_clause
                            | firstprivate_clause
                            | in_reduction_clause
                            | map_clause
                            | is_device_ptr_clause
                            | defaultmap_clause
                            | nowait_clause
                            | allocate_clause
                            | depend_with_modifier_clause
                            | uses_allocators_clause
                            | num_threads_clause
                            | default_clause             
                            | shared_clause
                            | copyin_clause
                            | reduction_clause
                            | proc_bind_clause                   
                            | lastprivate_clause 
                            | collapse_clause
                            | bind_clause
                            | order_clause 
                            ;
target_parallel_loop_simd_clause : if_target_parallel_simd_clause
                                | device_clause
                                | private_clause
                                | firstprivate_clause
                                | in_reduction_clause
                                | map_clause
                                | is_device_ptr_clause
                                | defaultmap_clause
                                | nowait_clause
                                | allocate_clause
                                | depend_with_modifier_clause
                                | uses_allocators_clause
                                | num_threads_clause
                                | default_clause
                                | shared_clause
                                | copyin_clause
                                | reduction_clause
                                | proc_bind_clause
                                | lastprivate_clause
                                | collapse_clause
                                | bind_clause
                                | order_clause
                                | safelen_clause
                                | simdlen_clause
                                | linear_clause
                                | aligned_clause
                                | nontemporal_clause
                                ;
target_loop_clause : if_target_clause
                   | device_clause
                   | map_clause
                   | allocate_clause
                   | private_clause
                   | firstprivate_clause
                   | lastprivate_clause
                   | reduction_clause
                   | in_reduction_clause
                   | bind_clause
                   | order_clause
                   | collapse_clause
                   | depend_with_modifier_clause
                   | nowait_clause
                   | uses_allocators_clause
                   | is_device_ptr_clause
                   | has_device_addr_clause
                   | defaultmap_clause
                   ;

target_loop_clause_seq : target_loop_clause
                       | target_loop_clause_seq target_loop_clause
                       | target_loop_clause_seq ',' { clause_separator_comma = true; } target_loop_clause
                       ;

target_loop_clause_optseq : /* empty */
                          | target_loop_clause_seq
                          ;

target_loop_simd_clause : target_loop_clause
                        | safelen_clause
                        | simdlen_clause
                        | aligned_clause
                        | nontemporal_clause
                        | linear_clause
                        ;

target_loop_simd_clause_seq : target_loop_simd_clause
                            | target_loop_simd_clause_seq target_loop_simd_clause
                            | target_loop_simd_clause_seq ',' { clause_separator_comma = true; } target_loop_simd_clause
                            ;

target_loop_simd_clause_optseq : /* empty */
                               | target_loop_simd_clause_seq
                               ;

target_loop_directive : TARGET LOOP {
                        current_directive = new OpenMPDirective(OMPD_target_loop);
                     }
                     target_loop_clause_optseq
                   ;
target_loop_simd_directive : TARGET LOOP SIMD {
                             current_directive =
                                 new OpenMPDirective(OMPD_target_loop_simd);
                           }
                           target_loop_simd_clause_optseq
                         ;
target_simd_directive : TARGET SIMD{
                        current_directive = new OpenMPDirective(OMPD_target_simd);
                                         }
                     target_simd_clause_optseq 
                      ;
target_simd_clause_optseq : /* empty */
                          | target_simd_clause_seq
                          ;
target_simd_clause_seq : target_simd_clause
                       | target_simd_clause_seq target_simd_clause
                       | target_simd_clause_seq ',' target_simd_clause
                       ;
target_simd_clause : if_target_simd_clause
                   | device_clause
                   | private_clause
                   | firstprivate_clause
                   | in_reduction_clause
                   | map_clause
                   | is_device_ptr_clause
                   | defaultmap_clause
                   | nowait_clause
                   | allocate_clause
                   | depend_with_modifier_clause
                   | uses_allocators_clause
                   | safelen_clause
                   | simdlen_clause
                   | linear_clause
                   | aligned_clause
                   | nontemporal_clause
                   | lastprivate_clause
                   | reduction_clause
                   | collapse_clause
                   | order_clause
                   ;
target_teams_directive : TARGET TEAMS{
                        current_directive = new OpenMPDirective(OMPD_target_teams);
                                         }
                     target_teams_clause_optseq 
                       ;
target_teams_clause_optseq : /* empty */
                           | target_teams_clause_seq
                           ;
target_teams_clause_seq : target_teams_clause
                        | target_teams_clause_seq target_teams_clause
                        | target_teams_clause_seq ',' target_teams_clause
                        ;
target_teams_clause : if_target_clause
                    | device_clause
                    | private_clause
                    | firstprivate_clause
                    | in_reduction_clause
                    | map_clause
                    | is_device_ptr_clause
                    | defaultmap_clause
                    | nowait_clause
                    | allocate_clause
                    | depend_with_modifier_clause
                    | uses_allocators_clause
                    | num_teams_clause
                    | thread_limit_clause
                    | default_clause
                    | shared_clause
                    | reduction_default_only_clause
                    ;
target_teams_distribute_directive : TARGET TEAMS DISTRIBUTE{
                        current_directive = new OpenMPDirective(OMPD_target_teams_distribute);
                                         }
                     target_teams_distribute_clause_optseq 
                                  ;
target_teams_distribute_clause_optseq : /* empty */
                                      | target_teams_distribute_clause_seq
                                      ;
target_teams_distribute_clause_seq : target_teams_distribute_clause
                                   | target_teams_distribute_clause_seq target_teams_distribute_clause
                                   | target_teams_distribute_clause_seq ',' target_teams_distribute_clause
                                   ;
target_teams_distribute_clause : if_target_clause
                               | device_clause
                               | private_clause
                               | firstprivate_clause
                               | in_reduction_clause
                               | map_clause
                               | is_device_ptr_clause
                               | defaultmap_clause
                               | nowait_clause
                               | allocate_clause
                               | depend_with_modifier_clause
                               | uses_allocators_clause
                               | num_teams_clause
                               | thread_limit_clause
                               | default_clause                   
                               | shared_clause
                               | reduction_default_only_clause
                               | lastprivate_distribute_clause
                               | collapse_clause
                               | dist_schedule_clause
                               ;
target_teams_distribute_simd_directive : TARGET TEAMS DISTRIBUTE SIMD{
                        current_directive = new OpenMPDirective(OMPD_target_teams_distribute_simd);
                                         }
                     target_teams_distribute_simd_clause_optseq 
                                       ;
target_teams_distribute_simd_clause_optseq : /* empty */
                                           | target_teams_distribute_simd_clause_seq
                                           ;
target_teams_distribute_simd_clause_seq : target_teams_distribute_simd_clause
                                        | target_teams_distribute_simd_clause_seq target_teams_distribute_simd_clause
                                        | target_teams_distribute_simd_clause_seq ',' target_teams_distribute_simd_clause
                                        ;
target_teams_distribute_simd_clause : if_target_simd_clause
                                    | device_clause
                                    | private_clause
                                    | firstprivate_clause
                                    | in_reduction_clause
                                    | map_clause
                                    | is_device_ptr_clause
                                    | defaultmap_clause
                                    | nowait_clause
                                    | allocate_clause
                                    | depend_with_modifier_clause
                                    | uses_allocators_clause 
                                    | num_teams_clause
                                    | thread_limit_clause
                                    | default_clause
                                    | shared_clause
                                    | reduction_clause
                                    | lastprivate_clause
                                    | collapse_clause
                                    | dist_schedule_clause
                                    | safelen_clause
                                    | simdlen_clause
                                    | linear_clause
                                    | aligned_clause
                                    | nontemporal_clause
                                    | order_clause
                                    ;
target_teams_loop_directive : TARGET TEAMS LOOP{
                        current_directive = new OpenMPDirective(OMPD_target_teams_loop);
                                         }
                     target_teams_loop_clause_optseq 
                            ;
target_teams_loop_simd_directive : TARGET TEAMS LOOP SIMD {
                          current_directive =
                              new OpenMPDirective(OMPD_target_teams_loop_simd);
                        }
                        target_teams_loop_simd_clause_optseq
                        ;
target_teams_loop_clause_optseq : /* empty */
                                | target_teams_loop_clause_seq
                                ;
target_teams_loop_simd_clause_optseq : /* empty */
                                    | target_teams_loop_simd_clause_seq
                                    ;
target_teams_loop_clause_seq : target_teams_loop_clause
                             | target_teams_loop_clause_seq target_teams_loop_clause
                             | target_teams_loop_clause_seq ',' { clause_separator_comma = true; } target_teams_loop_clause
                             ;
target_teams_loop_simd_clause_seq : target_teams_loop_simd_clause
                                  | target_teams_loop_simd_clause_seq target_teams_loop_simd_clause
                                  | target_teams_loop_simd_clause_seq ',' { clause_separator_comma = true; } target_teams_loop_simd_clause
                                  ;
target_teams_loop_clause : if_target_clause
                         | device_clause
                         | private_clause
                         | firstprivate_clause
                         | in_reduction_clause
                         | map_clause
                         | is_device_ptr_clause
                         | defaultmap_clause
                         | nowait_clause
                         | allocate_clause
                         | depend_with_modifier_clause
                         | uses_allocators_clause 
                         | num_teams_clause
                         | thread_limit_clause
                         | default_clause
                         | shared_clause
                         | reduction_default_only_clause                                 
                         | bind_clause
                         | collapse_clause
                         | order_clause
                         | lastprivate_clause
                         ;
target_teams_loop_simd_clause : if_target_simd_clause
                             | device_clause
                             | private_clause
                             | firstprivate_clause
                             | in_reduction_clause
                             | map_clause
                             | is_device_ptr_clause
                             | defaultmap_clause
                             | nowait_clause
                             | allocate_clause
                             | depend_with_modifier_clause
                             | uses_allocators_clause
                             | num_teams_clause
                             | thread_limit_clause
                             | default_clause
                             | shared_clause
                             | reduction_clause
                             | bind_clause
                             | collapse_clause
                             | order_clause
                             | lastprivate_clause
                             | safelen_clause
                             | simdlen_clause
                             | linear_clause
                             | aligned_clause
                             | nontemporal_clause
                             ;
target_teams_distribute_parallel_for_directive : TARGET TEAMS DISTRIBUTE PARALLEL FOR{
                        current_directive = new OpenMPDirective(OMPD_target_teams_distribute_parallel_for);
                                         }
                     target_teams_distribute_parallel_for_clause_optseq 
                                               ;
target_teams_distribute_parallel_loop_directive : TARGET TEAMS DISTRIBUTE PARALLEL LOOP {
                        current_directive = new OpenMPDirective(
                            OMPD_target_teams_distribute_parallel_loop);
                                         }
                     target_teams_distribute_parallel_for_clause_optseq
                                               ;
target_teams_distribute_parallel_for_clause_optseq : /* empty */
                                                   | target_teams_distribute_parallel_for_clause_seq
                                                   ;
target_teams_distribute_parallel_for_clause_seq : target_teams_distribute_parallel_for_clause
                                                | target_teams_distribute_parallel_for_clause_seq target_teams_distribute_parallel_for_clause
                                                | target_teams_distribute_parallel_for_clause_seq ',' target_teams_distribute_parallel_for_clause
                                                ;
target_teams_distribute_parallel_for_clause : if_target_parallel_clause
                                            | device_clause
                                            | private_clause
                                            | firstprivate_clause
                                            | in_reduction_clause
                                            | map_clause
                                            | is_device_ptr_clause
                                            | defaultmap_clause
                                            | nowait_clause
                                            | allocate_clause
                                            | depend_with_modifier_clause
                                            | uses_allocators_clause 
                                            | num_teams_clause
                                            | thread_limit_clause
                                            | default_clause                                 
                                            | shared_clause
                                            | reduction_clause                            
                                            | num_threads_clause                   
                                            | copyin_clause                            
                                            | proc_bind_clause                      
                                            | lastprivate_clause 
                                            | linear_clause
                                            | schedule_clause
                                            | collapse_clause
                                            | ordered_clause                                
                                            | order_clause 
                                            | dist_schedule_clause
                                            ;
target_teams_distribute_parallel_do_directive : TARGET TEAMS DISTRIBUTE PARALLEL DO{
                       current_directive = new OpenMPDirective(OMPD_target_teams_distribute_parallel_do);
                       current_directive->setCompactParallelDo(
                           openmp_consume_compact_parallel_do());
                     }
                      target_teams_distribute_parallel_do_clause_optseq
                    ;
target_teams_distribute_parallel_do_clause_optseq : /* empty */
                                                  | target_teams_distribute_parallel_do_clause_seq
                                                  ;
target_teams_distribute_parallel_do_clause_seq : target_teams_distribute_parallel_do_clause
                                               | target_teams_distribute_parallel_do_clause_seq target_teams_distribute_parallel_do_clause
                                               | target_teams_distribute_parallel_do_clause_seq ',' target_teams_distribute_parallel_do_clause
                                               ;
target_teams_distribute_parallel_do_clause : if_target_parallel_clause
                                           | device_clause
                                           | private_clause
                                           | firstprivate_clause
                                           | in_reduction_clause
                                           | map_clause
                                           | is_device_ptr_clause
                                           | defaultmap_clause
                                           | nowait_clause
                                           | allocate_clause
                                           | depend_with_modifier_clause
                                           | uses_allocators_clause 
                                           | num_teams_clause
                                           | thread_limit_clause
                                           | default_clause                                 
                                           | shared_clause
                                           | reduction_clause                            
                                           | num_threads_clause                   
                                           | copyin_clause                            
                                           | proc_bind_clause                      
                                           | lastprivate_clause 
                                           | linear_clause
                                           | schedule_clause
                                           | collapse_clause
                                           | ordered_clause                                
                                           | order_clause 
                                           | dist_schedule_clause
                                           ;
target_teams_distribute_parallel_for_simd_directive : TARGET TEAMS DISTRIBUTE PARALLEL FOR SIMD{
                        current_directive = new OpenMPDirective(OMPD_target_teams_distribute_parallel_for_simd);
                                         }
                     target_teams_distribute_parallel_for_simd_clause_optseq 
                                                    ;
target_teams_distribute_parallel_loop_simd_directive : TARGET TEAMS DISTRIBUTE PARALLEL LOOP SIMD{
                        current_directive = new OpenMPDirective(
                            OMPD_target_teams_distribute_parallel_loop_simd);
                                         }
                     target_teams_distribute_parallel_for_simd_clause_optseq
                                                    ;
target_teams_distribute_parallel_for_simd_clause_optseq : /* empty */
                                                        | target_teams_distribute_parallel_for_simd_clause_seq
                                                        ;
target_teams_distribute_parallel_for_simd_clause_seq : target_teams_distribute_parallel_for_simd_clause
                                                     | target_teams_distribute_parallel_for_simd_clause_seq target_teams_distribute_parallel_for_simd_clause
                                                     | target_teams_distribute_parallel_for_simd_clause_seq ',' target_teams_distribute_parallel_for_simd_clause
                                                     ;
target_teams_distribute_parallel_for_simd_clause : if_target_parallel_simd_clause
                                                 | device_clause
                                                 | private_clause
                                                 | firstprivate_clause
                                                 | in_reduction_clause
                                                 | map_clause
                                                 | is_device_ptr_clause
                                                 | defaultmap_clause
                                                 | nowait_clause
                                                 | allocate_clause
                                                 | depend_with_modifier_clause
                                                 | uses_allocators_clause 
                                                 | num_teams_clause
                                                 | thread_limit_clause
                                                 | default_clause                                     
                                                 | shared_clause
                                                 | reduction_clause
                                                 | num_threads_clause
                                                 | copyin_clause                               
                                                 | proc_bind_clause                                  
                                                 | lastprivate_clause 
                                                 | linear_clause
                                                 | schedule_clause
                                                 | collapse_clause
                                                 | ordered_clause                          
                                                 | order_clause 
                                                 | dist_schedule_clause
                                                 | safelen_clause
                                                 | simdlen_clause
                                                 | aligned_clause
                                                 | nontemporal_clause
                                                 ;
target_teams_distribute_parallel_do_simd_directive : TARGET TEAMS DISTRIBUTE PARALLEL DO SIMD{
                        current_directive = new OpenMPDirective(OMPD_target_teams_distribute_parallel_do_simd);
                                         }
                     target_teams_distribute_parallel_do_simd_clause_optseq 
                                                   ;
target_teams_distribute_parallel_do_simd_clause_optseq : /* empty */
                                                       | target_teams_distribute_parallel_do_simd_clause_seq
                                                       ;
target_teams_distribute_parallel_do_simd_clause_seq : target_teams_distribute_parallel_do_simd_clause
                                                    | target_teams_distribute_parallel_do_simd_clause_seq target_teams_distribute_parallel_do_simd_clause
                                                    | target_teams_distribute_parallel_do_simd_clause_seq ',' target_teams_distribute_parallel_do_simd_clause
                                                     ;
target_teams_distribute_parallel_do_simd_clause : if_target_parallel_simd_clause
                                                | device_clause
                                                | private_clause
                                                | firstprivate_clause
                                                | in_reduction_clause
                                                | map_clause
                                                | is_device_ptr_clause
                                                | defaultmap_clause
                                                | nowait_clause
                                                | allocate_clause
                                                | depend_with_modifier_clause
                                                | uses_allocators_clause 
                                                | num_teams_clause
                                                | thread_limit_clause
                                                | default_clause                                     
                                                | shared_clause
                                                | reduction_clause
                                                | num_threads_clause
                                                | copyin_clause                               
                                                | proc_bind_clause                                  
                                                | lastprivate_clause 
                                                | linear_clause
                                                | schedule_clause
                                                | collapse_clause
                                                | ordered_clause                          
                                                | order_clause 
                                                | dist_schedule_clause
                                                | safelen_clause
                                                | simdlen_clause
                                                | aligned_clause
                                                | nontemporal_clause
                                                ;
/*YAYING*/
for_directive : FOR {
                        current_directive = new OpenMPDirective(OMPD_for);
                     }
                     for_clause_optseq 
              ;
do_directive : DO {
                     current_directive = new OpenMPDirective(OMPD_do);
                  }
                  do_clause_optseq
             ;
do_paired_directive : DO {
                            current_directive = new OpenMPDirective(OMPD_do);
                         }
                         do_paried_clause_optseq
                    ;
simd_directive : SIMD {
                         current_directive = new OpenMPDirective(OMPD_simd);
                      }
                      simd_clause_optseq 
               ;
for_simd_directive : FOR SIMD {
                                current_directive = new OpenMPDirective(OMPD_for_simd);
                              }
                              for_simd_clause_optseq
                   ;
do_simd_directive : DO SIMD {
                               current_directive = new OpenMPDirective(OMPD_do_simd);
                             }
                             do_simd_clause_optseq
                  ;
do_simd_paired_directive : DO SIMD {
                                     current_directive = new OpenMPDirective(OMPD_do_simd);
                                   }
                                   do_simd_paried_clause_optseq
                         ;
parallel_for_simd_directive : PARALLEL FOR SIMD { 
                                current_directive = new OpenMPDirective(OMPD_parallel_for_simd);
                            }
                            parallel_for_simd_clause_optseq
                            ;
parallel_do_simd_directive : PARALLEL DO SIMD { 
                                current_directive = new OpenMPDirective(OMPD_parallel_do_simd);
                           }
                           parallel_for_simd_clause_optseq
                           ;
declare_simd_directive : DECLARE SIMD {
                          current_directive = new OpenMPDeclareSimdDirective;
                        }
                       declare_simd_clause_optseq
                       ;
declare_simd_fortran_directive : DECLARE SIMD {
                                    current_directive = new OpenMPDeclareSimdDirective();
                               } '(' proc_name ')'
                               declare_simd_clause_optseq
                               ;
proc_name : /* empty */
          | EXPR_STRING { ((OpenMPDeclareSimdDirective*)current_directive)->addProcName($1); }
          ;
distribute_directive : DISTRIBUTE {
                        current_directive = new OpenMPDirective(OMPD_distribute);
                     }
                     distribute_clause_optseq
                     ;
distribute_simd_directive : DISTRIBUTE SIMD {
                              current_directive = new OpenMPDirective(OMPD_distribute_simd);
                          }
                          distribute_simd_clause_optseq
                          ;
distribute_parallel_for_directive : DISTRIBUTE PARALLEL FOR {
                                       current_directive = new OpenMPDirective(OMPD_distribute_parallel_for);
                                  }
                                  distribute_parallel_for_clause_optseq
                                  ;
distribute_parallel_do_directive : DISTRIBUTE PARALLEL DO {
                                       current_directive = new OpenMPDirective(OMPD_distribute_parallel_do);
                                       current_directive->setCompactParallelDo(
                                           openmp_consume_compact_parallel_do());
                                 }
                                 distribute_parallel_do_clause_optseq
                                 ;
distribute_parallel_loop_directive : DISTRIBUTE PARALLEL LOOP {
                                      current_directive =
                                          new OpenMPDirective(OMPD_distribute_parallel_loop);
                                    }
                                    distribute_parallel_for_clause_optseq
                                    ;
distribute_parallel_for_simd_directive : DISTRIBUTE PARALLEL FOR SIMD {
                                             current_directive = new OpenMPDirective(OMPD_distribute_parallel_for_simd);
                                       }
                                       distribute_parallel_for_simd_clause_optseq
                                       ;
distribute_parallel_do_simd_directive : DISTRIBUTE PARALLEL DO SIMD {
                                             current_directive = new OpenMPDirective(OMPD_distribute_parallel_do_simd);
                                      }
                                      distribute_parallel_do_simd_clause_optseq
                                      ;
distribute_parallel_loop_simd_directive : DISTRIBUTE PARALLEL LOOP SIMD {
                                          current_directive = new OpenMPDirective(
                                              OMPD_distribute_parallel_loop_simd);
                                        }
                                        distribute_parallel_for_simd_clause_optseq
                                        ;
parallel_for_directive : PARALLEL FOR {
                         current_directive = new OpenMPDirective(OMPD_parallel_for);
                       }
                       parallel_for_clause_optseq
                       ;
parallel_do_directive : PARALLEL DO {
                        current_directive = new OpenMPDirective(OMPD_parallel_do);
                        current_directive->setCompactParallelDo(
                            openmp_consume_compact_parallel_do());
                     }
                      parallel_do_clause_optseq
                   ;
parallel_loop_directive : PARALLEL LOOP {
                         current_directive = new OpenMPDirective(OMPD_parallel_loop);
                        }
                        parallel_loop_clause_optseq
                        ;
parallel_loop_simd_directive : PARALLEL LOOP SIMD {
                               current_directive =
                                   new OpenMPDirective(OMPD_parallel_loop_simd);
                             }
                             parallel_loop_simd_clause_optseq
                             ;
parallel_sections_directive : PARALLEL SECTIONS {
                               current_directive = new OpenMPDirective(OMPD_parallel_sections);
                            }
                            parallel_sections_clause_optseq
                            ;
parallel_single_directive : PARALLEL SINGLE {
                               current_directive = new OpenMPDirective(OMPD_parallel_single);
                            }
                            parallel_single_clause_optseq
                            ;
parallel_workshare_directive : PARALLEL WORKSHARE {
                               if (user_set_lang == Lang_Fortran || auto_lang == Lang_Fortran) {
                                   current_directive = new OpenMPDirective(OMPD_parallel_workshare); } else {
                                       yyerror("parallel workshare is only supported in Fortran");
                                       YYABORT;
                               }
                             }
                             parallel_workshare_clause_optseq
                             ;
parallel_master_directive : PARALLEL MASTER {
                               current_directive = new OpenMPDirective(OMPD_parallel_master);
                          }
                          parallel_master_clause_optseq
                          ;
master_taskloop_directive : MASTER TASKLOOP {
                               current_directive = new OpenMPDirective(OMPD_master_taskloop);
                          }
                          master_taskloop_clause_optseq
                          ;
master_taskloop_simd_directive : MASTER TASKLOOP SIMD {
                                    current_directive = new OpenMPDirective(OMPD_master_taskloop_simd);
                               }
                               master_taskloop_simd_clause_optseq
                               ;
parallel_master_taskloop_directive : PARALLEL MASTER TASKLOOP {
                                          current_directive = new OpenMPDirective(OMPD_parallel_master_taskloop);
                                   }
                                   parallel_master_taskloop_clause_optseq
                                   ; 
parallel_master_taskloop_simd_directive : PARALLEL MASTER TASKLOOP SIMD {
                                          current_directive = new OpenMPDirective(OMPD_parallel_master_taskloop_simd);
                                        }
                                        parallel_master_taskloop_simd_clause_optseq
                                        ; 
loop_directive : LOOP {
                        current_directive = new OpenMPDirective(OMPD_loop);
                     }
               loop_clause_optseq
               ;
scan_directive : SCAN {
                        current_directive = new OpenMPDirective(OMPD_scan);
                      }
               scan_clause_optseq
               ;
sections_directive : SECTIONS {
                        current_directive = new OpenMPDirective(OMPD_sections);
                     }
                   sections_clause_optseq
                   ;
sections_paired_directive : SECTIONS {
                               current_directive = new OpenMPDirective(OMPD_sections);
                            }
                          sections_paired_clause_optseq
                          ;
section_directive : SECTION {
                        current_directive = new OpenMPDirective(OMPD_section);
                  }
                  ;
single_directive : SINGLE {
                        current_directive = new OpenMPDirective(OMPD_single);
                 }
                 single_clause_optseq
                 ;
single_paired_directive : SINGLE {
                               current_directive = new OpenMPDirective(OMPD_single);
                        }
                        single_paired_clause_optseq
                        ;
workshare_directive : WORKSHARE {
                         if (user_set_lang == Lang_Fortran || auto_lang == Lang_Fortran) {
                             current_directive = new OpenMPDirective(OMPD_workshare); } else {
                                 yyerror("workshare is only supported in Fortran");
                                 YYABORT;
                             }
                    }
                    ;
workshare_paired_directive : WORKSHARE {
                               current_directive = new OpenMPDirective(OMPD_workshare);
                           }
                           workshare_paired_clause_optseq
                           ;
cancel_directive : CANCEL {
                        current_directive = new OpenMPDirective(OMPD_cancel);
                 }
                 cancel_clause_optseq
                 ;
//cancel_fortran_directive : CANCEL {
//                              current_directive = new OpenMPDirective(OMPD_cancel);
//                         }
//                         cancel_clause_fortran_optseq
//                         ;
cancellation_point_directive : CANCELLATION POINT {
                                current_directive = new OpenMPDirective(OMPD_cancellation_point);
                                }
                             cancellation_point_clause_optseq
                             ;
//cancellation_point_fortran_directive : CANCELLATION POINT {
//                                         current_directive = new OpenMPDirective(OMPD_cancellation_point);
//                                     }
//                                     cancellation_point_clause_fortran_optseq
//                                     ;
teams_directive : TEAMS {
                        current_directive = new OpenMPDirective(OMPD_teams);
                }
                teams_clause_optseq
                ;

allocate_directive : ALLOCATE {
                        current_directive = new OpenMPAllocateDirective();
                   } allocate_list
                   allocate_clause_optseq
                   ;
allocate_list : '('directive_varlist')'
              ;

directive_variable : EXPR_STRING { ((OpenMPAllocateDirective*)current_directive)->addAllocateList($1); }
                   ;
directive_varlist : directive_variable
                  | directive_varlist ',' directive_variable
                  ;

threadprivate_directive : THREADPRIVATE { current_directive = new OpenMPThreadprivateDirective(); } '('threadprivate_list')'
                        ;
threadprivate_variable : EXPR_STRING { ((OpenMPThreadprivateDirective*)current_directive)->addThreadprivateList($1); }
                       ;
threadprivate_list : threadprivate_variable
                   | threadprivate_list ',' threadprivate_variable
                   ;

declare_reduction_directive : DECLARE REDUCTION { current_directive = new OpenMPDeclareReductionDirective(); } '(' reduction_list ')' declare_reduction_postfix
                           ;

reduction_list : reduction_identifiers ':' typername_list reduction_combiner_opt
               ;

reduction_combiner_opt : /* empty */
                       | ':' {
                           openmp_begin_raw_expression();
                         } EXPR_STRING {
                           auto *declare_reduction_directive =
                               static_cast<OpenMPDeclareReductionDirective *>(current_directive);
                           if (declare_reduction_directive != nullptr) {
                             declare_reduction_directive->setCombiner($3);
                           }
                         }
                       ;

reduction_identifiers : '+'{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("+"); }
                      | '-'{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("-"); }
                      | '*'{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("*"); }
                      | '&'{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("&"); }
                      | '|'{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("|"); }
                      | '^'{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("^"); }
                      | LOGAND{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("&&"); }
                      | LOGOR{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("||"); }
                      | MIN{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("min"); }
                      | MAX{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier("max"); }
                      | EXPR_STRING{ ((OpenMPDeclareReductionDirective*)current_directive)->setIdentifier($1); }
                      ; 

typername_variable : {
                         openmp_begin_type_string();
                      } EXPR_STRING {
                         ((OpenMPDeclareReductionDirective*)current_directive)->addTypenameList($2);
                      }
                   ;
typername_list : typername_variable
               | typername_list ',' typername_variable
               ;
declare_mapper_directive : DECLARE MAPPER { current_directive = new OpenMPDeclareMapperDirective(OMPD_DECLARE_MAPPER_IDENTIFIER_unspecified); } '(' mapper_list ')' declare_mapper_clause_optseq
                         ;

mapper_list : mapper_identifier_optseq 
            ;

mapper_identifier_optseq : type_var
                         | mapper_identifier ':' type_var
                         ;
 
mapper_identifier : IDENTIFIER_DEFAULT { ((OpenMPDeclareMapperDirective*)current_directive)->setIdentifier(OMPD_DECLARE_MAPPER_IDENTIFIER_default); }
                  | EXPR_STRING { ((OpenMPDeclareMapperDirective*)current_directive)->setIdentifier(OMPD_DECLARE_MAPPER_IDENTIFIER_user); ((OpenMPDeclareMapperDirective*)current_directive)->setUserDefinedIdentifier($1); }
                  ;
         
type_var : EXPR_STRING { 
               const char * _type_var = $1;
               std::string type_var = std::string(_type_var);
               // Handle Fortran style embedded "::" if present
               size_t dc_pos = type_var.find("::");
               if (dc_pos != std::string::npos && (user_set_lang == Lang_Fortran || auto_lang == Lang_Fortran || user_set_lang == Lang_unknown)) {
                   std::string _type = type_var.substr(0, dc_pos);
                   // Skip following spaces
                   size_t var_start = dc_pos + 2;
                   while (var_start < type_var.size() && (type_var[var_start] == ' ' || type_var[var_start] == '\t')) {
                       ++var_start;
                   }
                   std::string _var = type_var.substr(var_start);
                   ((OpenMPDeclareMapperDirective*)current_directive)->setDeclareMapperType(_type.c_str());
                   ((OpenMPDeclareMapperDirective*)current_directive)->setDeclareMapperVar(_var.c_str());
                   ((OpenMPDeclareMapperDirective*)current_directive)->setTypeVarHasSpace(true);
                   if (auto_lang == Lang_unknown) {
                       auto_lang = Lang_Fortran;
                   }
               } else if (user_set_lang == Lang_C || auto_lang == Lang_C) { 
                   int length = type_var.length() - 1;
                   for (int i = length; i >= 0; i--) {
                       if (type_var[i] == ' ' || type_var[i] == '*') { 
                           std::string _type = type_var.substr(0, i + 1);
                           std::string _var = type_var.substr(i + 1, length - i);
                           const char* type = _type.c_str();
                           const char* var = _var.c_str();
                           ((OpenMPDeclareMapperDirective*)current_directive)->setDeclareMapperType(type);
                           ((OpenMPDeclareMapperDirective*)current_directive)->setDeclareMapperVar(var);
                           ((OpenMPDeclareMapperDirective*)current_directive)->setTypeVarHasSpace(type_var[i] == ' ');
                           break;
                       }
                   }
               } else {
                   yyerror("The syntax should be \"type :: var\" in Fortran"); 
                   YYABORT; 
               }
         } 
         | declare_mapper_type DOUBLE_COLON declare_mapper_var { if (user_set_lang == Lang_C || auto_lang == Lang_C) yyerror("The syntax should be \"type var\" in C"); YYABORT; }
         ;
         
declare_mapper_type : EXPR_STRING { ((OpenMPDeclareMapperDirective*)current_directive)->setDeclareMapperType($1); }
                    ;
                    
declare_mapper_var : EXPR_STRING { ((OpenMPDeclareMapperDirective*)current_directive)->setDeclareMapperVar($1); }
                   ;

parallel_clause_optseq : /* empty */
                       | parallel_clause_seq
                       ;
teams_clause_optseq : /* empty */
                    | teams_clause_seq
                    ;

for_clause_optseq : /*empty*/
                  | for_clause_seq
                  ;
do_clause_optseq : /*empty*/
                 | do_clause_seq
                 ;
do_paried_clause_optseq : /*empty*/
                        | nowait_clause
                        ;
simd_clause_optseq : /*empty*/
                   | simd_clause_seq
                   ;
for_simd_clause_optseq : /*empty*/
                       | for_simd_clause_seq
                       ;
do_simd_clause_optseq : /*empty*/
                      | do_simd_clause_seq
                      ;
do_simd_paried_clause_optseq : /*empty*/
                             | nowait_clause
                             ;
parallel_for_simd_clause_optseq : /*empty*/
                                | parallel_for_simd_clause_seq
                                ;
declare_simd_clause_optseq : /*empty*/
                           | declare_simd_clause_seq
                           ;
distribute_clause_optseq : /*empty*/
                         | distribute_clause_seq
                         ;
distribute_simd_clause_optseq : /*empty*/
                              | distribute_simd_clause_seq
                              ;
distribute_parallel_for_clause_optseq : /*empty*/
                                      | distribute_parallel_for_clause_seq
                                      ;
distribute_parallel_do_clause_optseq : /*empty*/
                                     | distribute_parallel_do_clause_seq
                                     ;
distribute_parallel_for_simd_clause_optseq : /*empty*/
                                           | distribute_parallel_for_simd_clause_seq
                                           ;
distribute_parallel_do_simd_clause_optseq : /*empty*/
                                          | distribute_parallel_do_simd_clause_seq
                                          ;
parallel_for_clause_optseq : /*empty*/
                           | parallel_for_clause_seq
                           ;
parallel_do_clause_optseq : /*empty*/
                          | parallel_do_clause_seq
                          ;
parallel_loop_clause_optseq : /*empty*/
                            | parallel_loop_clause_seq
                            ;
parallel_loop_simd_clause_optseq : /*empty*/
                                | parallel_loop_simd_clause_seq
                                ;
parallel_sections_clause_optseq : /*empty*/
                                | parallel_sections_clause_seq
                                ;
parallel_single_clause_optseq : /*empty*/
                              | parallel_single_clause_seq
                              ;
parallel_workshare_clause_optseq : /*empty*/
                                 | parallel_workshare_clause_seq
                                 ;
parallel_master_clause_optseq : /*empty*/
                              | parallel_master_clause_seq
                              ;
master_taskloop_clause_optseq : /*empty*/
                              | master_taskloop_clause_seq
                              ;
master_taskloop_simd_clause_optseq : /*empty*/
                                   | master_taskloop_simd_clause_seq
                                   ;
parallel_master_taskloop_clause_optseq : /*empty*/
                                       | parallel_master_taskloop_clause_seq
                                       ;
parallel_master_taskloop_simd_clause_optseq : /*empty*/
                                            | parallel_master_taskloop_simd_clause_seq
                                            ;
loop_clause_optseq : /*empty*/
                   | loop_clause_seq
                   ;
scan_clause_optseq : scan_clause_seq
                   ;
sections_clause_optseq : /*empty*/
                       | sections_clause_seq
                       ;
sections_paired_clause_optseq : /*empty*/
                              | nowait_clause
                              ;
single_clause_optseq : /*empty*/
                     | single_clause_seq
                     ;
single_paired_clause_optseq : /*empty*/
                            | single_paired_clause_seq
                            ;
workshare_paired_clause_optseq : /*empty*/
                               | nowait_clause
                               ;
cancel_clause_optseq : /*empty*/
                     | cancel_clause_seq
                     ;
//cancel_clause_fortran_optseq : /*empty*/
//                             | cancel_clause_fortran_seq
//                             ;
cancellation_point_clause_optseq : /*empty*/
                                 | cancellation_point_clause_seq
                                 ;
//cancellation_point_clause_fortran_optseq : /*empty*/
//                                         | cancellation_point_clause_fortran_seq
                                         ;
allocate_clause_optseq : /*empty*/
                       | allocate_clause_seq
                       ;
allocate_clause_seq : allocate_directive_clause
                    | allocate_clause_seq allocate_directive_clause
                    | allocate_clause_seq ',' allocate_directive_clause
                    ; 



declare_reduction_postfix : /* empty */
                          | combiner_clause declare_reduction_initializer_opt
                          | initializer_clause
                          ;
declare_reduction_initializer_opt : /* empty */
                                  | initializer_clause
                                  ;
declare_mapper_clause_optseq : /*empty*/
                             | declare_mapper_clause_seq
                             ;
declare_mapper_clause_seq : declare_mapper_clause
                          | declare_mapper_clause_seq declare_mapper_clause
                          | declare_mapper_clause_seq ',' { clause_separator_comma = true; } declare_mapper_clause
                          ; 
parallel_clause_seq : parallel_clause
                    | parallel_clause_seq parallel_clause
                    | parallel_clause_seq ',' { clause_separator_comma = true; } parallel_clause
                    ;

teams_clause_seq : teams_clause
                 | teams_clause_seq teams_clause
                 | teams_clause_seq ',' { clause_separator_comma = true; } teams_clause
                 ;

for_clause_seq : for_clause
               | for_clause_seq for_clause
               | for_clause_seq ',' { clause_separator_comma = true; } for_clause
               ;

do_clause_seq : do_clause
              | do_clause_seq do_clause
              | do_clause_seq ',' { clause_separator_comma = true; } do_clause
              ;

simd_clause_seq : simd_clause
                | simd_clause_seq simd_clause
                | simd_clause_seq ',' { clause_separator_comma = true; } simd_clause
                ;

for_simd_clause_seq : for_simd_clause
                    | for_simd_clause_seq for_simd_clause
                    | for_simd_clause_seq ',' { clause_separator_comma = true; } for_simd_clause
                    ;
do_simd_clause_seq : do_simd_clause
                   | do_simd_clause_seq do_simd_clause
                   | do_simd_clause_seq ',' { clause_separator_comma = true; } do_simd_clause
                   ;
parallel_for_simd_clause_seq : parallel_for_simd_clause
                             | parallel_for_simd_clause_seq parallel_for_simd_clause
                             | parallel_for_simd_clause_seq ',' { clause_separator_comma = true; } parallel_for_simd_clause
                             ;
declare_simd_clause_seq : declare_simd_clause
                        | declare_simd_clause_seq declare_simd_clause
                        | declare_simd_clause_seq ',' { clause_separator_comma = true; } declare_simd_clause
                        ;
distribute_clause_seq : distribute_clause
                      | distribute_clause_seq distribute_clause
                      | distribute_clause_seq ',' { clause_separator_comma = true; } distribute_clause
                      ;
distribute_simd_clause_seq : distribute_simd_clause
                           | distribute_simd_clause_seq distribute_simd_clause
                           | distribute_simd_clause_seq ',' { clause_separator_comma = true; } distribute_simd_clause
                           ;
distribute_parallel_for_clause_seq : distribute_parallel_for_clause
                                   | distribute_parallel_for_clause_seq distribute_parallel_for_clause
                                   | distribute_parallel_for_clause_seq ',' { clause_separator_comma = true; } distribute_parallel_for_clause
                                   ;
distribute_parallel_do_clause_seq : distribute_parallel_do_clause
                                  | distribute_parallel_do_clause_seq distribute_parallel_do_clause
                                  | distribute_parallel_do_clause_seq ',' { clause_separator_comma = true; } distribute_parallel_do_clause
                                  ;
distribute_parallel_for_simd_clause_seq : distribute_parallel_for_simd_clause
                                        | distribute_parallel_for_simd_clause_seq distribute_parallel_for_simd_clause
                                        | distribute_parallel_for_simd_clause_seq ',' { clause_separator_comma = true; } distribute_parallel_for_simd_clause
                                        ;
distribute_parallel_do_simd_clause_seq : distribute_parallel_do_simd_clause
                                       | distribute_parallel_do_simd_clause_seq distribute_parallel_do_simd_clause
                                       | distribute_parallel_do_simd_clause_seq ',' { clause_separator_comma = true; } distribute_parallel_do_simd_clause
                                       ;
parallel_for_clause_seq : parallel_for_clause
                        | parallel_for_clause_seq parallel_for_clause
                        | parallel_for_clause_seq ',' { clause_separator_comma = true; } parallel_for_clause
                        ;
parallel_do_clause_seq : parallel_do_clause
                       | parallel_do_clause_seq parallel_do_clause
                       | parallel_do_clause_seq ',' { clause_separator_comma = true; } parallel_do_clause
                       ;
parallel_loop_clause_seq : parallel_loop_clause
                         | parallel_loop_clause_seq parallel_loop_clause
                         | parallel_loop_clause_seq ',' { clause_separator_comma = true; } parallel_loop_clause
                         ;
parallel_loop_simd_clause_seq : parallel_loop_simd_clause
                              | parallel_loop_simd_clause_seq parallel_loop_simd_clause
                              | parallel_loop_simd_clause_seq ',' { clause_separator_comma = true; } parallel_loop_simd_clause
                              ;
parallel_sections_clause_seq : parallel_sections_clause
                             | parallel_sections_clause_seq parallel_sections_clause
                             | parallel_sections_clause_seq ',' { clause_separator_comma = true; } parallel_sections_clause
                             ;
parallel_single_clause_seq : parallel_single_clause
                           | parallel_single_clause_seq parallel_single_clause
                           | parallel_single_clause_seq ',' { clause_separator_comma = true; } parallel_single_clause
                           ;
parallel_single_clause : parallel_only_clause
                       | single_only_clause
                       | parallel_single_common_clause
                       ;
parallel_only_clause : if_parallel_clause
                     | num_threads_clause
                     | default_clause
                     | shared_clause
                     | copyin_clause
                     | reduction_clause
                     | proc_bind_clause
                     | message_clause
                     | severity_clause
                     | safesync_clause
                     ;
single_only_clause : fortran_copyprivate_clause
                   | fortran_nowait_clause
                   ;
parallel_single_common_clause : private_clause
                              | firstprivate_clause
                              | allocate_clause
                              ;
parallel_workshare_clause_seq : parallel_workshare_clause
                               | parallel_workshare_clause_seq parallel_workshare_clause
                               | parallel_workshare_clause_seq ',' { clause_separator_comma = true; } parallel_workshare_clause
                              ;
parallel_master_clause_seq : parallel_master_clause
                           | parallel_master_clause_seq parallel_master_clause
                           | parallel_master_clause_seq ',' { clause_separator_comma = true; } parallel_master_clause
                           ;
master_taskloop_clause_seq : master_taskloop_clause
                           | master_taskloop_clause_seq master_taskloop_clause
                           | master_taskloop_clause_seq ',' { clause_separator_comma = true; } master_taskloop_clause
                           ;
master_taskloop_simd_clause_seq : master_taskloop_simd_clause
                                | master_taskloop_simd_clause_seq master_taskloop_simd_clause
                                | master_taskloop_simd_clause_seq ',' { clause_separator_comma = true; } master_taskloop_simd_clause
                                ;
parallel_master_taskloop_clause_seq : parallel_master_taskloop_clause
                                    | parallel_master_taskloop_clause_seq parallel_master_taskloop_clause
                                    | parallel_master_taskloop_clause_seq ',' { clause_separator_comma = true; } parallel_master_taskloop_clause
                                    ;
parallel_master_taskloop_simd_clause_seq : parallel_master_taskloop_simd_clause
                                         | parallel_master_taskloop_simd_clause_seq parallel_master_taskloop_simd_clause
                                         | parallel_master_taskloop_simd_clause_seq ',' { clause_separator_comma = true; } parallel_master_taskloop_simd_clause
                                         ;
loop_clause_seq : loop_clause
                | loop_clause_seq loop_clause
                | loop_clause_seq ',' { clause_separator_comma = true; } loop_clause
                ;
scan_clause_seq : scan_clause
                ;
sections_clause_seq : sections_clause
                    | sections_clause_seq sections_clause
                    | sections_clause_seq ',' { clause_separator_comma = true; } sections_clause
                    ;
//sections_clause_fortran_seq : sections_fortran_clause
//                            | sections_clause_fortran_seq sections_fortran_clause
//                            | sections_clause_fortran_seq ',' sections_fortran_clause
//                            ;
single_clause_seq : single_clause
                  | single_clause_seq single_clause
                  | single_clause_seq ',' { clause_separator_comma = true; } single_clause
                  ;
single_paired_clause_seq : single_paired_clause
                         | single_paired_clause_seq single_paired_clause
                         | single_paired_clause_seq ',' { clause_separator_comma = true; } single_paired_clause
                         ;
cancel_clause_seq : construct_type_clause
                  | construct_type_clause if_cancel_clause
                  | construct_type_clause ',' { clause_separator_comma = true; } if_cancel_clause
                  ;
//cancel_clause_fortran_seq : construct_type_clause_fortran
//                          | construct_type_clause_fortran if_cancel_clause
//                          | construct_type_clause_fortran ',' if_cancel_clause
//                          ;
cancellation_point_clause_seq : construct_type_clause
                              ;
//cancellation_point_clause_fortran_seq : construct_type_clause_fortran
//                                      ;
allocate_directive_clause : allocator_clause
                          | align_clause
                          ;
declare_mapper_clause : map_clause
                      ;
parallel_clause : if_parallel_clause
                | num_threads_clause
                | default_clause
                | private_clause
                | firstprivate_clause
                | shared_clause
                | copyin_clause
                | reduction_clause
                | proc_bind_clause
                | allocate_clause
                | message_clause
                | severity_clause
                | safesync_clause
                ;
teams_clause : if_teams_clause
             | num_teams_clause
             | thread_limit_clause
             | default_clause
             | private_clause
             | firstprivate_clause
             | shared_clause
             | reduction_default_only_clause
             | allocate_clause
             ;

for_clause : private_clause
           | firstprivate_clause
           | lastprivate_clause
           | linear_clause
           | reduction_clause
           | schedule_clause
           | collapse_clause
           | ordered_clause
           | nowait_clause
           | allocate_clause
           | order_clause
           | induction_clause
           ;

do_clause : private_clause
          | firstprivate_clause
          | lastprivate_clause
          | linear_clause
          | reduction_clause
          | schedule_clause
          | collapse_clause
          | ordered_clause
          | allocate_clause
          | order_clause
          | induction_clause
          ;

simd_clause : if_simd_clause
            | safelen_clause
            | simdlen_clause
            | linear_clause
            | aligned_clause
            | nontemporal_clause
            | private_clause
            | lastprivate_clause
            | reduction_clause
            | collapse_clause
            | order_clause
            ;

for_simd_clause : if_simd_clause
                | safelen_clause
                | simdlen_clause
                | linear_clause
                | aligned_clause
                | private_clause
                | firstprivate_clause 
                | lastprivate_clause
                | reduction_clause
                | schedule_clause
                | collapse_clause
                | ordered_clause
                | nowait_clause
                | allocate_clause
                | order_clause
                | nontemporal_clause
                ;
do_simd_clause : if_simd_clause
               | safelen_clause
               | simdlen_clause
               | linear_clause
               | aligned_clause
               | private_clause 
               | firstprivate_clause 
               | lastprivate_clause
               | reduction_clause
               | schedule_clause
               | collapse_clause
               | ordered_clause
               | allocate_clause
               | order_clause
               | nontemporal_clause
               ;
parallel_for_simd_clause : if_parallel_simd_clause
                         | num_threads_clause
                         | default_clause
                         | private_clause
                         | firstprivate_clause
                         | shared_clause
                         | copyin_clause
                         | reduction_clause
                         | proc_bind_clause
                         | allocate_clause
                         | lastprivate_clause 
                         | linear_clause
                         | schedule_clause
                         | collapse_clause
                         | ordered_clause
                         | order_clause
                         | safelen_clause
                         | simdlen_clause
                         | aligned_clause
                         | nontemporal_clause
                         | induction_clause
                         ;
 
declare_simd_clause : simdlen_clause
                    | linear_clause
                    | aligned_clause
                    | uniform_clause
                    | inbranch_clause
                    | notinbranch_clause
                    ;
 
distribute_clause : private_clause
                  | firstprivate_clause 
                  | lastprivate_distribute_clause
                  | collapse_clause
                  | dist_schedule_clause
                  | allocate_clause
                  ;
distribute_simd_clause : private_clause
                       | firstprivate_clause 
                       | lastprivate_clause
                       | collapse_clause
                       | dist_schedule_clause
                       | allocate_clause
                       | if_simd_clause
                       | safelen_clause
                       | simdlen_clause
                       | linear_clause
                       | aligned_clause
                       | nontemporal_clause
                       | reduction_clause
                       | order_clause
                       ;
distribute_parallel_for_clause : if_parallel_clause
                               | num_threads_clause
                               | default_clause
                               | private_clause
                               | firstprivate_clause
                               | shared_clause
                               | copyin_clause
                               | reduction_clause
                               | proc_bind_clause
                               | allocate_clause
                               | lastprivate_clause 
                               | linear_clause
                               | schedule_clause
                               | collapse_clause
                               | ordered_clause
                               | nowait_clause
                               | order_clause 
                               | dist_schedule_clause
                               ;
distribute_parallel_do_clause : if_parallel_clause
                              | num_threads_clause
                              | default_clause
                              | private_clause
                              | firstprivate_clause
                              | shared_clause
                              | copyin_clause
                              | reduction_clause
                              | proc_bind_clause
                              | allocate_clause
                              | lastprivate_clause 
                              | linear_clause
                              | schedule_clause
                              | collapse_clause
                              | ordered_clause
                              | order_clause 
                              | dist_schedule_clause
                              ;
distribute_parallel_for_simd_clause : if_parallel_simd_clause
                                    | num_threads_clause
                                    | default_clause
                                    | private_clause
                                    | firstprivate_clause
                                    | shared_clause
                                    | copyin_clause
                                    | reduction_clause
                                    | proc_bind_clause
                                    | allocate_clause
                                    | lastprivate_clause 
                                    | linear_clause
                                    | schedule_clause
                                    | collapse_clause
                                    | ordered_clause
                                    | nowait_clause
                                    | order_clause 
                                    | dist_schedule_clause
                                    | safelen_clause
                                    | simdlen_clause
                                    | aligned_clause
                                    | nontemporal_clause
                                    ;
distribute_parallel_do_simd_clause : if_parallel_simd_clause
                                   | num_threads_clause
                                   | default_clause
                                   | private_clause
                                   | firstprivate_clause
                                   | shared_clause
                                   | copyin_clause
                                   | reduction_clause
                                   | proc_bind_clause
                                   | allocate_clause
                                   | lastprivate_clause 
                                   | linear_clause
                                   | schedule_clause
                                   | collapse_clause
                                   | ordered_clause
                                   | order_clause 
                                   | dist_schedule_clause
                                   | safelen_clause
                                   | simdlen_clause
                                   | aligned_clause
                                   | nontemporal_clause
                                   ;
parallel_for_clause : if_parallel_clause
                    | num_threads_clause
                    | default_clause
                    | private_clause
                    | firstprivate_clause
                    | shared_clause
                    | copyin_clause
                    | reduction_clause
                    | proc_bind_clause
                    | allocate_clause
                    | lastprivate_clause 
                    | linear_clause
                    | schedule_clause
                    | collapse_clause
                    | ordered_clause
                    | nowait_clause
                    | order_clause 
                    | induction_clause
                    ;
parallel_do_clause : if_parallel_clause
                   | num_threads_clause
                   | default_clause
                   | private_clause
                   | firstprivate_clause
                   | shared_clause
                   | copyin_clause
                   | reduction_clause
                   | proc_bind_clause
                   | allocate_clause
                   | lastprivate_clause 
                   | linear_clause
                   | schedule_clause
                   | collapse_clause
                   | ordered_clause
                   | order_clause 
                   | induction_clause
                   ;
parallel_loop_clause : if_parallel_clause
                     | num_threads_clause
                     | default_clause
                     | private_clause
                     | firstprivate_clause
                     | shared_clause
                     | copyin_clause
                     | reduction_clause
                     | proc_bind_clause
                     | allocate_clause
                     | lastprivate_clause 
                     | collapse_clause
                     | bind_clause
                     | order_clause 
                     | induction_clause
                     ;
parallel_loop_simd_clause : if_parallel_simd_clause
                         | num_threads_clause
                         | default_clause
                         | private_clause
                         | firstprivate_clause
                         | shared_clause
                         | copyin_clause
                         | reduction_clause
                         | proc_bind_clause
                         | allocate_clause
                         | lastprivate_clause
                         | collapse_clause
                         | bind_clause
                         | order_clause
                         | induction_clause
                         | safelen_clause
                         | simdlen_clause
                         | linear_clause
                         | aligned_clause
                         | nontemporal_clause
                         ;
parallel_sections_clause : if_parallel_clause
                         | num_threads_clause
                         | default_clause
                         | private_clause
                         | firstprivate_clause
                         | shared_clause
                         | copyin_clause
                         | reduction_clause
                         | proc_bind_clause
                         | allocate_clause
                         | lastprivate_clause 
                         ;
parallel_workshare_clause : if_parallel_clause
                          | num_threads_clause
                          | default_clause
                     | private_clause
                     | firstprivate_clause
                     | shared_clause
                     | copyin_clause
                     | reduction_clause
                     | proc_bind_clause
                     | allocate_clause
                     | induction_clause
                     ;
parallel_master_clause : if_parallel_clause
                       | num_threads_clause
                       | default_clause
                       | private_clause
                       | firstprivate_clause
                       | shared_clause
                       | copyin_clause
                       | reduction_clause
                       | proc_bind_clause
                       | allocate_clause
                       ;
master_taskloop_clause : if_taskloop_clause
                       | shared_clause
                       | private_clause
                       | firstprivate_clause
                       | lastprivate_clause
                       | reduction_clause
                       | in_reduction_clause
                       | default_clause
                       | grainsize_clause
                       | num_tasks_clause
                       | collapse_clause
                       | final_clause
                       | priority_clause
                       | untied_clause
                       | mergeable_clause
                       | nogroup_clause
                       | allocate_clause
                       ;
master_taskloop_simd_clause : if_taskloop_simd_clause
                            | shared_clause
                            | private_clause
                            | firstprivate_clause
                            | lastprivate_clause
                            | reduction_clause
                            | in_reduction_clause
                            | default_clause
                            | grainsize_clause
                            | num_tasks_clause
                            | collapse_clause
                            | final_clause
                            | priority_clause
                            | untied_clause
                            | mergeable_clause
                            | nogroup_clause
                            | allocate_clause
                            | safelen_clause
                            | simdlen_clause
                            | linear_clause
                            | aligned_clause
                            | nontemporal_clause
                            | order_clause 
                            ;
parallel_master_taskloop_clause : if_parallel_taskloop_clause
                                | num_threads_clause
                                | default_clause
                                | private_clause
                                | firstprivate_clause
                                | shared_clause
                                | copyin_clause
                                | reduction_clause
                                | proc_bind_clause
                                | allocate_clause
                                | lastprivate_clause 
                                | nowait_clause 
                                | grainsize_clause
                                | num_tasks_clause
                                | collapse_clause
                                | final_clause
                                | priority_clause
                                | untied_clause
                                | mergeable_clause
                                | nogroup_clause
                                ;
parallel_master_taskloop_simd_clause : if_parallel_taskloop_simd_clause
                                     | num_threads_clause
                                     | default_clause
                                     | private_clause
                                     | firstprivate_clause
                                     | shared_clause
                                     | copyin_clause
                                     | reduction_clause
                                     | proc_bind_clause
                                     | allocate_clause
                                     | lastprivate_clause 
                                     | nowait_clause 
                                     | grainsize_clause
                                     | num_tasks_clause
                                     | collapse_clause
                                     | final_clause
                                     | priority_clause
                                     | untied_clause
                                     | mergeable_clause
                                     | nogroup_clause
                                     | safelen_clause
                                     | simdlen_clause
                                     | linear_clause
                                     | aligned_clause
                                     | nontemporal_clause
                                     | order_clause
                                     ;
loop_clause : bind_clause
            | collapse_clause
            | order_clause
            | private_clause
            | lastprivate_clause
            | reduction_default_only_clause
            | induction_clause
            ;
scan_clause : inclusive_clause
            | exclusive_clause
            | init_complete_clause
            ;
sections_clause : private_clause
                | firstprivate_clause
                | lastprivate_clause
                | reduction_clause
                | allocate_clause
                | fortran_nowait_clause
                ;
single_clause : private_clause
              | firstprivate_clause
              | fortran_copyprivate_clause
              | allocate_clause
              | fortran_nowait_clause
              ;
single_paired_clause : copyprivate_clause
                     | nowait_clause
                     ;
construct_type_clause : PARALLEL { current_clause = current_directive->addOpenMPClause(OMPC_parallel); }
                      | SECTIONS { current_clause = current_directive->addOpenMPClause(OMPC_sections); }
                      | FOR { if (user_set_lang != Lang_Fortran || auto_lang != Lang_Fortran) {current_clause = current_directive->addOpenMPClause(OMPC_for);} else {yyerror("cancel or cancellation direcitve does not support for clause in Fortran"); YYABORT; } }
                      | DO { if (user_set_lang == Lang_Fortran || auto_lang == Lang_Fortran) {current_clause = current_directive->addOpenMPClause(OMPC_do);} else {yyerror("cancel or cancellation direcitve does not support DO clause in C"); YYABORT; } }
                      | TASKGROUP { current_clause = current_directive->addOpenMPClause(OMPC_taskgroup); }
                      ;
//construct_type_clause_fortran : PARALLEL { current_clause = current_directive->addOpenMPClause(OMPC_parallel); }
//                              | SECTIONS { current_clause = current_directive->addOpenMPClause(OMPC_sections); }
//                              | DO { current_clause = current_directive->addOpenMPClause(OMPC_do); }
//                              | TASKGROUP { current_clause = current_directive->addOpenMPClause(OMPC_taskgroup); }
//                              ;
if_parallel_clause : IF '(' if_parallel_parameter ')' { ; }
                   ;

if_parallel_parameter : PARALLEL ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_parallel); }
                        expression { ; }
                      | EXPR_STRING {
                        current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                        current_clause->addLangExpr($1);
                        }
                      ;
if_task_clause : IF '(' if_task_parameter ')' { ; }
               ;

if_task_parameter : TASK ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_task); } expression { ; }
                  | EXPR_STRING {
                        current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                        current_clause->addLangExpr($1);
                        }
                  ;
if_taskloop_clause : IF '(' if_taskloop_parameter ')' { ; }
                   ;

if_taskloop_parameter : TASKLOOP ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_taskloop); } expression { ; }
                      | EXPR_STRING {
                            current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                            current_clause->addLangExpr($1);
                        }
                      ;
if_teams_clause : IF '(' if_teams_parameter ')' { ; }
               ;
if_teams_parameter : TEAMS ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_teams); } expression { ; }
                   | EXPR_STRING {
                        current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                        current_clause->addLangExpr($1);
                     }
                   ;
if_task_iteration_clause : IF '(' if_task_iteration_parameter ')' { ; }
                        ;
if_task_iteration_parameter : TASK_ITERATION ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_task_iteration); } expression { ; }
                          | EXPR_STRING {
                              current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                              current_clause->addLangExpr($1);
                            }
                          ;
if_taskgraph_clause : IF '(' if_taskgraph_parameter ')' { ; }
                  ;
if_taskgraph_parameter : TASKGRAPH ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_taskgraph); } expression { ; }
                     | EXPR_STRING {
                         current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                         current_clause->addLangExpr($1);
                       }
                     ;
if_target_data_clause : IF '(' if_target_data_parameter ')' { ; }
                      ;

if_target_data_parameter : TARGET DATA ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target_data); } expression { ; }
                         | EXPR_STRING {
                               current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                               current_clause->addLangExpr($1);
                           } 
                         ;
if_target_parallel_clause : IF '(' if_target_parallel_parameter ')' { ; }
                          ;

if_target_parallel_parameter : TARGET ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target); } expression { ; }
                             | PARALLEL ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_parallel); } expression { ; }
                             | EXPR_STRING {
                               current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                               current_clause->addLangExpr($1);
                           } 
                             ;
if_target_simd_clause : IF '(' if_target_simd_parameter ')' { ; }
                      ;

if_target_simd_parameter : TARGET ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target); } expression { ; }
                         | SIMD ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_simd); } expression { ; }
                         | EXPR_STRING {
                               current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                               current_clause->addLangExpr($1);
                           } 
                             ;
if_target_enter_data_clause : IF '(' if_target_enter_data_parameter ')' { ; }
                            ;

if_target_enter_data_parameter : TARGET ENTER DATA ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target_enter_data); } expression { ; }
                               | EXPR_STRING {
                                     current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                                     current_clause->addLangExpr($1);
                                 }
                               ;
if_target_exit_data_clause : IF '(' if_target_exit_data_parameter ')' { ; }
                           ;

if_target_exit_data_parameter : TARGET EXIT DATA ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target_exit_data); } expression { ; }
                              | EXPR_STRING {
                                    current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                                    current_clause->addLangExpr($1);
                                }
                              ;
if_target_clause : IF '(' if_target_parameter ')' { ; }
                 ;

if_target_parameter : TARGET ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target); } expression { ; }
                    | EXPR_STRING {
                          current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                          current_clause->addLangExpr($1);
                      }
                    ;
if_target_update_clause : IF '(' if_target_update_parameter ')' { ; }
                        ;

if_target_update_parameter : TARGET UPDATE ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target_update); } expression { ; }
                           | EXPR_STRING {
                                 current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                                 current_clause->addLangExpr($1);
                             }
                           ;
if_taskloop_simd_clause : IF '(' if_taskloop_simd_parameter ')' { ; }
                        ;

if_taskloop_simd_parameter : TASKLOOP ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_taskloop); } expression { ; }
                           | SIMD ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_simd); } expression { ; }
                           | EXPR_STRING {
                                 current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                                 current_clause->addLangExpr($1);
                             }
                           ;
if_simd_clause : IF '(' if_simd_parameter ')' { ; }
               ;
if_simd_parameter : SIMD ':' {current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_simd);} expression { ; }
                  | EXPR_STRING {
                        current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                        current_clause->addLangExpr($1);
                        }
                  ;
if_parallel_simd_clause : IF '(' if_parallel_simd_parameter ')' { ; }
                        ;
if_parallel_simd_parameter : SIMD ':' {current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_simd);} expression { ; }
                           | PARALLEL ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_parallel); } expression { ; }
                           | EXPR_STRING {
                                current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                                current_clause->addLangExpr($1);
                           }
                           ;
if_target_parallel_simd_clause : IF '(' if_target_parallel_simd_parameter ')' { ; }
                               ;
if_target_parallel_simd_parameter : SIMD ':' {current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_simd);} expression { ; }
                                  | PARALLEL ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_parallel); } expression { ; }
                                  | TARGET ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_target); } expression { ; }
                                  | EXPR_STRING {
                                current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                                current_clause->addLangExpr($1);
                           }
                                  ;
if_cancel_clause : IF '(' if_cancel_parameter ')' { ; }
                 ;
if_cancel_parameter : CANCEL ':' {current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_cancel);} expression { ; }
                    | EXPR_STRING {
                        current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                        current_clause->addLangExpr($1);
                        }
                    ;
if_parallel_taskloop_clause : IF '(' if_parallel_taskloop_parameter ')' { ; }
                            ;
if_parallel_taskloop_parameter : PARALLEL ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_parallel); } expression { ; }
                               | TASKLOOP ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_taskloop); } expression { ; }
                               | EXPR_STRING {
                               current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                               current_clause->addLangExpr($1);
                                }
                               ;
if_parallel_taskloop_simd_clause : IF '(' if_parallel_taskloop_simd_parameter ')' { ; }
                                 ;
if_parallel_taskloop_simd_parameter : PARALLEL ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_parallel); } expression { ; }
                                    | TASKLOOP ':' { current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_taskloop); } expression { ; }
                                    | SIMD ':' {current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_simd);} expression { ; }
                                    | EXPR_STRING {
                                      current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                                      current_clause->addLangExpr($1);
                                    }
                                    ;
/*if_clause : IF '(' if_parameter ')' { ; }
          ;

if_parameter : EXPR_STRING {
                current_clause = current_directive->addOpenMPClause(OMPC_if, OMPC_IF_MODIFIER_unspecified);
                current_clause->addLangExpr($1);
                }
             ;
*/
num_threads_clause: NUM_THREADS '(' num_threads_parameter ')'
                  ;
num_threads_parameter : {
                            current_clause = current_directive->addOpenMPClause(OMPC_num_threads);
                         } expression num_threads_optional_tail
                      | {
                            current_clause = current_directive->addOpenMPClause(OMPC_num_threads);
                        } { if (current_clause) { dynamic_cast<OpenMPNumThreadsClause*>(current_clause)->setStrict(true); } } STRICT ':' expression num_threads_optional_tail
                      ;
num_threads_optional_tail : /* empty */
                          | ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } expression
                          ;
num_teams_clause: NUM_TEAMS {
                            current_clause = current_directive->addOpenMPClause(OMPC_num_teams);
                         } '(' expression ')'
                ;
align_clause: ALIGN {
                  current_clause = current_directive->addOpenMPClause(OMPC_align);
                  } '(' expression ')'
            ;
                
thread_limit_clause: THREAD_LIMIT { current_clause = current_directive->addOpenMPClause(OMPC_thread_limit); } '(' expression ')'
                   ;
memscope_clause : MEMSCOPE {
                  current_clause =
                      current_directive->addOpenMPClause(OMPC_memscope);
                } '(' memscope_kind ')'
               ;

memscope_kind : DEVICE {
                 auto *memscope_clause =
                     static_cast<OpenMPMemscopeClause *>(current_clause);
                 if (memscope_clause != nullptr) {
                   memscope_clause->setScope(OMPC_MEMSCOPE_device);
                 }
               }
              | CGROUP {
                 auto *memscope_clause =
                     static_cast<OpenMPMemscopeClause *>(current_clause);
                 if (memscope_clause != nullptr) {
                   memscope_clause->setScope(OMPC_MEMSCOPE_cgroup);
                 }
               }
              | ALL {
                 auto *memscope_clause =
                     static_cast<OpenMPMemscopeClause *>(current_clause);
                 if (memscope_clause != nullptr) {
                   memscope_clause->setScope(OMPC_MEMSCOPE_all);
                 }
               }
              ;

device_safesync_clause : DEVICE_SAFESYNC { current_clause = current_directive->addOpenMPClause(OMPC_device_safesync); } opt_device_safesync_parens
                       ;

opt_device_safesync_parens : /* empty */
                           | '(' device_safesync_arg ')'
                           ;

device_safesync_arg : EXPR_STRING { if (current_clause) current_clause->addLangExpr($1); }
                    ;

safesync_clause: SAFESYNC { current_clause = current_directive->addOpenMPClause(OMPC_safesync); } '(' expression ')'
               ;
copyin_clause: COPYIN {
                current_clause = current_directive->addOpenMPClause(OMPC_copyin);
                } '(' var_list ')'
             ;

default_clause : DEFAULT '(' default_parameter ')' { } 
               ;

default_parameter : SHARED { current_clause = current_directive->addOpenMPClause(OMPC_default, OMPC_DEFAULT_shared); }
                  | NONE { current_clause = current_directive->addOpenMPClause(OMPC_default, OMPC_DEFAULT_none); }
                  | FIRSTPRIVATE { current_clause = current_directive->addOpenMPClause(OMPC_default, OMPC_DEFAULT_firstprivate); }
                  | PRIVATE { current_clause = current_directive->addOpenMPClause(OMPC_default, OMPC_DEFAULT_private); }
                  ;

default_variant_clause : DEFAULT '(' default_variant_directive ')' { }
                       ;

default_variant_directive : { current_clause = current_directive->addOpenMPClause(OMPC_default, OMPC_DEFAULT_variant);
                            current_parent_directive = current_directive;
                            current_parent_clause = current_clause; } variant_directive {
                            ((OpenMPDefaultClause*)current_parent_clause)->setVariantDirective(current_directive);
                            current_directive = current_parent_directive;
                            current_clause = current_parent_clause;
                            current_parent_directive = nullptr;
                            current_parent_clause = nullptr;
                            }
                          ;

proc_bind_clause : PROC_BIND '(' proc_bind_parameter ')' { } ;

proc_bind_parameter : MASTER { current_clause = current_directive->addOpenMPClause(OMPC_proc_bind, OMPC_PROC_BIND_master); }
                    | PRIMARY { current_clause = current_directive->addOpenMPClause(OMPC_proc_bind, OMPC_PROC_BIND_primary); }
                    | CLOSE { current_clause = current_directive->addOpenMPClause(OMPC_proc_bind, OMPC_PROC_BIND_close); }
                    | SPREAD { current_clause = current_directive->addOpenMPClause(OMPC_proc_bind, OMPC_PROC_BIND_spread); }
                    ;
bind_clause : BIND '(' bind_parameter ')' { } ;

bind_parameter : TEAMS { current_clause = current_directive->addOpenMPClause(OMPC_bind, OMPC_BIND_teams); }
               | PARALLEL { current_clause = current_directive->addOpenMPClause(OMPC_bind, OMPC_BIND_parallel); }
               | THREAD { current_clause = current_directive->addOpenMPClause(OMPC_bind, OMPC_BIND_thread); }
               ;
allocate_clause : ALLOCATE {
                    current_clause = nullptr;
                  } '(' allocate_parameter ')' ;

allocate_parameter : allocator_parameter_list ':' var_list
                   | allocate_parameter_no_allocator
                   ;

allocate_parameter_no_allocator : {
                                    if (current_clause == nullptr) {
                                      current_clause = current_directive->addOpenMPClause(
                                          OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_unspecified);
                                    }
                                  } var_list
                                ;
allocator_parameter_list : allocator_parameter
                         | allocator_parameter_list ',' allocator_parameter
                         ;
allocator_parameter : DEFAULT_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_default); }
                    | LARGE_CAP_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_large_cap); }
                    | CONST_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_cons_mem); }
                    | HIGH_BW_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_high_bw); }
                    | LOW_LAT_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_low_lat); }
                    | CGROUP_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_cgroup); }
                    | PTEAM_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_pteam); }
                    | THREAD_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_thread); }
                    | ALLOCATOR_IDENTIFIER {
                        if (current_clause == nullptr) {
                          current_clause = current_directive->addOpenMPClause(OMPC_allocate, OMPC_ALLOCATE_ALLOCATOR_user, $1);
                        }
                        if (current_clause) {
                          ((OpenMPAllocateClause*)current_clause)->setUserDefinedAllocator(const_cast<char*>($1));
                        }
                      }
                    ;

private_clause : PRIVATE {
                current_clause = current_directive->addOpenMPClause(OMPC_private);
                if (!current_clause->getExpressions()->empty()) {
                  current_expr_separator = OMPC_CLAUSE_SEP_comma;
                } else {
                  current_expr_separator = OMPC_CLAUSE_SEP_space;
                }
                    } '(' var_list ')' { 
               }
               ;

firstprivate_clause : FIRSTPRIVATE {
                         current_clause = current_directive->addOpenMPClause(OMPC_firstprivate);
                         if (!current_clause->getExpressions()->empty()) {
                           current_expr_separator = OMPC_CLAUSE_SEP_comma;
                         } else {
                           current_expr_separator = OMPC_CLAUSE_SEP_space;
                         }
                        } '(' firstprivate_parameter ')' {
                    }
                    ;
firstprivate_parameter : {
                          auto *firstprivate_clause =
                              dynamic_cast<OpenMPFirstprivateClause *>(current_clause);
                          if (firstprivate_clause != nullptr) {
                            firstprivate_clause->setSaved(false);
                          }
                        } var_list
                      | SAVED ':' {
                          auto *firstprivate_clause =
                              dynamic_cast<OpenMPFirstprivateClause *>(current_clause);
                          if (firstprivate_clause != nullptr) {
                            firstprivate_clause->setSaved();
                          }
                        } var_list
                      ;

copyprivate_clause : COPYPRIVATE {
                           current_clause = current_directive->addOpenMPClause(OMPC_copyprivate);
                        } '(' var_list ')' {
                   }
                   ;
fortran_copyprivate_clause : COPYPRIVATE {
                                 if (user_set_lang == Lang_C || auto_lang == Lang_C) {current_clause = current_directive->addOpenMPClause(OMPC_copyprivate);} else {yyerror("Single does not support copyprivate_clause in Fortran."); YYABORT;}
                               } '(' var_list ')' {
                           }
                           ;
lastprivate_clause : LASTPRIVATE '(' lastprivate_parameter ')' ;

lastprivate_parameter : EXPR_STRING { current_clause = current_directive->addOpenMPClause(OMPC_lastprivate, OMPC_LASTPRIVATE_MODIFIER_unspecified); current_clause->addLangExpr($1); }
                      | EXPR_STRING ',' { current_clause = current_directive->addOpenMPClause(OMPC_lastprivate, OMPC_LASTPRIVATE_MODIFIER_unspecified); current_expr_separator = OMPC_CLAUSE_SEP_comma; current_clause->addLangExpr($1, OMPC_CLAUSE_SEP_space); } var_list
                      | lastprivate_modifier ':'{;} var_list
                      ;

lastprivate_distribute_clause : LASTPRIVATE {
                         current_clause = current_directive->addOpenMPClause(OMPC_lastprivate, OMPC_LASTPRIVATE_MODIFIER_unspecified);
                        } '(' var_list ')' {
                    }

lastprivate_modifier : MODIFIER_CONDITIONAL { current_clause = current_directive->addOpenMPClause(OMPC_lastprivate,OMPC_LASTPRIVATE_MODIFIER_conditional); }
                     ;

linear_clause : LINEAR '(' linear_parameter ')'
              | LINEAR '(' linear_parameter ':' EXPR_STRING { ((OpenMPLinearClause*)current_clause)->setUserDefinedStep($5); ((OpenMPLinearClause*)current_clause)->mergeLinear(current_directive, current_clause); } ')'
              | LINEAR '(' linear_parameter ':' linear_modifier_kind { ((OpenMPLinearClause*)current_clause)->mergeLinear(current_directive, current_clause); } ')'
              | LINEAR '(' linear_parameter ':' linear_modifier_kind ',' EXPR_STRING { ((OpenMPLinearClause*)current_clause)->setUserDefinedStep($7); ((OpenMPLinearClause*)current_clause)->mergeLinear(current_directive, current_clause); } ')'
              ;

linear_parameter : EXPR_STRING  { current_clause = current_directive->addOpenMPClause(OMPC_linear, OMPC_LINEAR_MODIFIER_unspecified);
                                  if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; }
                                  current_clause->addLangExpr($1, current_expr_separator);
                                  current_expr_separator = OMPC_CLAUSE_SEP_space;
                                }
                 | EXPR_STRING ',' { current_clause = current_directive->addOpenMPClause(OMPC_linear, OMPC_LINEAR_MODIFIER_unspecified);
                                      if (!current_clause->getExpressions()->empty()) { current_expr_separator = OMPC_CLAUSE_SEP_comma; } else { current_expr_separator = OMPC_CLAUSE_SEP_space; }
                                      current_clause->addLangExpr($1, current_expr_separator);
                                      current_expr_separator = OMPC_CLAUSE_SEP_comma; } var_list
                 | linear_modifier '(' var_list ')' { ((OpenMPLinearClause*)current_clause)->setModifierFirstSyntax(true); }
                 ;
linear_modifier : MODOFIER_VAL { current_clause = current_directive->addOpenMPClause(OMPC_linear,OMPC_LINEAR_MODIFIER_val); }
                | MODOFIER_REF { if (user_set_lang == Lang_unknown && auto_lang == Lang_C){ auto_lang = Lang_Cplusplus; } if (user_set_lang == Lang_C) {yyerror("REF modifier is not supportted in C."); YYABORT; } else { current_clause = current_directive->addOpenMPClause(OMPC_linear, OMPC_LINEAR_MODIFIER_ref); } }
                | MODOFIER_UVAL { if (user_set_lang == Lang_unknown && auto_lang == Lang_C){ auto_lang = Lang_Cplusplus; } if (user_set_lang == Lang_C) {yyerror("UVAL modifier is not supportted in C."); YYABORT;} else { current_clause = current_directive->addOpenMPClause(OMPC_linear, OMPC_LINEAR_MODIFIER_uval); } }
                ;
linear_modifier_kind : MODOFIER_VAL { ((OpenMPLinearClause*)current_clause)->setModifier(OMPC_LINEAR_MODIFIER_val); }
                     | MODOFIER_REF { if (user_set_lang == Lang_unknown && auto_lang == Lang_C){ auto_lang = Lang_Cplusplus; } if (user_set_lang == Lang_C) {yyerror("REF modifier is not supportted in C."); YYABORT; } else { ((OpenMPLinearClause*)current_clause)->setModifier(OMPC_LINEAR_MODIFIER_ref); } }
                     | MODOFIER_UVAL { if (user_set_lang == Lang_unknown && auto_lang == Lang_C){ auto_lang = Lang_Cplusplus; } if (user_set_lang == Lang_C) {yyerror("UVAL modifier is not supportted in C."); YYABORT;} else { ((OpenMPLinearClause*)current_clause)->setModifier(OMPC_LINEAR_MODIFIER_uval); } }
                     ;

aligned_clause : ALIGNED '(' aligned_parameter ')'
               | ALIGNED '(' aligned_parameter ':' EXPR_STRING { ((OpenMPAlignedClause*)current_clause)->setUserDefinedAlignment($5);} ')'
               ;


aligned_parameter : EXPR_STRING { current_clause = current_directive->addOpenMPClause(OMPC_aligned); current_clause->addLangExpr($1);  }
                  | EXPR_STRING ',' {current_clause = current_directive->addOpenMPClause(OMPC_aligned); current_expr_separator = OMPC_CLAUSE_SEP_comma; current_clause->addLangExpr($1, OMPC_CLAUSE_SEP_space); } var_list
                  ;

initializer_clause : INITIALIZER '(' {
                       openmp_begin_raw_expression();
                     } EXPR_STRING ')' {
                       current_clause = current_directive->addOpenMPClause(OMPC_initializer, OMPC_INITIALIZER_PRIV_user, $4);
                       if (current_clause != nullptr) {
                         current_clause->addLangExpr($4);
                       }
                     }
                   ;

safelen_clause: SAFELEN { current_clause = current_directive->addOpenMPClause(OMPC_safelen); } '(' var_list ')' {
                        }
              ;

simdlen_clause: SIMDLEN { current_clause = current_directive->addOpenMPClause(OMPC_simdlen); } '(' var_list ')' {
                        }
              ;

nontemporal_clause: NONTEMPORAL { current_clause = current_directive->addOpenMPClause(OMPC_nontemporal); } '(' var_list ')' {
                        }
                      ;

collapse_clause: COLLAPSE { current_clause = current_directive->addOpenMPClause(OMPC_collapse);
                            if (!current_clause->getExpressions()->empty()) {
                              current_expr_separator = OMPC_CLAUSE_SEP_comma;
                            } else {
                              current_expr_separator = OMPC_CLAUSE_SEP_space;
                            }
                          } '(' expression ')' {
                        }
               ;

ordered_clause: ORDERED { current_clause = current_directive->addOpenMPClause(OMPC_ordered); } '(' expression ')'
              | ORDERED { current_clause = current_directive->addOpenMPClause(OMPC_ordered); }
              ;
partial_clause: PARTIAL { current_clause = current_directive->addOpenMPClause(OMPC_partial); } '(' expression ')'
              | PARTIAL { current_clause = current_directive->addOpenMPClause(OMPC_partial); }
              ;
fortran_nowait_clause: NOWAIT { if (user_set_lang == Lang_C || auto_lang == Lang_C) {current_clause = current_directive->addOpenMPClause(OMPC_nowait);} else {yyerror("Sections does not support nowait clause in Fortran."); YYABORT;} }
                     ;
nowait_clause: NOWAIT { 
                         OpenMPDirective *target_directive = current_directive;
                         if (current_parent_directive != nullptr &&
                             current_parent_directive->getKind() == OMPD_end) {
                           target_directive = current_parent_directive;
                         }
                         current_clause = target_directive->addOpenMPClause(OMPC_nowait);
                       }
             | NOWAIT { 
                         OpenMPDirective *target_directive = current_directive;
                         if (current_parent_directive != nullptr &&
                             current_parent_directive->getKind() == OMPD_end) {
                           target_directive = current_parent_directive;
                         }
                         current_clause = target_directive->addOpenMPClause(OMPC_nowait);
                       } '(' expression ')'
             ;
indirect_clause: INDIRECT { current_clause = current_directive->addOpenMPClause(OMPC_indirect); }
               ;
full_clause: FULL { current_clause = current_directive->addOpenMPClause(OMPC_full); }
             ;
order_clause: ORDER '(' order_parameter ')' { }
            ;

order_parameter : order_modifier ':' CONCURRENT { current_clause = OpenMPOrderClause::addOrderClause(current_directive, (OpenMPOrderClauseModifier)firstParameter, OMPC_ORDER_concurrent); }
                | CONCURRENT { current_clause = OpenMPOrderClause::addOrderClause(current_directive, OMPC_ORDER_concurrent); }
                | order_modifier ':' CONCURRENT ':' var_list {
                    current_clause = OpenMPOrderClause::addOrderClause(current_directive, (OpenMPOrderClauseModifier)firstParameter, OMPC_ORDER_concurrent);
                    auto *ord_clause = static_cast<OpenMPOrderClause *>(current_clause);
                    ord_clause->clearOperands();
                    current_expr_separator = OMPC_CLAUSE_SEP_comma;
                  }
                | CONCURRENT ':' var_list {
                    current_clause = OpenMPOrderClause::addOrderClause(current_directive, OMPC_ORDER_concurrent);
                    auto *ord_clause = static_cast<OpenMPOrderClause *>(current_clause);
                    ord_clause->clearOperands();
                    current_expr_separator = OMPC_CLAUSE_SEP_comma;
                  }
                ;

order_modifier : REPRODUCIBLE { firstParameter = OMPC_ORDER_MODIFIER_reproducible; }
               | UNCONSTRAINED { firstParameter = OMPC_ORDER_MODIFIER_unconstrained; }
               ;

uniform_clause: UNIFORM { current_clause = current_directive->addOpenMPClause(OMPC_uniform); } '(' var_list ')'
              ;

inbranch_clause: INBRANCH { current_clause = current_directive->addOpenMPClause(OMPC_inbranch); }
               ;

notinbranch_clause: NOTINBRANCH { current_clause = current_directive->addOpenMPClause(OMPC_notinbranch); }
                  ;
inclusive_clause: INCLUSIVE { current_clause = current_directive->addOpenMPClause(OMPC_inclusive); } '(' var_list ')'
                ;
exclusive_clause: EXCLUSIVE { current_clause = current_directive->addOpenMPClause(OMPC_exclusive); } '(' var_list ')'
                ;
allocator_clause: ALLOCATOR '(' allocator1_parameter ')';
allocator1_parameter : DEFAULT_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_default); }
                     | LARGE_CAP_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_large_cap); }
                     | CONST_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_cons_mem); }
                     | HIGH_BW_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_high_bw); }
                     | LOW_LAT_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_low_lat); }
                     | CGROUP_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_cgroup); }
                     | PTEAM_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_pteam); }
                     | THREAD_MEM_ALLOC { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_thread); }
                     | EXPR_STRING { current_clause = current_directive->addOpenMPClause(OMPC_allocator, OMPC_ALLOCATOR_ALLOCATOR_user, $1); }
                     ;

dist_schedule_clause : DIST_SCHEDULE '(' dist_schedule_parameter ')' {}
                     ;
dist_schedule_parameter : STATIC { current_clause = current_directive->addOpenMPClause(OMPC_dist_schedule,OMPC_DIST_SCHEDULE_KIND_static); }
                        | STATIC { current_clause = current_directive->addOpenMPClause(OMPC_dist_schedule,OMPC_DIST_SCHEDULE_KIND_static); } ',' EXPR_STRING { ((OpenMPDistScheduleClause*)current_clause)->setChunkSize($4); }
                        ;
schedule_clause : SCHEDULE { firstParameter = OMPC_SCHEDULE_MODIFIER_unspecified; secondParameter = OMPC_SCHEDULE_MODIFIER_unspecified; }'(' schedule_parameter ')' {
                }
                ;

schedule_parameter : schedule_kind {}
                   | schedule_modifier ':' schedule_kind
                   ;


schedule_kind : schedule_enum_kind { }
              | schedule_enum_kind ','  EXPR_STRING { ((OpenMPScheduleClause*)current_clause)->setChunkSize($3); }
              ;

schedule_modifier : schedule_enum_modifier ',' schedule_modifier2
                  | schedule_enum_modifier
                  ;

schedule_modifier2 : MODIFIER_MONOTONIC { if (firstParameter == OMPC_SCHEDULE_MODIFIER_simd) {secondParameter = OMPC_SCHEDULE_MODIFIER_monotonic;} else{yyerror("Two modifiers are incorrect"); YYABORT; } }
                   | MODIFIER_NONMONOTONIC { if (firstParameter == OMPC_SCHEDULE_MODIFIER_simd){secondParameter = OMPC_SCHEDULE_MODIFIER_nonmonotonic;}else{yyerror("Two modifiers are incorrect"); YYABORT; } }
                   | MODIFIER_SIMD { if (firstParameter == OMPC_SCHEDULE_MODIFIER_simd){yyerror("Two modifiers are incorrect"); YYABORT; } else{secondParameter = OMPC_SCHEDULE_MODIFIER_simd;} }
                   ;
schedule_enum_modifier : MODIFIER_MONOTONIC { firstParameter = OMPC_SCHEDULE_MODIFIER_monotonic; }
                       | MODIFIER_NONMONOTONIC { firstParameter = OMPC_SCHEDULE_MODIFIER_nonmonotonic; }
                       | MODIFIER_SIMD { firstParameter = OMPC_SCHEDULE_MODIFIER_simd; }
                       ;

schedule_enum_kind : STATIC { if (current_directive != nullptr) current_clause = current_directive->addOpenMPClause(OMPC_schedule, firstParameter, secondParameter, OMPC_SCHEDULE_KIND_static); }
                   | DYNAMIC { if (current_directive != nullptr) current_clause = current_directive->addOpenMPClause(OMPC_schedule, firstParameter, secondParameter, OMPC_SCHEDULE_KIND_dynamic); }
                   | GUIDED { if (current_directive != nullptr) current_clause = current_directive->addOpenMPClause(OMPC_schedule, firstParameter, secondParameter, OMPC_SCHEDULE_KIND_guided); }
                   | AUTO { if (current_directive != nullptr) current_clause = current_directive->addOpenMPClause(OMPC_schedule, firstParameter, secondParameter, OMPC_SCHEDULE_KIND_auto); }
                   | RUNTIME { if (current_directive != nullptr) current_clause = current_directive->addOpenMPClause(OMPC_schedule, firstParameter, secondParameter, OMPC_SCHEDULE_KIND_runtime); }
                   ;  
shared_clause : SHARED {
                current_clause = current_directive->addOpenMPClause(OMPC_shared);
                if (!current_clause->getExpressions()->empty()) {
                  current_expr_separator = OMPC_CLAUSE_SEP_comma;
                } else {
                  current_expr_separator = OMPC_CLAUSE_SEP_space;
                }
                    } '(' var_list ')'
              ;

reduction_clause : REDUCTION { firstParameter = OMPC_REDUCTION_MODIFIER_unspecified;
                               reduction_modifier_expression = nullptr;
                             } '(' reduction_parameter ':' reduction_var_list ')' {
                               reduction_modifier_expression = nullptr;
                             }
                 ;

reduction_parameter : reduction_identifier {}
                    | reduction_modifier ',' reduction_identifier
                    ;

reduction_identifier : reduction_enum_identifier {}
                     | EXPR_STRING {
                         current_clause = current_directive->addOpenMPClause(
                             OMPC_reduction, firstParameter,
                             OMPC_REDUCTION_IDENTIFIER_user,
                             reduction_modifier_expression, $1);
                         reduction_modifier_expression = nullptr;
                       }
                     ;

reduction_modifier : MODIFIER_INSCAN { firstParameter = OMPC_REDUCTION_MODIFIER_inscan; }
                   | MODIFIER_TASK { firstParameter = OMPC_REDUCTION_MODIFIER_task; }
                   | MODIFIER_DEFAULT { firstParameter = OMPC_REDUCTION_MODIFIER_default; }
                   | reduction_user_modifier
                   ;

reduction_var_list : reduction_var
                   | reduction_var_list ',' { current_expr_separator = OMPC_CLAUSE_SEP_comma; } reduction_var
                   ;

reduction_var : EXPR_STRING {
                  auto *red_clause = static_cast<OpenMPReductionClause *>(current_clause);
                  red_clause->addOperand($1, current_expr_separator);
                  current_expr_separator = OMPC_CLAUSE_SEP_space;
                }
               ;

reduction_enum_identifier : '+'{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_plus,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | '-'{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_minus,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | '*'{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_mul,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | '&'{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_bitand,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | '|'{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_bitor,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | '^'{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_bitxor,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | LOGAND{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_logand,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | LOGOR{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_logor,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | MAX{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_max,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          | MIN{
                              current_clause = current_directive->addOpenMPClause(
                                  OMPC_reduction, firstParameter,
                                  OMPC_REDUCTION_IDENTIFIER_min,
                                  reduction_modifier_expression, (char *)nullptr);
                              reduction_modifier_expression = nullptr;
                            }
                          ;

reduction_user_modifier : EXPR_STRING {
                             reduction_modifier_expression = $1;
                             firstParameter = OMPC_REDUCTION_MODIFIER_unknown;
                           }
                         ;

reduction_default_only_clause : REDUCTION { firstParameter = OMPC_REDUCTION_MODIFIER_unspecified; } '(' reduction_default_only_parameter ':' var_list ')' {
                              }
                              ;

reduction_default_only_parameter : reduction_identifier {}
                                 | reduction_default_only_modifier ',' reduction_identifier
                                 ;

reduction_default_only_modifier : MODIFIER_DEFAULT { firstParameter = OMPC_REDUCTION_MODIFIER_default; }
                                ;

%%

int yyerror(const char *s) {
    // printf(" %s!\n", s);
    fprintf(stderr,"error: %s\n",s);
    current_directive = nullptr;
    return 0;
}
 
int yywrap()
{
    return 1;
} 

// Standalone ompparser
OpenMPDirective* parseOpenMP(const char* _input, void * _exprParse(const char*)) {
    OpenMPBaseLang base_lang = Lang_C;
    current_directive = nullptr;
    std::string input_string;  // Must persist until after start_lexer()
    const char *input = _input;
    current_pragma_raw = (_input != nullptr) ? std::string(_input) : "";
    std::regex fortran_regex ("[!cC*][$][Oo][Mm][Pp]");

    if (_input == nullptr) {
        yyerror("Null input provided to parseOpenMP.");
        return nullptr;
    }

    // Check input length to avoid undefined behavior
    size_t input_len = std::strlen(input);
    size_t check_len = (input_len < 5) ? input_len : 5;
    std::string prefix_check(input, check_len);
    
    if (user_set_lang == Lang_unknown){
        auto_lang = Lang_C;
        exprParse = _exprParse;
        if (std::regex_search(prefix_check, fortran_regex)) {
            base_lang = Lang_Fortran;
            auto_lang = Lang_Fortran;
            input_string = std::string(input);
            std::transform(input_string.begin(), input_string.end(), input_string.begin(), ::tolower);
            input = input_string.c_str();
        }
    } else {
        base_lang = user_set_lang;
        exprParse = _exprParse;
        /* Ensure auto_lang reflects the explicitly set language to avoid
           stale auto-detection state from previous parses. */
        auto_lang = user_set_lang;
        if (std::regex_search(prefix_check, fortran_regex)) {
            /* Input appears to be Fortran-style (e.g. starts with !$OMP or !C$OMP)
               Normalize to lowercase so the lexer rules that expect lowercase
               "omp" match correctly, then check for language mismatch. */
            input_string = std::string(input);
            std::transform(input_string.begin(), input_string.end(), input_string.begin(), ::tolower);
            input = input_string.c_str();
            if (user_set_lang != Lang_Fortran){
                yyerror("The language is set to C/C++, but the input is Fortran.");
                return nullptr;
            }
        } else {
            if (user_set_lang == Lang_Fortran){
                yyerror("The language is set to Fortran, but the input is C/C++.");
                return nullptr;
            }
        }
    }
    openmp_reset_lexer_flags();
    start_lexer(input);
    yyparse();
    end_lexer();
    if (current_directive) {
        current_directive->setBaseLang(base_lang);
    }
    return current_directive;
}
