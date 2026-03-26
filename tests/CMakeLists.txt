set(TESTDATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/testdata)

# Auto-discover tests under libs/cxgen/tests
file(GLOB CXGEN_C_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.test.c")
file(GLOB CXGEN_CPP_TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.test.cpp")

# YAML-dependent test basenames
set(CXGEN_YAML_DEPENDENT
    code_generator
    yaml_types
    integration
    warning_colors
    vec3
    hexapod_leg
    all_types
)

foreach(test_file IN LISTS CXGEN_C_TESTS)
    get_filename_component(test_name "${test_file}" NAME_WE)      # e.g. string_utils.test
    string(REPLACE ".test" "" test_base "${test_name}")           # e.g. string_utils

    list(FIND CXGEN_YAML_DEPENDENT "${test_base}" yaml_idx)
    if(yaml_idx GREATER -1 AND NOT CXGEN_YAML_SUPPORT)
        continue()
    endif()

    set(target_name "cxgen_test_${test_base}")
    add_executable("${target_name}" "${test_file}")
    target_link_libraries("${target_name}" PRIVATE cxgen)

    add_test(NAME "cxgen_${test_base}" COMMAND "$<TARGET_FILE:${target_name}>"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()

foreach(test_file IN LISTS CXGEN_CPP_TESTS)
    get_filename_component(test_name "${test_file}" NAME_WE)      # e.g. cpp_wrapper.test
    string(REPLACE ".test" "" test_base "${test_name}")           # e.g. cpp_wrapper

    set(target_name "cxgen_test_${test_base}")
    add_executable("${target_name}" "${test_file}")
    target_compile_features("${target_name}" PRIVATE cxx_std_14)
    target_link_libraries("${target_name}" PRIVATE cxgen)
    target_include_directories("${target_name}" PRIVATE "${CMAKE_SOURCE_DIR}/include")

    add_test(NAME "cxgen_${test_base}" COMMAND "$<TARGET_FILE:${target_name}>"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
