// GENERATED FILE - DO NOT EDIT
// Source YAML: tests/fixtures/all_types.yaml
// Source Header: tests/fixtures/all_types.hpp
#pragma once

#include <variant>
#include "fixtures/all_types.hpp"

namespace {
static constexpr char _spool_0[] = "hello cxgn";
static constexpr char _spool_1[] = "string_view";
static constexpr char _spool_2[] = "alpha";
static constexpr char _spool_3[] = "beta";
static constexpr char _spool_4[] = "gamma";
static constexpr double _backing_AllTypesConfig_doubleArray_data[] = {1.1, 2.2, 3.3};
static constexpr std::string _backing_AllTypesConfig_strArray_data[] = {_spool_2, _spool_3, _spool_4};
static constexpr int _backing_AllTypesConfig_intArray_data[] = {1, 2, 3, 4, 5};
static constexpr bool _backing_AllTypesConfig_boolArray_data[] = {true, false, true};
} // namespace

constexpr AllTypesConfig config = {
    42,  // intVal
    1000000,  // longVal
    32767,  // shortVal
    2147483647,  // i32Val
    9000000000000,  // i64Val
    128,  // sizeVal
    100ULL,  // uintVal
    5000000000,  // llVal
    127,  // i8Val
    30000,  // i16Val
    255ULL,  // u8Val
    65535ULL,  // u16Val
    4000000000ULL,  // u32Val
    10000000000000000000ULL,  // u64Val
    3.14,  // floatVal
    2.71828182845,  // doubleVal
    1.41421356237,  // ldVal
    true,  // boolTrue
    false,  // boolFalse
    _spool_0,  // strVal
    _spool_1,  // svVal
    Array<int>{_backing_AllTypesConfig_intArray_data, 5},  // intArray
    Array<double>{_backing_AllTypesConfig_doubleArray_data, 3},  // doubleArray
    Array<std::string>{_backing_AllTypesConfig_strArray_data, 3},  // strArray
    Array<bool>{_backing_AllTypesConfig_boolArray_data, 3},  // boolArray
    Optional<int>{99},  // optIntPresent
    Optional<double>{0.5},  // optDoublePresent
    Optional<int>::empty(),  // optIntAbsent
    std::variant<int, Point2D>{std::in_place_index<0>, 7},  // variantScalar
    std::variant<int, Point2D>{std::in_place_index<1>, Point2D{
        10,  // x
        20  // y
    }},  // variantMapping
    {
        3,  // x
        4  // y
    }  // nested
};
