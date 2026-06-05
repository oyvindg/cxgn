cmake_minimum_required(VERSION 3.20)
project(cxgn_minimal_example LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_program(CXGN_EXECUTABLE NAMES cxgn
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../../build
    REQUIRED
)
find_path(CXGN_INCLUDE_DIR NAMES cxgn/macros.h
    HINTS ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    REQUIRED
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/scene.gen.h
    COMMAND ${CXGN_EXECUTABLE}
            --yaml ${CMAKE_CURRENT_SOURCE_DIR}/scene.yaml
            --header ${CMAKE_CURRENT_SOURCE_DIR}/scene.h
            --output ${CMAKE_CURRENT_BINARY_DIR}/scene.gen.h
            --helpers-header cxgn/macros.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scene.yaml
            ${CMAKE_CURRENT_SOURCE_DIR}/scene.h
)

add_custom_target(generate_scene_header DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/scene.gen.h)

add_executable(minimal_demo main.c ${CMAKE_CURRENT_BINARY_DIR}/scene.gen.h)
add_dependencies(minimal_demo generate_scene_header)
target_include_directories(minimal_demo PRIVATE
    ${CXGN_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
