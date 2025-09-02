# Copyright (c) 2024, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.21)

file(READ VERSION MUUID_VERSION)
if (NOT MUUID_VERSION)
    message(FATAL_ERROR "Cannot determine library version (lib/VERSION file not found)")
endif()
string(STRIP ${MUUID_VERSION} MUUID_VERSION)

project(modern-uuid VERSION ${MUUID_VERSION} LANGUAGES C CXX)

get_property(MUUID_ALLOW_SHARED GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS)


if (NOT DEFINED MUUID_STATIC AND NOT DEFINED MUUID_SHARED)

    if (BUILD_SHARED_LIBS OR PROJECT_IS_TOP_LEVEL)
        set(MUUID_SHARED ${MUUID_ALLOW_SHARED})
    else()
        set(MUUID_SHARED OFF)
    endif()
    if(NOT BUILD_SHARED_LIBS OR PROJECT_IS_TOP_LEVEL)
        set(MUUID_STATIC ON)
    else()
        set(MUUID_STATIC OFF)
    endif()

endif()
set(MUUID_SHARED ${MUUID_SHARED} CACHE BOOL "Whether to produce shared lib" FORCE)
set(MUUID_STATIC ${MUUID_STATIC} CACHE BOOL "Whether to produce static lib" FORCE)

option(MUUID_NO_TESTS "Whether to disable test targets" OFF)

include(CheckIPOSupported)
check_ipo_supported(RESULT IPO_SUPPORTED)

set(INCDIR ${CMAKE_CURRENT_SOURCE_DIR}/inc)
set(SRCDIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(modern-uuid-header INTERFACE EXCLUDE_FROM_ALL)

# Single interface only library for the public headers
# This is to avoid having the headers in both shared and static libs
# Which is not well supported by some generators (e.g. Xcode)
list(APPEND INSTALL_LIBS modern-uuid-header)

set(PUBLIC_HEADER_NAMES
    common.h
    bit_packer.h
    nanoid.h
    ulid.h
    uuid.h
)
foreach(name ${PUBLIC_HEADER_NAMES})
    list(APPEND PUBLIC_HEADERS $<BUILD_INTERFACE:${INCDIR}/modern-uuid/${name}>)
    list(APPEND PUBLIC_HEADERS $<INSTALL_INTERFACE:include/modern-uuid/${name}>)
    list(APPEND PROPERTY_PUBLIC_HEADERS ${INCDIR}/modern-uuid/${name})
endforeach()


target_sources(modern-uuid-header 
INTERFACE
    ${PUBLIC_HEADERS}
)
set_target_properties(modern-uuid-header PROPERTIES PUBLIC_HEADER "${PROPERTY_PUBLIC_HEADERS}")


if (MUUID_SHARED)
    list(APPEND BUILD_SUFFIXES "shared")
endif()
if (MUUID_STATIC)
    list(APPEND BUILD_SUFFIXES "static")
endif()

set(LIBTYPE_static STATIC)
set(LIBTYPE_shared SHARED)

foreach(suffix ${BUILD_SUFFIXES})
    add_library(modern-uuid-${suffix} ${LIBTYPE_${suffix}})

    target_compile_features(modern-uuid-${suffix} PUBLIC cxx_std_20)

    set_target_properties(modern-uuid-${suffix} PROPERTIES 
        CXX_VISIBILITY_PRESET hidden
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
    )

    target_link_libraries(modern-uuid-${suffix}
    PUBLIC
        modern-uuid-header
    PRIVATE
        $<$<PLATFORM_ID:SunOS>:socket>
        $<$<BOOL:${MINGW}>:iphlpapi.lib>
    )

    target_compile_definitions(modern-uuid-${suffix}
    PRIVATE
        MUUID_BUILDING_MUUID=1
    )

    if (DEFINED ENV{SOURCE_DATE_EPOCH})

        string(TIMESTAMP MUUID_BUILD_DATE_TIME "%Y-%m-%d %H:%M:%S" UTC)

        message("modern-uuid-${suffix} will use reproducible timestamp: ${MUUID_BUILD_DATE_TIME}")

        target_compile_definitions(modern-uuid-${suffix}
        PRIVATE
            MUUID_BUILD_DATE_TIME=\"${MUUID_BUILD_DATE_TIME}\"
        )

    endif()

    target_compile_options(modern-uuid-${suffix}
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8;/W4>
        $<$<CXX_COMPILER_ID:AppleClang>:-Wall;-Wextra;-pedantic>
        $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-pedantic>
        $<$<BOOL:${EMSCRIPTEN}>:-sNO_DISABLE_EXCEPTION_CATCHING;-pthread>
    )

    if ("${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")
        target_compile_options(modern-uuid-${suffix}
        PRIVATE
            $<$<CXX_COMPILER_ID:Clang>:/W4>
        )
    else()
        target_compile_options(modern-uuid-${suffix}
        PRIVATE
            $<$<CXX_COMPILER_ID:Clang>:-Wall;-Wextra;-pedantic>
        )
    endif()

    target_include_directories(modern-uuid-${suffix}
    PUBLIC
        $<BUILD_INTERFACE:${INCDIR}>
        $<INSTALL_INTERFACE:include>  # <prefix>/include
    )

    target_sources(modern-uuid-${suffix}
    PRIVATE
        ${SRCDIR}/external/md5.h
        ${SRCDIR}/external/md5.c
        ${SRCDIR}/external/sha1.h
        ${SRCDIR}/external/sha1.c
        ${SRCDIR}/external/randutils.hpp
        ${SRCDIR}/external/chacha20.hpp

        ${SRCDIR}/clocks.h
        ${SRCDIR}/clocks.cpp
        ${SRCDIR}/fork_handler.h
        ${SRCDIR}/node_id.h
        ${SRCDIR}/node_id.cpp
        ${SRCDIR}/random_generator.h
        ${SRCDIR}/random_generator.cpp
        ${SRCDIR}/threading.h

        ${SRCDIR}/nanoid.cpp
        ${SRCDIR}/ulid.cpp
        ${SRCDIR}/uuid.cpp
    )

    add_library(modern-uuid::modern-uuid-${suffix} ALIAS modern-uuid-${suffix})
    list(APPEND INSTALL_LIBS modern-uuid-${suffix})

endforeach()


if (MUUID_STATIC AND NOT ("${CMAKE_IMPORT_LIBRARY_SUFFIX}" STREQUAL "${CMAKE_STATIC_LIBRARY_SUFFIX}"))
    set_target_properties(modern-uuid-static PROPERTIES 
        OUTPUT_NAME "modern-uuid"
    )
endif()

#Shared library versioning
if (MUUID_SHARED)
    if(IPO_SUPPORTED)
        set_target_properties(modern-uuid-shared PROPERTIES 
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE
        )
    endif()

    set_target_properties(modern-uuid-shared PROPERTIES
        OUTPUT_NAME "modern-uuid"
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
    )

    target_compile_definitions(modern-uuid-shared
    PUBLIC
        MUUID_SHARED=1
    )

endif()

if (PROJECT_IS_TOP_LEVEL)

    include(cmake/install.cmake)

    if (NOT MUUID_NO_TESTS)
        add_subdirectory(test)
    endif()

endif()



