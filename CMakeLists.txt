#******************************************************************************************************************#
# Copyright (c) 2018-2025, High Performance Computing Architecture and System
# research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
# and Lawrence Livermore National Security, LLC.
#
# SPDX-License-Identifier: (BSD-3-Clause)
#*****************************************************************************************************************#

cmake_minimum_required(VERSION 3.20)
project(ompparser)

include(CTest)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

set(OMPPARSER_VERSION_MAJOR 0)
set(OMPPARSER_VERSION_MINOR 2)
set(OMPPARSER_VERSION ${OMPPARSER_VERSION_MAJOR}.${OMPPARSER_VERSION_MINOR})

find_package(BISON)
find_package(FLEX)

configure_file(src/ompparser_config.h.cmake "ompparser_config.h" @ONLY)

# Not used, but keep this for the purpose to keep track of the original source files
set(OMPPARSER_SOURCE_FILES
    src/omplexer.ll
    src/ompparser.yy
    src/OpenMPIR.h
    src/OpenMPIRToDOT.cpp
    src/OpenMPIRToString.cpp
    src/OpenMPIR.cpp)

# OpenMPIR source files
set(OMPIR_SOURCE_FILES
    src/OpenMPIRToDOT.cpp
    src/OpenMPIRToString.cpp
    src/OpenMPIR.cpp)

# tools based on omparser
set(OMPP_TOOLS
    utils/preprocess_c.cpp)

BISON_TARGET(OMPBISONParser src/ompparser.yy ${CMAKE_CURRENT_BINARY_DIR}/ompparser.cc)
FLEX_TARGET(OMPFLEXScanner src/omplexer.ll ${CMAKE_CURRENT_BINARY_DIR}/omplexer.cc)
ADD_FLEX_BISON_DEPENDENCY(OMPFLEXScanner OMPBISONParser)

add_compile_options(-Wall)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# The ompparser library for install and distribution
add_library(ompparser SHARED ${BISON_OMPBISONParser_OUTPUTS} ${FLEX_OMPFLEXScanner_OUTPUTS} ${OMPIR_SOURCE_FILES})

# Test runner executable for built-in tests
add_executable(tester tests/tester.cpp)
target_link_libraries(tester ompparser)

# Standalone ompparser executable for openmp_vv suite
add_executable(ompp ${OMPP_TOOLS} utils/ompparser.cpp)
target_link_libraries(ompp ompparser)

if(BUILD_TESTING)
  # Register built-in .txt test files as CTest tests
  file(GLOB TEST_SUITE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/builtin/*.txt")
  list(SORT TEST_SUITE_FILES)
  foreach(TEST_FILE IN LISTS TEST_SUITE_FILES)
    get_filename_component(TEST_NAME "${TEST_FILE}" NAME_WE)
    add_test(NAME builtin_${TEST_NAME}
             COMMAND tester "${TEST_FILE}"
             WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endforeach()

  # Register OpenMP VV extracted pragma files as CTest tests
  file(GLOB_RECURSE OPENMP_VV_FILES
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.c"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.cpp"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.cc"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.cxx"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.f"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.f90"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.f95"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.F"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv/*.F90")
  list(SORT OPENMP_VV_FILES)
  foreach(TEST_FILE IN LISTS OPENMP_VV_FILES)
    # Get relative path from tests/openmp_vv/ to include directory structure in test name
    file(RELATIVE_PATH TEST_REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_vv" "${TEST_FILE}")
    # Replace directory separators with underscores for test name
    string(REPLACE "/" "_" TEST_NAME "${TEST_REL_PATH}")
    add_test(NAME openmp_vv_${TEST_NAME}
             COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_pragma.sh" "${TEST_FILE}" "$<TARGET_FILE:ompp>"
             WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endforeach()

  # Register OpenMP Examples extracted pragma files as CTest tests
  file(GLOB_RECURSE OPENMP_EXAMPLES_FILES
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.c"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.cpp"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.cc"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.cxx"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.f"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.f90"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.f95"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.F"
       "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples/*.F90")
  list(SORT OPENMP_EXAMPLES_FILES)
  foreach(TEST_FILE IN LISTS OPENMP_EXAMPLES_FILES)
    # Get relative path from tests/openmp_examples/ to include directory structure in test name
    file(RELATIVE_PATH TEST_REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tests/openmp_examples" "${TEST_FILE}")
    # Replace directory separators with underscores for test name
    string(REPLACE "/" "_" TEST_NAME "${TEST_REL_PATH}")
    add_test(NAME openmp_examples_${TEST_NAME}
             COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_single_pragma.sh" "${TEST_FILE}" "$<TARGET_FILE:ompp>"
             WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  endforeach()
endif()

# Install headers and libraries
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ompparser_config.h
        src/OpenMPIR.h
        src/OpenMPKinds.h
        DESTINATION include)
install(TARGETS ${ompparser_targets}
        LIBRARY DESTINATION lib
        )
install(TARGETS ${executable_targets} OPTIONAL
        RUNTIME DESTINATION bin
        )
