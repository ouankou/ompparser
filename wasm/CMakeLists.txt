#******************************************************************************************************************#
# Copyright (c) 2018-2025, High Performance Computing Architecture and System
# research laboratory at University of North Carolina at Charlotte (HPCAS@UNCC)
# and Lawrence Livermore National Security, LLC.
#
# SPDX-License-Identifier: (BSD-3-Clause)
#*****************************************************************************************************************#

add_executable(ompparser_wasm
    ompparser_wasm.cpp)

target_link_libraries(ompparser_wasm PRIVATE ompparser)

target_compile_features(ompparser_wasm PRIVATE cxx_std_17)

set_target_properties(ompparser_wasm PROPERTIES
    OUTPUT_NAME "ompparser"
    SUFFIX ".js"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm")

target_link_options(ompparser_wasm PRIVATE
    "--bind"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createOmpparserModule"
    "-sENVIRONMENT=web,node"
    "-sASSERTIONS=0")
