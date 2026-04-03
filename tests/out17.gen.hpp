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
static const std::string _backing_AllTypesConfig_strArray_data[] = {_spool_2, _spool_3, _spool_4};
static constexpr int _backing_AllTypesConfig_intArray_data[] = {1, 2, 3, 4, 5};
static constexpr bool _backing_AllTypesConfig_boolArray_data[] = {true, false, true};
} // namespace

// Usage examples:
//   Access fields directly on config.
//   int value = config.intVal;
//   long value = config.longVal;
//   short value = config.shortVal;
//   int32_t value = config.i32Val;
//   int64_t value = config.i64Val;
//   size_t value = config.sizeVal;
//   unsigned int value = config.uintVal;
//   long long value = config.llVal;
//   int8_t value = config.i8Val;
//   int16_t value = config.i16Val;
//   uint8_t value = config.u8Val;
//   uint16_t value = config.u16Val;
//   uint32_t value = config.u32Val;
//   uint64_t value = config.u64Val;
//   float value = config.floatVal;
//   double value = config.doubleVal;
//   long double value = config.ldVal;
//   if (config.boolTrue) {
//   }
//   if (config.boolFalse) {
//   }
//   std::string s = config.strVal;
//   const char* cstr = config.strVal.c_str();
//   std::string_view s = config.svVal;
//   const char* data = config.svVal.data();
//   const int* data = config.intArray.data;
//   size_t count = config.intArray.size;
//   const double* data = config.doubleArray.data;
//   size_t count = config.doubleArray.size;
//   const std::string* data = config.strArray.data;
//   size_t count = config.strArray.size;
//   const bool* data = config.boolArray.data;
//   size_t count = config.boolArray.size;
//   if (config.optIntPresent) {
//       int value = config.optIntPresent.value;
//   }
//   if (config.optDoublePresent) {
//       double value = config.optDoublePresent.value;
//   }
//   if (config.optIntAbsent) {
//       int value = config.optIntAbsent.value;
//   }
//   // variantScalar alternatives:
//   if (const int* value = std::get_if<int>(&config.variantScalar)) {
//       (void)*value;
//   }
//   if (const Point2D* value = std::get_if<Point2D>(&config.variantScalar)) {
//       (void)value->x;
//   }
//   // variantMapping alternatives:
//   if (const int* value = std::get_if<int>(&config.variantMapping)) {
//       (void)*value;
//   }
//   if (const Point2D* value = std::get_if<Point2D>(&config.variantMapping)) {
//       (void)value->x;
//   }
//   // shape alternatives:
//   if (const Circle* value = std::get_if<Circle>(&config.shape)) {
//       (void)value->radius;
//   }
//   if (const Rectangle* value = std::get_if<Rectangle>(&config.shape)) {
//       (void)value->width;
//   }
//   const Point2D& value = config.nested;
//   (void)value.x;

const AllTypesConfig config = {
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
    std::variant<Circle, Rectangle>{std::in_place_index<0>, Circle{
        12.5  // radius
    }},  // shape
    {
        3,  // x
        4  // y
    }  // nested
};
