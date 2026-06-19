// GENERATED FILE - DO NOT EDIT
// Source YAML: libs/cxgn/tests/fixtures/all_types.yaml
// Source Header: libs/cxgn/tests/fixtures/all_types.h
#ifndef CXGN_LIBS_CXGN_TESTS_ALL_TYPES_GEN_H_H
#define CXGN_LIBS_CXGN_TESTS_ALL_TYPES_GEN_H_H

#include <stddef.h>
#include <stdbool.h>
#include "fixtures/all_types.h"

static const int _backing_AllTypesConfig_intArray_0_data[] = {1, 2, 3, 4, 5};
static const double _backing_AllTypesConfig_doubleArray_1_data[] = {1.1, 2.2, 3.3};
static const cxgn_cstr_t _backing_AllTypesConfig_strArray_2_data[] = {"alpha", "beta", "gamma"};
static const bool _backing_AllTypesConfig_boolArray_3_data[] = {true, false, true};

static const AllTypesConfig config = {
    .intVal = 42,
    .longVal = 1000000,
    .shortVal = 32767,
    .i32Val = 2147483647,
    .i64Val = 9000000000000,
    .sizeVal = 128,
    .uintVal = 100,
    .llVal = 5000000000,
    .i8Val = 127,
    .i16Val = 30000,
    .u8Val = 255,
    .u16Val = 65535,
    .u32Val = 4000000000,
    .u64Val = 10000000000000000000ULL,
    .floatVal = 3.14,
    .doubleVal = 2.71828182845,
    .ldVal = 1.41421356237,
    .boolTrue = true,
    .boolFalse = false,
    .strVal = "hello cxgn",
    .svVal = "string_view",
    .intArray = {.data = _backing_AllTypesConfig_intArray_0_data, .count = 5},
    .doubleArray = {.data = _backing_AllTypesConfig_doubleArray_1_data, .count = 3},
    .strArray = {.data = _backing_AllTypesConfig_strArray_2_data, .count = 3},
    .boolArray = {.data = _backing_AllTypesConfig_boolArray_3_data, .count = 3},
    .optIntPresent = {.value = 99, .has_value = true},
    .optDoublePresent = {.value = 0.5, .has_value = true},
    .optIntAbsent = {.has_value = false},
    .nested = {
        .x = 3,
        .y = 4
    },
    .shapeCircle = {0},
    .shapeRect = {0}
};

#endif /* CXGN_LIBS_CXGN_TESTS_ALL_TYPES_GEN_H_H */
