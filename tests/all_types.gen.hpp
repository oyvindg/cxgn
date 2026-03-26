// GENERATED FILE - DO NOT EDIT
// Source YAML: tests/fixtures/all_types.yaml
// Source Header: tests/fixtures/all_types.hpp
#pragma once

#include "fixtures/all_types.hpp"

namespace {
static constexpr int _backing_0_data[] = {1, 2, 3, 4, 5};
static constexpr size_t _backing_0_count = 5;
static constexpr double _backing_1_data[] = {1.1, 2.2, 3.3};
static constexpr size_t _backing_1_count = 3;
static constexpr std::string _backing_2_data[] = {"alpha", "beta", "gamma"};
static constexpr size_t _backing_2_count = 3;
static constexpr bool _backing_3_data[] = {true, false, true};
static constexpr size_t _backing_3_count = 3;
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
    "hello cxgen",  // strVal
    "string_view",  // svVal
    Array<int>{_backing_0_data, _backing_0_count},  // intArray
    Array<double>{_backing_1_data, _backing_1_count},  // doubleArray
    Array<std::string>{_backing_2_data, _backing_2_count},  // strArray
    Array<bool>{_backing_3_data, _backing_3_count},  // boolArray
    Optional<int>{99},  // optIntPresent
    Optional<double>{0.5},  // optDoublePresent
    Optional<int>::empty(),  // optIntAbsent
    OneOf<int, Point2D>{std::in_place_index<0>, 7},  // oneOfScalar
    OneOf<int, Point2D>{std::in_place_index<1>, Point2D{
        10,  // x
        20  // y
    }},  // oneOfMapping
    {
        3,  // x
        4  // y
    }  // nested
};
