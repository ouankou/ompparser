if(NOT TARGET ompparser)
    message(FATAL_ERROR "ompparser target not found. Ensure ompparser library is built before adding tests subdirectory.")
endif()

enable_testing()

add_compile_options(-Wall)

add_executable(tester
    tester.cpp)
add_dependencies(tester ompparser)
target_link_libraries(tester ompparser)

add_executable(omp_roundtrip
    omp_roundtrip.cpp
    test_preprocess.cpp)
add_dependencies(omp_roundtrip ompparser)
target_link_libraries(omp_roundtrip ompparser)

# Register built-in .txt test files as CTest tests
file(GLOB TEST_SUITE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/builtin/*.txt")
list(SORT TEST_SUITE_FILES)
foreach(TEST_FILE IN LISTS TEST_SUITE_FILES)
  get_filename_component(TEST_NAME "${TEST_FILE}" NAME_WE)
  add_test(NAME builtin_${TEST_NAME}
           COMMAND tester "${TEST_FILE}"
           WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endforeach()

# Register OpenMP VV extracted pragma files as CTest tests
file(GLOB_RECURSE OPENMP_VV_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.cc"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.cxx"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.f"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.f90"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.f95"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.F"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv/*.F90")
list(SORT OPENMP_VV_FILES)
foreach(TEST_FILE IN LISTS OPENMP_VV_FILES)
  file(RELATIVE_PATH TEST_REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/openmp_vv" "${TEST_FILE}")
  string(REPLACE "/" "_" TEST_NAME "${TEST_REL_PATH}")
  add_test(NAME openmp_vv_${TEST_NAME}
           COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_single_pragma.sh" "${TEST_FILE}" "$<TARGET_FILE:omp_roundtrip>"
           WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endforeach()

# Register OpenMP Examples extracted pragma files as CTest tests
file(GLOB_RECURSE OPENMP_EXAMPLES_FILES
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.cc"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.cxx"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.f"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.f90"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.f95"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.F"
     "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples/*.F90")
list(SORT OPENMP_EXAMPLES_FILES)
foreach(TEST_FILE IN LISTS OPENMP_EXAMPLES_FILES)
  file(RELATIVE_PATH TEST_REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/openmp_examples" "${TEST_FILE}")
  string(REPLACE "/" "_" TEST_NAME "${TEST_REL_PATH}")
  add_test(NAME openmp_examples_${TEST_NAME}
           COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/test_single_pragma.sh" "${TEST_FILE}" "$<TARGET_FILE:omp_roundtrip>"
           WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endforeach()

set(executable_targets tester omp_roundtrip)

install(TARGETS ${executable_targets} OPTIONAL
        RUNTIME DESTINATION bin
        )
