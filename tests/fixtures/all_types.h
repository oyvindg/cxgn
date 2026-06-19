#pragma once

#include <cxgn/macros.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "shapes.h"

typedef const char* cxgn_cstr_t;

typedef struct Point2D {
    int x;
    int y;
} Point2D;

CXGN_ARRAY_TYPEDEF(int, cxgn_array_int_t)
CXGN_ARRAY_TYPEDEF(double, cxgn_array_double_t)
CXGN_ARRAY_TYPEDEF(cxgn_cstr_t, cxgn_array_cstr_t)
CXGN_ARRAY_TYPEDEF(bool, cxgn_array_bool_t)
CXGN_OPTIONAL_TYPEDEF(int, cxgn_optional_int_t)
CXGN_OPTIONAL_TYPEDEF(double, cxgn_optional_double_t)

typedef struct AllTypesConfig {
    int intVal;
    long longVal;
    short shortVal;
    int32_t i32Val;
    int64_t i64Val;
    size_t sizeVal;
    unsigned int uintVal;
    long long llVal;
    int8_t i8Val;
    int16_t i16Val;
    uint8_t u8Val;
    uint16_t u16Val;
    uint32_t u32Val;
    uint64_t u64Val;
    float floatVal;
    double doubleVal;
    long double ldVal;
    bool boolTrue;
    bool boolFalse;
    const char* strVal;
    const char* svVal;
    cxgn_array_int_t intArray;
    cxgn_array_double_t doubleArray;
    cxgn_array_cstr_t strArray;
    cxgn_array_bool_t boolArray;
    cxgn_optional_int_t optIntPresent;
    cxgn_optional_double_t optDoublePresent;
    cxgn_optional_int_t optIntAbsent;
    Point2D nested;
    Circle shapeCircle;
    Rectangle shapeRect;
} AllTypesConfig;
