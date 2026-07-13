# ompparser: A Standalone and Unified OpenMP Parser

ompparser is a standalone OpenMP parser for C/C++ and Fortran. It can be used as an independent tool or embedded into compiler pipelines. The Flex/Bison grammar parses OpenMP directives into a source-faithful, typed intermediate representation (IR). The implementation tracks OpenMP 6.0 and its published errata and is released under the BSD-3-Clause license.

## Build and Usage
1. clone the repo and configure a build directory

       git clone https://github.com/ouankou/ompparser.git
       cmake -S ompparser -B build -DCMAKE_BUILD_TYPE=Release

2. build and install

       cmake --build build
       cmake --install build --prefix /path/to/install

3. run the regression tests (requires Flex and Bison in PATH)

       cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
       cmake --build build --target check

## ompparser API

```cpp
#include <OpenMPParser.h>

ompparser::ParseOptions options;
options.language = ompparser::BaseLanguage::CXX;

ompparser::ParseResult parsed = ompparser::parseDirective(
    "#pragma omp parallel private(value)", options);
if (!parsed.success()) {
  for (const ompparser::Diagnostic &diagnostic : parsed.diagnostics) {
    // Report diagnostic.code, diagnostic.range, and diagnostic.message.
  }
  return;
}

ompparser::UnparseResult text = ompparser::unparse(*parsed.directive);
ompparser::DotResult dot = ompparser::toDot(*parsed.directive);
```

`ParseResult` owns the directive. Syntax errors, schema violations, invalid AST invariants, and unsupported extensions are returned as structured diagnostics; they are not printed to standard error. Unknown extensions are rejected by default and must be enabled explicitly with `ExtensionPolicy::AllowRegistered`.

Host-language expressions, variables, locators, types, and declarators are stored as `HostFragment` records with their original spelling, role, source range, and optional semantic node. An embedding compiler implements both `HostLanguageHooks::parse` and `HostLanguageHooks::validate` to attach semantic nodes and enforce contextual base-language rules. `context_checks_complete` is true only after both hook stages run on a successfully constructed OpenMP AST.

The 1.0 API intentionally preserves each source clause occurrence. It does not merge clauses, deduplicate list items, rewrite operators, or repair malformed ASTs during unparsing. Consumers that used the pre-1.0 raw `parseOpenMP` entry point or depended on normalization should migrate to `parseDirective`, inspect diagnostics, and perform any policy-specific canonicalization in a separate pass.

## Features and Limitation
1. OpenMP 6.0 standard support for both C/C++ and Fortran, including parsing and unparsing
1. Flex lexer rules and Bison grammars for OpenMP 6.0 syntax
1. Intermediate representation of OpenMP constructs
1. Structured parse, validation, unparse, and in-memory DOT APIs
1. Syntax, typed-schema, extension-policy, and AST-invariant checking
1. Source-faithful clause occurrences and host-language fragment spellings
1. Reentrant per-call Flex/Bison scanner contexts and thread-safe parsing
1. Optional host-frontend hooks for C, C++, and Fortran semantic nodes
1. Testing driver and test cases for OpenMP constructs
1. Side-effect-free DOT graph rendering, plus a compatibility file writer
1. Conversion between perfectly-nested OpenMP constructs and combined constructs (ongoing work)

For an AddressSanitizer and UndefinedBehaviorSanitizer build, configure with `-DOMPPARSER_ENABLE_SANITIZERS=ON` using Clang or GCC.

## Contribution
Submit contribution as github pull request to this repository. We require all new contributions must be made with the similar license. 

## Getting Involved and Technical Support
Submit feature request, bugs and questions from the repository's "[Issues](https://github.com/passlab/ompparser/issues)" tab. 

## Acknowledgement
The work has been performed with support from Department of Energy Lawrence Livermore National Laboratory and the National Science Foundation. To cite, please use following paper:

[Anjia Wang, Yaying Shi, Xinyao Yi, Yonghong Yan, Chunhua Liao and Bronis R. de Supinski, ompparser: A Standalone and Unified OpenMP Parser, the 15th International Workshop on OpenMP (IWOMP), 11th - 13th September, in Auckland, New Zealand](https://link.springer.com/chapter/10.1007%2F978-3-030-28596-8_10). [The presentation of the paper](http://parallel.auckland.ac.nz/iwomp2019/slides_ompparser.pdf), which is from IWOMP'19 website. 

## Authors

Anjia Wang, Yaying Shi, Xinyao Yi, Yonghong Yan, Chunhua Liao and Bronis R. de Supinski

## Contact
Please contact Yonghong Yan (@yanyh15) from github or gmail. 

## License

ompparser is released under a BSD license. For more details see the file LICENSE.

LLNL-CODE-798101
