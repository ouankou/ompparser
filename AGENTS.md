# Repository Agent Instructions

- Always consult the latest OpenMP 6.0 specification when updating or extending the parser to ensure semantic accuracy.
- Preserve LLVM coding style across all source files, including C, C++, Flex (.ll), and Bison (.yy) sources.
- To run the regression tests via CMake/CTest:
  1. Ensure both Flex and Bison are installed and visible to CMake (e.g., `which flex` should succeed).
  2. Configure the build directory: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`.
  3. Execute the test suite (which builds the required `tester` binary automatically) with `cmake --build build --target check`.
  4. Alternatively, from the `build` directory invoke `ctest --output-on-failure` after building the `tester` target.
