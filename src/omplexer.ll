/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

%option prefix="openmp_"
%option stack
%option

%x AFFINITY_STATE
%x ALIGNED_STATE
%x ALLOCATE_EXPR_STATE
%x ALLOCATE_STATE
%x ALLOCATOR_CALL_STATE
%x ALLOCATOR_STATE
%x ALLOC_EXPR_STATE
%x ARCH_STATE
%x ATOMIC_DEFAULT_MEM_ORDER_STATE
%x BIND_STATE
%x COLLAPSE_STATE
%x SIZES_STATE
%x GRAINSIZE_STATE
%x NUM_TASKS_STATE
%x LOOPRANGE_STATE
%x INIT_STATE
%x INTEROP_CLAUSE_STATE
%x APPLY_STATE
%x CANCEL_STATE
%x RAW_EXPR_STATE
%x ADJUST_ARGS_STATE
%x INDUCTION_STATE
%x CONDITION_STATE
%x COPYIN_STATE
%x COPYPRIVATE_STATE
%x DEFAULTMAP_STATE
%x DEFAULT_STATE
%x DEPEND_STATE
%x DOACROSS_STATE
%x ENTER_STATE
%x THREAD_LIMIT_STATE
%x DEVICE_STATE
%x DEVICE_TYPE_STATE
%x DIST_SCHEDULE_STATE
%x EXPR_STATE
%x EXTENSION_STATE
%x FINAL_STATE
%x FIRSTPRIVATE_STATE
%x FROM_MAPPER_STATE
%x FROM_STATE
%x ID_EXPR_STATE
%x IF_STATE
%x IMPLEMENTATION_STATE
%x INITIALIZER_STATE
%x IN_REDUCTION_STATE
%x ITERATOR_CAPTURE_STATE
%x NOCONTEXT_STATE
%x NOVARIANTS_STATE
%x ISA_STATE
%x LASTPRIVATE_STATE
%x LINEAR_STATE
%x MAPPER_STATE
%x MAP_MAPPER_STATE
%x MAP_STATE
%x MAP_VAR_STATE
%x MATCH_STATE
%x NONTEMPORAL_STATE
%x NUM_TEAMS_STATE
%x NUM_THREADS_STATE
%x ORDERED_STATE
%x ORDER_STATE
%x PARTIAL_STATE
%x PRIVATE_STATE
%x PROC_BIND_STATE
%x REDUCTION_STATE
%x SAFELEN_STATE
%x SCHEDULE_STATE
%x SCORE_STATE
%x SHARED_STATE
%x SIMDLEN_STATE
%x SIMD_STATE
%x TASK_REDUCTION_STATE
%x THREADPRIVATE_STATE
%x TO_MAPPER_STATE
%x TO_STATE
%x TYPE_STR_STATE
%x UPDATE_STATE
%x USES_ALLOCATORS_STATE
%x VENDOR_STATE
%x WHEN_STATE

