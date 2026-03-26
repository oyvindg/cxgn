/**
 * @file OneOf.hpp
 * @brief Public declarations for One Of.
 */

#pragma once

#include <variant>

namespace cxgen {

/**
 * @brief Alias representing one of two alternative types.
 *
 * Thin wrapper around `std::variant<A, B>` used in generated-code
 * declarations where a field accepts one of two schema alternatives.
 *
 * @tparam A First allowed type.
 * @tparam B Second allowed type.
 */
template<typename A, typename B>
using OneOf = std::variant<A, B>;

} // namespace cxgen

using cxgen::OneOf;
