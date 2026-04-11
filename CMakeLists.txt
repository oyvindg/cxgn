cmake_minimum_required(VERSION 3.20)
project(cxgn VERSION 1.0.1 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C11 core
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(CXGN_ENABLE_YAML "Enable libyaml-backed generation support" ON)

# Dependencies (libyaml for C)
if(CXGN_ENABLE_YAML)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(YAML yaml-0.1)
else()
    set(YAML_FOUND OFF)
endif()

if(NOT CXGN_ENABLE_YAML)
    message(STATUS "Building cxgn without YAML support (CXGN_ENABLE_YAML=OFF).")
    set(CXGN_YAML_SUPPORT OFF)
elseif(NOT YAML_FOUND)
    message(WARNING "libyaml not found. Install libyaml-dev to enable cxgn code generation.")
    message(WARNING "Building cxgn without YAML support (string_utils and path only).")
    set(CXGN_YAML_SUPPORT OFF)
else()
    set(CXGN_YAML_SUPPORT ON)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# C Library (core)
# ═══════════════════════════════════════════════════════════════════════════
set(CXGN_SOURCES
    src/string_utils.c
    src/path.c
    src/struct_parser.c
)

if(CXGN_YAML_SUPPORT)
    list(APPEND CXGN_SOURCES
        src/code_generator.c
        src/glob.c
        src/batch.c
    )
else()
    list(APPEND CXGN_SOURCES
        src/yaml_stubs.c
    )
endif()

add_library(cxgn ${CXGN_SOURCES})

target_include_directories(cxgn PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CXGN_YAML_SUPPORT)
    target_include_directories(cxgn PRIVATE ${YAML_INCLUDE_DIRS})
    target_link_libraries(cxgn PRIVATE ${YAML_LIBRARIES})
    target_compile_definitions(cxgn PUBLIC CXGN_YAML_SUPPORT=1)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# CLI Tool (C) - only build if YAML support is available
# ═══════════════════════════════════════════════════════════════════════════
if(CXGN_YAML_SUPPORT)
    add_executable(cxgn-cli src/main.c)
    target_link_libraries(cxgn-cli PRIVATE cxgn)
    set_target_properties(cxgn-cli PROPERTIES OUTPUT_NAME cxgn)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# Install
# ═══════════════════════════════════════════════════════════════════════════
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CXGN_INSTALL_TARGETS cxgn)
if(CXGN_YAML_SUPPORT)
    list(APPEND CXGN_INSTALL_TARGETS cxgn-cli)
endif()

install(TARGETS ${CXGN_INSTALL_TARGETS}
    EXPORT cxgnTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/cxgn DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cxgnTargets
    FILE cxgnTargets.cmake
    NAMESPACE cxgn::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxgn
)

configure_package_config_file(
    cmake/cxgnConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cxgnConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxgn
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cxgnConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cxgnConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cxgnConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxgn
)

set(CXGN_PC_REQUIRES_PRIVATE "")
if(CXGN_YAML_SUPPORT)
    set(CXGN_PC_REQUIRES_PRIVATE "yaml-0.1")
endif()

configure_file(
    cmake/cxgn.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/cxgn.pc
    @ONLY
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cxgn.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

# ═══════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════
option(CXGN_BUILD_TESTS "Build cxgn tests" ON)
if(CXGN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
