/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

%option prefix="openmp_"
%option stack
%option noyy_top_state
%option caseless
%option reentrant
%option bison-bridge
%option bison-locations
%option noyywrap

%top{
#include "ompparser.hh"
#define YYSTYPE OPENMP_STYPE
#define YYLTYPE OPENMP_LTYPE
}

%x AFFINITY_EXPR_STATE
%x AFFINITY_ITERATOR_STATE
%x AFFINITY_STATE
%x ALIGNED_STATE
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
%x APPEND_ARGS_STATE
%x OPTIONAL_EXPR_STATE
%x INDUCTION_STATE
%x CONDITION_STATE
%x COPYIN_STATE
%x COPYPRIVATE_STATE
%x DEFAULTMAP_STATE
%x DEFAULT_STATE
%x DEPEND_EXPR_STATE
%x DEPEND_ITERATOR_STATE
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
%x NOCONTEXT_STATE
%x NOVARIANTS_STATE
%x ISA_STATE
%x LASTPRIVATE_STATE
%x LINEAR_STATE
%x MAPPER_STATE
%x MAP_MAPPER_STATE
%x MAP_ITERATOR_STATE
%x MAP_ITER_EXPR_STATE
%x MAP_STATE
%x MAP_VAR_STATE
%x MATCH_STATE
%x NONTEMPORAL_STATE
%x NUM_TEAMS_STATE
%x NUM_THREADS_STATE
%x ORDERED_STATE
%x ORDER_STATE
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
%x TO_ITERATOR_STATE
%x TO_ITER_EXPR_STATE
%x TO_STATE
%x TYPE_STR_STATE
%x UPDATE_STATE
%x USES_ALLOCATORS_STATE
%x VENDOR_STATE
%x WHEN_STATE

