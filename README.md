# ompparser: A Standalone and Unified OpenMP Parser

ompparser is a standalone OpenMP parser for C/C++ and Fortran. It can be used as an independent tool or embedded into compiler pipelines. The Flex/Bison grammar parses OpenMP directives and builds an intermediate representation (IR) that supports normalization and round-trip unparsing. The implementation tracks OpenMP 6.0 constructs and is released under the BSD-3-Clause license.

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
enum OpenMPBaseLang {
    Lang_C,
    Lang_Cplusplus,
    Lang_Fortran,
    Lang_unknown
};

struct OpenMPParseOptions {
    OpenMPBaseLang base_lang;
    bool normalize_clauses = true;
    OpenMPExprParseCallback expression_callback = nullptr;
    void *expression_callback_user_data = nullptr;
};

class OpenMPClause : public SourceLocation {
 ...
}
 
class OpenMPDirective : public SourceLocation  {
 ...
}

std::unique_ptr<OpenMPDirective>
parseOpenMP(const char *directive, const OpenMPParseOptions &options);

OpenMPParseOptions options;
options.base_lang = Lang_Cplusplus;
std::unique_ptr<OpenMPDirective> directive =
    parseOpenMP("#pragma omp parallel", options);

```

The caller owns the returned directive through `std::unique_ptr`. A successful
call always returns a nonnull directive. The base language is mandatory because
C and C++ cannot be inferred from directive text. Null input, an unknown or
mismatched language, syntax errors, semantic violations, malformed IR, and
reentrant calls emit one `OMPPARSER_*` diagnostic and abort; there is no
error-recovery or null-result fallback API.

## Features and Limitation
1. OpenMP 6.0 standard support for both C/C++ and Fortran, including parsing and unparsing
1. Flex lexer rules and Bison grammars for OpenMP 6.0 syntax
1. Intermediate representation of OpenMP constructs
1. Interface to parse OpenMP constructs and emit the OpenMP IR
1. Syntax checking in grammar, parsing, IR construction, and post-parsing
1. Clause normalization, e.g., combining multiple shared clauses into one shared clause
1. Limited semantics checking when a construct uses C/C++/Fortran identifiers or expressions
1. Testing driver and test cases for OpenMP constructs
1. DOT graph output of OpenMP constructs
1. Conversion between perfectly-nested OpenMP constructs and combined constructs (ongoing work)

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