%{

/* lex requires me to use extern "C" here */
extern "C" int openmp_wrap() { return 1; }

extern int openmp_lex();

#include "ompparser.hh"
#include "OpenMPIR.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern void openmpSetExprParseMode(OpenMPExprParseMode mode);

/* Moved from Makefile.am to the source file to work with --with-pch Liao
   12/10/2009 */
#define YY_NO_POP_STATE

/* Forward declaration - actual token values will be available after parser header */
static inline int emit_expr_string_and_unput(char ch);
static inline int emit_expr_string_no_unput();

static const char *ompparserinput = nullptr;
static std::string current_string;
static std::string original_input_string;
static int parenthesis_local_count = 0;
static int parenthesis_global_count = 1;
static int bracket_count;
static int brace_count = 0;
static int ternary_count = 0;
static bool inside_quotes = false;
static char raw_expression_quote = '\0';
static char current_char;

static bool compact_parallel_do = false;
static bool declare_target_underscore = false;
static bool compact_enddo = false;
static bool implementation_selector_parenthesis_pending = false;

static std::vector<int> apply_paren_depth;
static int induction_spec_paren_depth = 0;
static bool induction_step_waiting = false;  // True when expecting expression after step(
static int if_paren_depth = 0;
static int uses_allocators_paren_depth = 0;
static int allocate_modifier_token = 0;
static std::vector<std::unique_ptr<char[]>> lexeme_storage;

struct AllocateExpressionCaptureState {
  std::vector<char> delimiters;
  char quote = '\0';
  bool escaped = false;
  int ternary_depth = 0;
};

static AllocateExpressionCaptureState allocate_expression_capture;
static AllocateExpressionCaptureState allocate_modifier_capture;

struct LexerLocationState {
  int line = 1;
  int column = 1;
  std::size_t offset = 0;
  std::size_t token_begin_offset = 0;
  int last_token_line = 0;
  int last_token_column = 0;
  bool tracking_enabled = false;
};

static LexerLocationState lexer_location_state;
static constexpr std::size_t kMaxLexerPositionHistory = 64;
static std::array<std::pair<int, int>, kMaxLexerPositionHistory>
    lexer_position_history;
static std::size_t lexer_position_history_begin = 0;
static std::size_t lexer_position_history_size = 0;

static inline void reset_lexer_location_state() {
  lexer_location_state.line = 1;
  lexer_location_state.column = 1;
  lexer_location_state.offset = 0;
  lexer_location_state.token_begin_offset = 0;
  lexer_location_state.last_token_line = 0;
  lexer_location_state.last_token_column = 0;
  lexer_position_history_begin = 0;
  lexer_position_history_size = 0;
  openmp_lloc.first_line = 1;
  openmp_lloc.first_column = 1;
  openmp_lloc.last_line = 1;
  openmp_lloc.last_column = 1;
}

static inline void record_lexer_position_before_advance() {
  if (!lexer_location_state.tracking_enabled) {
    return;
  }

  const std::pair<int, int> position(lexer_location_state.line,
                                     lexer_location_state.column);
  if (lexer_position_history_size < kMaxLexerPositionHistory) {
    const std::size_t index =
        (lexer_position_history_begin + lexer_position_history_size) %
        kMaxLexerPositionHistory;
    lexer_position_history[index] = position;
    ++lexer_position_history_size;
    return;
  }

  lexer_position_history[lexer_position_history_begin] = position;
  lexer_position_history_begin =
      (lexer_position_history_begin + 1) % kMaxLexerPositionHistory;
}

static inline bool pop_lexer_position_history(std::pair<int, int> *position) {
  if (position == nullptr || lexer_position_history_size == 0) {
    return false;
  }

  const std::size_t index =
      (lexer_position_history_begin + lexer_position_history_size - 1) %
      kMaxLexerPositionHistory;
  *position = lexer_position_history[index];
  --lexer_position_history_size;
  if (lexer_position_history_size == 0) {
    lexer_position_history_begin = 0;
  }
  return true;
}

static inline void advance_lexer_position(char ch) {
  lexer_location_state.offset++;
  if (ch == '\n') {
    lexer_location_state.line++;
    lexer_location_state.column = 1;
    return;
  }
  lexer_location_state.column++;
}

static inline void rewind_lexer_position_for_unput(char ch) {
  if (!lexer_location_state.tracking_enabled) {
    return;
  }

  if (lexer_location_state.offset == 0) {
    std::fprintf(stderr,
                 "REX_OMPPARSER_INVARIANT[lexer-offset]: cannot rewind "
                 "before the start of the directive\n");
    std::abort();
  }
  lexer_location_state.offset--;

  std::pair<int, int> previous_position;
  if (pop_lexer_position_history(&previous_position)) {
    lexer_location_state.line = previous_position.first;
    lexer_location_state.column = previous_position.second;
    return;
  }

  if (ch == '\n') {
    if (lexer_location_state.line > 1) {
      lexer_location_state.line--;
      lexer_location_state.column = 1;
    }
    return;
  }

  if (lexer_location_state.column > 1) {
    lexer_location_state.column--;
  }
}

static inline void update_token_location(const char *text, size_t length) {
  if (!lexer_location_state.tracking_enabled || text == nullptr || length == 0) {
    return;
  }

  const int first_line = lexer_location_state.line;
  const int first_column = lexer_location_state.column;
  lexer_location_state.token_begin_offset = lexer_location_state.offset;

  int last_line = first_line;
  int last_column = first_column;
  for (size_t index = 0; index < length; ++index) {
    last_line = lexer_location_state.line;
    last_column = lexer_location_state.column;
    record_lexer_position_before_advance();
    advance_lexer_position(text[index]);
  }

  openmp_lloc.first_line = first_line;
  openmp_lloc.first_column = first_column;
  openmp_lloc.last_line = last_line;
  openmp_lloc.last_column = last_column;
  lexer_location_state.last_token_line = first_line;
  lexer_location_state.last_token_column = first_column;
}

int openmpGetCurrentTokenLine() {
  if (!lexer_location_state.tracking_enabled) {
    return 0;
  }
  return lexer_location_state.last_token_line;
}

int openmpGetCurrentTokenColumn() {
  if (!lexer_location_state.tracking_enabled) {
    return 0;
  }
  return lexer_location_state.last_token_column;
}

static inline int tracked_yyinput();
static inline void tracked_unput(int ch);

#define YY_USER_ACTION update_token_location(yytext, static_cast<size_t>(yyleng));

#define TRACKED_YYLESS(count)                                                   \
  do {                                                                          \
    const int tracked_count = (count);                                          \
    if (tracked_count < 0 || tracked_count > yyleng) {                          \
      std::fprintf(stderr,                                                      \
                   "REX_OMPPARSER_INVARIANT[lexer-offset]: invalid yyless "    \
                   "length\n");                                                \
      std::abort();                                                             \
    }                                                                           \
    for (int tracked_index = yyleng; tracked_index > tracked_count;             \
         --tracked_index) {                                                     \
      rewind_lexer_position_for_unput(yytext[tracked_index - 1]);               \
    }                                                                           \
    yyless(tracked_count);                                                      \
  } while (0)

static std::string original_token_text(std::size_t length) {
  const std::size_t begin = lexer_location_state.token_begin_offset;
  if (begin > original_input_string.size() ||
      length > original_input_string.size() - begin) {
    std::fprintf(stderr,
                 "REX_OMPPARSER_INVARIANT[lexer-source]: token range exceeds "
                 "the original directive\n");
    std::abort();
  }
  return original_input_string.substr(begin, length);
}

static char original_token_char(std::size_t index = 0) {
  const std::string token = original_token_text(static_cast<std::size_t>(yyleng));
  if (index >= token.size()) {
    std::fprintf(stderr,
                 "REX_OMPPARSER_INVARIANT[lexer-source]: token character "
                 "index is out of range\n");
    std::abort();
  }
  return token[index];
}

static const char *store_lexeme(const std::string &text) {
  auto buffer = std::make_unique<char[]>(text.size() + 1);
  std::memcpy(buffer.get(), text.c_str(), text.size() + 1);
  const char *ptr = buffer.get();
  lexeme_storage.push_back(std::move(buffer));
  return ptr;
}

static inline void push_apply_state();
static inline void pop_apply_state();
static inline int &current_apply_paren_depth();
static inline void begin_allocate_expression_capture(char initial_char);
static inline int consume_allocate_expression_char(char ch);
static inline void begin_allocate_modifier_capture(int modifier_token);
static inline int consume_allocate_modifier_char(char ch);
[[noreturn]] static void fail_allocate_capture(const char *kind,
                                               const char *detail);

extern "C" void openmp_reset_lexer_flags() {
  compact_parallel_do = false;
  declare_target_underscore = false;
  compact_enddo = false;
  implementation_selector_parenthesis_pending = false;
  apply_paren_depth.clear();
  induction_spec_paren_depth = 0;
  induction_step_waiting = false;
  if_paren_depth = 0;
  uses_allocators_paren_depth = 0;
  allocate_modifier_token = 0;
  allocate_expression_capture = {};
  allocate_modifier_capture = {};
}

extern "C" bool openmp_consume_compact_parallel_do() {
  bool value = compact_parallel_do;
  compact_parallel_do = false;
  return value;
}

extern "C" bool openmp_consume_declare_target_underscore() {
  bool value = declare_target_underscore;
  declare_target_underscore = false;
  return value;
}

extern "C" bool openmp_consume_compact_enddo() {
  bool value = compact_enddo;
  compact_enddo = false;
  return value;
}

/* Helper functions for expression state management */
static inline void reset_expression_counters() {
  parenthesis_local_count = 0;
  parenthesis_global_count = 1;
  bracket_count = 0;
  brace_count = 0;
  ternary_count = 0;
  inside_quotes = false;
  raw_expression_quote = '\0';
}

static inline void clear_expression_buffer() {
  current_string.clear();
  reset_expression_counters();
}

static inline void prepare_expression_capture() {
  clear_expression_buffer();
}

static inline void prepare_expression_capture(char initial_char) {
  clear_expression_buffer();
  const char source_char = original_token_char();
  current_string.push_back(source_char);
  if (source_char == '"') {
    inside_quotes = true;
  }
  (void)initial_char;
}

static inline void prepare_expression_capture_str(const char* initial_str) {
  clear_expression_buffer();
  current_string = original_token_text(static_cast<std::size_t>(yyleng));
  (void)initial_str;
}

static inline bool is_firstprivate_blank(char ch) {
  return ch == ' ';
}

static inline bool is_firstprivate_identifier_char(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

static inline void skip_firstprivate_blanks(const std::string &text,
                                            std::size_t &pos) {
  while (pos < text.size() && is_firstprivate_blank(text[pos])) {
    ++pos;
  }
}

static bool consume_firstprivate_modifier_word(const std::string &text,
                                               std::size_t &pos,
                                               const char *word) {
  const std::size_t start = pos;
  for (std::size_t index = 0; word[index] != '\0'; ++index) {
    if (pos + index >= text.size() || text[pos + index] != word[index]) {
      pos = start;
      return false;
    }
  }
  pos += std::strlen(word);
  if (pos < text.size() && is_firstprivate_identifier_char(text[pos])) {
    pos = start;
    return false;
  }
  return true;
}

static bool consume_firstprivate_modifier(const std::string &text,
                                          std::size_t &pos) {
  const std::size_t start = pos;
  if (consume_firstprivate_modifier_word(text, pos, "target")) {
    std::size_t after_target = pos;
    skip_firstprivate_blanks(text, after_target);
    std::size_t after_data = after_target;
    if (consume_firstprivate_modifier_word(text, after_data, "data")) {
      pos = after_data;
      return true;
    }
    return true;
  }
  pos = start;

  static const char *const modifiers[] = {
      "distribute", "parallel", "sections", "single", "taskloop",
      "task",       "teams",    "scope",    "saved",  "for",
      "do"};
  for (const char *modifier : modifiers) {
    if (consume_firstprivate_modifier_word(text, pos, modifier)) {
      return true;
    }
  }

  return false;
}

static bool firstprivate_modifier_list_follows_current_token() {
  std::vector<int> peeked_chars;
  std::string lookahead;
  for (std::size_t index = 0; index < 256; ++index) {
    const int ch = tracked_yyinput();
    if (ch == EOF) {
      break;
    }
    peeked_chars.push_back(ch);
    lookahead.push_back(static_cast<char>(ch));
    if (ch == ':' || ch == ')' || ch == '\n') {
      break;
    }
  }

  for (auto it = peeked_chars.rbegin(); it != peeked_chars.rend(); ++it) {
    tracked_unput(*it);
  }

  std::size_t pos = 0;
  skip_firstprivate_blanks(lookahead, pos);
  if (pos >= lookahead.size()) {
    return false;
  }
  if (lookahead[pos] == ':') {
    return true;
  }
  if (lookahead[pos] != ',') {
    return false;
  }

  while (pos < lookahead.size() && lookahead[pos] == ',') {
    ++pos;
    skip_firstprivate_blanks(lookahead, pos);
    if (!consume_firstprivate_modifier(lookahead, pos)) {
      return false;
    }
    skip_firstprivate_blanks(lookahead, pos);
    if (pos < lookahead.size() && lookahead[pos] == ':') {
      return true;
    }
    if (pos >= lookahead.size() || lookahead[pos] != ',') {
      return false;
    }
  }

  return false;
}

#define RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(Token)                            \
  do {                                                                         \
    const std::string firstprivate_token_text(yytext, yyleng);                 \
    if (firstprivate_modifier_list_follows_current_token()) {                  \
      return Token;                                                            \
    }                                                                          \
    yy_push_state(EXPR_STATE);                                                 \
    prepare_expression_capture_str(firstprivate_token_text.c_str());           \
  } while (0)

#define REWIND_LINEAR_DELIMITER(delim)                                             \
  do {                                                                            \
    int keep_len = yyleng;                                                        \
    while (keep_len > 0 &&                                                        \
           std::isspace(static_cast<unsigned char>(yytext[keep_len - 1]))) {      \
      keep_len--;                                                                 \
    }                                                                             \
    if (keep_len > 0 && yytext[keep_len - 1] == (delim)) {                        \
      keep_len--;                                                                 \
      while (keep_len > 0 &&                                                      \
             std::isspace(static_cast<unsigned char>(yytext[keep_len - 1]))) {    \
        keep_len--;                                                               \
      }                                                                           \
      TRACKED_YYLESS(keep_len);                                                   \
    }                                                                             \
  } while (0)

/* Liao 6/11/2010, OpenMP does not preclude the use of clause names as regular
   variable names. For example, num_threads could be a clause name or a
   variable in the variable list.

   We introduce a flag to indicate the context: within a variable list like
   (a,y,y) or outside of it We check '(' or ')' to set it to true or false as
   parsing proceed */
extern bool b_within_variable_list; /* = false; */

/* pass user specified string to buf, indicate the size using 'result', and
   shift the current position pointer of user input afterwards to prepare next
   round of token recognition!! */
#define YY_INPUT(buf, result, max_size)                                            \
  {                                                                                \
    if (ompparserinput == nullptr || *ompparserinput == '\0') {                   \
      if (buf != nullptr && (max_size) > 0) {                                     \
        buf[0] = '\0';                                                            \
      }                                                                            \
      result = 0;                                                                  \
    } else {                                                                       \
      const size_t remaining = std::strlen(ompparserinput);                        \
      const size_t to_copy = std::min(remaining, static_cast<size_t>(max_size));  \
      std::memcpy(buf, ompparserinput, to_copy);                                   \
      if (to_copy < static_cast<size_t>(max_size)) {                              \
        buf[to_copy] = '\0';                                                      \
      }                                                                            \
      result = static_cast<int>(to_copy);                                          \
      ompparserinput += to_copy;                                                   \
    }                                                                              \
  }

%}

blank           [ ]
newline         [\n]
comment         [\/\/].*
id_char         [a-zA-Z0-9_]
identifier      [a-zA-Z_][a-zA-Z0-9_]*

%%

[!c*]$ompx      { return OMPX; }
[!c*]$omp       { ; }
#pragma         { ; }
omp/{blank}     { ; }
paralleldo      { compact_parallel_do = true; TRACKED_YYLESS(8); return PARALLEL; }
parallel        { compact_parallel_do = false; return PARALLEL; }
metadirective   { return METADIRECTIVE; }
task            { return TASK; }
if              { if_paren_depth = 0; yy_push_state(IF_STATE); return IF; }
simdlen         { yy_push_state(SIMDLEN_STATE); return SIMDLEN; }
simd/{blank}*\( { yy_push_state(SIMD_STATE); return SIMD; }
simd            { return SIMD; }
num_threads     { yy_push_state(NUM_THREADS_STATE); return NUM_THREADS; }
num_teams       { yy_push_state(NUM_TEAMS_STATE); return NUM_TEAMS; }
thread_limit            { yy_push_state(THREAD_LIMIT_STATE); return THREAD_LIMIT; }
default         { yy_push_state(DEFAULT_STATE); return DEFAULT; }
private         { yy_push_state(PRIVATE_STATE); return PRIVATE; }
firstprivate    { yy_push_state(FIRSTPRIVATE_STATE); return FIRSTPRIVATE; }
shared          { yy_push_state(SHARED_STATE); return SHARED; }
none            { return NONE; }
reduction       { yy_push_state(REDUCTION_STATE); return REDUCTION; }
copyin          { yy_push_state(COPYIN_STATE); return COPYIN; }
proc_bind       { yy_push_state(PROC_BIND_STATE); return PROC_BIND; }
allocate        { yy_push_state(ALLOCATE_STATE); return ALLOCATE; }
close           { return CLOSE; }
spread          { return SPREAD; } /* master should already be recognized */
teams           { return TEAMS; }
master          { return MASTER; } /*YAYING */
for             { return FOR; }
do              {
                  if (b_within_variable_list) {
                    yy_push_state(EXPR_STATE);
                    prepare_expression_capture_str("do");
                  } else {
                    return DO;
                  }
                }
lastprivate     { yy_push_state(LASTPRIVATE_STATE); return LASTPRIVATE; }
linear          { yy_push_state(LINEAR_STATE); return LINEAR; }
schedule        { yy_push_state(SCHEDULE_STATE); return SCHEDULE; }
collapse        { yy_push_state(COLLAPSE_STATE); return COLLAPSE; }
ordered/{blank}*\( { yy_push_state(ORDERED_STATE); return ORDERED; }
ordered         { return ORDERED; }
partial/{blank}*\( { yy_push_state(PARTIAL_STATE); return PARTIAL; }
partial         { return PARTIAL; }
nowait          { return NOWAIT; }
full            { return FULL; }
order           { yy_push_state(ORDER_STATE); return ORDER; }
safelen         { yy_push_state(SAFELEN_STATE); return SAFELEN; }
nontemporal     { yy_push_state(NONTEMPORAL_STATE); return NONTEMPORAL; }
aligned         { yy_push_state(ALIGNED_STATE); return ALIGNED; }
align           { return ALIGN; }
declare_target  { declare_target_underscore = true; TRACKED_YYLESS(7); return DECLARE; }
"_target"       { return TARGET; }
declare         { declare_target_underscore = false; return DECLARE; }
uniform         { return UNIFORM; }
inbranch        { return INBRANCH; }
notinbranch     { return NOTINBRANCH; }
distribute      { return DISTRIBUTE; }
dist_schedule   { yy_push_state(DIST_SCHEDULE_STATE); return DIST_SCHEDULE; }
loop            { return LOOP; }
bind            { yy_push_state(BIND_STATE); return BIND; }
scan            { return SCAN; }
inclusive       { return INCLUSIVE; }
exclusive       { return EXCLUSIVE; }
sections        { return SECTIONS; }
section         { return SECTION; }
single          { return SINGLE; }
copyprivate     { yy_push_state(COPYPRIVATE_STATE); return COPYPRIVATE; }
cancel          { return CANCEL; }
workshare       { return WORKSHARE; }
taskgroup       { return TASKGROUP; }
allocator       { yy_push_state(ALLOCATOR_STATE); return ALLOCATOR; }
threadprivate/{blank}*\( { yy_push_state(THREADPRIVATE_STATE); return THREADPRIVATE; }
threadprivate   { return THREADPRIVATE; }
cancellation    { yy_push_state(CANCEL_STATE); return CANCELLATION; }
<CANCEL_STATE>{blank}+                 { ; }
<CANCEL_STATE>{newline}+               { ; }
<CANCEL_STATE>point                    { yy_pop_state(); return POINT; }
<CANCEL_STATE>.                        { yy_pop_state(); tracked_unput(yytext[0]); }
variant         { return VARIANT; }
when            { yy_push_state(WHEN_STATE); return WHEN; }
match           { yy_push_state(MATCH_STATE); return MATCH; }
initializer     { yy_push_state(INITIALIZER_STATE); return INITIALIZER; }
mapper          { yy_push_state(MAPPER_STATE); return MAPPER; }
unroll          { return UNROLL;}
tile            { return TILE;}

error           { return ERROR_DIR; }
nothing         { return NOTHING; }
masked          { return MASKED; }
scope           { return SCOPE; }
interop/{blank}*\(  { yy_push_state(INTEROP_CLAUSE_STATE); return INTEROP; }
interop         { return INTEROP; }

assume          { return ASSUME; }
assumes         { return ASSUMES; }
begin           { return BEGIN_DIR; }

allocators      { return ALLOCATORS; }
taskgraph       { return TASKGRAPH; }
task_iteration  { return TASK_ITERATION; }
dispatch        { return DISPATCH; }
groupprivate    { return GROUPPRIVATE; }
workdistribute  { return WORKDISTRIBUTE; }
fuse            { return FUSE; }
interchange     { return INTERCHANGE; }
reverse         { return REVERSE; }
split           { return SPLIT; }
stripe          { return STRIPE; }
induction/{blank}      {
                  induction_spec_paren_depth = 0;
                  yy_push_state(INDUCTION_STATE);
                  return INDUCTION;
                }
induction/"("          {
                  induction_spec_paren_depth = 0;
                  yy_push_state(INDUCTION_STATE);
                  return INDUCTION;
                }

enddo           { compact_enddo = true; TRACKED_YYLESS(3); return END; }
end             { compact_enddo = false; return END; }
score           { return SCORE; }
condition       { yy_push_state(CONDITION_STATE); return CONDITION; }
kind            { return KIND; }
device_num/"("  { return DEVICE_NUM; }
host{id_char}+  { yy_push_state(EXPR_STATE); prepare_expression_capture_str(yytext); }
host/{blank}    { return HOST; }
host/"("        { return HOST; }
host/","        { return HOST; }
host/")"        { return HOST; }
host/":"        { return HOST; }
nohost          { return NOHOST; }
any             { return ANY; }
cpu             { return CPU; }
gpu             { return GPU; }
fpga            { return FPGA; }
isa             { yy_push_state(ISA_STATE); return ISA; }
arch            { yy_push_state(ARCH_STATE); return ARCH; }
uid             { return UID; }
vendor          { yy_push_state(VENDOR_STATE); return VENDOR; }
extension       { yy_push_state(EXTENSION_STATE); return EXTENSION; }

final           { yy_push_state(FINAL_STATE); return FINAL; }
untied          { return UNTIED; }
mergeable       { return MERGEABLE; }
in_reduction    { yy_push_state(IN_REDUCTION_STATE); return IN_REDUCTION; }
depend          { yy_push_state(DEPEND_STATE); return DEPEND; }
doacross        { yy_push_state(DOACROSS_STATE); return DOACROSS; }
priority        { return PRIORITY; }
affinity        { yy_push_state(AFFINITY_STATE); return AFFINITY; }
detach          { return DETACH; }

taskloop        { return TASKLOOP; }
taskyield       { return TASKYIELD; }
grainsize/{blank}       { yy_push_state(GRAINSIZE_STATE); return GRAINSIZE; }
grainsize/"("           { yy_push_state(GRAINSIZE_STATE); return GRAINSIZE; }
num_tasks/{blank}       { yy_push_state(NUM_TASKS_STATE); return NUM_TASKS; }
num_tasks/"("           { yy_push_state(NUM_TASKS_STATE); return NUM_TASKS; }
nogroup         { return NOGROUP; }

requires                  { return REQUIRES; } 
reverse_offload           { return REVERSE_OFFLOAD; }
unified_address           { return UNIFIED_ADDRESS; }
unified_shared_memory     { return UNIFIED_SHARED_MEMORY; }
atomic_default_mem_order  { yy_push_state(ATOMIC_DEFAULT_MEM_ORDER_STATE); return ATOMIC_DEFAULT_MEM_ORDER; }
dynamic_allocators        { return DYNAMIC_ALLOCATORS; }
self_maps                 { return SELF_MAPS; }
seq_cst                   { return SEQ_CST; }
acq_rel                   { return ACQ_REL; }
relaxed                   { return RELAXED; }
use_device_ptr            { return USE_DEVICE_PTR; }
use_device_addr           { return USE_DEVICE_ADDR; }
target/{blank}            { return TARGET; }
target/{newline}          { return TARGET; }
target/"("                { return TARGET; }
target/","                { return TARGET; }
target/")"                { return TARGET; }
target/":"                { return TARGET; }
target                    { return TARGET; }
data/{blank}              { return DATA; }
data/{newline}            { return DATA; }
data/"("                  { return DATA; }
data/","                  { return DATA; }
data/")"                  { return DATA; }
data/":"                  { return DATA; }
data                      { return DATA; }
device/{blank}            { yy_push_state(DEVICE_STATE); return DEVICE; }
device/{newline}          { yy_push_state(DEVICE_STATE); return DEVICE; }
device/"("                { yy_push_state(DEVICE_STATE); return DEVICE; }
device/","                { yy_push_state(DEVICE_STATE); return DEVICE; }
device/")"                { yy_push_state(DEVICE_STATE); return DEVICE; }
device/":"                { yy_push_state(DEVICE_STATE); return DEVICE; }
device                    { yy_push_state(DEVICE_STATE); return DEVICE; }  
enter                     { yy_push_state(ENTER_STATE); return ENTER; }
exit                      { return EXIT; }
is_device_ptr             { return IS_DEVICE_PTR; }
has_device_addr           { return HAS_DEVICE_ADDR; }
defaultmap                { yy_push_state(DEFAULTMAP_STATE); return DEFAULTMAP; }
update                    { yy_push_state(UPDATE_STATE); return UPDATE; }

to                        { yy_push_state(TO_STATE); return TO; }
from                      { yy_push_state(FROM_STATE); return FROM; }
uses_allocators           { yy_push_state(USES_ALLOCATORS_STATE); uses_allocators_paren_depth = 0; return USES_ALLOCATORS; }
link                      { return LINK; }
device_type               { yy_push_state(DEVICE_TYPE_STATE); return DEVICE_TYPE; }
map                       { yy_push_state(MAP_STATE); return MAP; }
ext_                      { parenthesis_global_count = 0; yy_push_state(EXPR_STATE); return EXT_; }
barrier                   { return BARRIER; }
taskwait                  { return TASKWAIT; }
task_reduction            { yy_push_state(TASK_REDUCTION_STATE); return TASK_REDUCTION; }
flush                     { return FLUSH; }
release                   { return RELEASE; }
acquire                   { return ACQUIRE; }
atomic                    { return ATOMIC; }
read                      { return READ; }
write                     { return WRITE; }
capture                   { return CAPTURE; }
hint                      { return HINT; }
critical                  { return CRITICAL; }
depobj                    { return DEPOBJ; }
destroy                   { return DESTROY; }
threads                   { return THREADS; }
sizes                     { yy_push_state(SIZES_STATE); return SIZES; }

filter                    { return FILTER; }
compare                   { return COMPARE; }
fail                      { return FAIL; }
weak                      { return WEAK; }
at                        { return AT; }
severity                  { return SEVERITY; }
message                   { return MESSAGE; }
compilation               { return COMPILATION; }
execution                 { return EXECUTION; }
fatal                     { return FATAL; }
warning                   { return WARNING; }

absent                    { return ABSENT; }
present                   { return PRESENT; }
contains                  { return CONTAINS; }
holds                     { return HOLDS; }
otherwise                 { return OTHERWISE; }

graph_id                  { return GRAPH_ID; }
graph_reset               { return GRAPH_RESET; }
transparent               { return TRANSPARENT; }
replayable                { return REPLAYABLE; }
threadset                 { return THREADSET; }
indirect                  { return INDIRECT; }
local/{blank}             { return LOCAL; }
local/"("                 { return LOCAL; }
init/{blank}              { yy_push_state(INIT_STATE); return INIT; }
init/"("                  { yy_push_state(INIT_STATE); return INIT; }
init_complete             { return INIT_COMPLETE; }
safesync                  { return SAFESYNC; }
device_safesync           { return DEVICE_SAFESYNC; }
target_data/{blank}       { return TARGET_DATA_COMPOSITE; }  /* OpenMP 6.0 task-generating construct */
target_enter_data/{blank} { TRACKED_YYLESS(7); return TARGET; }
target_exit_data/{blank}  { TRACKED_YYLESS(7); return TARGET; }
target{id_char}+          { yy_push_state(EXPR_STATE); prepare_expression_capture_str(yytext); }
data{id_char}+            { yy_push_state(EXPR_STATE); prepare_expression_capture_str(yytext); }
device{id_char}+          { yy_push_state(EXPR_STATE); prepare_expression_capture_str(yytext); }
all{id_char}+             { yy_push_state(EXPR_STATE); prepare_expression_capture_str(yytext); }
cgroup{id_char}+          { yy_push_state(EXPR_STATE); prepare_expression_capture_str(yytext); }
memscope                  { return MEMSCOPE; }
looprange                 { yy_push_state(LOOPRANGE_STATE); return LOOPRANGE; }
permutation               { return PERMUTATION; }
counts                    { return COUNTS; }
inductor                  { return INDUCTOR; }
collector                 { return COLLECTOR; }
combiner                  { return COMBINER; }
adjust_args/{blank}       { yy_push_state(ADJUST_ARGS_STATE); return ADJUST_ARGS; }
adjust_args/"("           { yy_push_state(ADJUST_ARGS_STATE); return ADJUST_ARGS; }
append_args               { return APPEND_ARGS; }
apply/{blank}             { push_apply_state(); return APPLY; }
apply/"("                 { push_apply_state(); return APPLY; }
traits                    { return TRAITS; }
no_openmp                 { return NO_OPENMP; }
no_openmp_constructs      { return NO_OPENMP_CONSTRUCTS; }
no_openmp_routines        { return NO_OPENMP_ROUTINES; }
no_parallelism            { return NO_PARALLELISM; }
nocontext/{blank}         { yy_push_state(NOCONTEXT_STATE); return NOCONTEXT; }
nocontext/"("             { yy_push_state(NOCONTEXT_STATE); return NOCONTEXT; }
novariants/{blank}        { yy_push_state(NOVARIANTS_STATE); return NOVARIANTS; }
novariants/"("            { yy_push_state(NOVARIANTS_STATE); return NOVARIANTS; }
use                       { return USE; }
all                       { return ALL; }
cgroup                    { return CGROUP; }


<RAW_EXPR_STATE>\\.                        { current_string += original_token_text(static_cast<std::size_t>(yyleng)); }
<RAW_EXPR_STATE>["']                       {
                                             const char quote = original_token_char();
                                             if (raw_expression_quote == '\0') {
                                               raw_expression_quote = quote;
                                             } else if (raw_expression_quote == quote) {
                                               raw_expression_quote = '\0';
                                             }
                                             current_string.push_back(quote);
                                           }
<RAW_EXPR_STATE>"("                        {
                                             if (raw_expression_quote != '\0') {
                                               current_string.push_back('(');
                                             } else {
                                               parenthesis_local_count++;
                                               parenthesis_global_count++;
                                               current_string.push_back('(');
                                             }
                                           }
<RAW_EXPR_STATE>")"                        {
                                             if (raw_expression_quote != '\0') {
                                               current_string.push_back(')');
                                             } else {
                                               parenthesis_local_count--;
                                               parenthesis_global_count--;
                                               if (parenthesis_global_count == 0) {
                                                 yy_pop_state();
                                                 if (!current_string.empty()) {
                                                   return emit_expr_string_and_unput(')');
                                                 }
                                               } else {
                                                 current_string.push_back(')');
                                               }
                                             }
                                           }
<RAW_EXPR_STATE>"{"                        { if (raw_expression_quote == '\0') brace_count++; current_string.push_back('{'); }
<RAW_EXPR_STATE>"}"                        { if (raw_expression_quote == '\0') brace_count--; current_string.push_back('}'); }
<RAW_EXPR_STATE>"["                        { if (raw_expression_quote == '\0') bracket_count++; current_string.push_back('['); }
<RAW_EXPR_STATE>"]"                        { if (raw_expression_quote == '\0') bracket_count--; current_string.push_back(']'); }
<RAW_EXPR_STATE>{newline}+                 { current_string.push_back('\n'); }
<RAW_EXPR_STATE>.                          { current_string.push_back(original_token_char()); }

"("             { return '('; }
")"             { return ')'; }
":"             { return ':'; }
"}"             { yy_pop_state(); return '}'; }
","             { return ','; }
"\\"            { ; }
"<="            { return LESSOREQUAL; }
">="            { return MOREOREQUAL; }
"!="            { return NOTEQUAL; }

{comment}       { ; }


{newline}       { ; }

<ALLOCATE_STATE>omp_default_mem_alloc/{blank}*:       { return DEFAULT_MEM_ALLOC; }
<ALLOCATE_STATE>omp_large_cap_mem_alloc/{blank}*:     { return LARGE_CAP_MEM_ALLOC; }
<ALLOCATE_STATE>omp_const_mem_alloc/{blank}*:         { return CONST_MEM_ALLOC; }
<ALLOCATE_STATE>omp_high_bw_mem_alloc/{blank}*:       { return HIGH_BW_MEM_ALLOC; }
<ALLOCATE_STATE>omp_low_lat_mem_alloc/{blank}*:       { return LOW_LAT_MEM_ALLOC; }
<ALLOCATE_STATE>omp_cgroup_mem_alloc/{blank}*:        { return CGROUP_MEM_ALLOC; }
<ALLOCATE_STATE>omp_pteam_mem_alloc/{blank}*:         { return PTEAM_MEM_ALLOC; }
<ALLOCATE_STATE>omp_thread_mem_alloc/{blank}*:        { return THREAD_MEM_ALLOC; }
<ALLOCATE_STATE>allocator{blank}*\( {
                                              begin_allocate_modifier_capture(
                                                  ALLOCATOR_MODIFIER);
                                              yy_push_state(ALLOCATOR_CALL_STATE);
                                            }
<ALLOCATE_STATE>align{blank}*\( {
                                              begin_allocate_modifier_capture(
                                                  ALIGN_MODIFIER);
                                              yy_push_state(ALLOCATOR_CALL_STATE);
                                            }
<ALLOCATE_STATE>"("                                   { return '('; }
<ALLOCATE_STATE>")"                                   { yy_pop_state(); return ')'; }
<ALLOCATE_STATE>","                                   { return ','; }
<ALLOCATE_STATE>":"                                   { return ':'; }
<ALLOCATE_STATE>{blank}*                              { ; }
<ALLOCATE_STATE>.                                     {
                                              begin_allocate_expression_capture(
                                                  original_token_char());
                                              yy_push_state(ALLOCATE_EXPR_STATE);
                                            }

<ALLOCATE_EXPR_STATE>"::" {
                                              if (allocate_expression_capture.escaped) {
                                                allocate_expression_capture.escaped = false;
                                              }
                                              current_string +=
                                                  original_token_text(2);
                                            }
<ALLOCATE_EXPR_STATE>{newline} {
                                              int token =
                                                  consume_allocate_expression_char(
                                                      original_token_char());
                                              if (token != 0) {
                                                return token;
                                              }
                                            }
<ALLOCATE_EXPR_STATE>. {
                                              int token =
                                                  consume_allocate_expression_char(
                                                      original_token_char());
                                              if (token != 0) {
                                                return token;
                                              }
                                            }
<ALLOCATE_EXPR_STATE><<EOF>> {
                                              fail_allocate_capture(
                                                  "expression",
                                                  "input ended before the allocate clause was closed");
                                            }

<ALLOCATOR_CALL_STATE>{newline} {
                                              int token =
                                                  consume_allocate_modifier_char(
                                                      original_token_char());
                                              if (token != 0) {
                                                return token;
                                              }
                                            }
<ALLOCATOR_CALL_STATE>. {
                                              int token =
                                                  consume_allocate_modifier_char(
                                                      original_token_char());
                                              if (token != 0) {
                                                return token;
                                              }
                                            }
<ALLOCATOR_CALL_STATE><<EOF>> {
                                              fail_allocate_capture(
                                                  "modifier",
                                                  "input ended before the modifier was closed");
                                            }

<IF_STATE>parallel{blank}*/:                { return PARALLEL; }
<IF_STATE>simd{blank}*/:                    { return SIMD; }
<IF_STATE>task{blank}*/:                    { return TASK; }
<IF_STATE>task_iteration{blank}*/:          { return TASK_ITERATION; }
<IF_STATE>taskgraph{blank}*/:               { return TASKGRAPH; }
<IF_STATE>taskloop{blank}*/:                { return TASKLOOP; }
<IF_STATE>teams{blank}*/:                   { return TEAMS; }
<IF_STATE>cancel{blank}*/:                  { return CANCEL; }
<IF_STATE>target/{blank}*[{data}{enter}{exit}{update}{:}] { return TARGET; }
<IF_STATE>data/{blank}*:                    { return DATA; }
<IF_STATE>enter/{blank}*data                { return ENTER; }
<IF_STATE>exit/{blank}*data                 { return EXIT; }
<IF_STATE>update/{blank}*:                  { return UPDATE; }

<IF_STATE>"("                               {
                                              if (if_paren_depth == 0) {
                                                if_paren_depth++;
                                                return '(';
                                              }
                                              yy_push_state(EXPR_STATE);
                                              prepare_expression_capture('(');
                                              parenthesis_local_count++;
                                              parenthesis_global_count++;
                                            }
<IF_STATE>")"                               { yy_pop_state(); return ')'; }
<IF_STATE>":"                               { return ':'; }
<IF_STATE>{blank}*                          { ; }
<IF_STATE>.                                 { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<NOCONTEXT_STATE>dispatch{blank}*/:         { return DISPATCH; }
<NOCONTEXT_STATE>"("                       { return '('; }
<NOCONTEXT_STATE>")"                       { yy_pop_state(); return ')'; }
<NOCONTEXT_STATE>":"                       { return ':'; }
<NOCONTEXT_STATE>{blank}*                  { ; }
<NOCONTEXT_STATE>.                         { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<NOVARIANTS_STATE>dispatch{blank}*/:        { return DISPATCH; }
<NOVARIANTS_STATE>"("                      { return '('; }
<NOVARIANTS_STATE>")"                      { yy_pop_state(); return ')'; }
<NOVARIANTS_STATE>":"                      { return ':'; }
<NOVARIANTS_STATE>{blank}*                 { ; }
<NOVARIANTS_STATE>.                        { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<INTEROP_CLAUSE_STATE>dispatch{blank}*/:    { return DISPATCH; }
<INTEROP_CLAUSE_STATE>"("                  { return '('; }
<INTEROP_CLAUSE_STATE>")"                  { yy_pop_state(); return ')'; }
<INTEROP_CLAUSE_STATE>","                  { return ','; }
<INTEROP_CLAUSE_STATE>":"                  { return ':'; }
<INTEROP_CLAUSE_STATE>{blank}*             { ; }
<INTEROP_CLAUSE_STATE>.                    { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }


<PROC_BIND_STATE>master                     { return MASTER; }
<PROC_BIND_STATE>primary                    { return PRIMARY; }
<PROC_BIND_STATE>close                      { return CLOSE; }
<PROC_BIND_STATE>spread                     { return SPREAD; }
<PROC_BIND_STATE>"("                        { return '('; }
<PROC_BIND_STATE>")"                        { yy_pop_state(); return ')'; }
<PROC_BIND_STATE>{blank}*                   { ; }
<PROC_BIND_STATE>.                          { return -1; }

<DEFAULT_STATE>shared                       { return SHARED; }
<DEFAULT_STATE>none                         { return NONE; }
<DEFAULT_STATE>firstprivate                 { return FIRSTPRIVATE; }
<DEFAULT_STATE>private                      { return PRIVATE; }
<DEFAULT_STATE>"("                          { return '('; }
<DEFAULT_STATE>")"                          { yy_pop_state(); return ')'; }
<DEFAULT_STATE>{blank}*                     { ; }
<DEFAULT_STATE>.                            { yy_push_state(INITIAL); tracked_unput(yytext[0]); } /* So far, only for default in metadirective meaning that a new directive is coming up. */

<ORDER_STATE>reproducible                   { return REPRODUCIBLE; }
<ORDER_STATE>unconstrained                  { return UNCONSTRAINED; }
<ORDER_STATE>concurrent                     { return CONCURRENT; }
<ORDER_STATE>":"                            { return ':'; }
<ORDER_STATE>"("                            { return '('; }
<ORDER_STATE>")"                            { yy_pop_state(); return ')'; }
<ORDER_STATE>{blank}*                       { ; }
<ORDER_STATE>.                              { yy_push_state(INITIAL); }

<REDUCTION_STATE>inscan/{blank}*,           { return MODIFIER_INSCAN; }
<REDUCTION_STATE>task/{blank}*,             { return MODIFIER_TASK; }
<REDUCTION_STATE>default/{blank}*,          { return MODIFIER_DEFAULT; }
<REDUCTION_STATE>"("                        { return '('; }
<REDUCTION_STATE>")"                        { yy_pop_state(); return ')'; }
<REDUCTION_STATE>","                        { return ','; }
<REDUCTION_STATE>":"                        { return ':'; }
<REDUCTION_STATE>"+"                        { return '+'; }
<REDUCTION_STATE>"-"                        { return '-'; }
<REDUCTION_STATE>"*"                        { return '*'; }
<REDUCTION_STATE>"&"                        { return '&'; }
<REDUCTION_STATE>"|"                        { return '|'; }
<REDUCTION_STATE>"^"                        { return '^'; }
<REDUCTION_STATE>"&&"                       { return LOGAND; }
<REDUCTION_STATE>"||"                       { return LOGOR; }
<REDUCTION_STATE>min/{blank}*:              { return MIN; }
<REDUCTION_STATE>max/{blank}*:              { return MAX; }
<REDUCTION_STATE>{blank}*                   { ; }
<REDUCTION_STATE>.                          { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<SIMD_STATE>"("                             { yy_push_state(EXPR_STATE); return '('; }
<SIMD_STATE>")"                             { yy_pop_state(); return ')'; }
<SIMD_STATE>{blank}*                        { ; }

<THREADPRIVATE_STATE>"("                    { yy_push_state(EXPR_STATE); return '('; }
<THREADPRIVATE_STATE>")"                    { yy_pop_state(); return ')'; }
<THREADPRIVATE_STATE>{blank}*               { ; }

<PRIVATE_STATE>"("                          { return '('; }
<PRIVATE_STATE>")"                          { yy_pop_state(); return ')'; }
<PRIVATE_STATE>{blank}*                     { ; }
<PRIVATE_STATE>.                            { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<FIRSTPRIVATE_STATE>"("                     { return '('; }
<FIRSTPRIVATE_STATE>")"                     { yy_pop_state(); return ')'; }
<FIRSTPRIVATE_STATE>target{blank}+data      { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_TARGET_DATA); }
<FIRSTPRIVATE_STATE>distribute              { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_DISTRIBUTE); }
<FIRSTPRIVATE_STATE>parallel                { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_PARALLEL); }
<FIRSTPRIVATE_STATE>sections                { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_SECTIONS); }
<FIRSTPRIVATE_STATE>single                  { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_SINGLE); }
<FIRSTPRIVATE_STATE>target                  { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_TARGET); }
<FIRSTPRIVATE_STATE>taskloop                { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_TASKLOOP); }
<FIRSTPRIVATE_STATE>task                    { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_TASK); }
<FIRSTPRIVATE_STATE>teams                   { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_TEAMS); }
<FIRSTPRIVATE_STATE>scope                   { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_SCOPE); }
<FIRSTPRIVATE_STATE>for                     { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_FOR); }
<FIRSTPRIVATE_STATE>do                      { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(FIRSTPRIVATE_MODIFIER_DO); }
<FIRSTPRIVATE_STATE>saved                   { RETURN_FIRSTPRIVATE_MODIFIER_OR_EXPR(SAVED); }
<FIRSTPRIVATE_STATE>":"                     { return ':'; }
<FIRSTPRIVATE_STATE>","                     { return ','; }
<FIRSTPRIVATE_STATE>{blank}*                { ; }
<FIRSTPRIVATE_STATE>.                       { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<SHARED_STATE>"("                           { return '('; }
<SHARED_STATE>")"                           { yy_pop_state(); return ')'; }
<SHARED_STATE>{blank}*                      { ; }
<SHARED_STATE>.                             { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<COPYPRIVATE_STATE>"("                      { return '('; }
<COPYPRIVATE_STATE>")"                      { yy_pop_state(); return ')'; }
<COPYPRIVATE_STATE>{blank}*                 { ; }
<COPYPRIVATE_STATE>.                        { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<COPYIN_STATE>"("                           { return '('; }
<COPYIN_STATE>")"                           { yy_pop_state(); return ')'; }
<COPYIN_STATE>{blank}*                      { ; }
<COPYIN_STATE>.                             { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<LASTPRIVATE_STATE>conditional/{blank}*:    { return MODIFIER_CONDITIONAL; }
<LASTPRIVATE_STATE>"("                      { return '('; }
<LASTPRIVATE_STATE>")"                      { yy_pop_state(); return ')'; }
<LASTPRIVATE_STATE>":"                      { return ':'; }
<LASTPRIVATE_STATE>{blank}*                 { ; }
<LASTPRIVATE_STATE>.                        { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<LINEAR_STATE>"("                           { return '('; }
<LINEAR_STATE>")"                           { yy_pop_state(); return ')'; }
<LINEAR_STATE>val/{blank}*"("               { REWIND_LINEAR_DELIMITER('('); return MODOFIER_VAL; }
<LINEAR_STATE>ref/{blank}*"("               { REWIND_LINEAR_DELIMITER('('); return MODOFIER_REF; }
<LINEAR_STATE>uval/{blank}*"("              { REWIND_LINEAR_DELIMITER('('); return MODOFIER_UVAL; }
<LINEAR_STATE>val/{blank}*","               { REWIND_LINEAR_DELIMITER(','); return MODOFIER_VAL; }
<LINEAR_STATE>ref/{blank}*","               { REWIND_LINEAR_DELIMITER(','); return MODOFIER_REF; }
<LINEAR_STATE>uval/{blank}*","              { REWIND_LINEAR_DELIMITER(','); return MODOFIER_UVAL; }
<LINEAR_STATE>val/{blank}*")"               { REWIND_LINEAR_DELIMITER(')'); return MODOFIER_VAL; }
<LINEAR_STATE>ref/{blank}*")"               { REWIND_LINEAR_DELIMITER(')'); return MODOFIER_REF; }
<LINEAR_STATE>uval/{blank}*")"              { REWIND_LINEAR_DELIMITER(')'); return MODOFIER_UVAL; }
<LINEAR_STATE>":"                           { return ':'; }
<LINEAR_STATE>","                           { return ','; }
<LINEAR_STATE>{blank}*                      { ; }
<LINEAR_STATE>.                             { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<SCHEDULE_STATE>monotonic                   { return MODIFIER_MONOTONIC; }
<SCHEDULE_STATE>nonmonotonic                { return MODIFIER_NONMONOTONIC; }
<SCHEDULE_STATE>simd                        { return MODIFIER_SIMD; }
<SCHEDULE_STATE>static                      { return STATIC; }
<SCHEDULE_STATE>dynamic                     { return DYNAMIC; }
<SCHEDULE_STATE>guided                      { return GUIDED; }
<SCHEDULE_STATE>auto                        { return AUTO; }
<SCHEDULE_STATE>runtime                     { return RUNTIME; }
<SCHEDULE_STATE>","                         { return ','; }
<SCHEDULE_STATE>":"                         { return ':'; }
<SCHEDULE_STATE>"("                         { return '('; }
<SCHEDULE_STATE>")"                         { yy_pop_state(); return ')'; }
<SCHEDULE_STATE>{blank}*                    { ; }
<SCHEDULE_STATE>.                           { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<COLLAPSE_STATE>"("                         { return '('; }
<COLLAPSE_STATE>")"                         { yy_pop_state(); return ')'; }
<COLLAPSE_STATE>{blank}*                    { ; }
<COLLAPSE_STATE>.                           { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<SIZES_STATE>"("                            { yy_push_state(EXPR_STATE); return '('; }
<SIZES_STATE>")"                            { yy_pop_state(); return ')'; }
<SIZES_STATE>","                            { return ','; }
<SIZES_STATE>{blank}*                       { ; }
<SIZES_STATE>.                              { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<PARTIAL_STATE>"("                          { yy_push_state(EXPR_STATE); return '('; }
<PARTIAL_STATE>")"                          { yy_pop_state(); return ')'; }
<PARTIAL_STATE>{blank}*                     { ; }
<PARTIAL_STATE>.                            { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<LOOPRANGE_STATE>"("                        { return '('; }
<LOOPRANGE_STATE>")"                        { yy_pop_state(); return ')'; }
<LOOPRANGE_STATE>","                        { return ','; }
<LOOPRANGE_STATE>{blank}*                   { ; }
<LOOPRANGE_STATE>.                          { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<INIT_STATE>"("                             { return '('; }
<INIT_STATE>")"                             { yy_pop_state(); return ')'; }
<INIT_STATE>":"                             { return ':'; }
<INIT_STATE>","                             { return ','; }
<INIT_STATE>prefer_type{blank}*/"("         { return PREFER_TYPE; }
<INIT_STATE>mutexinoutset{blank}*/"("       { return MUTEXINOUTSET; }
<INIT_STATE>inoutset{blank}*/"("            { return INOUTSET; }
<INIT_STATE>inout{blank}*/"("               { return INOUT; }
<INIT_STATE>out{blank}*/"("                 { return OUT; }
<INIT_STATE>in{blank}*/"("                  { return IN; }
<INIT_STATE>targetsync{blank}*/[,:]         { return TARGETSYNC; }
<INIT_STATE>target{blank}*/[,:]             { return TARGET; }
<INIT_STATE>depobj{blank}*/[,:]             { return DEPOBJ; }
<INIT_STATE>interop{blank}*/[,:]            { return INTEROP; }
<INIT_STATE>{blank}*                        { ; }
<INIT_STATE>.                               { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<APPLY_STATE>"("                            { current_apply_paren_depth()++; return '('; }
<APPLY_STATE>")"                            {
                                              if (current_apply_paren_depth() > 0) {
                                                current_apply_paren_depth()--;
                                              }
                                              if (current_apply_paren_depth() == 0) {
                                                pop_apply_state();
                                              }
                                              return ')';
                                            }
<APPLY_STATE>":"                            { return ':'; }
<APPLY_STATE>","                            { return ','; }
<APPLY_STATE>unrollpartial                  { TRACKED_YYLESS(6); return UNROLL; }
<APPLY_STATE>unrollfull                     { TRACKED_YYLESS(6); return UNROLL; }
<APPLY_STATE>unroll                         { return UNROLL; }
<APPLY_STATE>partial                        { return PARTIAL; }
<APPLY_STATE>full                           { return FULL; }
<APPLY_STATE>reverse                        { return REVERSE; }
<APPLY_STATE>interchange                    { return INTERCHANGE; }
<APPLY_STATE>nothing                        { return NOTHING; }
<APPLY_STATE>tile                           { return TILE; }
<APPLY_STATE>sizes                          { return SIZES; }
<APPLY_STATE>apply/{blank}                  { push_apply_state(); return APPLY; }
<APPLY_STATE>apply/"("                      { push_apply_state(); return APPLY; }
<APPLY_STATE>{blank}*                       { ; }
<APPLY_STATE>.                              { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<ADJUST_ARGS_STATE>"("                      { return '('; }
<ADJUST_ARGS_STATE>")"                      { yy_pop_state(); return ')'; }
<ADJUST_ARGS_STATE>":"                      { return ':'; }
<ADJUST_ARGS_STATE>","                      { return ','; }
<ADJUST_ARGS_STATE>need_device_ptr/{blank}*: { return NEED_DEVICE_PTR; }
<ADJUST_ARGS_STATE>{blank}*                 { ; }
<ADJUST_ARGS_STATE>.                        { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<INDUCTION_STATE>step/{blank}*\(            { induction_step_waiting = true; return STEP; }
<INDUCTION_STATE>"("                         {
                                              induction_spec_paren_depth++;
                                              if (induction_step_waiting) {
                                                // After step(, we need to capture expression
                                                induction_step_waiting = false;
                                                prepare_expression_capture();
                                                yy_push_state(EXPR_STATE);
                                                return '(';
                                              }
                                              return '(';
                                            }
<INDUCTION_STATE>")"                         {
                                              if (induction_spec_paren_depth > 0) {
                                                induction_spec_paren_depth--;
                                              }
                                              if (induction_spec_paren_depth == 0) {
                                                yy_pop_state();
                                              }
                                              return ')';
                                            }
<INDUCTION_STATE>","                         { return ','; }
<INDUCTION_STATE>":"                         { return ':'; }
<INDUCTION_STATE>{blank}*                    { ; }
<INDUCTION_STATE>.                           { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<ORDERED_STATE>"("                          { yy_push_state(EXPR_STATE); return '('; }
<ORDERED_STATE>")"                          { yy_pop_state(); return ')'; }
<ORDERED_STATE>{blank}*                     { ; }

<SIMDLEN_STATE>"("                          { return '('; }
<SIMDLEN_STATE>")"                          { yy_pop_state(); return ')'; }
<SIMDLEN_STATE>{blank}*                     { ; }
<SIMDLEN_STATE>.                            { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<SAFELEN_STATE>"("                          { return '('; }
<SAFELEN_STATE>")"                          { yy_pop_state(); return ')'; }
<SAFELEN_STATE>{blank}*                     { ; }
<SAFELEN_STATE>.                            { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<NONTEMPORAL_STATE>"("                      { return '('; }
<NONTEMPORAL_STATE>")"                      { yy_pop_state(); return ')'; }
<NONTEMPORAL_STATE>{blank}*                 { ; }
<NONTEMPORAL_STATE>.                        { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<NUM_TEAMS_STATE>"("                        { return '('; }
<NUM_TEAMS_STATE>")"                        { yy_pop_state(); return ')'; }
<NUM_TEAMS_STATE>{blank}*                   { ; }
<NUM_TEAMS_STATE>.                          { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<NUM_THREADS_STATE>strict/{blank}*:         { return STRICT; }
<NUM_THREADS_STATE>"("                      { return '('; }
<NUM_THREADS_STATE>")"                      { yy_pop_state(); return ')'; }
<NUM_THREADS_STATE>":"                      { return ':'; }
<NUM_THREADS_STATE>{blank}*                 { ; }
<NUM_THREADS_STATE>.                        { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<GRAINSIZE_STATE>strict/{blank}*:            { return STRICT; }
<GRAINSIZE_STATE>"("                         { return '('; }
<GRAINSIZE_STATE>")"                         { yy_pop_state(); return ')'; }
<GRAINSIZE_STATE>":"                         { return ':'; }
<GRAINSIZE_STATE>{blank}*                    { ; }
<GRAINSIZE_STATE>.                           { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<NUM_TASKS_STATE>strict/{blank}*:            { return STRICT; }
<NUM_TASKS_STATE>"("                         { return '('; }
<NUM_TASKS_STATE>")"                         { yy_pop_state(); return ')'; }
<NUM_TASKS_STATE>":"                         { return ':'; }
<NUM_TASKS_STATE>{blank}*                    { ; }
<NUM_TASKS_STATE>.                           { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<ALIGNED_STATE>"("                          { return '('; }
<ALIGNED_STATE>":"                          { return ':'; }
<ALIGNED_STATE>")"                          { yy_pop_state(); return ')'; }
<ALIGNED_STATE>{blank}*                     { ; }
<ALIGNED_STATE>.                            { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<DIST_SCHEDULE_STATE>static/{blank}*        {return STATIC; }
<DIST_SCHEDULE_STATE>"("                    { return '('; }
<DIST_SCHEDULE_STATE>","                    { return ','; }
<DIST_SCHEDULE_STATE>")"                    { yy_pop_state(); return ')'; }
<DIST_SCHEDULE_STATE>{blank}*               { ; }
<DIST_SCHEDULE_STATE>.                      { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<BIND_STATE>teams                           { return TEAMS; }
<BIND_STATE>parallel                        { return PARALLEL; }
<BIND_STATE>thread                          { return THREAD; }
<BIND_STATE>"("                             { return '('; }
<BIND_STATE>")"                             { yy_pop_state(); return ')'; }
<BIND_STATE>{blank}*                        { ; }
<BIND_STATE>.                               { return -1; }

<ALLOCATOR_STATE>omp_default_mem_alloc      { return DEFAULT_MEM_ALLOC; }
<ALLOCATOR_STATE>omp_large_cap_mem_alloc    { return LARGE_CAP_MEM_ALLOC; }
<ALLOCATOR_STATE>omp_const_mem_alloc        { return CONST_MEM_ALLOC; }
<ALLOCATOR_STATE>omp_high_bw_mem_alloc      { return HIGH_BW_MEM_ALLOC; }
<ALLOCATOR_STATE>omp_low_lat_mem_alloc      { return LOW_LAT_MEM_ALLOC; }
<ALLOCATOR_STATE>omp_cgroup_mem_alloc       { return CGROUP_MEM_ALLOC; }
<ALLOCATOR_STATE>omp_pteam_mem_alloc        { return PTEAM_MEM_ALLOC; }
<ALLOCATOR_STATE>omp_thread_mem_alloc       { return THREAD_MEM_ALLOC; }
<ALLOCATOR_STATE>{blank}*                   { ; }
<ALLOCATOR_STATE>"("                        { return '('; }
<ALLOCATOR_STATE>")"                        { yy_pop_state(); return ')'; }
<ALLOCATOR_STATE>.                          { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<INITIALIZER_STATE>omp_priv                 { return OMP_PRIV; }
<INITIALIZER_STATE>"="                      { return '='; }
<INITIALIZER_STATE>{blank}*                 { ; }
<INITIALIZER_STATE>"("                      { return '('; }
<INITIALIZER_STATE>")"                      { yy_pop_state(); return ')'; }
<INITIALIZER_STATE>.                        { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<MAPPER_STATE>{blank}*                      { ; }
<MAPPER_STATE>"("                           { prepare_expression_capture(); yy_push_state(RAW_EXPR_STATE); return '('; }
<MAPPER_STATE>")"                           { yy_pop_state(); return ')'; }
<MAPPER_STATE>.                             {
                                              std::fprintf(stderr,
                                                           "REX_OMPPARSER_LEXER[declare-mapper]: expected '('\n");
                                              std::abort();
                                            }

<TYPE_STR_STATE>"::"                        { current_string.append("::"); }
<TYPE_STR_STATE>.                           { current_char = original_token_char();
                                            switch (current_char) {
                                                case '(': {
                                                    parenthesis_local_count++;
                                                    parenthesis_global_count++;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case ')': {
                                                    parenthesis_local_count--;
                                                    parenthesis_global_count--;
                                                    if (parenthesis_global_count == 0) {
                                                        yy_pop_state();
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(')');
                                                        }
                                                    } else {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case ' ': {
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case ',': {
                                                    yy_pop_state();
                                                    if (!current_string.empty()) {
                                                        return emit_expr_string_and_unput(',');
                                                    }
                                                    return ',';
                                                }
                                                case ':': {
                                                    if (parenthesis_local_count == 0) {
                                                        yy_pop_state();
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(':');
                                                        }
                                                        return ':';
                                                    }
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                default: {
                                                    if (current_char != ' ' || parenthesis_local_count != 0) {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                            }
                                        }

<WHEN_STATE>"("                             { return '('; }
<WHEN_STATE>":"                             { yy_push_state(INITIAL); return ':'; }
<WHEN_STATE>")"                             { yy_pop_state(); return ')'; }
<WHEN_STATE>"="                             { return '='; }
<WHEN_STATE>"{"                             { yy_push_state(INITIAL); return '{'; } /* now parsrsing enters to pass a full construct, directive, condition, etc */
<WHEN_STATE>"}"                             { return '}'; }
<WHEN_STATE>","                             { return ','; }
<WHEN_STATE>user                            { return USER; }
<WHEN_STATE>construct                       { return CONSTRUCT; }
<WHEN_STATE>device                          { return DEVICE; }
<WHEN_STATE>target_device                   { return TARGET_DEVICE; }
<WHEN_STATE>implementation                  { yy_push_state(IMPLEMENTATION_STATE); return IMPLEMENTATION; }
<WHEN_STATE>{blank}*                        { ; }
<WHEN_STATE>.                               { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<IMPLEMENTATION_STATE>"("                            {
                                                  if (implementation_selector_parenthesis_pending) {
                                                    implementation_selector_parenthesis_pending = false;
                                                    yy_push_state(EXTENSION_STATE);
                                                  }
                                                  return '(';
                                                }
<IMPLEMENTATION_STATE>","                            {
                                                  implementation_selector_parenthesis_pending = false;
                                                  return ',';
                                                }
<IMPLEMENTATION_STATE>")"                            { return ')'; }
<IMPLEMENTATION_STATE>"="                            { return '='; }
<IMPLEMENTATION_STATE>"{"                            { brace_count++; return '{'; }
<IMPLEMENTATION_STATE>"}"                            {
                                                  implementation_selector_parenthesis_pending = false;
                                                  yy_pop_state();
                                                  return '}';
                                                }
<IMPLEMENTATION_STATE>vendor/{blank}*\(              { yy_push_state(VENDOR_STATE); return VENDOR; }
<IMPLEMENTATION_STATE>extension/{blank}*\(           { yy_push_state(EXTENSION_STATE); return EXTENSION; }
<IMPLEMENTATION_STATE>requires/{blank}*\(            { yy_push_state(EXTENSION_STATE); return REQUIRES; }
<IMPLEMENTATION_STATE>atomic_default_mem_order/{blank}*\( {
                                                  yy_push_state(ATOMIC_DEFAULT_MEM_ORDER_STATE);
                                                  return ATOMIC_DEFAULT_MEM_ORDER;
                                                }
<IMPLEMENTATION_STATE>{blank}*                       { ; }
<IMPLEMENTATION_STATE>{identifier}                   {
                                                  implementation_selector_parenthesis_pending = true;
                                                  openmp_lval.stype = store_lexeme(
                                                      original_token_text(
                                                          static_cast<std::size_t>(yyleng)));
                                                  return EXPR_STRING;
                                                }
<IMPLEMENTATION_STATE>.                              { return -1; }

<MATCH_STATE>"("                            { return '('; }
<MATCH_STATE>":"                            { yy_push_state(INITIAL); return ':'; }
<MATCH_STATE>")"                            { yy_pop_state(); return ')'; }
<MATCH_STATE>"="                            { return '='; }
<MATCH_STATE>"{"                            { yy_push_state(INITIAL); return '{'; } /* now parsing enters to pass a full construct, directive, condition, etc */
<MATCH_STATE>"}"                            { return '}'; }
<MATCH_STATE>","                            { return ','; }
<MATCH_STATE>user                           { return USER; }
<MATCH_STATE>construct                      { return CONSTRUCT; }
<MATCH_STATE>device                         { return DEVICE; }
<MATCH_STATE>target_device                  { return TARGET_DEVICE; }
<MATCH_STATE>implementation                 { yy_push_state(IMPLEMENTATION_STATE); return IMPLEMENTATION; }
<MATCH_STATE>{blank}*                       { ; }
<MATCH_STATE>.                              { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<ISA_STATE>"("/score{blank}*\(              { return '('; }
<ISA_STATE>"("                              { parenthesis_global_count = 1; return '('; }
<ISA_STATE>")"                              { yy_pop_state(); return ')'; }
<ISA_STATE>{blank}*                         { ; }
<ISA_STATE>score/{blank}*\(                 { yy_push_state(SCORE_STATE); return SCORE; }
<ISA_STATE>.                                { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<ARCH_STATE>"("/score{blank}*\(             { return '('; }
<ARCH_STATE>"("                             { parenthesis_global_count = 1; return '('; }
<ARCH_STATE>")"                             { yy_pop_state(); return ')'; }
<ARCH_STATE>{blank}*                        { ; }
<ARCH_STATE>score/{blank}*\(                { yy_push_state(SCORE_STATE); return SCORE; }
<ARCH_STATE>.                               { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<SCORE_STATE>"("{blank}*                    { yy_push_state(EXPR_STATE); parenthesis_global_count = 1; return '('; }
<SCORE_STATE>")"                            { return ')'; }
<SCORE_STATE>":"                            { yy_pop_state(); parenthesis_global_count = 1; return ':'; }
<SCORE_STATE>{blank}*                       { ; }

<CONDITION_STATE>"("/score{blank}*\(        { return '('; }
<CONDITION_STATE>"("                        { parenthesis_global_count = 1; return '('; }
<CONDITION_STATE>")"                        { yy_pop_state(); return ')'; }
<CONDITION_STATE>{blank}*                   { ; }
<CONDITION_STATE>score/{blank}*\(           { yy_push_state(SCORE_STATE); return SCORE; }
<CONDITION_STATE>.                          { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<VENDOR_STATE>"("                           { return '('; }
<VENDOR_STATE>")"                           { yy_pop_state(); return ')'; }
<VENDOR_STATE>{blank}*                      { ; }
<VENDOR_STATE>","                           { return ','; }
<VENDOR_STATE>amd                           { return AMD; }
<VENDOR_STATE>arm                           { return ARM; }
<VENDOR_STATE>bsc                           { return BSC; }
<VENDOR_STATE>cray                          { return CRAY; }
<VENDOR_STATE>fujitsu                       { return FUJITSU; }
<VENDOR_STATE>gnu                           { return GNU; }
<VENDOR_STATE>ibm                           { return IBM; }
<VENDOR_STATE>intel                         { return INTEL; }
<VENDOR_STATE>llvm                          { return LLVM; }
<VENDOR_STATE>nvidia                        { return NVIDIA; }
<VENDOR_STATE>pgi                           { return PGI; }
<VENDOR_STATE>ti                            { return TI; }
<VENDOR_STATE>user                          { return USER; }
<VENDOR_STATE>unknown                       { return UNKNOWN; }
<VENDOR_STATE>score/{blank}*\(              { yy_push_state(SCORE_STATE); return SCORE; }

<EXTENSION_STATE>"("                        { return '('; }
<EXTENSION_STATE>")"                        { yy_pop_state(); return ')'; }
<EXTENSION_STATE>{blank}*                   { ; }
<EXTENSION_STATE>score/{blank}*\(           { yy_push_state(SCORE_STATE); return SCORE; }
<EXTENSION_STATE>.                          { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<IN_REDUCTION_STATE>"("                     { return '('; }
<IN_REDUCTION_STATE>")"                     { yy_pop_state(); return ')'; }
<IN_REDUCTION_STATE>","                     { return ','; }
<IN_REDUCTION_STATE>":"                     { return ':'; }
<IN_REDUCTION_STATE>"+"                     { return '+'; }
<IN_REDUCTION_STATE>"-"                     { return '-'; }
<IN_REDUCTION_STATE>"*"                     { return '*'; }
<IN_REDUCTION_STATE>"&"                     { return '&'; }
<IN_REDUCTION_STATE>"|"                     { return '|'; }
<IN_REDUCTION_STATE>"^"                     { return '^'; }
<IN_REDUCTION_STATE>"&&"                    { return LOGAND; }
<IN_REDUCTION_STATE>"||"                    { return LOGOR; }
<IN_REDUCTION_STATE>min/{blank}*:           { return MIN; }
<IN_REDUCTION_STATE>max/{blank}*:           { return MAX; }
<IN_REDUCTION_STATE>{blank}*                { ; }
<IN_REDUCTION_STATE>.                       { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<DEPEND_STATE>"("                           { return '('; }
<DEPEND_STATE>")"                           { yy_pop_state(); return ')'; }
<DEPEND_STATE>","                           { return ','; }
<DEPEND_STATE>"="                           { return '='; }
<DEPEND_STATE>":"                           { yy_push_state(EXPR_STATE); return ':'; }
<DEPEND_STATE>iterator/{blank}*"("          { yy_push_state(ITERATOR_CAPTURE_STATE); return MODIFIER_ITERATOR; }

<DEPEND_STATE>in                            { return IN; }
<DEPEND_STATE>out                           { return OUT; }
<DEPEND_STATE>inout                         { return INOUT; }
<DEPEND_STATE>inoutset                      { return INOUTSET; }
<DEPEND_STATE>mutexinoutset                 { return MUTEXINOUTSET; }
<DEPEND_STATE>depobj                        { return DEPOBJ; }
<DEPEND_STATE>source                        { return SOURCE; }
<DEPEND_STATE>sink                          { return SINK; }
<DEPEND_STATE>{blank}*                      { ; }
<DEPEND_STATE>.                             { yy_push_state(EXPR_STATE); tracked_unput(yytext[0]); }

<DOACROSS_STATE>"("                         { return '('; }
<DOACROSS_STATE>")"                         { yy_pop_state(); return ')'; }
<DOACROSS_STATE>":"                         { return ':'; }
<DOACROSS_STATE>source                      { return SOURCE; }
<DOACROSS_STATE>sink                        { return SINK; }
<DOACROSS_STATE>{blank}*                    { ; }
<DOACROSS_STATE>.                           { yy_push_state(EXPR_STATE); tracked_unput(yytext[0]); }

<AFFINITY_STATE>"("                         { return '('; }
<AFFINITY_STATE>")"                         { yy_pop_state(); return ')'; }
<AFFINITY_STATE>","                         { return ','; }
<AFFINITY_STATE>":"                         { return ':'; }
<AFFINITY_STATE>iterator/{blank}*"("        { yy_push_state(ITERATOR_CAPTURE_STATE); return MODIFIER_ITERATOR; }
<AFFINITY_STATE>{blank}*                    { ; }
<AFFINITY_STATE>.                           { yy_push_state(EXPR_STATE); tracked_unput(yytext[0]); }

<ITERATOR_CAPTURE_STATE>{blank}*            { ; }
<ITERATOR_CAPTURE_STATE>"("                  { prepare_expression_capture(); yy_push_state(RAW_EXPR_STATE); return '('; }
<ITERATOR_CAPTURE_STATE>")"                  { yy_pop_state(); return ')'; }
<ITERATOR_CAPTURE_STATE>.                    {
                                              std::fprintf(stderr,
                                                           "REX_OMPPARSER_LEXER[iterator]: expected '('\n");
                                              std::abort();
                                            }

<FINAL_STATE>"("                            { return '('; }
<FINAL_STATE>")"                            { yy_pop_state(); return ')'; }
<FINAL_STATE>{blank}*                       { ; }
<FINAL_STATE>.                              { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<ATOMIC_DEFAULT_MEM_ORDER_STATE>seq_cst     { return SEQ_CST; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>acq_rel     { return ACQ_REL; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>acquire     { return ACQUIRE; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>release     { return RELEASE; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>relaxed     { return RELAXED; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>"("         { return '('; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>")"         { yy_pop_state(); return ')'; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>score/{blank}*\( { yy_push_state(SCORE_STATE); return SCORE; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>{blank}*    { ; }

<DEVICE_STATE>ancestor/{blank}*:            { return ANCESTOR; }
<DEVICE_STATE>device_num/{blank}*:          { return DEVICE_NUM; }
<DEVICE_STATE>"("                           { return '('; }
<DEVICE_STATE>")"                           { yy_pop_state(); return ')'; }
<DEVICE_STATE>":"                           { return ':'; }
<DEVICE_STATE>{blank}*                      { ; }
<DEVICE_STATE>.                             { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<DEFAULTMAP_STATE>alloc/{blank}*            { return BEHAVIOR_ALLOC; }
<DEFAULTMAP_STATE>to/{blank}*               { return BEHAVIOR_TO; }
<DEFAULTMAP_STATE>from/{blank}*             { return BEHAVIOR_FROM; }
<DEFAULTMAP_STATE>tofrom/{blank}*           { return BEHAVIOR_TOFROM; }
<DEFAULTMAP_STATE>firstprivate/{blank}*     { return BEHAVIOR_FIRSTPRIVATE; }
<DEFAULTMAP_STATE>none/{blank}*             { return BEHAVIOR_NONE; }
<DEFAULTMAP_STATE>default/{blank}*          { return BEHAVIOR_DEFAULT; }
<DEFAULTMAP_STATE>present/{blank}*          { return BEHAVIOR_PRESENT; }
<DEFAULTMAP_STATE>scalar/{blank}*           { return CATEGORY_SCALAR; }
<DEFAULTMAP_STATE>aggregate/{blank}*        { return CATEGORY_AGGREGATE; }
<DEFAULTMAP_STATE>pointer/{blank}*          { return CATEGORY_POINTER; }
<DEFAULTMAP_STATE>all/{blank}*              { return CATEGORY_ALL; }
<DEFAULTMAP_STATE>allocatable/{blank}*      { return CATEGORY_ALLOCATABLE; }
<DEFAULTMAP_STATE>"("                       { return '('; }
<DEFAULTMAP_STATE>")"                       { yy_pop_state(); return ')'; }
<DEFAULTMAP_STATE>":"                       { return ':'; }
<DEFAULTMAP_STATE>{blank}*                  { ; }

<TO_STATE>"("                               {
                                              if (!b_within_variable_list) {
                                                b_within_variable_list = true;
                                                return '(';
                                              }
                                              yy_push_state(EXPR_STATE);
                                              prepare_expression_capture();
                                              tracked_unput('(');
                                            }
<TO_STATE>")"                               { b_within_variable_list = false; yy_pop_state(); return ')'; }
<TO_STATE>","                               { return ','; }
<TO_STATE>":"                               { return ':'; }
<TO_STATE>iterator/{blank}*"("              { yy_push_state(ITERATOR_CAPTURE_STATE); return TO_ITERATOR; }
<TO_STATE>mapper/{blank}*"("                { prepare_expression_capture(); yy_push_state(TO_MAPPER_STATE);return TO_MAPPER; }
<TO_STATE>present                           { return PRESENT; }
<TO_STATE>{blank}*                          { ; }
<TO_STATE>.                                 { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }


<TO_MAPPER_STATE>"("                        { return '('; }
<TO_MAPPER_STATE>")"                        { yy_pop_state(); return ')'; }
<TO_MAPPER_STATE>.                          { yy_push_state(EXPR_STATE); tracked_unput(yytext[0]); }

<FROM_STATE>"("                             {
                                              if (!b_within_variable_list) {
                                                b_within_variable_list = true;
                                                return '(';
                                              }
                                              yy_push_state(EXPR_STATE);
                                              prepare_expression_capture();
                                              tracked_unput('(');
                                            }
<FROM_STATE>")"                             { b_within_variable_list = false; yy_pop_state(); return ')'; }
<FROM_STATE>","                             { return ','; }
<FROM_STATE>":"                             { return ':'; }
<FROM_STATE>iterator/{blank}*"("            { yy_push_state(ITERATOR_CAPTURE_STATE); return FROM_ITERATOR; }
<FROM_STATE>mapper/{blank}*"("              { prepare_expression_capture(); yy_push_state(FROM_MAPPER_STATE);return FROM_MAPPER; }
<FROM_STATE>present                         { return PRESENT; }
<FROM_STATE>{blank}*                        { ; }
<FROM_STATE>.                               { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<FROM_MAPPER_STATE>"("                      { return '('; }
<FROM_MAPPER_STATE>")"                      { yy_pop_state(); return ')'; }
<FROM_MAPPER_STATE>.                        { yy_push_state(EXPR_STATE); tracked_unput(yytext[0]); }

<ENTER_STATE>"("                            {
                                               if (!b_within_variable_list) {
                                                 b_within_variable_list = true;
                                                 return '(';
                                               }
                                               yy_push_state(EXPR_STATE);
                                               prepare_expression_capture();
                                               tracked_unput('(');
                                             }
<ENTER_STATE>")"                            { b_within_variable_list = false; yy_pop_state(); return ')'; }
<ENTER_STATE>","                            { return ','; }
<ENTER_STATE>{blank}*                       { ; }
<ENTER_STATE>data/{blank}                   { yy_pop_state(); return DATA; }
<ENTER_STATE>data/{newline}                 { yy_pop_state(); return DATA; }
<ENTER_STATE>data/"("                       { yy_pop_state(); return DATA; }
<ENTER_STATE>data/","                       { yy_pop_state(); return DATA; }
<ENTER_STATE>data/")"                       { yy_pop_state(); return DATA; }
<ENTER_STATE>data/":"                       { yy_pop_state(); return DATA; }
<ENTER_STATE>data                          { yy_pop_state(); return DATA; }
<ENTER_STATE>.                              { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<THREAD_LIMIT_STATE>"("                     {
                                               if (!b_within_variable_list) {
                                                 b_within_variable_list = true;
                                               }
                                               return '(';
                                             }
<THREAD_LIMIT_STATE>")"                     { b_within_variable_list = false; yy_pop_state(); return ')'; }
<THREAD_LIMIT_STATE>","                     { return ','; }
<THREAD_LIMIT_STATE>{blank}*                { ; }
<THREAD_LIMIT_STATE>.                       { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<USES_ALLOCATORS_STATE>"("                                     { uses_allocators_paren_depth++; return '('; }
<USES_ALLOCATORS_STATE>","                                     { return ','; }
<USES_ALLOCATORS_STATE>")"                                     { if (uses_allocators_paren_depth > 0) { uses_allocators_paren_depth--; } if (uses_allocators_paren_depth == 0) { yy_pop_state(); } return ')'; }
<USES_ALLOCATORS_STATE>omp_default_mem_alloc/{blank}*"("       { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return DEFAULT_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_large_cap_mem_alloc/{blank}*"("     { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return LARGE_CAP_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_const_mem_alloc/{blank}*"("         { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return CONST_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_high_bw_mem_alloc/{blank}*"("       { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return HIGH_BW_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_low_lat_mem_alloc/{blank}*"("       { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return LOW_LAT_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_cgroup_mem_alloc/{blank}*"("        { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return CGROUP_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_pteam_mem_alloc/{blank}*"("         { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return PTEAM_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_thread_mem_alloc/{blank}*"("        { prepare_expression_capture(); yy_push_state(ALLOC_EXPR_STATE);return THREAD_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_default_mem_alloc       { return DEFAULT_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_large_cap_mem_alloc     { return LARGE_CAP_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_const_mem_alloc         { return CONST_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_high_bw_mem_alloc       { return HIGH_BW_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_low_lat_mem_alloc       { return LOW_LAT_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_cgroup_mem_alloc        { return CGROUP_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_pteam_mem_alloc         { return PTEAM_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_thread_mem_alloc        { return THREAD_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>traits                      { return TRAITS; }
<USES_ALLOCATORS_STATE>{blank}*                                { ; }
<USES_ALLOCATORS_STATE>.                                       { yy_push_state(EXPR_STATE); tracked_unput(yytext[0]); }

<ALLOC_EXPR_STATE>"("                        { uses_allocators_paren_depth++; return '('; }
<ALLOC_EXPR_STATE>")"                        { yy_pop_state(); return emit_expr_string_and_unput(')'); }
<ALLOC_EXPR_STATE>.                          { current_string.push_back(original_token_char()); }


<DEVICE_TYPE_STATE>host                      { return HOST; }
<DEVICE_TYPE_STATE>nohost                    { return NOHOST; }
<DEVICE_TYPE_STATE>any                       { return ANY; }
<DEVICE_TYPE_STATE>"("                       { return '('; }
<DEVICE_TYPE_STATE>")"                       { yy_pop_state(); return ')'; }
<DEVICE_TYPE_STATE>{blank}*                  { ; }
<DEVICE_TYPE_STATE>.                         { yy_push_state(INITIAL); tracked_unput(yytext[0]); } 

<MAP_STATE>always/{blank}*,                  { return MAP_MODIFIER_ALWAYS; }
<MAP_STATE>close/{blank}*,                   { return MAP_MODIFIER_CLOSE; }
<MAP_STATE>present/{blank}*,                 { return MAP_MODIFIER_PRESENT; }
<MAP_STATE>self/{blank}*,                    { return MAP_MODIFIER_SELF; }
<MAP_STATE>ref_ptee/{blank}*[,:]             { return MAP_REF_MODIFIER_REF_PTEE; }
<MAP_STATE>ref_ptr_ptee/{blank}*[,:]         { return MAP_REF_MODIFIER_REF_PTR_PTEE; }
<MAP_STATE>ref_ptr/{blank}*[,:]              { return MAP_REF_MODIFIER_REF_PTR; }
<MAP_STATE>mapper/{blank}*"("                { prepare_expression_capture(); yy_push_state(MAP_MAPPER_STATE);return MAP_MODIFIER_MAPPER; }
<MAP_STATE>iterator/{blank}*"("              { yy_push_state(ITERATOR_CAPTURE_STATE); return MAP_MODIFIER_ITERATOR; }
<MAP_STATE>"("                               {
                                               if (!b_within_variable_list) {
                                                 b_within_variable_list = true;
                                                 return '(';
                                               }
                                               yy_push_state(EXPR_STATE);
                                               prepare_expression_capture();
                                               tracked_unput('(');
                                             }
<MAP_STATE>")"                               { b_within_variable_list = false; yy_pop_state(); return ')'; }
<MAP_STATE>","                               { return ','; }
<MAP_STATE>":"                               { yy_push_state(MAP_VAR_STATE); return ':'; }
<MAP_STATE>to/{blank}*:                      { return MAP_TYPE_TO; }
<MAP_STATE>from/{blank}*:                    { return MAP_TYPE_FROM; }
<MAP_STATE>tofrom/{blank}*:                  { return MAP_TYPE_TOFROM; }
<MAP_STATE>storage/{blank}*:                 { return MAP_TYPE_STORAGE; }
<MAP_STATE>alloc/{blank}*:                   { return MAP_TYPE_ALLOC; }
<MAP_STATE>release/{blank}*:                 { return MAP_TYPE_RELEASE; }
<MAP_STATE>delete                            { return MAP_TYPE_DELETE; }
<MAP_STATE>present/{blank}*:                 { return MAP_TYPE_PRESENT; }
<MAP_STATE>self/{blank}*:                    { return MAP_TYPE_SELF; }
<MAP_STATE>{blank}*                          { ; }
<MAP_STATE>.                                 { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<MAP_VAR_STATE>"("                           {
                                               yy_push_state(EXPR_STATE);
                                               prepare_expression_capture();
                                               tracked_unput('(');
                                             }
<MAP_VAR_STATE>")"                           {
                                               b_within_variable_list = false;
                                               yy_pop_state();
                                               yy_pop_state();
                                               return ')';
                                             }
<MAP_VAR_STATE>","                           { return ','; }
<MAP_VAR_STATE>{blank}*                      { ; }
<MAP_VAR_STATE>.                             { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<MAP_MAPPER_STATE>"("                        { return '('; }
<MAP_MAPPER_STATE>")"                        { yy_pop_state(); return ')'; }
<MAP_MAPPER_STATE>.                          { yy_push_state(EXPR_STATE); tracked_unput(yytext[0]); }

<TASK_REDUCTION_STATE>"("                     { return '('; }
<TASK_REDUCTION_STATE>")"                     { yy_pop_state(); return ')'; }
<TASK_REDUCTION_STATE>","                     { return ','; }
<TASK_REDUCTION_STATE>":"                     { return ':'; }
<TASK_REDUCTION_STATE>"+"                     { return '+'; }
<TASK_REDUCTION_STATE>"-"                     { return '-'; }
<TASK_REDUCTION_STATE>"*"                     { return '*'; }
<TASK_REDUCTION_STATE>"&"                     { return '&'; }
<TASK_REDUCTION_STATE>"|"                     { return '|'; }
<TASK_REDUCTION_STATE>"^"                     { return '^'; }
<TASK_REDUCTION_STATE>"&&"                    { return LOGAND; }
<TASK_REDUCTION_STATE>"||"                    { return LOGOR; }
<TASK_REDUCTION_STATE>min/{blank}*:           { return MIN; }
<TASK_REDUCTION_STATE>max/{blank}*:           { return MAX; }
<TASK_REDUCTION_STATE>{blank}*                { ; }
<TASK_REDUCTION_STATE>.                       { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

<UPDATE_STATE>"("                             { return '('; }
<UPDATE_STATE>")"                             { yy_pop_state(); return ')'; }
<UPDATE_STATE>source                          { return SOURCE; }
<UPDATE_STATE>in                              { return IN; }
<UPDATE_STATE>out                             { return OUT; }
<UPDATE_STATE>inout                           { return INOUT; }
<UPDATE_STATE>mutexinoutset                   { return MUTEXINOUTSET; }
<UPDATE_STATE>depobj                          { return DEPOBJ; }
<UPDATE_STATE>sink                            { return SINK; }
<UPDATE_STATE>{blank}*                        { ; }
<UPDATE_STATE>.                               { yy_pop_state(); tracked_unput(yytext[0]); }

<EXPR_STATE>.                           { current_char = original_token_char();
                                            switch (current_char) {
                                                case '\n': {
                                                    break;
                                                }
                                                case '(': {
                                                    parenthesis_local_count++;
                                                    parenthesis_global_count++;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case ')': {
                                                    parenthesis_local_count--;
                                                    parenthesis_global_count--;
                                                    if (parenthesis_global_count == 0) {
                                                        yy_pop_state();
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(')');
                                                        }
                                                    } else {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case ',': {
                                                    if (current_string.empty()) {
                                                        clear_expression_buffer();
                                                        return ',';
                                                    } else {
                                                        const int open_paren =
                                                            std::count(current_string.begin(), current_string.end(), '(');
                                                        const int close_paren =
                                                            std::count(current_string.begin(), current_string.end(), ')');
                                                        const int open_brace =
                                                            std::count(current_string.begin(), current_string.end(), '{');
                                                        const int close_brace =
                                                            std::count(current_string.begin(), current_string.end(), '}');
                                                        const int open_bracket =
                                                            std::count(current_string.begin(), current_string.end(), '[');
                                                        const int close_bracket =
                                                            std::count(current_string.begin(), current_string.end(), ']');
                                                        if (parenthesis_local_count == 0 && brace_count == 0 &&
                                                            bracket_count == 0 && open_paren == close_paren &&
                                                            open_brace == close_brace && open_bracket == close_bracket) {
                                                            // Clauses with item-level grammar return to their clause
                                                            // lexer so the next allocator or induction keyword is
                                                            // recognized. Other comma-separated expression lists
                                                            // intentionally remain in EXPR_STATE for the next item.
                                                            if (yy_top_state() == USES_ALLOCATORS_STATE ||
                                                                yy_top_state() == INDUCTION_STATE ||
                                                                yy_top_state() == INIT_STATE) {
                                                                yy_pop_state();
                                                            }
                                                            return emit_expr_string_and_unput(',');
                                                        }
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case '[': {
                                                    bracket_count++;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case ']': {
                                                    bracket_count--;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case '{': {
                                                    brace_count++;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case '}': {
                                                    brace_count--;
                                                    if (brace_count == 0) {
                                                        yy_pop_state();
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput('}');
                                                        } else {
                                                            tracked_unput('}');
                                                        }
                                                    } else {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case '?': {
                                                    ternary_count++;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                    case ':': {
                                                    if (current_string.empty()) {
                                                        clear_expression_buffer();
                                                        return ':';
                                                    } else if (ternary_count > 0) {
                                                        ternary_count--;
                                                        current_string.push_back(current_char);
                                                    } else if (inside_quotes) {
                                                        current_string.push_back(current_char);
                                                    } else if (parenthesis_local_count > 0 || brace_count > 0) {
                                                        current_string.push_back(current_char);
                                                    } else if (bracket_count == 0) {
                                                        yy_pop_state();
                                                        return emit_expr_string_and_unput(':');
                                                    } else {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case ' ': {
                                                    if (parenthesis_global_count == 0 && !inside_quotes) {
                                                        yy_pop_state();
                                                        return emit_expr_string_no_unput();
                                                    } else if (inside_quotes) {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case '"': {
                                                    inside_quotes = !inside_quotes;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                default: {
                                                    if (current_char != ' ' || parenthesis_local_count != 0 || inside_quotes) {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                            }
                                        }
<ID_EXPR_STATE>.                           { current_char = original_token_char();
                                            switch (current_char) {
                                                case '(': {
                                                    parenthesis_local_count++;
                                                    parenthesis_global_count++;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case ')': {
                                                    parenthesis_local_count--;
                                                    parenthesis_global_count--;
                                                    if (parenthesis_global_count == 0) {
                                                        yy_pop_state();
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(')');
                                                        }
                                                    } else {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case '?': {
                                                    ternary_count++;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                case ':': {
                                                    if (current_string.empty()) {
                                                        clear_expression_buffer();
                                                        return ':';
                                                    } else if (ternary_count > 0) {
                                                        ternary_count--;
                                                        current_string.push_back(current_char);
                                                    } else {
                                                        int next_char = tracked_yyinput();
                                                        if (next_char != EOF) {
                                                            tracked_unput(next_char);
                                                        }
                                                        bool next_is_paren = (next_char == '(');
                                                        if (bracket_count == 0 && parenthesis_global_count == 0 && !next_is_paren) {
                                                            yy_pop_state();
                                                            return emit_expr_string_and_unput(':');
                                                        }
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case ' ': {
                                                    if (parenthesis_global_count == 0 && !inside_quotes) {
                                                        yy_pop_state();
                                                        openmp_lval.stype =
                                                            store_lexeme(current_string);
                                                        current_string.clear();
                                                        parenthesis_local_count = 0;
                                                        parenthesis_global_count = 1;
                                                        bracket_count = 0;
                                                        inside_quotes = false;
                                                        return EXPR_STRING;
                                                    }
                                                    else {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case '"': {
                                                    inside_quotes = !inside_quotes;
                                                    current_string.push_back(current_char);
                                                    break;
                                                }
                                                default: {
                                                    if (current_char != ' ' || parenthesis_local_count != 0 || inside_quotes) {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                            }
                                        }

<<EOF>>         {
                      if (!current_string.empty()) {
                        return emit_expr_string_no_unput();
                      }
                      return 0;
                }

expr            {return (EXPRESSION); }

{blank}*        ;
.               { yy_push_state(EXPR_STATE); prepare_expression_capture(yytext[0]); }

%%

static inline void push_apply_state() {
  apply_paren_depth.push_back(0);
  yy_push_state(APPLY_STATE);
}

static inline void pop_apply_state() {
  if (!apply_paren_depth.empty()) {
    apply_paren_depth.pop_back();
  }
  yy_pop_state();
}

static inline int &current_apply_paren_depth() {
  if (apply_paren_depth.empty()) {
    apply_paren_depth.push_back(0);
  }
  return apply_paren_depth.back();
}

static inline int tracked_yyinput() {
  int next_char = yyinput();
  if (next_char == EOF || !lexer_location_state.tracking_enabled) {
    return next_char;
  }

  record_lexer_position_before_advance();
  advance_lexer_position(static_cast<char>(next_char));
  return next_char;
}

static inline void tracked_unput(int ch) {
  unput(ch);
  rewind_lexer_position_for_unput(static_cast<char>(ch));
}

[[noreturn]] static void fail_allocate_capture(const char *kind,
                                               const char *detail) {
  std::cerr << "OMPPARSER_SYNTAX[allocate-" << kind << "]: " << detail
            << "\n";
  std::abort();
}

static inline bool allocate_capture_is_blank() {
  return current_string.empty() ||
         std::all_of(current_string.begin(), current_string.end(), [](char ch) {
           return std::isspace(static_cast<unsigned char>(ch));
         });
}

static inline void trim_allocate_expression_blanks() {
  const auto first = std::find_if_not(current_string.begin(),
                                      current_string.end(), [](char ch) {
                                        return std::isspace(
                                            static_cast<unsigned char>(ch));
                                      });
  const auto last = std::find_if_not(current_string.rbegin(),
                                     current_string.rend(), [](char ch) {
                                       return std::isspace(
                                           static_cast<unsigned char>(ch));
                                     }).base();
  if (first >= last) {
    current_string.clear();
    return;
  }
  current_string = std::string(first, last);
}

static inline bool closes_allocate_delimiter(char opening, char closing) {
  return (opening == '(' && closing == ')') ||
         (opening == '[' && closing == ']') ||
         (opening == '{' && closing == '}');
}

static inline int emit_allocate_expression_and_unput(char delimiter) {
  trim_allocate_expression_blanks();
  if (allocate_capture_is_blank()) {
    fail_allocate_capture("expression", "allocator or list item is empty");
  }
  if (allocate_expression_capture.quote != '\0' ||
      allocate_expression_capture.escaped ||
      !allocate_expression_capture.delimiters.empty() ||
      allocate_expression_capture.ternary_depth != 0) {
    fail_allocate_capture("expression",
                          "allocator or list item is unbalanced");
  }

  openmpSetExprParseMode(OMP_EXPR_PARSE_expression);
  openmp_lval.stype = store_lexeme(current_string);
  current_string.clear();
  allocate_expression_capture = {};
  yy_pop_state();
  tracked_unput(delimiter);
  return EXPR_STRING;
}

static inline void begin_allocate_expression_capture(char initial_char) {
  current_string.clear();
  allocate_expression_capture = {};
  if (consume_allocate_expression_char(initial_char) != 0) {
    fail_allocate_capture("expression",
                          "first expression character is a delimiter");
  }
}

static inline int consume_allocate_expression_char(char ch) {
  auto &state = allocate_expression_capture;
  if (state.escaped) {
    current_string.push_back(ch);
    state.escaped = false;
    return 0;
  }
  if (state.quote != '\0') {
    current_string.push_back(ch);
    if (ch == '\\') {
      state.escaped = true;
    } else if (ch == state.quote) {
      state.quote = '\0';
    }
    return 0;
  }

  if (ch == '\\') {
    current_string.push_back(ch);
    state.escaped = true;
    return 0;
  }
  if (ch == '\'' || ch == '"') {
    current_string.push_back(ch);
    state.quote = ch;
    return 0;
  }
  if (ch == '(' || ch == '[' || ch == '{') {
    current_string.push_back(ch);
    state.delimiters.push_back(ch);
    return 0;
  }
  if (ch == ')' || ch == ']' || ch == '}') {
    if (state.delimiters.empty()) {
      if (ch == ')') {
        return emit_allocate_expression_and_unput(ch);
      }
      fail_allocate_capture("expression", "closing delimiter is unmatched");
    }
    if (!closes_allocate_delimiter(state.delimiters.back(), ch)) {
      fail_allocate_capture("expression", "closing delimiter is mismatched");
    }
    state.delimiters.pop_back();
    current_string.push_back(ch);
    return 0;
  }
  if (ch == '?') {
    ++state.ternary_depth;
    current_string.push_back(ch);
    return 0;
  }
  if (ch == ':') {
    if (!state.delimiters.empty()) {
      if (state.ternary_depth > 0) {
        --state.ternary_depth;
      }
      current_string.push_back(ch);
      return 0;
    }
    if (state.ternary_depth > 0) {
      --state.ternary_depth;
      current_string.push_back(ch);
      return 0;
    }
    return emit_allocate_expression_and_unput(ch);
  }
  if (ch == ',' && state.delimiters.empty()) {
    if (state.ternary_depth != 0) {
      fail_allocate_capture("expression",
                            "conditional expression has no matching colon");
    }
    return emit_allocate_expression_and_unput(ch);
  }

  current_string.push_back(ch);
  return 0;
}

static inline void begin_allocate_modifier_capture(int modifier_token) {
  if (modifier_token != ALLOCATOR_MODIFIER &&
      modifier_token != ALIGN_MODIFIER) {
    fail_allocate_capture("modifier", "modifier token is invalid");
  }
  current_string.clear();
  allocate_modifier_capture = {};
  allocate_modifier_capture.delimiters.push_back('(');
  allocate_modifier_token = modifier_token;
}

static inline int consume_allocate_modifier_char(char ch) {
  auto &state = allocate_modifier_capture;
  if (state.delimiters.empty()) {
    fail_allocate_capture("modifier", "modifier capture is not active");
  }
  if (state.escaped) {
    current_string.push_back(ch);
    state.escaped = false;
    return 0;
  }
  if (state.quote != '\0') {
    current_string.push_back(ch);
    if (ch == '\\') {
      state.escaped = true;
    } else if (ch == state.quote) {
      state.quote = '\0';
    }
    return 0;
  }

  if (ch == '\\') {
    current_string.push_back(ch);
    state.escaped = true;
    return 0;
  }
  if (ch == '\'' || ch == '"') {
    current_string.push_back(ch);
    state.quote = ch;
    return 0;
  }
  if (ch == '(' || ch == '[' || ch == '{') {
    current_string.push_back(ch);
    state.delimiters.push_back(ch);
    return 0;
  }
  if (ch == ')' || ch == ']' || ch == '}') {
    if (!closes_allocate_delimiter(state.delimiters.back(), ch)) {
      fail_allocate_capture("modifier", "closing delimiter is mismatched");
    }
    if (state.delimiters.size() == 1) {
      if (ch != ')' || allocate_capture_is_blank()) {
        fail_allocate_capture("modifier",
                              "modifier expression is empty or unbalanced");
      }
      if (allocate_modifier_token != ALLOCATOR_MODIFIER &&
          allocate_modifier_token != ALIGN_MODIFIER) {
        fail_allocate_capture("modifier", "modifier token is unset");
      }
      const int modifier_token = allocate_modifier_token;
      allocate_modifier_token = 0;
      state = {};
      yy_pop_state();
      openmpSetExprParseMode(OMP_EXPR_PARSE_expression);
      openmp_lval.stype = store_lexeme(current_string);
      current_string.clear();
      return modifier_token;
    }
    state.delimiters.pop_back();
    current_string.push_back(ch);
    return 0;
  }

  current_string.push_back(ch);
  return 0;
}

/* Implementation of inline functions that use parser tokens */
static inline int emit_expr_string_and_unput(char ch) {
  openmpSetExprParseMode(OMP_EXPR_PARSE_expression);
  openmp_lval.stype = store_lexeme(current_string);
  clear_expression_buffer();
  tracked_unput(ch);
  return EXPR_STRING;
}

static inline int emit_expr_string_no_unput() {
  openmpSetExprParseMode(OMP_EXPR_PARSE_expression);
  openmp_lval.stype = store_lexeme(current_string);
  clear_expression_buffer();
  return EXPR_STRING;
}

/* yy_push_state can't be called outside of this file, provide a wrapper */
extern void openmp_parse_expr() { yy_push_state(EXPR_STATE); }

/* entry point invoked by callers to start scanning for a string */
extern void openmp_lexer_init(const char *str) {
  ompparserinput = str;
  original_input_string = str != nullptr ? str : "";
  lexer_location_state.tracking_enabled = true;
  reset_lexer_location_state();
  /* We have openmp_ suffix for all flex functions */
  openmp_restart(openmp_in);
}

extern void openmp_begin_type_string() {
  clear_expression_buffer();
  yy_push_state(TYPE_STR_STATE);
}

extern void openmp_begin_raw_expression() {
  clear_expression_buffer();
  yy_push_state(RAW_EXPR_STATE);
}

/* Standalone ompparser */
void start_lexer(const char *input, const char *original_input) {
  if (input == nullptr || original_input == nullptr ||
      std::strlen(input) != std::strlen(original_input)) {
    std::fprintf(stderr,
                 "REX_OMPPARSER_INVARIANT[lexer-source]: normalized and "
                 "original directives must have equal nonnull byte ranges\n");
    std::abort();
  }
  original_input_string = original_input;
  lexer_location_state.tracking_enabled = true;
  reset_lexer_location_state();
  yy_scan_string(input);
}

void end_lexer(void) {
  // If the lexer exited due to some error, the condition stack could be nonempty.
  // In this case, it has to be reset to the initial state manually, where
  // yy_start_stack_ptr == 0.
  while (yy_start_stack_ptr > 0) {
    yy_pop_state();
  };
  openmp_reset_lexer_flags();
  lexer_location_state.tracking_enabled = false;
  yy_delete_buffer(YY_CURRENT_BUFFER);
  lexeme_storage.clear();
  original_input_string.clear();
}
