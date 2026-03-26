cmake_minimum_required(VERSION 3.20)
project(cxgen VERSION 1.0.0 LANGUAGES C)

# C11 core
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Dependencies (libyaml for C)
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAML yaml-0.1)

if(NOT YAML_FOUND)
    message(WARNING "libyaml not found. Install libyaml-dev to enable cxgen code generation.")
    message(WARNING "Building cxgen without YAML support (string_utils and path only).")
    set(CXGEN_YAML_SUPPORT OFF)
else()
    set(CXGEN_YAML_SUPPORT ON)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# C Library (core)
# ═══════════════════════════════════════════════════════════════════════════
set(CXGEN_SOURCES
    src/string_utils.c
    src/path.c
    src/struct_parser.c
)

if(CXGEN_YAML_SUPPORT)
    list(APPEND CXGEN_SOURCES src/code_generator.c)
endif()

add_library(cxgen ${CXGEN_SOURCES})

target_include_directories(cxgen PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(CXGEN_YAML_SUPPORT)
    target_include_directories(cxgen PRIVATE ${YAML_INCLUDE_DIRS})
    target_link_libraries(cxgen PRIVATE ${YAML_LIBRARIES})
    target_compile_definitions(cxgen PUBLIC CXGEN_YAML_SUPPORT=1)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# CLI Tool (C) - only build if YAML support is available
# ═══════════════════════════════════════════════════════════════════════════
if(CXGEN_YAML_SUPPORT)
    add_executable(cxgen-cli src/main.c)
    target_link_libraries(cxgen-cli PRIVATE cxgen)
    set_target_properties(cxgen-cli PROPERTIES OUTPUT_NAME cxgen)
endif()

# ═══════════════════════════════════════════════════════════════════════════
# Install
# ═══════════════════════════════════════════════════════════════════════════
include(GNUInstallDirs)

set(CXGEN_INSTALL_TARGETS cxgen)
if(CXGEN_YAML_SUPPORT)
    list(APPEND CXGEN_INSTALL_TARGETS cxgen-cli)
endif()

install(TARGETS ${CXGEN_INSTALL_TARGETS}
    EXPORT cxgenTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/cxgen DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT cxgenTargets
    FILE cxgenConfig.cmake
    NAMESPACE cxgen::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxgen
)

# ═══════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════
option(CXGEN_BUILD_TESTS "Build cxgen tests" ON)
if(CXGEN_BUILD_TESTS)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
    add_subdirectory(tests)
endif()
