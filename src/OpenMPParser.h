/*
 * Copyright (c) 2018-2026, High Performance Computing Architecture and System
 * research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
 * and Lawrence Livermore National Security, LLC.
 *
 * SPDX-License-Identifier: (BSD-3-Clause)
 */

#ifndef OMPPARSER_OPENMPPARSER_H
#define OMPPARSER_OPENMPPARSER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class OpenMPDirective;

namespace ompparser {

enum class BaseLanguage { C, CXX, Fortran };

enum class ExtensionPolicy { RejectUnknown, AllowRegistered };

enum class HostFragmentRole {
  Expression,
  Condition,
  Variable,
  Locator,
  Type,
  Declarator,
  Initializer,
  Verbatim
};

struct SourcePosition {
  uint32_t offset = 0;
  uint32_t line = 0;
  uint32_t column = 0;
};

struct SourceRange {
  SourcePosition begin;
  SourcePosition end;
};

class HostSemanticNode {
public:
  virtual ~HostSemanticNode() = default;
};

struct HostFragment {
  std::string spelling;
  SourceRange range;
  HostFragmentRole role = HostFragmentRole::Expression;
  std::shared_ptr<const HostSemanticNode> semantic;
};

using HostFragmentVisitor = std::function<void(HostFragment &)>;

enum class DiagnosticSeverity { Note, Warning, Error };

enum class DiagnosticCode {
  NullInput,
  LanguageMismatch,
  LexicalError,
  SyntaxError,
  DuplicateClause,
  InvalidDirective,
  InvalidClause,
  InvalidAst,
  UnsupportedExtension,
  HostLanguageError
};

struct Diagnostic {
  DiagnosticCode code = DiagnosticCode::SyntaxError;
  DiagnosticSeverity severity = DiagnosticSeverity::Error;
  SourceRange range;
  std::string message;
};

class HostLanguageHooks {
public:
  virtual ~HostLanguageHooks() = default;

  virtual std::shared_ptr<const HostSemanticNode>
  parse(const HostFragment &fragment,
        std::vector<Diagnostic> &diagnostics) const = 0;

  virtual void validate(const OpenMPDirective &directive,
                        std::vector<Diagnostic> &diagnostics) const = 0;
};

struct ParseOptions {
  BaseLanguage language = BaseLanguage::C;
  ExtensionPolicy extensions = ExtensionPolicy::RejectUnknown;
  const HostLanguageHooks *host_hooks = nullptr;
};

struct ParseResult {
  std::unique_ptr<OpenMPDirective> directive;
  std::vector<Diagnostic> diagnostics;
  bool context_checks_complete = false;

  ParseResult();
  ~ParseResult();
  ParseResult(ParseResult &&) noexcept;
  ParseResult &operator=(ParseResult &&) noexcept;
  ParseResult(const ParseResult &) = delete;
  ParseResult &operator=(const ParseResult &) = delete;

  bool success() const;
};

struct UnparseResult {
  std::string text;
  std::vector<Diagnostic> diagnostics;

  bool success() const;
};

struct DotResult {
  std::string text;
  std::vector<Diagnostic> diagnostics;

  bool success() const;
};

struct ValidationResult {
  std::vector<Diagnostic> diagnostics;

  bool success() const;
};

ParseResult parseDirective(std::string_view input,
                           const ParseOptions &options = {});
ValidationResult validate(const OpenMPDirective &directive);
UnparseResult unparse(const OpenMPDirective &directive);
DotResult toDot(const OpenMPDirective &directive);

} // namespace ompparser

#endif // OMPPARSER_OPENMPPARSER_H
