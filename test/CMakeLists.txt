# Copyright (c) 2024, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)


include(FetchContent)

set(DECLARED_DEPENDENCIES "")

FetchContent_Declare(doctest
    URL  https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
    DOWNLOAD_NO_EXTRACT TRUE
    SOURCE_DIR downloaded/doctest
)

list(APPEND DECLARED_DEPENDENCIES doctest)

set(FMT_INSTALL OFF)

FetchContent_Declare(fmt
    GIT_REPOSITORY  https://github.com/fmtlib/fmt
    GIT_TAG         11.1.4
    GIT_SHALLOW     TRUE
    GIT_SUBMODULES_RECURSE FALSE
)
list(APPEND DECLARED_DEPENDENCIES fmt)

FetchContent_MakeAvailable(${DECLARED_DEPENDENCIES})

if (${CMAKE_SYSTEM_NAME} STREQUAL Android)
    set(ANDROID_TEST_DIR /data/local/tmp/muuid_test)
    set(ANDROID_SDK_DIR ${CMAKE_ANDROID_NDK}/../..)
    set(ADB ${ANDROID_SDK_DIR}/platform-tools/adb)

    if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4")
        set(ANDROID_LD_LIBRARY_PATH /apex/com.android.art/lib:/apex/com.android.runtime/lib)
    else()
        set(ANDROID_LD_LIBRARY_PATH /apex/com.android.art/lib64:/apex/com.android.runtime/lib64)
    endif()
endif()

foreach(suffix ${BUILD_SUFFIXES})

    add_executable(test-${suffix} EXCLUDE_FROM_ALL)

    set_target_properties(test-${suffix} PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
        RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:modern-uuid-${suffix}>
        XCODE_GENERATE_SCHEME TRUE
        XCODE_SCHEME_ARGUMENTS "-ni;-nc"
    )

    target_link_libraries(test-${suffix}
    PRIVATE
        modern-uuid::modern-uuid-${suffix}
        fmt::fmt
        "$<$<PLATFORM_ID:Darwin>:-framework CoreFoundation>"
        $<$<AND:$<NOT:$<BOOL:${WIN32}>>,$<NOT:$<BOOL:${ANDROID}>>>:pthread>
    )

    target_include_directories(test-${suffix}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/downloaded
    )

    target_compile_definitions(test-${suffix}
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    )

    target_compile_options(test-${suffix}
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8;/W4;/WX>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wall;-Wextra;-pedantic;-Wno-self-assign-overloaded;-Wno-self-move>
        $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-pedantic;-Wno-psabi>
        $<$<BOOL:${EMSCRIPTEN}>:-sNO_DISABLE_EXCEPTION_CATCHING;-pthread>
    )

    if ("${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")
        target_compile_options(test-${suffix}
        PRIVATE
            $<$<CXX_COMPILER_ID:Clang>:/W4;/WX>
        )
    else()
        target_compile_options(test-${suffix}
        PRIVATE
            $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-pedantic;-Wno-self-assign-overloaded;-Wno-self-move>
        )
    endif()

    target_link_options(test-${suffix}
    PRIVATE
        $<$<BOOL:${EMSCRIPTEN}>:-sNO_DISABLE_EXCEPTION_CATCHING;-pthread;-sPTHREAD_POOL_SIZE=3>
    )

    target_sources(test-${suffix}
    PRIVATE
        test_util.h

        main.cpp
        test_basics.cpp
        test_ulid_basics.cpp
        test_nanoid_basics.cpp
        test_cuid2_basics.cpp

        test_fmt.cpp
        test_fork.cpp
        test_persistence.cpp
        test_system.cpp
        test_sha3.cpp
    )

    if (${CMAKE_SYSTEM_NAME} STREQUAL Android)

        if (${suffix} STREQUAL "shared")
            set(LIB_TO_PUSH "$<TARGET_FILE:modern-uuid-${suffix}>")
        else()
            set(LIB_TO_PUSH "")
        endif()

        add_custom_target(run-test-${suffix}
            DEPENDS test-${suffix}
            COMMAND echo "Running test-${suffix}"
            COMMAND ${ADB} shell mkdir -p ${ANDROID_TEST_DIR}
            COMMAND ${ADB} push "$<TARGET_FILE:test-${suffix}>" ${LIB_TO_PUSH} ${ANDROID_TEST_DIR}
            COMMAND ${ADB} shell cd ${ANDROID_TEST_DIR} \"&&\" LD_LIBRARY_PATH=${ANDROID_LD_LIBRARY_PATH}:${ANDROID_TEST_DIR} ./test-${suffix} -ni
        )

    elseif (DEFINED EMSCRIPTEN)

        add_custom_target(run-test-${suffix}
            DEPENDS test-${suffix}
            COMMAND echo "Running test-${suffix}"
            COMMAND node $<TARGET_FILE:test-${suffix}> -ni
        )

    else()

        add_custom_target(run-test-${suffix}
            DEPENDS test-${suffix}
            COMMAND echo "Running test-${suffix}"
            COMMAND test-${suffix} -ni
        )

    endif()

    list(APPEND TEST_RUNNERS run-test-${suffix})

endforeach()

add_custom_target(run-tests
    DEPENDS ${TEST_RUNNERS}
)

