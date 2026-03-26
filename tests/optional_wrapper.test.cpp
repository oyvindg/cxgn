/**
 * @file optional_wrapper.test.cpp
 * @brief Unit tests for the Optional<T> wrapper.
 *
 * Tests covered:
 * - operator bool: present and empty
 * - Default construction produces empty optional
 * - factory: Optional<T>::empty()
 * - Dereference with operator*
 * - value_or fallback
 * - operator-> member access
 * - constexpr correctness (static_assert)
 */

#include <cxgen/Optional.hpp>
#include <cassert>
#include <iostream>
#include <string>

/* ── constexpr correctness ────────────────────────────────────────────────── */

static_assert(Optional<int>{42},           "present optional must be truthy");
static_assert(!Optional<int>::empty(),     "empty() optional must be falsy");
static_assert(!Optional<int>{},            "default-constructed optional must be falsy");
static_assert(*Optional<int>{7} == 7,      "dereference must return stored value");
static_assert(Optional<int>{5}.value_or(0) == 5,  "value_or on present must return value");
static_assert(Optional<int>::empty().value_or(9) == 9, "value_or on empty must return default");

/* ── operator bool ────────────────────────────────────────────────────────── */

static void test_operator_bool_present() {
    constexpr Optional<int> opt{42};
    assert(opt);
    assert(!!opt);
    std::cout << "  \u2713 test_operator_bool_present\n";
}

static void test_operator_bool_empty_factory() {
    constexpr Optional<int> opt = Optional<int>::empty();
    assert(!opt);
    std::cout << "  \u2713 test_operator_bool_empty_factory\n";
}

static void test_operator_bool_default_constructed() {
    constexpr Optional<int> opt;
    assert(!opt);
    std::cout << "  \u2713 test_operator_bool_default_constructed\n";
}

/* ── dereference ──────────────────────────────────────────────────────────── */

static void test_dereference() {
    constexpr Optional<int> opt{99};
    assert(*opt == 99);
    std::cout << "  \u2713 test_dereference\n";
}

static void test_arrow_operator() {
    struct Point { int x; int y; };
    constexpr Optional<Point> opt{Point{3, 4}};
    assert(opt->x == 3);
    assert(opt->y == 4);
    std::cout << "  \u2713 test_arrow_operator\n";
}

/* ── value_or ─────────────────────────────────────────────────────────────── */

static void test_value_or_present() {
    constexpr Optional<int> opt{5};
    assert(opt.value_or(0) == 5);
    std::cout << "  \u2713 test_value_or_present\n";
}

static void test_value_or_empty() {
    constexpr Optional<int> opt = Optional<int>::empty();
    assert(opt.value_or(42) == 42);
    std::cout << "  \u2713 test_value_or_empty\n";
}

/* ── non-trivial type ─────────────────────────────────────────────────────── */

static void test_string_optional() {
    Optional<std::string> opt{"hello"};
    assert(opt);
    assert(*opt == "hello");

    Optional<std::string> absent = Optional<std::string>::empty();
    assert(!absent);
    assert(absent.value_or("fallback") == "fallback");

    std::cout << "  \u2713 test_string_optional\n";
}

/* ── typical if-guard usage ───────────────────────────────────────────────── */

static void test_if_guard_idiom() {
    constexpr Optional<int> present{7};
    constexpr Optional<int> absent = Optional<int>::empty();

    int seen = 0;
    if (present) seen += *present;
    if (absent)  seen += 100;  /* must not execute */
    assert(seen == 7);

    std::cout << "  \u2713 test_if_guard_idiom\n";
}

/* ── main ─────────────────────────────────────────────────────────────────── */

int main() {
    std::cout << "Running optional_wrapper tests...\n";
    test_operator_bool_present();
    test_operator_bool_empty_factory();
    test_operator_bool_default_constructed();
    test_dereference();
    test_arrow_operator();
    test_value_or_present();
    test_value_or_empty();
    test_string_optional();
    test_if_guard_idiom();
    std::cout << "All optional_wrapper tests passed!\n";
    return 0;
}
