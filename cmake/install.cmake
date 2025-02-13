# Copyright (c) 2024, Eugene Gershnik
# SPDX-License-Identifier: BSD-3-Clause

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(
    TARGETS ${INSTALL_LIBS}
    EXPORT modern-uuid
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    EXPORT modern-uuid 
    NAMESPACE modern-uuid:: 
    FILE modern-uuid-exports.cmake 
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/modern-uuid
)

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/modern-uuid-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/modern-uuid-config.cmake
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/modern-uuid
)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/modern-uuid-config-version.cmake
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/modern-uuid-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/modern-uuid-config-version.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/modern-uuid
)

file(RELATIVE_PATH FROM_PCFILEDIR_TO_PREFIX ${CMAKE_INSTALL_FULL_DATAROOTDIR}/modern-uuid ${CMAKE_INSTALL_PREFIX})
string(REGEX REPLACE "/+$" "" FROM_PCFILEDIR_TO_PREFIX "${FROM_PCFILEDIR_TO_PREFIX}") 

configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/modern-uuid.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/modern-uuid.pc
    @ONLY
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/modern-uuid.pc
    DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)
