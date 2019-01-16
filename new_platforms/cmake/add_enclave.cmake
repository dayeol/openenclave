# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

function(add_enclave)
    # Borrowed from ../cmake/add_enclave.cmake
    # Using the same signature so that the functions are easier to merge.
    set(options CXX)
    set(oneValueArgs TARGET CONFIG KEY)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ENCLAVE
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN})

    add_library(${ENCLAVE_TARGET} MODULE ${ENCLAVE_SOURCES})

    target_include_directories(${ENCLAVE_TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    target_link_libraries(${ENCLAVE_TARGET} oeenclave oestdio_enc oesocket_enc)

    if(NOT (TZ AND SIM))
        target_compile_options(${ENCLAVE_TARGET} PUBLIC "/X")
        target_compile_definitions(${ENCLAVE_TARGET} PUBLIC OE_NO_SAL)
    endif()

    string(REPLACE "/RTC1" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG} PARENT_SCOPE)

    if(SGX)
        # NOTE: These three work for CMake 3.13+, but Azure DevOps currently has
        # 3.12 installed:
        #
        # target_link_options(${ENCLAVE_TARGET} BEFORE PRIVATE "/NODEFAULTLIB")
        # target_link_options(${ENCLAVE_TARGET} BEFORE PRIVATE "/NOENTRY")
        # target_link_options(${ENCLAVE_TARGET} BEFORE PRIVATE "/MANIFEST:NO")
        #
        # Workaround follows:
        set_target_properties(${ENCLAVE_TARGET} PROPERTIES LINK_FLAGS "/NODEFAULTLIB /NOENTRY /MANIFEST:NO")

        add_custom_command(TARGET ${ENCLAVE_TARGET} POST_BUILD
            COMMAND ${SGX_SDK_SIGN_TOOL} sign
                -key ${CMAKE_CURRENT_SOURCE_DIR}/${ENCLAVE_TARGET}_private.pem
                -enclave ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<CONFIG>/${ENCLAVE_TARGET}.dll
                -out ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/$<CONFIG>/${ENCLAVE_TARGET}.signed.dll
                -config ${CMAKE_CURRENT_SOURCE_DIR}/${ENCLAVE_TARGET}.config.xml)
    endif()
endfunction()
