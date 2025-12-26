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
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
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

BISON_TARGET(OMPBISONParser src/ompparser.yy ${CMAKE_CURRENT_BINARY_DIR}/ompparser.cc)
FLEX_TARGET(OMPFLEXScanner src/omplexer.ll ${CMAKE_CURRENT_BINARY_DIR}/omplexer.cc)
ADD_FLEX_BISON_DEPENDENCY(OMPFLEXScanner OMPBISONParser)

add_compile_options(-Wall)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# The ompparser library for install and distribution
add_library(ompparser SHARED ${BISON_OMPBISONParser_OUTPUTS} ${FLEX_OMPFLEXScanner_OUTPUTS} ${OMPIR_SOURCE_FILES})
target_include_directories(ompparser PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

set(ompparser_targets ompparser)

# Install headers and libraries
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ompparser_config.h
        src/OpenMPIR.h
        src/OpenMPKinds.h
        DESTINATION include)
install(TARGETS ${ompparser_targets}
        LIBRARY DESTINATION lib
        )
