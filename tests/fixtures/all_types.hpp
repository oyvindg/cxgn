#pragma once
#include <cxgn/Array.hpp>
#include <cxgn/Optional.hpp>
#include <variant>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstddef>
#include "shapes.hpp"

/** Nested struct used to test std::variant mapping and nested struct generation. */
struct Point2D {
    int x;
    int y;
};

/**
 * Exercises every YAML-representable type that cxgn can generate.
 *
 * YAML type → C++ field(s):
 *   integer  → int, long, short, int32_t, int64_t, size_t,
 *               unsigned int, long long,
 *               int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t, uint64_t
 *   float    → float, double, long double
 *   bool     → bool
 *   string   → std::string, std::string_view
 *   null     → Optional<T> (absent)
 *   scalar   → Optional<T> (present)
 *   sequence → Array<T>
 *   mapping  → nested struct, std::variant<A, Struct> (index 1),
 *               std::variant<StructA, StructB> (Circle case)
 */
struct AllTypesConfig {
    /* ── Integer types (YAML integer) ──────────────────────────────────── */
    int          intVal;
    long         longVal;
    short        shortVal;
    int32_t      i32Val;
    int64_t      i64Val;
    size_t       sizeVal;
    unsigned int uintVal;
    long long    llVal;
    int8_t       i8Val;
    int16_t      i16Val;
    uint8_t      u8Val;
    uint16_t     u16Val;
    uint32_t     u32Val;
    uint64_t     u64Val;

    /* ── Float types (YAML float) ───────────────────────────────────────── */
    float       floatVal;
    double      doubleVal;
    long double ldVal;

    /* ── Boolean (YAML bool) ────────────────────────────────────────────── */
    bool boolTrue;
    bool boolFalse;

    /* ── String types (YAML string) ─────────────────────────────────────── */
    std::string      strVal;
    std::string_view svVal;

    /* ── Arrays (YAML sequence) ─────────────────────────────────────────── */
    Array<int>         intArray;
    Array<double>      doubleArray;
    Array<std::string> strArray;
    Array<bool>        boolArray;

    /* ── Optionals ──────────────────────────────────────────────────────── */
    Optional<int>    optIntPresent;
    Optional<double> optDoublePresent;
    Optional<int>    optIntAbsent;

    /* ── std::variant: scalar → index 0, mapping → index 1 ──────────────── */
    std::variant<int, Point2D> variantScalar;
    std::variant<int, Point2D> variantMapping;
    std::variant<Circle, Rectangle> shape;

    /* ── Nested struct (YAML mapping) ───────────────────────────────────── */
    Point2D nested;
};
