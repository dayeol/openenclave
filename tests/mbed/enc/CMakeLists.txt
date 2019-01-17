# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

# >> In one of the test "ssl" we are using a variable called new which is a function in C++
#    causes improper behaviour at compile time leads us to use C instead of C++

include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/get_testcase_name.cmake)
include_directories(${CMAKE_BINARY_DIR}/3rdparty/mbedtls/mbedtls-wrap-prefix/src/mbedtls-wrap/include/)
# helper lib to contain file needed by some tests

oeedl_file(../mbed.edl enclave gen)

# helper function to create enclave binary
function(add_mbed_test_enclave NAME)
  string(FIND "${NAME}" "." cond)
  if(${cond} GREATER 0 )
    set(data_name ${NAME})
    string(REPLACE "." ";" suite ${NAME})
    list(GET suite 0 suite_name)
  elseif(${cond} LESS 0 )
    set(suite_name ${NAME})
    set(data_name ${NAME})
  endif()

  set(MBEDTLS_TESTS_DIR "${PROJECT_SOURCE_DIR}/3rdparty/mbedtls/mbedtls/tests")

  add_custom_command(
    OUTPUT test_suite_${NAME}.c
    COMMAND ${PERL_EXECUTABLE} ${MBEDTLS_TESTS_DIR}/scripts/generate_code.pl ${MBEDTLS_TESTS_DIR}/suites test_suite_${suite_name} test_suite_${data_name}
    DEPENDS
    mbedcrypto
    ${MBEDTLS_TESTS_DIR}/scripts/generate_code.pl
    ${MBEDTLS_TESTS_DIR}/suites/helpers.function
    ${MBEDTLS_TESTS_DIR}/suites/main_test.function
    ${MBEDTLS_TESTS_DIR}/suites/test_suite_${suite_name}.function
    ${MBEDTLS_TESTS_DIR}/suites/test_suite_${data_name}.data)

  add_enclave(TARGET mbedtest_suite_${NAME}
    SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/test_suite_${NAME}.c
    enc.c
    start.S
    ${gen})

  target_compile_options(mbedtest_suite_${NAME} PRIVATE -Wno-error)
  target_compile_definitions(mbedtest_suite_${NAME} PRIVATE -D__TEST__="${NAME}")
  target_include_directories(mbedtest_suite_${NAME} PRIVATE
    ..
    ${PROJECT_SOURCE_DIR}/3rdparty/mbedtls/mbedtls/tests
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/3rdparty/mbedtls/mbedtls-wrap-prefix/src/mbedtls-wrap/include/)
endfunction()

function(add_mbed_selftest_enclave NAME SOURCE)
  add_enclave(TARGET mbedtest_selftest_${NAME}
    SOURCES
    ${PROJECT_SOURCE_DIR}/3rdparty/mbedtls/mbedtls/programs/test/selftest.c
    enc.c
    start.S
    ${gen})

  target_compile_options(mbedtest_selftest_${NAME} PRIVATE -Wno-conversion -Wno-pointer-arith)
  target_compile_definitions(mbedtest_selftest_${NAME} PRIVATE -D__TEST__="${NAME}")
  target_include_directories(mbedtest_selftest_${NAME} PRIVATE
    ..
    ${PROJECT_SOURCE_DIR}/3rdparty/mbedtls/mbedtls/tests
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/3rdparty/mbedtls/mbedtls-wrap-prefix/src/mbedtls-wrap/include/)
endfunction()

string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)
if (NOT "${build_type}" STREQUAL "DEBUG")
  set(is_nondebug_build TRUE)
endif ()

# iterate over the supported tests and create a binary for each.
file(STRINGS "../tests.supported" alltests)
foreach (testcase ${alltests})
  string(FIND "${testcase}" "selftest" found_selftest)
  if (${found_selftest} GREATER 0)
    get_testcase_name(${testcase} name "../../3rdparty/mbedtls/mbedtls/programs/test/")
    add_mbed_selftest_enclave(${name} ${testcase})
  else ()
    if (${is_nondebug_build})
      string(FIND "${testcase}" "debug" found_debug)
      if (NOT ${found_debug} LESS 0)
        continue()
      endif ()
    endif ()
    add_mbed_test_enclave(${testcase})
  endif ()
endforeach ()

file(COPY ${CMAKE_SOURCE_DIR}/3rdparty/mbedtls/mbedtls/tests/data_files DESTINATION ${CMAKE_BINARY_DIR}/tests/mbed )
