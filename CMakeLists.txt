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

# Single interface only library for the public header
# This is to avoid having the header in both shared and static libs
# Which is not well supported by some generators (e.g. Xcode)
list(APPEND INSTALL_LIBS modern-uuid-header)

target_sources(modern-uuid-header 
INTERFACE
    $<BUILD_INTERFACE:${INCDIR}/modern-uuid/uuid.h>
    $<INSTALL_INTERFACE:modern-uuid/uuid.h>
)
set_target_properties(modern-uuid-header PROPERTIES PUBLIC_HEADER ${INCDIR}/modern-uuid/uuid.h)


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

        ${SRCDIR}/clocks.h
        ${SRCDIR}/clocks.cpp
        ${SRCDIR}/fork_handler.h
        ${SRCDIR}/node_id.h
        ${SRCDIR}/node_id.cpp
        ${SRCDIR}/random_generator.h
        ${SRCDIR}/random_generator.cpp
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

if (PROJECT_IS_TOP_LEVEL AND NOT ${MUUID_NO_TESTS})

    include(cmake/install.cmake)

    add_subdirectory(test)

endif()



