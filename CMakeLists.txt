cmake_minimum_required(VERSION 3.20)
project(cxgn VERSION 1.0.0 LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C11 core
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Dependencies (libyaml for C)
find_package(PkgConfig REQUIRED)
pkg_check_modules(YAML yaml-0.1)

if(NOT YAML_FOUND)
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
    list(APPEND CXGN_SOURCES src/code_generator.c)
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
    FILE cxgnConfig.cmake
    NAMESPACE cxgn::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cxgn
)

# ═══════════════════════════════════════════════════════════════════════════
# Tests
# ═══════════════════════════════════════════════════════════════════════════
option(CXGN_BUILD_TESTS "Build cxgn tests" ON)
if(CXGN_BUILD_TESTS)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    enable_testing()
    add_subdirectory(tests)
endif()
