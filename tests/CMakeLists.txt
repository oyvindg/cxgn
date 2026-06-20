set(CXGN_TEST_BASES
    capability
    cli
    document
    string_utils
    path
    errors
    hexapod_leg
    ownership
    struct_parser
    struct_parser_text
    validation
    all_types
    type_options
    shapes
    vec3
    yaml_types
    yaml_document
    document_merge
    code_generator
    integration
    batch
    map
)

set(CXGN_YAML_DEPENDENT
    cli
    hexapod_leg
    ownership
    validation
    all_types
    type_options
    shapes
    vec3
    yaml_types
    yaml_document
    document_merge
    code_generator
    integration
    batch
    map
)

foreach(test_base IN LISTS CXGN_TEST_BASES)
    set(test_file "${CMAKE_CURRENT_SOURCE_DIR}/${test_base}.test.c")

    list(FIND CXGN_YAML_DEPENDENT "${test_base}" yaml_idx)
    if(yaml_idx GREATER -1 AND NOT CXGN_YAML_SUPPORT)
        continue()
    endif()

    set(target_name "cxgn_test_${test_base}")
    add_executable("${target_name}" "${test_file}")
    target_link_libraries("${target_name}" PRIVATE cxgn)
    if(test_base STREQUAL "document" OR test_base STREQUAL "yaml_document")
        target_include_directories("${target_name}" PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
    endif()
    if(test_base STREQUAL "cli")
        target_compile_definitions("${target_name}" PRIVATE CXGN_CLI_PATH="$<TARGET_FILE:cxgn-cli>")
        add_dependencies("${target_name}" cxgn-cli)
    endif()

    add_test(NAME "cxgn_${test_base}" COMMAND "$<TARGET_FILE:${target_name}>"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
