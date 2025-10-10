#******************************************************************************************************************#
# Copyright (c) 2018-2025, High Performance Computing Architecture and System
# research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
# and Lawrence Livermore National Security, LLC.
#
# SPDX-License-Identifier: (BSD-3-Clause)
#*****************************************************************************************************************#

cmake_minimum_required(VERSION 3.10)
project(ompparser)

include(CTest)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_BUILD_TYPE Debug)

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
    preprocess_c.cpp)

BISON_TARGET(OMPBISONParser src/ompparser.yy ${CMAKE_CURRENT_BINARY_DIR}/ompparser.cc)
FLEX_TARGET(OMPFLEXScanner src/omplexer.ll ${CMAKE_CURRENT_BINARY_DIR}/omplexer.cc)
ADD_FLEX_BISON_DEPENDENCY(OMPFLEXScanner OMPBISONParser)

add_compile_options(-Wall)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# The ompparser library for install and distribution
add_library(ompparser SHARED ${BISON_OMPBISONParser_OUTPUTS} ${FLEX_OMPFLEXScanner_OUTPUTS} ${OMPIR_SOURCE_FILES})

# The temporary test file
add_executable(main EXCLUDE_FROM_ALL main.cpp)
add_dependencies(main ompparser)
target_link_libraries(main ompparser)

# The temporary test file
add_executable(tester EXCLUDE_FROM_ALL tester.cpp)
add_dependencies(tester ompparser)
target_link_libraries(tester ompparser)

# The standalone ompparser executable
add_executable(ompp EXCLUDE_FROM_ALL ${OMPP_TOOLS} ompparser.cpp)
add_dependencies(ompp ompparser)
target_link_libraries(ompp ompparser)

set(ompparser_targets ompparser)
set(executable_targets
    main
    ompp
    tester)

if(BUILD_TESTING)
  file(GLOB TEST_SUITE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.txt")
  list(SORT TEST_SUITE_FILES)
  foreach(TEST_FILE IN LISTS TEST_SUITE_FILES)
    get_filename_component(TEST_NAME "${TEST_FILE}" NAME_WE)
    add_test(NAME ompparser_${TEST_NAME}
             COMMAND $<TARGET_FILE:tester> "${TEST_FILE}")
  endforeach()

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS tester)
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
