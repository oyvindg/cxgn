cmake_minimum_required(VERSION 3.20)
project(cxgn_batch_example LANGUAGES C)

find_program(CXGN_EXECUTABLE NAMES cxgn REQUIRED)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/strategies.all.gen.h
    COMMAND ${CXGN_EXECUTABLE}
            --yaml ${CMAKE_CURRENT_SOURCE_DIR}/strategies/*.yaml
            --header ${CMAKE_CURRENT_SOURCE_DIR}/strategy.h
            --output ${CMAKE_CURRENT_BINARY_DIR}/strategies.all.gen.h
            --map-root ${CMAKE_CURRENT_SOURCE_DIR}/strategies
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/strategy.h
            ${CMAKE_CURRENT_SOURCE_DIR}/strategies/alpha.yaml
            ${CMAKE_CURRENT_SOURCE_DIR}/strategies/beta.yaml
)

add_custom_target(generate_strategy_batch DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/strategies.all.gen.h)
