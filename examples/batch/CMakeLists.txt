cmake_minimum_required(VERSION 3.20)
project(cxgn_batch_example LANGUAGES C)

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
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/strategies.all.gen.h
    COMMAND ${CXGN_EXECUTABLE}
            --yaml ${CMAKE_CURRENT_SOURCE_DIR}/strategies/*.yaml
            --header ${CMAKE_CURRENT_SOURCE_DIR}/strategy.h
            --output ${CMAKE_CURRENT_BINARY_DIR}/strategies.all.gen.h
            --map-root ${CMAKE_CURRENT_SOURCE_DIR}/strategies
            --helpers-header cxgn/macros.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/strategy.h
            ${CMAKE_CURRENT_SOURCE_DIR}/strategies/alpha.yaml
            ${CMAKE_CURRENT_SOURCE_DIR}/strategies/beta.yaml
)

add_custom_target(generate_strategy_batch DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/strategies.all.gen.h)

add_executable(batch_demo main.c ${CMAKE_CURRENT_BINARY_DIR}/strategies.all.gen.h)
add_dependencies(batch_demo generate_strategy_batch)
target_include_directories(batch_demo PRIVATE
    ${CXGN_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)