%{

#include "ompparser.hh"
#include "OpenMPIR.h"
#include "OpenMPParserInternal.h"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/* Moved from Makefile.am to the source file to work with --with-pch Liao
   12/10/2009 */
#define YY_NO_TOP_STATE
#define YY_NO_POP_STATE

/* Forward declaration - actual token values will be available after parser header */
static inline int emit_expr_string_and_unput(
    char ch, OPENMP_STYPE *semantic_value, void *scanner);
static inline int emit_expr_string_no_unput(OPENMP_STYPE *semantic_value);

static thread_local const char *ompparserinput = nullptr;
static thread_local std::string current_string;
static thread_local int parenthesis_local_count = 0;
static thread_local int parenthesis_global_count = 1;
static thread_local int bracket_count;
static thread_local int brace_count = 0;
static thread_local int ternary_count = 0;
static thread_local bool inside_quotes = false;
static thread_local char active_quote = '\0';
static thread_local bool quote_escape = false;
static thread_local bool inside_raw_string = false;
static thread_local bool raw_string_body_started = false;
static thread_local std::string raw_string_delimiter;
static thread_local std::string raw_string_terminator;
static thread_local char current_char;

static thread_local bool compact_parallel_do = false;
static thread_local bool declare_target_underscore = false;
static thread_local bool compact_enddo = false;

static thread_local std::vector<int> apply_paren_depth;
static thread_local int induction_spec_paren_depth = 0;
static thread_local bool induction_step_waiting = false;
static thread_local int if_paren_depth = 0;
static thread_local int uses_allocators_paren_depth = 0;
static thread_local int init_parenthesis_depth = 0;
static thread_local int append_args_parenthesis_depth = 0;
static thread_local std::vector<std::unique_ptr<char[]>> lexeme_storage;
struct StoredLexemeRange {
  const char *lexeme = nullptr;
  ompparser::SourceRange range;
};
static thread_local std::vector<StoredLexemeRange> lexeme_ranges;
static thread_local std::string original_input;
static constexpr std::size_t InvalidOffset =
    static_cast<std::size_t>(-1);
static thread_local std::size_t expression_begin_offset = InvalidOffset;

struct LexerLocationState {
  int line = 1;
  int column = 1;
  int last_token_line = 0;
  int last_token_column = 0;
  std::size_t offset = 0;
  std::size_t last_token_offset = 0;
  bool tracking_enabled = false;
};

struct LexerPosition {
  int line;
  int column;
  std::size_t offset;
};

static thread_local LexerLocationState lexer_location_state;
static thread_local std::vector<LexerPosition> lexer_position_history;

static inline void reset_lexer_location_state() {
  lexer_location_state.line = 1;
  lexer_location_state.column = 1;
  lexer_location_state.last_token_line = 0;
  lexer_location_state.last_token_column = 0;
  lexer_location_state.offset = 0;
  lexer_location_state.last_token_offset = 0;
  lexer_position_history.clear();
  expression_begin_offset = InvalidOffset;
}

static inline void record_lexer_position_before_advance() {
  if (!lexer_location_state.tracking_enabled) {
    return;
  }

  lexer_position_history.push_back(
      {lexer_location_state.line, lexer_location_state.column,
       lexer_location_state.offset});
}

static inline bool pop_lexer_position_history(LexerPosition *position) {
  if (position == nullptr || lexer_position_history.empty()) {
    return false;
  }
  *position = lexer_position_history.back();
  lexer_position_history.pop_back();
  return true;
}

static inline void advance_lexer_position(char ch) {
  if (ch == '\n') {
    lexer_location_state.line++;
    lexer_location_state.column = 1;
  } else {
    lexer_location_state.column++;
  }
  lexer_location_state.offset++;
}

static inline void rewind_lexer_position_for_unput(char ch) {
  if (!lexer_location_state.tracking_enabled) {
    return;
  }

  LexerPosition previous_position;
  if (pop_lexer_position_history(&previous_position)) {
    lexer_location_state.line = previous_position.line;
    lexer_location_state.column = previous_position.column;
    lexer_location_state.offset = previous_position.offset;
    return;
  }

  if (ch == '\n') {
    if (lexer_location_state.line > 1) {
      lexer_location_state.line--;
      lexer_location_state.column = 1;
    }
    if (lexer_location_state.offset > 0) {
      lexer_location_state.offset--;
    }
    return;
  }

  if (lexer_location_state.column > 1) {
    lexer_location_state.column--;
  }
  if (lexer_location_state.offset > 0) {
    lexer_location_state.offset--;
  }
}

static inline void update_token_location(const char *text, size_t length,
                                         OPENMP_LTYPE *location) {
  if (!lexer_location_state.tracking_enabled || text == nullptr || length == 0) {
    return;
  }

  const int first_line = lexer_location_state.line;
  const int first_column = lexer_location_state.column;
  const std::size_t first_offset = lexer_location_state.offset;

  int last_line = first_line;
  int last_column = first_column;
  for (size_t index = 0; index < length; ++index) {
    last_line = lexer_location_state.line;
    last_column = lexer_location_state.column;
    record_lexer_position_before_advance();
    advance_lexer_position(text[index]);
  }

  if (location != nullptr) {
    location->first_line = first_line;
    location->first_column = first_column;
    location->last_line = last_line;
    location->last_column = last_column;
  }
  lexer_location_state.last_token_line = first_line;
  lexer_location_state.last_token_column = first_column;
  lexer_location_state.last_token_offset = first_offset;
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

static inline int tracked_yyinput(void *scanner);
static inline void tracked_unput(int ch, void *scanner);

#define YY_USER_ACTION                                                         \
  update_token_location(yytext, static_cast<size_t>(yyleng), yylloc);

static const char *store_lexeme(
    const std::string &text,
    std::size_t explicit_end_offset = InvalidOffset) {
  std::string spelling = text;
  std::size_t begin_offset = expression_begin_offset;
  std::size_t end_offset = InvalidOffset;
  if (expression_begin_offset != InvalidOffset) {
    end_offset = explicit_end_offset != InvalidOffset
                     ? explicit_end_offset
                     : lexer_location_state.last_token_offset;
    if (end_offset < expression_begin_offset) {
      end_offset = lexer_location_state.offset;
    }
    if (expression_begin_offset <= original_input.size() &&
        end_offset <= original_input.size() &&
        end_offset >= expression_begin_offset) {
      spelling = original_input.substr(expression_begin_offset,
                                       end_offset - expression_begin_offset);
    }
    expression_begin_offset = InvalidOffset;
  }
  auto buffer = std::make_unique<char[]>(spelling.size() + 1);
  std::memcpy(buffer.get(), spelling.c_str(), spelling.size() + 1);
  const char *ptr = buffer.get();
  lexeme_storage.push_back(std::move(buffer));
  if (begin_offset != InvalidOffset && end_offset != InvalidOffset &&
      begin_offset <= end_offset && end_offset <= original_input.size()) {
    auto positionAt = [](std::size_t offset) {
      ompparser::SourcePosition position;
      position.offset = static_cast<uint32_t>(offset);
      position.line = 1;
      position.column = 1;
      for (std::size_t index = 0;
           index < offset && index < original_input.size(); ++index) {
        if (original_input[index] == '\n') {
          ++position.line;
          position.column = 1;
        } else {
          ++position.column;
        }
      }
      return position;
    };
    StoredLexemeRange stored_range;
    stored_range.lexeme = ptr;
    stored_range.range.begin = positionAt(begin_offset);
    stored_range.range.end = positionAt(end_offset);
    lexeme_ranges.push_back(std::move(stored_range));
  }
  return ptr;
}

bool openmpGetLexemeSourceRange(const char *lexeme,
                               ompparser::SourceRange &range) {
  for (auto iterator = lexeme_ranges.rbegin();
       iterator != lexeme_ranges.rend(); ++iterator) {
    if (iterator->lexeme == lexeme) {
      range = iterator->range;
      return true;
    }
  }
  return false;
}

static inline void push_apply_state(void *yyscanner);
static inline void pop_apply_state(void *yyscanner);
static inline int &current_apply_paren_depth();

extern "C" void openmp_reset_lexer_flags() {
  compact_parallel_do = false;
  declare_target_underscore = false;
  compact_enddo = false;
  apply_paren_depth.clear();
  induction_spec_paren_depth = 0;
  induction_step_waiting = false;
  if_paren_depth = 0;
  uses_allocators_paren_depth = 0;
  init_parenthesis_depth = 0;
  append_args_parenthesis_depth = 0;
  current_string.clear();
  parenthesis_local_count = 0;
  parenthesis_global_count = 1;
  bracket_count = 0;
  brace_count = 0;
  ternary_count = 0;
  inside_quotes = false;
  active_quote = '\0';
  quote_escape = false;
  inside_raw_string = false;
  raw_string_body_started = false;
  raw_string_delimiter.clear();
  raw_string_terminator.clear();
  current_char = '\0';
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
  active_quote = '\0';
  quote_escape = false;
}

static inline bool consume_quoted_character(char ch) {
  if (!inside_quotes) {
    return false;
  }
  current_string.push_back(ch);
  if (inside_raw_string) {
    if (!raw_string_body_started) {
      if (ch == '(') {
        raw_string_body_started = true;
        raw_string_terminator = ")" + raw_string_delimiter + "\"";
      } else {
        raw_string_delimiter.push_back(ch);
      }
    } else if (current_string.size() >= raw_string_terminator.size() &&
               current_string.compare(current_string.size() -
                                          raw_string_terminator.size(),
                                      raw_string_terminator.size(),
                                      raw_string_terminator) == 0) {
      inside_quotes = false;
      inside_raw_string = false;
      raw_string_body_started = false;
      raw_string_delimiter.clear();
      raw_string_terminator.clear();
      active_quote = '\0';
    }
    return true;
  }
  if (quote_escape) {
    quote_escape = false;
  } else if (ch == '\\') {
    quote_escape = true;
  } else if (ch == active_quote) {
    inside_quotes = false;
    active_quote = '\0';
  }
  return true;
}

static inline void begin_quoted_fragment(char quote) {
  inside_quotes = true;
  active_quote = quote;
  quote_escape = false;
  inside_raw_string = quote == '"' && !current_string.empty() &&
                      current_string.back() == 'R';
  raw_string_body_started = false;
  raw_string_delimiter.clear();
  raw_string_terminator.clear();
  current_string.push_back(quote);
}

static inline void clear_expression_buffer() {
  current_string.clear();
  reset_expression_counters();
}

static inline void prepare_expression_capture() {
  clear_expression_buffer();
  expression_begin_offset = lexer_location_state.offset;
}

static inline void prepare_expression_capture(char initial_char) {
  clear_expression_buffer();
  expression_begin_offset = lexer_location_state.last_token_offset;
  current_string.push_back(initial_char);
  if (initial_char == '"' || initial_char == '\'') {
    inside_quotes = true;
    active_quote = initial_char;
  }
}

static inline void prepare_expression_capture_str(const char* initial_str) {
  clear_expression_buffer();
  expression_begin_offset = lexer_location_state.last_token_offset;
  current_string = initial_str;
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

static bool
firstprivate_modifier_list_follows_current_token(void *yyscanner) {
  std::vector<int> peeked_chars;
  std::string lookahead;
  for (std::size_t index = 0; index < 256; ++index) {
    const int ch = tracked_yyinput(yyscanner);
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
    tracked_unput(*it, yyscanner);
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
    if (firstprivate_modifier_list_follows_current_token(yyscanner)) {                  \
      return Token;                                                            \
    }                                                                          \
    yy_push_state(EXPR_STATE, yyscanner);                                                 \
    prepare_expression_capture_str(firstprivate_token_text.c_str());           \
  } while (0)

/* Liao 6/11/2010, OpenMP does not preclude the use of clause names as regular
   variable names. For example, num_threads could be a clause name or a
   variable in the variable list.

   We introduce a flag to indicate the context: within a variable list like
   (a,y,y) or outside of it We check '(' or ')' to set it to true or false as
   parsing proceed */
extern thread_local bool b_within_variable_list; /* = false; */

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

blank           [[:blank:]]
newline         [\n]
comment         [\/\/].*
id_char         [a-zA-Z0-9_]

%%

[!c*]$ompx      { return OMPX; }
[!c*]$omp       { ; }
#pragma         { ; }
omp/{blank}     { ; }
parallel/do     { compact_parallel_do = true; return PARALLEL; }
parallel        { compact_parallel_do = false; return PARALLEL; }
metadirective   { return METADIRECTIVE; }
task            { return TASK; }
if              { if_paren_depth = 0; yy_push_state(IF_STATE, yyscanner); return IF; }
simdlen         { yy_push_state(SIMDLEN_STATE, yyscanner); return SIMDLEN; }
simd/{blank}*\( { yy_push_state(SIMD_STATE, yyscanner); return SIMD; }
simd            { return SIMD; }
num_threads     { yy_push_state(NUM_THREADS_STATE, yyscanner); return NUM_THREADS; }
num_teams       { yy_push_state(NUM_TEAMS_STATE, yyscanner); return NUM_TEAMS; }
thread_limit            { yy_push_state(THREAD_LIMIT_STATE, yyscanner); return THREAD_LIMIT; }
default         { yy_push_state(DEFAULT_STATE, yyscanner); return DEFAULT; }
private         { yy_push_state(PRIVATE_STATE, yyscanner); return PRIVATE; }
firstprivate    { yy_push_state(FIRSTPRIVATE_STATE, yyscanner); return FIRSTPRIVATE; }
shared          { yy_push_state(SHARED_STATE, yyscanner); return SHARED; }
none            { return NONE; }
reduction       { yy_push_state(REDUCTION_STATE, yyscanner); return REDUCTION; }
copyin          { yy_push_state(COPYIN_STATE, yyscanner); return COPYIN; }
proc_bind       { yy_push_state(PROC_BIND_STATE, yyscanner); return PROC_BIND; }
allocate        { yy_push_state(ALLOCATE_STATE, yyscanner); return ALLOCATE; }
close           { return CLOSE; }
spread          { return SPREAD; } /* master should already be recognized */
teams           { return TEAMS; }
master          { return MASTER; } /*YAYING */
for             { return FOR; }
do              {
                  if (b_within_variable_list) {
                    yy_push_state(EXPR_STATE, yyscanner);
                    prepare_expression_capture_str("do");
                  } else {
                    return DO;
                  }
                }
lastprivate     { yy_push_state(LASTPRIVATE_STATE, yyscanner); return LASTPRIVATE; }
linear          { yy_push_state(LINEAR_STATE, yyscanner); return LINEAR; }
schedule        { yy_push_state(SCHEDULE_STATE, yyscanner); return SCHEDULE; }
collapse        { yy_push_state(COLLAPSE_STATE, yyscanner); return COLLAPSE; }
ordered/{blank}*\( { yy_push_state(ORDERED_STATE, yyscanner); return ORDERED; }
ordered         { return ORDERED; }
partial         { return PARTIAL; }
nowait          { return NOWAIT; }
full            { return FULL; }
order           { yy_push_state(ORDER_STATE, yyscanner); return ORDER; }
safelen         { yy_push_state(SAFELEN_STATE, yyscanner); return SAFELEN; }
nontemporal     { yy_push_state(NONTEMPORAL_STATE, yyscanner); return NONTEMPORAL; }
aligned         { yy_push_state(ALIGNED_STATE, yyscanner); return ALIGNED; }
align           { return ALIGN; }
declare/_target { declare_target_underscore = true; return DECLARE; }
"_target"       { return TARGET; }
declare         { declare_target_underscore = false; return DECLARE; }
uniform         { return UNIFORM; }
inbranch        { return INBRANCH; }
notinbranch     { return NOTINBRANCH; }
distribute      { return DISTRIBUTE; }
dist_schedule   { yy_push_state(DIST_SCHEDULE_STATE, yyscanner); return DIST_SCHEDULE; }
loop            { return LOOP; }
bind            { yy_push_state(BIND_STATE, yyscanner); return BIND; }
scan            { return SCAN; }
inclusive       { return INCLUSIVE; }
exclusive       { return EXCLUSIVE; }
sections        { return SECTIONS; }
section         { return SECTION; }
single          { return SINGLE; }
copyprivate     { yy_push_state(COPYPRIVATE_STATE, yyscanner); return COPYPRIVATE; }
cancel          { return CANCEL; }
workshare       { return WORKSHARE; }
taskgroup       { return TASKGROUP; }
allocator       { yy_push_state(ALLOCATOR_STATE, yyscanner); return ALLOCATOR; }
threadprivate/{blank}*\( { yy_push_state(THREADPRIVATE_STATE, yyscanner); return THREADPRIVATE; }
threadprivate   { return THREADPRIVATE; }
cancellation    { yy_push_state(CANCEL_STATE, yyscanner); return CANCELLATION; }
<CANCEL_STATE>{blank}+                 { ; }
<CANCEL_STATE>{newline}+               { ; }
<CANCEL_STATE>point                    { yy_pop_state(yyscanner); return POINT; }
<CANCEL_STATE>.                        { yy_pop_state(yyscanner); tracked_unput(yytext[0], yyscanner); }
variant         { return VARIANT; }
when            { yy_push_state(WHEN_STATE, yyscanner); return WHEN; }
match           { yy_push_state(MATCH_STATE, yyscanner); return MATCH; }
initializer     { yy_push_state(INITIALIZER_STATE, yyscanner); return INITIALIZER; }
mapper          { yy_push_state(MAPPER_STATE, yyscanner); return MAPPER; }
unroll          { return UNROLL;}
tile            { return TILE;}

error           { return ERROR_DIR; }
nothing         { return NOTHING; }
masked          { return MASKED; }
scope           { return SCOPE; }
interop/{blank}*\(  { yy_push_state(INTEROP_CLAUSE_STATE, yyscanner); return INTEROP; }
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
                  yy_push_state(INDUCTION_STATE, yyscanner);
                  return INDUCTION;
                }
induction/"("          {
                  induction_spec_paren_depth = 0;
                  yy_push_state(INDUCTION_STATE, yyscanner);
                  return INDUCTION;
                }

end/do          { compact_enddo = true; return END; }
end             { compact_enddo = false; return END; }
score           { return SCORE; }
condition       { yy_push_state(CONDITION_STATE, yyscanner); return CONDITION; }
kind            { return KIND; }
device_num/"("  { return DEVICE_NUM; }
host{id_char}+  { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture_str(yytext); }
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
isa             { yy_push_state(ISA_STATE, yyscanner); return ISA; }
arch            { yy_push_state(ARCH_STATE, yyscanner); return ARCH; }
vendor          { yy_push_state(VENDOR_STATE, yyscanner); return VENDOR; }
extension       { yy_push_state(EXTENSION_STATE, yyscanner); return EXTENSION; }

final           { yy_push_state(FINAL_STATE, yyscanner); return FINAL; }
untied/"("      { yy_push_state(OPTIONAL_EXPR_STATE, yyscanner); return UNTIED; }
untied          { return UNTIED; }
mergeable/"("   { yy_push_state(OPTIONAL_EXPR_STATE, yyscanner); return MERGEABLE; }
mergeable       { return MERGEABLE; }
in_reduction    { yy_push_state(IN_REDUCTION_STATE, yyscanner); return IN_REDUCTION; }
depend          { yy_push_state(DEPEND_STATE, yyscanner); return DEPEND; }
doacross        { yy_push_state(DOACROSS_STATE, yyscanner); return DOACROSS; }
priority        { return PRIORITY; }
affinity        { yy_push_state(AFFINITY_STATE, yyscanner); return AFFINITY; }
detach          { return DETACH; }

taskloop        { return TASKLOOP; }
taskyield       { return TASKYIELD; }
grainsize/{blank}       { yy_push_state(GRAINSIZE_STATE, yyscanner); return GRAINSIZE; }
grainsize/"("           { yy_push_state(GRAINSIZE_STATE, yyscanner); return GRAINSIZE; }
num_tasks/{blank}       { yy_push_state(NUM_TASKS_STATE, yyscanner); return NUM_TASKS; }
num_tasks/"("           { yy_push_state(NUM_TASKS_STATE, yyscanner); return NUM_TASKS; }
nogroup         { return NOGROUP; }

requires                  { return REQUIRES; } 
reverse_offload           { return REVERSE_OFFLOAD; }
unified_address           { return UNIFIED_ADDRESS; }
unified_shared_memory     { return UNIFIED_SHARED_MEMORY; }
atomic_default_mem_order  { yy_push_state(ATOMIC_DEFAULT_MEM_ORDER_STATE, yyscanner); return ATOMIC_DEFAULT_MEM_ORDER; }
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
device/{blank}            { yy_push_state(DEVICE_STATE, yyscanner); return DEVICE; }
device/{newline}          { yy_push_state(DEVICE_STATE, yyscanner); return DEVICE; }
device/"("                { yy_push_state(DEVICE_STATE, yyscanner); return DEVICE; }
device/","                { yy_push_state(DEVICE_STATE, yyscanner); return DEVICE; }
device/")"                { yy_push_state(DEVICE_STATE, yyscanner); return DEVICE; }
device/":"                { yy_push_state(DEVICE_STATE, yyscanner); return DEVICE; }
device                    { yy_push_state(DEVICE_STATE, yyscanner); return DEVICE; }
enter_/data{blank}         { yy_push_state(ENTER_STATE, yyscanner); return ENTER; }
enter                     { yy_push_state(ENTER_STATE, yyscanner); return ENTER; }
exit_/data{blank}          { return EXIT; }
exit                      { return EXIT; }
is_device_ptr             { return IS_DEVICE_PTR; }
has_device_addr           { return HAS_DEVICE_ADDR; }
defaultmap                { yy_push_state(DEFAULTMAP_STATE, yyscanner); return DEFAULTMAP; }
update                    { yy_push_state(UPDATE_STATE, yyscanner); return UPDATE; }

to                        { yy_push_state(TO_STATE, yyscanner); return TO; }
from                      { yy_push_state(FROM_STATE, yyscanner); return FROM; }
uses_allocators           { yy_push_state(USES_ALLOCATORS_STATE, yyscanner); uses_allocators_paren_depth = 0; return USES_ALLOCATORS; }
link                      { return LINK; }
device_type               { yy_push_state(DEVICE_TYPE_STATE, yyscanner); return DEVICE_TYPE; }
map                       { yy_push_state(MAP_STATE, yyscanner); return MAP; }
ext_                      { parenthesis_global_count = 0; yy_push_state(EXPR_STATE, yyscanner); return EXT_; }
barrier                   { return BARRIER; }
taskwait                  { return TASKWAIT; }
task_reduction            { yy_push_state(TASK_REDUCTION_STATE, yyscanner); return TASK_REDUCTION; }
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
sizes                     { yy_push_state(SIZES_STATE, yyscanner); return SIZES; }

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
transparent/"("           { yy_push_state(OPTIONAL_EXPR_STATE, yyscanner); return TRANSPARENT; }
transparent               { return TRANSPARENT; }
replayable/"("            { yy_push_state(OPTIONAL_EXPR_STATE, yyscanner); return REPLAYABLE; }
replayable                { return REPLAYABLE; }
threadset                 { return THREADSET; }
indirect                  { return INDIRECT; }
local/{blank}             { return LOCAL; }
local/"("                 { return LOCAL; }
init/{blank}              { yy_push_state(INIT_STATE, yyscanner); return INIT; }
init/"("                  { yy_push_state(INIT_STATE, yyscanner); return INIT; }
init_complete/"("         { yy_push_state(OPTIONAL_EXPR_STATE, yyscanner); return INIT_COMPLETE; }
init_complete             { return INIT_COMPLETE; }
safesync                  { return SAFESYNC; }
device_safesync           { return DEVICE_SAFESYNC; }
target_data/{blank}       { return TARGET_DATA_COMPOSITE; }  /* OpenMP 6.0 task-generating construct */
target_/enter_data{blank} { return TARGET; }
target_/exit_data{blank}  { return TARGET; }
target{id_char}+          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture_str(yytext); }
data{id_char}+            { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture_str(yytext); }
device{id_char}+          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture_str(yytext); }
all{id_char}+             { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture_str(yytext); }
cgroup{id_char}+          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture_str(yytext); }
memscope                  { return MEMSCOPE; }
looprange                 { yy_push_state(LOOPRANGE_STATE, yyscanner); return LOOPRANGE; }
permutation               { return PERMUTATION; }
counts                    { return COUNTS; }
inductor                  { return INDUCTOR; }
collector                 { return COLLECTOR; }
combiner                  { return COMBINER; }
adjust_args/{blank}       { yy_push_state(ADJUST_ARGS_STATE, yyscanner); return ADJUST_ARGS; }
adjust_args/"("           { yy_push_state(ADJUST_ARGS_STATE, yyscanner); return ADJUST_ARGS; }
append_args               { yy_push_state(APPEND_ARGS_STATE, yyscanner); return APPEND_ARGS; }
apply/{blank}             { push_apply_state(yyscanner); return APPLY; }
apply/"("                 { push_apply_state(yyscanner); return APPLY; }
traits                    { return TRAITS; }
no_openmp                 { return NO_OPENMP; }
no_openmp_constructs      { return NO_OPENMP_CONSTRUCTS; }
no_openmp_routines        { return NO_OPENMP_ROUTINES; }
no_parallelism            { return NO_PARALLELISM; }
nocontext/{blank}         { yy_push_state(NOCONTEXT_STATE, yyscanner); return NOCONTEXT; }
nocontext/"("             { yy_push_state(NOCONTEXT_STATE, yyscanner); return NOCONTEXT; }
novariants/{blank}        { yy_push_state(NOVARIANTS_STATE, yyscanner); return NOVARIANTS; }
novariants/"("            { yy_push_state(NOVARIANTS_STATE, yyscanner); return NOVARIANTS; }
use                       { return USE; }
all                       { return ALL; }
cgroup                    { return CGROUP; }


<RAW_EXPR_STATE>"("                        { parenthesis_local_count++; parenthesis_global_count++; current_string.push_back('('); }
<RAW_EXPR_STATE>")"                        {
                                             parenthesis_local_count--;
                                             parenthesis_global_count--;
                                             if (parenthesis_global_count == 0) {
                                               yy_pop_state(yyscanner);
                                               if (!current_string.empty()) {
                                                 return emit_expr_string_and_unput(')', yylval, yyscanner);
                                               }
                                             } else {
                                               current_string.push_back(')');
                                             }
                                           }
<RAW_EXPR_STATE>"{"                        { brace_count++; current_string.push_back('{'); }
<RAW_EXPR_STATE>"}"                        { brace_count--; current_string.push_back('}'); }
<RAW_EXPR_STATE>"["                        { bracket_count++; current_string.push_back('['); }
<RAW_EXPR_STATE>"]"                        { bracket_count--; current_string.push_back(']'); }
<RAW_EXPR_STATE>"\""                       { inside_quotes = !inside_quotes; current_string.push_back('"'); }
<RAW_EXPR_STATE>{newline}+                 { current_string.push_back('\n'); }
<RAW_EXPR_STATE>.                          { current_string.push_back(yytext[0]); }

"("             { return '('; }
")"             { return ')'; }
":"             { return ':'; }
"}"             { yy_pop_state(yyscanner); return '}'; }
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
                                              prepare_expression_capture_str(yytext);
                                              parenthesis_local_count = 1;
                                              parenthesis_global_count = 1;
                                              yy_push_state(ALLOCATOR_CALL_STATE, yyscanner);
                                            }
<ALLOCATE_STATE>align{blank}*\( {
                                              prepare_expression_capture_str(yytext);
                                              parenthesis_local_count = 1;
                                              parenthesis_global_count = 1;
                                              yy_push_state(ALLOCATOR_CALL_STATE, yyscanner);
                                            }
<ALLOCATE_STATE>[A-Za-z_][A-Za-z0-9_]*{blank}*/":" {
                                              size_t len = yyleng;
                                              while (len > 0 &&
                                                     std::isspace(static_cast<unsigned char>(yytext[len - 1]))) {
                                                len--;
                                              }
                                              std::string allocator(yytext, len);
                                              yylval->stype =
                                                  store_lexeme(allocator);
                                              return ALLOCATOR_IDENTIFIER;
                                            }
<ALLOCATE_STATE>"("                                   { return '('; }
<ALLOCATE_STATE>")"                                   { yy_pop_state(yyscanner); return ')'; }
<ALLOCATE_STATE>","                                   { return ','; }
<ALLOCATE_STATE>":"                                   { return ':'; }
<ALLOCATE_STATE>{blank}*                              { ; }
<ALLOCATE_STATE>.                                     { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ALLOCATOR_CALL_STATE>"(" {
                                              parenthesis_local_count++;
                                              parenthesis_global_count++;
                                              current_string.push_back('(');
                                            }
<ALLOCATOR_CALL_STATE>")" {
                                              current_string.push_back(')');
                                              parenthesis_local_count--;
                                              parenthesis_global_count--;
                                              if (parenthesis_global_count == 0) {
                                                std::string allocator_expr = current_string;
                                                clear_expression_buffer();
                                                yy_pop_state(yyscanner);
                                                yylval->stype =
                                                    store_lexeme(
                                                        allocator_expr,
                                                        lexer_location_state.offset);
                                                return ALLOCATOR_IDENTIFIER;
                                              }
                                            }
<ALLOCATOR_CALL_STATE>{blank}+ {
                                              current_string.append(yytext, yyleng);
                                            }
<ALLOCATOR_CALL_STATE>{newline}+ {
                                              current_string.append(yytext, yyleng);
                                            }
<ALLOCATOR_CALL_STATE>. {
                                              current_string.push_back(yytext[0]);
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
                                              yy_push_state(EXPR_STATE, yyscanner);
                                              prepare_expression_capture('(');
                                              parenthesis_local_count++;
                                              parenthesis_global_count++;
                                            }
<IF_STATE>")"                               { yy_pop_state(yyscanner); return ')'; }
<IF_STATE>":"                               { return ':'; }
<IF_STATE>{blank}*                          { ; }
<IF_STATE>.                                 { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<NOCONTEXT_STATE>dispatch{blank}*/:         { return DISPATCH; }
<NOCONTEXT_STATE>"("                       { return '('; }
<NOCONTEXT_STATE>")"                       { yy_pop_state(yyscanner); return ')'; }
<NOCONTEXT_STATE>":"                       { return ':'; }
<NOCONTEXT_STATE>{blank}*                  { ; }
<NOCONTEXT_STATE>.                         { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<NOVARIANTS_STATE>dispatch{blank}*/:        { return DISPATCH; }
<NOVARIANTS_STATE>"("                      { return '('; }
<NOVARIANTS_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<NOVARIANTS_STATE>":"                      { return ':'; }
<NOVARIANTS_STATE>{blank}*                 { ; }
<NOVARIANTS_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<INTEROP_CLAUSE_STATE>dispatch{blank}*/:    { return DISPATCH; }
<INTEROP_CLAUSE_STATE>"("                  { return '('; }
<INTEROP_CLAUSE_STATE>")"                  { yy_pop_state(yyscanner); return ')'; }
<INTEROP_CLAUSE_STATE>","                  { return ','; }
<INTEROP_CLAUSE_STATE>":"                  { return ':'; }
<INTEROP_CLAUSE_STATE>{blank}*             { ; }
<INTEROP_CLAUSE_STATE>.                    { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }


<PROC_BIND_STATE>master                     { return MASTER; }
<PROC_BIND_STATE>primary                    { return PRIMARY; }
<PROC_BIND_STATE>close                      { return CLOSE; }
<PROC_BIND_STATE>spread                     { return SPREAD; }
<PROC_BIND_STATE>"("                        { return '('; }
<PROC_BIND_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<PROC_BIND_STATE>{blank}*                   { ; }
<PROC_BIND_STATE>.                          { return -1; }

<DEFAULT_STATE>shared                       { return SHARED; }
<DEFAULT_STATE>none                         { return NONE; }
<DEFAULT_STATE>firstprivate                 { return FIRSTPRIVATE; }
<DEFAULT_STATE>private                      { return PRIVATE; }
<DEFAULT_STATE>scalar                       { return CATEGORY_SCALAR; }
<DEFAULT_STATE>aggregate                    { return CATEGORY_AGGREGATE; }
<DEFAULT_STATE>pointer                      { return CATEGORY_POINTER; }
<DEFAULT_STATE>all                          { return CATEGORY_ALL; }
<DEFAULT_STATE>allocatable                  { return CATEGORY_ALLOCATABLE; }
<DEFAULT_STATE>"("                          { return '('; }
<DEFAULT_STATE>")"                          { yy_pop_state(yyscanner); return ')'; }
<DEFAULT_STATE>":"                          { return ':'; }
<DEFAULT_STATE>{blank}*                     { ; }
<DEFAULT_STATE>.                            { yy_push_state(INITIAL, yyscanner); tracked_unput(yytext[0], yyscanner); } /* So far, only for default in metadirective meaning that a new directive is coming up. */

<ORDER_STATE>reproducible                   { return REPRODUCIBLE; }
<ORDER_STATE>unconstrained                  { return UNCONSTRAINED; }
<ORDER_STATE>concurrent                     { return CONCURRENT; }
<ORDER_STATE>":"                            { return ':'; }
<ORDER_STATE>"("                            { return '('; }
<ORDER_STATE>")"                            { yy_pop_state(yyscanner); return ')'; }
<ORDER_STATE>{blank}*                       { ; }
<ORDER_STATE>.                              { yy_push_state(INITIAL, yyscanner); }

<REDUCTION_STATE>inscan/{blank}*,           { return MODIFIER_INSCAN; }
<REDUCTION_STATE>task/{blank}*,             { return MODIFIER_TASK; }
<REDUCTION_STATE>default/{blank}*,          { return MODIFIER_DEFAULT; }
<REDUCTION_STATE>"("                        { return '('; }
<REDUCTION_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
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
<REDUCTION_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<SIMD_STATE>"("                             { yy_push_state(EXPR_STATE, yyscanner); return '('; }
<SIMD_STATE>")"                             { yy_pop_state(yyscanner); return ')'; }
<SIMD_STATE>{blank}*                        { ; }

<THREADPRIVATE_STATE>"("                    { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(); return '('; }
<THREADPRIVATE_STATE>")"                    { yy_pop_state(yyscanner); return ')'; }
<THREADPRIVATE_STATE>{blank}*               { ; }

<PRIVATE_STATE>"("                          { return '('; }
<PRIVATE_STATE>")"                          { yy_pop_state(yyscanner); return ')'; }
<PRIVATE_STATE>{blank}*                     { ; }
<PRIVATE_STATE>.                            { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<FIRSTPRIVATE_STATE>"("                     { return '('; }
<FIRSTPRIVATE_STATE>")"                     { yy_pop_state(yyscanner); return ')'; }
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
<FIRSTPRIVATE_STATE>.                       { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<SHARED_STATE>"("                           { return '('; }
<SHARED_STATE>")"                           { yy_pop_state(yyscanner); return ')'; }
<SHARED_STATE>{blank}*                      { ; }
<SHARED_STATE>.                             { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<COPYPRIVATE_STATE>"("                      { return '('; }
<COPYPRIVATE_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<COPYPRIVATE_STATE>{blank}*                 { ; }
<COPYPRIVATE_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<COPYIN_STATE>"("                           { return '('; }
<COPYIN_STATE>")"                           { yy_pop_state(yyscanner); return ')'; }
<COPYIN_STATE>{blank}*                      { ; }
<COPYIN_STATE>.                             { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<LASTPRIVATE_STATE>conditional/{blank}*:    { return MODIFIER_CONDITIONAL; }
<LASTPRIVATE_STATE>"("                      { return '('; }
<LASTPRIVATE_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<LASTPRIVATE_STATE>":"                      { return ':'; }
<LASTPRIVATE_STATE>{blank}*                 { ; }
<LASTPRIVATE_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<LINEAR_STATE>"("                           { return '('; }
<LINEAR_STATE>")"                           { yy_pop_state(yyscanner); return ')'; }
<LINEAR_STATE>val/{blank}*[(),]               { return MODOFIER_VAL; }
<LINEAR_STATE>ref/{blank}*[(),]               { return MODOFIER_REF; }
<LINEAR_STATE>uval/{blank}*[(),]              { return MODOFIER_UVAL; }
<LINEAR_STATE>":"                           { return ':'; }
<LINEAR_STATE>","                           { return ','; }
<LINEAR_STATE>{blank}*                      { ; }
<LINEAR_STATE>.                             { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

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
<SCHEDULE_STATE>")"                         { yy_pop_state(yyscanner); return ')'; }
<SCHEDULE_STATE>{blank}*                    { ; }
<SCHEDULE_STATE>.                           { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<COLLAPSE_STATE>"("                         { return '('; }
<COLLAPSE_STATE>")"                         { yy_pop_state(yyscanner); return ')'; }
<COLLAPSE_STATE>{blank}*                    { ; }
<COLLAPSE_STATE>.                           { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<SIZES_STATE>"("                            { return '('; }
<SIZES_STATE>")"                            { yy_pop_state(yyscanner); return ')'; }
<SIZES_STATE>","                            { return ','; }
<SIZES_STATE>{blank}*                       { ; }
<SIZES_STATE>.                              { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<LOOPRANGE_STATE>"("                        { return '('; }
<LOOPRANGE_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<LOOPRANGE_STATE>","                        { return ','; }
<LOOPRANGE_STATE>{blank}*                   { ; }
<LOOPRANGE_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<INIT_STATE>"("                             { init_parenthesis_depth++; return '('; }
<INIT_STATE>")"                             {
                                              if (init_parenthesis_depth > 0) {
                                                init_parenthesis_depth--;
                                              }
                                              if (init_parenthesis_depth == 0) {
                                                yy_pop_state(yyscanner);
                                              }
                                              return ')';
                                            }
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
<INIT_STATE>.                               { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<APPEND_ARGS_STATE>"("                      { append_args_parenthesis_depth++; return '('; }
<APPEND_ARGS_STATE>")"                      {
                                              if (append_args_parenthesis_depth > 0) {
                                                append_args_parenthesis_depth--;
                                              }
                                              if (append_args_parenthesis_depth == 0) {
                                                yy_pop_state(yyscanner);
                                              }
                                              return ')';
                                            }
<APPEND_ARGS_STATE>":"                      { return ':'; }
<APPEND_ARGS_STATE>","                      { return ','; }
<APPEND_ARGS_STATE>prefer_type{blank}*/"("  { return PREFER_TYPE; }
<APPEND_ARGS_STATE>mutexinoutset{blank}*/"(" { return MUTEXINOUTSET; }
<APPEND_ARGS_STATE>inoutset{blank}*/"("      { return INOUTSET; }
<APPEND_ARGS_STATE>inout{blank}*/"("         { return INOUT; }
<APPEND_ARGS_STATE>out{blank}*/"("           { return OUT; }
<APPEND_ARGS_STATE>in{blank}*/"("            { return IN; }
<APPEND_ARGS_STATE>targetsync{blank}*/[,:)]  { return TARGETSYNC; }
<APPEND_ARGS_STATE>target{blank}*/[,:)]      { return TARGET; }
<APPEND_ARGS_STATE>depobj{blank}*/[,:)]      { return DEPOBJ; }
<APPEND_ARGS_STATE>interop                   { return INTEROP; }
<APPEND_ARGS_STATE>{blank}*                  { ; }
<APPEND_ARGS_STATE>.                         { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<APPLY_STATE>"("                            { current_apply_paren_depth()++; return '('; }
<APPLY_STATE>")"                            {
                                              if (current_apply_paren_depth() > 0) {
                                                current_apply_paren_depth()--;
                                              }
                                              if (current_apply_paren_depth() == 0) {
                                                pop_apply_state(yyscanner);
                                              }
                                              return ')';
                                            }
<APPLY_STATE>":"                            { return ':'; }
<APPLY_STATE>","                            { return ','; }
<APPLY_STATE>unroll/partial                 { return UNROLL; }
<APPLY_STATE>unroll/full                    { return UNROLL; }
<APPLY_STATE>unroll                         { return UNROLL; }
<APPLY_STATE>partial                        { return PARTIAL; }
<APPLY_STATE>full                           { return FULL; }
<APPLY_STATE>reverse                        { return REVERSE; }
<APPLY_STATE>interchange                    { return INTERCHANGE; }
<APPLY_STATE>nothing                        { return NOTHING; }
<APPLY_STATE>tile                           { return TILE; }
<APPLY_STATE>sizes                          { return SIZES; }
<APPLY_STATE>apply/{blank}                  { push_apply_state(yyscanner); return APPLY; }
<APPLY_STATE>apply/"("                      { push_apply_state(yyscanner); return APPLY; }
<APPLY_STATE>{blank}*                       { ; }
<APPLY_STATE>.                              { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ADJUST_ARGS_STATE>"("                      { return '('; }
<ADJUST_ARGS_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<ADJUST_ARGS_STATE>":"                      { return ':'; }
<ADJUST_ARGS_STATE>","                      { return ','; }
<ADJUST_ARGS_STATE>need_device_addr/{blank}*: { return NEED_DEVICE_ADDR; }
<ADJUST_ARGS_STATE>need_device_ptr/{blank}*: { return NEED_DEVICE_PTR; }
<ADJUST_ARGS_STATE>nothing/{blank}*:         { return NOTHING; }
<ADJUST_ARGS_STATE>{blank}*                 { ; }
<ADJUST_ARGS_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<OPTIONAL_EXPR_STATE>"("                    { return '('; }
<OPTIONAL_EXPR_STATE>")"                    { yy_pop_state(yyscanner); return ')'; }
<OPTIONAL_EXPR_STATE>{blank}*               { ; }
<OPTIONAL_EXPR_STATE>.                      { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<INDUCTION_STATE>step/{blank}*\(            { induction_step_waiting = true; return STEP; }
<INDUCTION_STATE>"("                         {
                                              induction_spec_paren_depth++;
                                              if (induction_step_waiting) {
                                                // After step(, we need to capture expression
                                                induction_step_waiting = false;
                                                yy_push_state(EXPR_STATE, yyscanner);
                                                return '(';
                                              }
                                              return '(';
                                            }
<INDUCTION_STATE>")"                         {
                                              if (induction_spec_paren_depth > 0) {
                                                induction_spec_paren_depth--;
                                              }
                                              if (induction_spec_paren_depth == 0) {
                                                yy_pop_state(yyscanner);
                                              }
                                              return ')';
                                            }
<INDUCTION_STATE>","                         { return ','; }
<INDUCTION_STATE>":"                         { return ':'; }
<INDUCTION_STATE>{blank}*                    { ; }
<INDUCTION_STATE>.                           { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ORDERED_STATE>"("                          { yy_push_state(EXPR_STATE, yyscanner); return '('; }
<ORDERED_STATE>")"                          { yy_pop_state(yyscanner); return ')'; }
<ORDERED_STATE>{blank}*                     { ; }

<SIMDLEN_STATE>"("                          { return '('; }
<SIMDLEN_STATE>")"                          { yy_pop_state(yyscanner); return ')'; }
<SIMDLEN_STATE>{blank}*                     { ; }
<SIMDLEN_STATE>.                            { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<SAFELEN_STATE>"("                          { return '('; }
<SAFELEN_STATE>")"                          { yy_pop_state(yyscanner); return ')'; }
<SAFELEN_STATE>{blank}*                     { ; }
<SAFELEN_STATE>.                            { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<NONTEMPORAL_STATE>"("                      { return '('; }
<NONTEMPORAL_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<NONTEMPORAL_STATE>{blank}*                 { ; }
<NONTEMPORAL_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<NUM_TEAMS_STATE>"("                        { return '('; }
<NUM_TEAMS_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<NUM_TEAMS_STATE>{blank}*                   { ; }
<NUM_TEAMS_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<NUM_THREADS_STATE>strict/{blank}*:         { return STRICT; }
<NUM_THREADS_STATE>"("                      { return '('; }
<NUM_THREADS_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<NUM_THREADS_STATE>":"                      { return ':'; }
<NUM_THREADS_STATE>{blank}*                 { ; }
<NUM_THREADS_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<GRAINSIZE_STATE>strict/{blank}*:            { return STRICT; }
<GRAINSIZE_STATE>"("                         { return '('; }
<GRAINSIZE_STATE>")"                         { yy_pop_state(yyscanner); return ')'; }
<GRAINSIZE_STATE>":"                         { return ':'; }
<GRAINSIZE_STATE>{blank}*                    { ; }
<GRAINSIZE_STATE>.                           { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<NUM_TASKS_STATE>strict/{blank}*:            { return STRICT; }
<NUM_TASKS_STATE>"("                         { return '('; }
<NUM_TASKS_STATE>")"                         { yy_pop_state(yyscanner); return ')'; }
<NUM_TASKS_STATE>":"                         { return ':'; }
<NUM_TASKS_STATE>{blank}*                    { ; }
<NUM_TASKS_STATE>.                           { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ALIGNED_STATE>"("                          { return '('; }
<ALIGNED_STATE>":"                          { return ':'; }
<ALIGNED_STATE>")"                          { yy_pop_state(yyscanner); return ')'; }
<ALIGNED_STATE>{blank}*                     { ; }
<ALIGNED_STATE>.                            { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<DIST_SCHEDULE_STATE>static/{blank}*        {return STATIC; }
<DIST_SCHEDULE_STATE>"("                    { return '('; }
<DIST_SCHEDULE_STATE>","                    { return ','; }
<DIST_SCHEDULE_STATE>")"                    { yy_pop_state(yyscanner); return ')'; }
<DIST_SCHEDULE_STATE>{blank}*               { ; }
<DIST_SCHEDULE_STATE>.                      { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<BIND_STATE>teams                           { return TEAMS; }
<BIND_STATE>parallel                        { return PARALLEL; }
<BIND_STATE>thread                          { return THREAD; }
<BIND_STATE>"("                             { return '('; }
<BIND_STATE>")"                             { yy_pop_state(yyscanner); return ')'; }
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
<ALLOCATOR_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<ALLOCATOR_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<INITIALIZER_STATE>omp_priv                 { return OMP_PRIV; }
<INITIALIZER_STATE>"="                      { return '='; }
<INITIALIZER_STATE>{blank}*                 { ; }
<INITIALIZER_STATE>"("                      { return '('; }
<INITIALIZER_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<INITIALIZER_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<MAPPER_STATE>default                       { return IDENTIFIER_DEFAULT; }
<MAPPER_STATE>"::"                          { return DOUBLE_COLON; }
<MAPPER_STATE>":"                           { yy_push_state(ID_EXPR_STATE, yyscanner); return ':'; }
<MAPPER_STATE>map                           { yy_pop_state(yyscanner); yy_push_state(MAP_STATE, yyscanner); return MAP; }
<MAPPER_STATE>{blank}*                      { ; }
<MAPPER_STATE>"("                           { return '('; }
<MAPPER_STATE>")"                           { yy_pop_state(yyscanner); return ')'; }
<MAPPER_STATE>.                             { yy_push_state(ID_EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<TYPE_STR_STATE>"::"                        { current_string.append("::"); }
<TYPE_STR_STATE>.                           { current_char = yytext[0];
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
                                                        yy_pop_state(yyscanner);
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(')', yylval, yyscanner);
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
                                                    yy_pop_state(yyscanner);
                                                    if (!current_string.empty()) {
                                                        return emit_expr_string_and_unput(',', yylval, yyscanner);
                                                    }
                                                    return ',';
                                                }
                                                case ':': {
                                                    if (parenthesis_local_count == 0) {
                                                        yy_pop_state(yyscanner);
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(':', yylval, yyscanner);
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
<WHEN_STATE>":"                             { yy_push_state(INITIAL, yyscanner); return ':'; }
<WHEN_STATE>")"                             { yy_pop_state(yyscanner); return ')'; }
<WHEN_STATE>"="                             { return '='; }
<WHEN_STATE>"{"                             { yy_push_state(INITIAL, yyscanner); return '{'; } /* now parsrsing enters to pass a full construct, directive, condition, etc */
<WHEN_STATE>"}"                             { return '}'; }
<WHEN_STATE>","                             { ; }
<WHEN_STATE>user                            { return USER; }
<WHEN_STATE>construct                       { return CONSTRUCT; }
<WHEN_STATE>device                          { return DEVICE; }
<WHEN_STATE>target_device                   { return TARGET_DEVICE; }
<WHEN_STATE>implementation                  { yy_push_state(IMPLEMENTATION_STATE, yyscanner); return IMPLEMENTATION; }
<WHEN_STATE>{blank}*                        { ; }
<WHEN_STATE>.                               { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<IMPLEMENTATION_STATE>"("                            { return '('; }
<IMPLEMENTATION_STATE>","                            { return ','; }
<IMPLEMENTATION_STATE>")"                            { return ')'; }
<IMPLEMENTATION_STATE>"="                            { return '='; }
<IMPLEMENTATION_STATE>"{"                            { brace_count++; return '{'; }
<IMPLEMENTATION_STATE>"}"                            { yy_pop_state(yyscanner); return '}'; }
<IMPLEMENTATION_STATE>vendor/{blank}*\(              { yy_push_state(VENDOR_STATE, yyscanner); return VENDOR; }
<IMPLEMENTATION_STATE>extension/{blank}*\(           { yy_push_state(EXTENSION_STATE, yyscanner); return EXTENSION; }
<IMPLEMENTATION_STATE>requires/{blank}*\(            { return REQUIRES; }
<IMPLEMENTATION_STATE>{blank}*                       { ; }
<IMPLEMENTATION_STATE>.                              { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<MATCH_STATE>"("                            { return '('; }
<MATCH_STATE>":"                            { yy_push_state(INITIAL, yyscanner); return ':'; }
<MATCH_STATE>")"                            { yy_pop_state(yyscanner); return ')'; }
<MATCH_STATE>"="                            { return '='; }
<MATCH_STATE>"{"                            { yy_push_state(INITIAL, yyscanner); return '{'; } /* now parsing enters to pass a full construct, directive, condition, etc */
<MATCH_STATE>"}"                            { return '}'; }
<MATCH_STATE>","                            { return ','; }
<MATCH_STATE>user                           { return USER; }
<MATCH_STATE>construct                      { return CONSTRUCT; }
<MATCH_STATE>device                         { return DEVICE; }
<MATCH_STATE>target_device                  { return TARGET_DEVICE; }
<MATCH_STATE>implementation                 { yy_push_state(IMPLEMENTATION_STATE, yyscanner); return IMPLEMENTATION; }
<MATCH_STATE>{blank}*                       { ; }
<MATCH_STATE>.                              { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ISA_STATE>"("/score{blank}*\(              { return '('; }
<ISA_STATE>"("                              { parenthesis_global_count = 1; return '('; }
<ISA_STATE>")"                              { yy_pop_state(yyscanner); return ')'; }
<ISA_STATE>{blank}*                         { ; }
<ISA_STATE>score/{blank}*\(                 { yy_push_state(SCORE_STATE, yyscanner); return SCORE; }
<ISA_STATE>.                                { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ARCH_STATE>"("/score{blank}*\(             { return '('; }
<ARCH_STATE>"("                             { parenthesis_global_count = 1; return '('; }
<ARCH_STATE>")"                             { yy_pop_state(yyscanner); return ')'; }
<ARCH_STATE>{blank}*                        { ; }
<ARCH_STATE>score/{blank}*\(                { yy_push_state(SCORE_STATE, yyscanner); return SCORE; }
<ARCH_STATE>.                               { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<SCORE_STATE>"("{blank}*                    { yy_push_state(EXPR_STATE, yyscanner); parenthesis_global_count = 1; return '('; }
<SCORE_STATE>")"                            { return ')'; }
<SCORE_STATE>":"                            { yy_pop_state(yyscanner); parenthesis_global_count = 1; return ':'; }
<SCORE_STATE>{blank}*                       { ; }

<CONDITION_STATE>"("/score{blank}*\(        { return '('; }
<CONDITION_STATE>"("                        { parenthesis_global_count = 1; return '('; }
<CONDITION_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<CONDITION_STATE>{blank}*                   { ; }
<CONDITION_STATE>score/{blank}*\(           { yy_push_state(SCORE_STATE, yyscanner); return SCORE; }
<CONDITION_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<VENDOR_STATE>"("                           { return '('; }
<VENDOR_STATE>")"                           { yy_pop_state(yyscanner); return ')'; }
<VENDOR_STATE>{blank}*                      { ; }
<VENDOR_STATE>amd/{blank}*\)                { return AMD; }
<VENDOR_STATE>arm/{blank}*\)                { return ARM; }
<VENDOR_STATE>bsc/{blank}*\)                { return BSC; }
<VENDOR_STATE>cray/{blank}*\)               { return CRAY; }
<VENDOR_STATE>fujitsu/{blank}*\)            { return FUJITSU; }
<VENDOR_STATE>gnu/{blank}*\)                { return GNU; }
<VENDOR_STATE>ibm/{blank}*\)                { return IBM; }
<VENDOR_STATE>intel/{blank}*\)              { return INTEL; }
<VENDOR_STATE>llvm/{blank}*\)               { return LLVM; }
<VENDOR_STATE>nvidia/{blank}*\)             { return NVIDIA; }
<VENDOR_STATE>pgi/{blank}*\)                { return PGI; }
<VENDOR_STATE>ti/{blank}*\)                 { return TI; }
<VENDOR_STATE>unknown/{blank}*\)            { return UNKNOWN; }
<VENDOR_STATE>score/{blank}*\(              { yy_push_state(SCORE_STATE, yyscanner); return SCORE; }

<EXTENSION_STATE>"("                        { return '('; }
<EXTENSION_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<EXTENSION_STATE>{blank}*                   { ; }
<EXTENSION_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<IN_REDUCTION_STATE>"("                     { return '('; }
<IN_REDUCTION_STATE>")"                     { yy_pop_state(yyscanner); return ')'; }
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
<IN_REDUCTION_STATE>.                       { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<DEPEND_STATE>"("                           { return '('; }
<DEPEND_STATE>")"                           { yy_pop_state(yyscanner); return ')'; }
<DEPEND_STATE>","                           { return ','; }
<DEPEND_STATE>"="                           { return '='; }
<DEPEND_STATE>":"                           { yy_push_state(EXPR_STATE, yyscanner); return ':'; }
<DEPEND_STATE>iterator/{blank}*"("          { yy_push_state(DEPEND_ITERATOR_STATE, yyscanner); return MODIFIER_ITERATOR; }

<DEPEND_STATE>in                            { return IN; }
<DEPEND_STATE>out                           { return OUT; }
<DEPEND_STATE>inout                         { return INOUT; }
<DEPEND_STATE>inoutset                      { return INOUTSET; }
<DEPEND_STATE>mutexinoutset                 { return MUTEXINOUTSET; }
<DEPEND_STATE>depobj                        { return DEPOBJ; }
<DEPEND_STATE>source                        { return SOURCE; }
<DEPEND_STATE>sink                          { return SINK; }
<DEPEND_STATE>{blank}*                      { ; }
<DEPEND_STATE>.                             { yy_push_state(EXPR_STATE, yyscanner); tracked_unput(yytext[0], yyscanner); }

<DOACROSS_STATE>"("                         { return '('; }
<DOACROSS_STATE>")"                         { yy_pop_state(yyscanner); return ')'; }
<DOACROSS_STATE>":"                         { return ':'; }
<DOACROSS_STATE>source                      { return SOURCE; }
<DOACROSS_STATE>sink                        { return SINK; }
<DOACROSS_STATE>{blank}*                    { ; }
<DOACROSS_STATE>.                           { yy_push_state(EXPR_STATE, yyscanner); tracked_unput(yytext[0], yyscanner); }

<DEPEND_ITERATOR_STATE>"("                  { return '('; }
<DEPEND_ITERATOR_STATE>"="                  { return '='; }
<DEPEND_ITERATOR_STATE>","                  { return ','; }
<DEPEND_ITERATOR_STATE>":"                  { return ':'; }
<DEPEND_ITERATOR_STATE>")"                  { yy_pop_state(yyscanner); return ')'; }
<DEPEND_ITERATOR_STATE>{blank}*             { ; }
<DEPEND_ITERATOR_STATE>.                    { yy_push_state(DEPEND_EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<DEPEND_EXPR_STATE>"("{blank}*              { return '('; }
<DEPEND_EXPR_STATE>{blank}*")"              { yy_pop_state(yyscanner); return emit_expr_string_and_unput(')', yylval, yyscanner); }
<DEPEND_EXPR_STATE>","                      { yy_pop_state(yyscanner); return emit_expr_string_and_unput(',', yylval, yyscanner); }
<DEPEND_EXPR_STATE>{blank}                  { yy_pop_state(yyscanner); return emit_expr_string_no_unput(yylval); }
<DEPEND_EXPR_STATE>"="                      { yy_pop_state(yyscanner); return emit_expr_string_and_unput('=', yylval, yyscanner); }
<DEPEND_EXPR_STATE>":"                      { yy_pop_state(yyscanner); return emit_expr_string_and_unput(':', yylval, yyscanner); }
<DEPEND_EXPR_STATE>.                        { current_string.push_back(yytext[0]); }

<AFFINITY_STATE>"("                         { return '('; }
<AFFINITY_STATE>")"                         { yy_pop_state(yyscanner); return ')'; }
<AFFINITY_STATE>","                         { return ','; }
<AFFINITY_STATE>":"                         { return ':'; }
<AFFINITY_STATE>iterator/{blank}*"("        { yy_push_state(AFFINITY_ITERATOR_STATE, yyscanner); return MODIFIER_ITERATOR; }
<AFFINITY_STATE>{blank}*                    { ; }
<AFFINITY_STATE>.                           { yy_push_state(EXPR_STATE, yyscanner); tracked_unput(yytext[0], yyscanner); }

<AFFINITY_EXPR_STATE>"("{blank}*            { return '('; }
<AFFINITY_EXPR_STATE>{blank}*")"            { yy_pop_state(yyscanner); return emit_expr_string_and_unput(')', yylval, yyscanner); }
<AFFINITY_EXPR_STATE>{blank}                { yy_pop_state(yyscanner); return emit_expr_string_no_unput(yylval); }
<AFFINITY_EXPR_STATE>","                    { yy_pop_state(yyscanner); return emit_expr_string_and_unput(',', yylval, yyscanner); }
<AFFINITY_EXPR_STATE>"="                    { yy_pop_state(yyscanner); return emit_expr_string_and_unput('=', yylval, yyscanner); }
<AFFINITY_EXPR_STATE>":"                    { yy_pop_state(yyscanner); return emit_expr_string_and_unput(':', yylval, yyscanner); }
<AFFINITY_EXPR_STATE>.                      { current_string.push_back(yytext[0]); }

<AFFINITY_ITERATOR_STATE>"("                { return '('; }
<AFFINITY_ITERATOR_STATE>"="                { return '='; }
<AFFINITY_ITERATOR_STATE>":"                { return ':'; }
<AFFINITY_ITERATOR_STATE>","                { return ','; }
<AFFINITY_ITERATOR_STATE>")"                { yy_pop_state(yyscanner); return ')'; }
<AFFINITY_ITERATOR_STATE>{blank}*           { ; }
<AFFINITY_ITERATOR_STATE>.                  { yy_push_state(AFFINITY_EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<FINAL_STATE>"("                            { return '('; }
<FINAL_STATE>")"                            { yy_pop_state(yyscanner); return ')'; }
<FINAL_STATE>{blank}*                       { ; }
<FINAL_STATE>.                              { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ATOMIC_DEFAULT_MEM_ORDER_STATE>seq_cst     { return SEQ_CST; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>acq_rel     { return ACQ_REL; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>relaxed     { return RELAXED; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>"("         { return '('; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>")"         { yy_pop_state(yyscanner); return ')'; }
<ATOMIC_DEFAULT_MEM_ORDER_STATE>{blank}*    { ; }

<DEVICE_STATE>ancestor/{blank}*:            { return ANCESTOR; }
<DEVICE_STATE>device_num/{blank}*:          { return DEVICE_NUM; }
<DEVICE_STATE>"("                           { return '('; }
<DEVICE_STATE>")"                           { yy_pop_state(yyscanner); return ')'; }
<DEVICE_STATE>":"                           { return ':'; }
<DEVICE_STATE>{blank}*                      { ; }
<DEVICE_STATE>.                             { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

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
<DEFAULTMAP_STATE>")"                       { yy_pop_state(yyscanner); return ')'; }
<DEFAULTMAP_STATE>":"                       { return ':'; }
<DEFAULTMAP_STATE>{blank}*                  { ; }

<TO_STATE>"("                               {
                                              if (!b_within_variable_list) {
                                                b_within_variable_list = true;
                                                return '(';
                                              }
                                              yy_push_state(EXPR_STATE, yyscanner);
                                              tracked_unput('(', yyscanner);
                                              prepare_expression_capture();
                                            }
<TO_STATE>")"                               { b_within_variable_list = false; yy_pop_state(yyscanner); return ')'; }
<TO_STATE>","                               { return ','; }
<TO_STATE>":"                               { return ':'; }
<TO_STATE>iterator/{blank}*"("              { yy_push_state(TO_ITERATOR_STATE, yyscanner); return TO_ITERATOR; }
<TO_STATE>mapper/{blank}*"("                { yy_push_state(TO_MAPPER_STATE, yyscanner); return TO_MAPPER; }
<TO_STATE>present                           { return PRESENT; }
<TO_STATE>{blank}*                          { ; }
<TO_STATE>.                                 { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }


<TO_MAPPER_STATE>"("                        { return '('; }
<TO_MAPPER_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<TO_MAPPER_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<TO_ITERATOR_STATE>"("                      { return '('; }
<TO_ITERATOR_STATE>"="                      { return '='; }
<TO_ITERATOR_STATE>","                      { return ','; }
<TO_ITERATOR_STATE>":"                      { return ':'; }
<TO_ITERATOR_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<TO_ITERATOR_STATE>{blank}*                 { ; }
<TO_ITERATOR_STATE>.                        { yy_push_state(TO_ITER_EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<TO_ITER_EXPR_STATE>"("{blank}*             { return '('; }
<TO_ITER_EXPR_STATE>{blank}*")"             { yy_pop_state(yyscanner); return emit_expr_string_and_unput(')', yylval, yyscanner); }
<TO_ITER_EXPR_STATE>","                     { yy_pop_state(yyscanner); return emit_expr_string_and_unput(',', yylval, yyscanner); }
<TO_ITER_EXPR_STATE>{blank}                 { yy_pop_state(yyscanner); return emit_expr_string_no_unput(yylval); }
<TO_ITER_EXPR_STATE>"="                     { yy_pop_state(yyscanner); return emit_expr_string_and_unput('=', yylval, yyscanner); }
<TO_ITER_EXPR_STATE>":"                     { yy_pop_state(yyscanner); return emit_expr_string_and_unput(':', yylval, yyscanner); }
<TO_ITER_EXPR_STATE>.                       { current_string.push_back(yytext[0]); }

<FROM_STATE>"("                             {
                                              if (!b_within_variable_list) {
                                                b_within_variable_list = true;
                                                return '(';
                                              }
                                              yy_push_state(EXPR_STATE, yyscanner);
                                              tracked_unput('(', yyscanner);
                                              prepare_expression_capture();
                                            }
<FROM_STATE>")"                             { b_within_variable_list = false; yy_pop_state(yyscanner); return ')'; }
<FROM_STATE>","                             { return ','; }
<FROM_STATE>":"                             { return ':'; }
<FROM_STATE>mapper/{blank}*"("              { yy_push_state(FROM_MAPPER_STATE, yyscanner); return FROM_MAPPER; }
<FROM_STATE>present                         { return PRESENT; }
<FROM_STATE>{blank}*                        { ; }
<FROM_STATE>.                               { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<FROM_MAPPER_STATE>"("                      { return '('; }
<FROM_MAPPER_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<FROM_MAPPER_STATE>.                        { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<ENTER_STATE>"("                            {
                                               if (!b_within_variable_list) {
                                                 b_within_variable_list = true;
                                                 return '(';
                                               }
                                               yy_push_state(EXPR_STATE, yyscanner);
                                               tracked_unput('(', yyscanner);
                                               prepare_expression_capture();
                                             }
<ENTER_STATE>")"                            { b_within_variable_list = false; yy_pop_state(yyscanner); return ')'; }
<ENTER_STATE>","                            { return ','; }
<ENTER_STATE>{blank}*                       { ; }
<ENTER_STATE>data/{blank}                   { yy_pop_state(yyscanner); return DATA; }
<ENTER_STATE>data/{newline}                 { yy_pop_state(yyscanner); return DATA; }
<ENTER_STATE>data/"("                       { yy_pop_state(yyscanner); return DATA; }
<ENTER_STATE>data/","                       { yy_pop_state(yyscanner); return DATA; }
<ENTER_STATE>data/")"                       { yy_pop_state(yyscanner); return DATA; }
<ENTER_STATE>data/":"                       { yy_pop_state(yyscanner); return DATA; }
<ENTER_STATE>data                          { yy_pop_state(yyscanner); return DATA; }
<ENTER_STATE>.                              { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<THREAD_LIMIT_STATE>"("                     {
                                               if (!b_within_variable_list) {
                                                 b_within_variable_list = true;
                                               }
                                               return '(';
                                             }
<THREAD_LIMIT_STATE>")"                     { b_within_variable_list = false; yy_pop_state(yyscanner); return ')'; }
<THREAD_LIMIT_STATE>","                     { return ','; }
<THREAD_LIMIT_STATE>{blank}*                { ; }
<THREAD_LIMIT_STATE>.                       { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<USES_ALLOCATORS_STATE>"("                                     { uses_allocators_paren_depth++; return '('; }
<USES_ALLOCATORS_STATE>","                                     { return ','; }
<USES_ALLOCATORS_STATE>")"                                     { if (uses_allocators_paren_depth > 0) { uses_allocators_paren_depth--; } if (uses_allocators_paren_depth == 0) { yy_pop_state(yyscanner); } return ')'; }
<USES_ALLOCATORS_STATE>omp_default_mem_alloc/{blank}*"("       { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return DEFAULT_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_large_cap_mem_alloc/{blank}*"("     { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return LARGE_CAP_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_const_mem_alloc/{blank}*"("         { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return CONST_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_high_bw_mem_alloc/{blank}*"("       { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return HIGH_BW_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_low_lat_mem_alloc/{blank}*"("       { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return LOW_LAT_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_cgroup_mem_alloc/{blank}*"("        { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return CGROUP_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_pteam_mem_alloc/{blank}*"("         { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return PTEAM_MEM_ALLOC; }
<USES_ALLOCATORS_STATE>omp_thread_mem_alloc/{blank}*"("        { yy_push_state(ALLOC_EXPR_STATE, yyscanner); return THREAD_MEM_ALLOC; }
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
<USES_ALLOCATORS_STATE>.                                       { yy_push_state(EXPR_STATE, yyscanner); tracked_unput(yytext[0], yyscanner); }

<ALLOC_EXPR_STATE>"("                        { uses_allocators_paren_depth++; prepare_expression_capture(); return '('; }
<ALLOC_EXPR_STATE>")"                        { yy_pop_state(yyscanner); return emit_expr_string_and_unput(')', yylval, yyscanner); }
<ALLOC_EXPR_STATE>.                          { current_string.push_back(yytext[0]); }


<DEVICE_TYPE_STATE>host                      { return HOST; }
<DEVICE_TYPE_STATE>nohost                    { return NOHOST; }
<DEVICE_TYPE_STATE>any                       { return ANY; }
<DEVICE_TYPE_STATE>"("                       { return '('; }
<DEVICE_TYPE_STATE>")"                       { yy_pop_state(yyscanner); return ')'; }
<DEVICE_TYPE_STATE>{blank}*                  { ; }
<DEVICE_TYPE_STATE>.                         { yy_push_state(INITIAL, yyscanner); tracked_unput(yytext[0], yyscanner); }

<MAP_STATE>always/{blank}*,                  { return MAP_MODIFIER_ALWAYS; }
<MAP_STATE>close/{blank}*,                   { return MAP_MODIFIER_CLOSE; }
<MAP_STATE>present/{blank}*,                 { return MAP_MODIFIER_PRESENT; }
<MAP_STATE>self/{blank}*,                    { return MAP_MODIFIER_SELF; }
<MAP_STATE>ref_ptee/{blank}*[,:]             { return MAP_REF_MODIFIER_REF_PTEE; }
<MAP_STATE>ref_ptr_ptee/{blank}*[,:]         { return MAP_REF_MODIFIER_REF_PTR_PTEE; }
<MAP_STATE>ref_ptr/{blank}*[,:]              { return MAP_REF_MODIFIER_REF_PTR; }
<MAP_STATE>mapper/{blank}*"("                { yy_push_state(MAP_MAPPER_STATE, yyscanner); return MAP_MODIFIER_MAPPER; }
<MAP_STATE>iterator/{blank}*"("              { yy_push_state(MAP_ITERATOR_STATE, yyscanner); return MAP_MODIFIER_ITERATOR; }
<MAP_STATE>"("                               {
                                               if (!b_within_variable_list) {
                                                 b_within_variable_list = true;
                                                 return '(';
                                               }
                                               yy_push_state(EXPR_STATE, yyscanner);
                                               tracked_unput('(', yyscanner);
                                               prepare_expression_capture();
                                             }
<MAP_STATE>")"                               { b_within_variable_list = false; yy_pop_state(yyscanner); return ')'; }
<MAP_STATE>","                               { return ','; }
<MAP_STATE>":"                               { yy_push_state(MAP_VAR_STATE, yyscanner); return ':'; }
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
<MAP_STATE>.                                 { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<MAP_VAR_STATE>"("                           {
                                               yy_push_state(EXPR_STATE, yyscanner);
                                               tracked_unput('(', yyscanner);
                                               prepare_expression_capture();
                                             }
<MAP_VAR_STATE>")"                           {
                                               b_within_variable_list = false;
                                               yy_pop_state(yyscanner);
                                               yy_pop_state(yyscanner);
                                               return ')';
                                             }
<MAP_VAR_STATE>","                           { return ','; }
<MAP_VAR_STATE>{blank}*                      { ; }
<MAP_VAR_STATE>.                             { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<MAP_MAPPER_STATE>"("                        { return '('; }
<MAP_MAPPER_STATE>")"                        { yy_pop_state(yyscanner); return ')'; }
<MAP_MAPPER_STATE>.                          { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<MAP_ITERATOR_STATE>"("                      { return '('; }
<MAP_ITERATOR_STATE>"="                      { return '='; }
<MAP_ITERATOR_STATE>","                      { return ','; }
<MAP_ITERATOR_STATE>":"                      { return ':'; }
<MAP_ITERATOR_STATE>")"                      { yy_pop_state(yyscanner); return ')'; }
<MAP_ITERATOR_STATE>{blank}*                 { ; }
<MAP_ITERATOR_STATE>.                        { yy_push_state(MAP_ITER_EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<MAP_ITER_EXPR_STATE>"("{blank}*             { return '('; }
<MAP_ITER_EXPR_STATE>{blank}*")"             { yy_pop_state(yyscanner); return emit_expr_string_and_unput(')', yylval, yyscanner); }
<MAP_ITER_EXPR_STATE>","                     { yy_pop_state(yyscanner); return emit_expr_string_and_unput(',', yylval, yyscanner); }
<MAP_ITER_EXPR_STATE>{blank}                 { yy_pop_state(yyscanner); return emit_expr_string_no_unput(yylval); }
<MAP_ITER_EXPR_STATE>"="                     { yy_pop_state(yyscanner); return emit_expr_string_and_unput('=', yylval, yyscanner); }
<MAP_ITER_EXPR_STATE>":"                     { yy_pop_state(yyscanner); return emit_expr_string_and_unput(':', yylval, yyscanner); }
<MAP_ITER_EXPR_STATE>.                       { current_string.push_back(yytext[0]); }

<TASK_REDUCTION_STATE>"("                     { return '('; }
<TASK_REDUCTION_STATE>")"                     { yy_pop_state(yyscanner); return ')'; }
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
<TASK_REDUCTION_STATE>.                       { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

<UPDATE_STATE>"("                             { return '('; }
<UPDATE_STATE>")"                             { yy_pop_state(yyscanner); return ')'; }
<UPDATE_STATE>source                          { return SOURCE; }
<UPDATE_STATE>in                              { return IN; }
<UPDATE_STATE>out                             { return OUT; }
<UPDATE_STATE>inout                           { return INOUT; }
<UPDATE_STATE>mutexinoutset                   { return MUTEXINOUTSET; }
<UPDATE_STATE>depobj                          { return DEPOBJ; }
<UPDATE_STATE>sink                            { return SINK; }
<UPDATE_STATE>{blank}*                        { ; }
<UPDATE_STATE>.                               { yy_pop_state(yyscanner); tracked_unput(yytext[0], yyscanner); }

<EXPR_STATE>.                           { current_char = yytext[0];
                                            if (consume_quoted_character(current_char)) {
                                              /* Host-language delimiters inside a literal
                                                 never terminate an OpenMP operand. */
                                            } else {
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
                                                        yy_pop_state(yyscanner);
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(')', yylval, yyscanner);
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
                                                        return emit_expr_string_and_unput(',', yylval, yyscanner);
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
                                                        yy_pop_state(yyscanner);
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput('}', yylval, yyscanner);
                                                        } else {
                                                            tracked_unput('}', yyscanner);
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
                                                        yy_pop_state(yyscanner);
                                                        return emit_expr_string_and_unput(':', yylval, yyscanner);
                                                    } else {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case ' ': {
                                                    if (parenthesis_global_count == 0 && !inside_quotes) {
                                                        yy_pop_state(yyscanner);
                                                        return emit_expr_string_no_unput(yylval);
                                                    } else if (inside_quotes) {
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case '"':
                                                case '\'': {
                                                    begin_quoted_fragment(current_char);
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
                                        }
<ID_EXPR_STATE>.                           { current_char = yytext[0];
                                            if (consume_quoted_character(current_char)) {
                                              /* Keep quoted declarator text opaque. */
                                            } else {
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
                                                        yy_pop_state(yyscanner);
                                                        if (!current_string.empty()) {
                                                            return emit_expr_string_and_unput(')', yylval, yyscanner);
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
                                                        int next_char = tracked_yyinput(yyscanner);
                                                        if (next_char != EOF) {
                                                            tracked_unput(next_char, yyscanner);
                                                        }
                                                        bool next_is_paren = (next_char == '(');
                                                        if (bracket_count == 0 && parenthesis_global_count == 0 && !next_is_paren) {
                                                            yy_pop_state(yyscanner);
                                                            return emit_expr_string_and_unput(':', yylval, yyscanner);
                                                        }
                                                        current_string.push_back(current_char);
                                                    }
                                                    break;
                                                }
                                                case ' ': {
                                                    if (parenthesis_global_count == 0 && !inside_quotes) {
                                                        yy_pop_state(yyscanner);
                                                        yylval->stype =
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
                                                case '"':
                                                case '\'': {
                                                    begin_quoted_fragment(current_char);
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
                                        }

<<EOF>>         {
                      if (!current_string.empty()) {
                        lexer_location_state.last_token_offset =
                            lexer_location_state.offset;
                        return emit_expr_string_no_unput(yylval);
                      }
                      return 0;
                }

expr            {return (EXPRESSION); }

{blank}*        ;
.               { yy_push_state(EXPR_STATE, yyscanner); prepare_expression_capture(yytext[0]); }

%%

static inline void push_apply_state(void *yyscanner) {
  apply_paren_depth.push_back(0);
  yy_push_state(APPLY_STATE, yyscanner);
}

static inline void pop_apply_state(void *yyscanner) {
  if (!apply_paren_depth.empty()) {
    apply_paren_depth.pop_back();
  }
  yy_pop_state(yyscanner);
}

static inline int &current_apply_paren_depth() {
  if (apply_paren_depth.empty()) {
    apply_paren_depth.push_back(0);
  }
  return apply_paren_depth.back();
}

static inline int tracked_yyinput(void *yyscanner) {
  int next_char = yyinput(yyscanner);
  if (next_char == EOF || !lexer_location_state.tracking_enabled) {
    return next_char;
  }

  record_lexer_position_before_advance();
  advance_lexer_position(static_cast<char>(next_char));
  return next_char;
}

static inline void tracked_unput(int ch, void *yyscanner) {
  struct yyguts_t *yyg = static_cast<struct yyguts_t *>(yyscanner);
  unput(ch);
  rewind_lexer_position_for_unput(static_cast<char>(ch));
}

/* Implementation of inline functions that use parser tokens */
static inline int
emit_expr_string_and_unput(char ch, OPENMP_STYPE *semantic_value,
                           void *yyscanner) {
  semantic_value->stype = store_lexeme(current_string);
  clear_expression_buffer();
  tracked_unput(ch, yyscanner);
  return EXPR_STRING;
}

static inline int
emit_expr_string_no_unput(OPENMP_STYPE *semantic_value) {
  semantic_value->stype = store_lexeme(current_string);
  clear_expression_buffer();
  return EXPR_STRING;
}

/* yy_push_state can't be called outside of this file, provide a wrapper */
extern void openmp_parse_expr(void *yyscanner) {
  yy_push_state(EXPR_STATE, yyscanner);
}

extern void openmp_begin_type_string(void *yyscanner) {
  clear_expression_buffer();
  expression_begin_offset = lexer_location_state.offset;
  yy_push_state(TYPE_STR_STATE, yyscanner);
}

extern void openmp_begin_raw_expression(void *yyscanner) {
  clear_expression_buffer();
  expression_begin_offset = lexer_location_state.offset;
  yy_push_state(RAW_EXPR_STATE, yyscanner);
}

/* Standalone ompparser */
void start_lexer(const char *input, void *yyscanner) {
  lexer_location_state.tracking_enabled = true;
  reset_lexer_location_state();
  original_input = input != nullptr ? input : "";
  yy_scan_string(input, yyscanner);
}

void end_lexer(void *yyscanner) {
  struct yyguts_t *yyg = static_cast<struct yyguts_t *>(yyscanner);
  // If the lexer exited due to some error, the condition stack could be nonempty.
  // In this case, it has to be reset to the initial state manually, where
  // yy_start_stack_ptr == 0.
  while (yyg->yy_start_stack_ptr > 0) {
    yy_pop_state(yyscanner);
  };
  openmp_reset_lexer_flags();
  lexer_location_state.tracking_enabled = false;
  yy_delete_buffer(YY_CURRENT_BUFFER, yyscanner);
  lexeme_storage.clear();
  lexeme_ranges.clear();
  original_input.clear();
  expression_begin_offset = InvalidOffset;
}
