/**
 * @file Optional.hpp
 * @brief Constexpr optional for cxgen generated code.
 *
 * A simple, constexpr-friendly optional type for use in
 * generated configuration code.
 */

#pragma once

namespace cxgen {

/**
 * @brief Constexpr optional value.
 *
 * @tparam T Value type
 *
 * A minimal optional implementation for constexpr contexts.
 * Unlike std::optional, this has a trivial destructor and
 * is always constexpr-constructible.
 *
 * @code
 * constexpr Optional<int> some{42};
 * constexpr Optional<int> none = Optional<int>::empty();
 * static_assert(some);
 * static_assert(!none);
 * @endcode
 */
template<typename T>
struct Optional {
private:
    T value_{};
    bool has_value_ = false;

public:
    constexpr Optional() noexcept = default;

    constexpr explicit Optional(const T& value) noexcept
        : value_(value), has_value_(true) {}

    constexpr explicit Optional(T&& value) noexcept
        : value_(static_cast<T&&>(value)), has_value_(true) {}

    /**
     * @brief Create an empty optional.
     */
    [[nodiscard]] static constexpr Optional empty() noexcept {
        return Optional{};
    }

    /**
     * @brief Returns true if a value is present.
     */
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return has_value_; }

    /**
     * @brief Get the value (undefined if empty).
     */
    [[nodiscard]] constexpr const T& value() const noexcept { return value_; }

    /**
     * @brief Get the value (undefined if empty).
     */
    [[nodiscard]] constexpr T& value() noexcept { return value_; }

    /**
     * @brief Get value or default.
     */
    [[nodiscard]] constexpr T value_or(const T& default_value) const noexcept {
        return has_value_ ? value_ : default_value;
    }

    /**
     * @brief Dereference operator.
     */
    [[nodiscard]] constexpr const T& operator*() const noexcept { return value_; }
    [[nodiscard]] constexpr T& operator*() noexcept { return value_; }

    /**
     * @brief Arrow operator.
     */
    [[nodiscard]] constexpr const T* operator->() const noexcept { return &value_; }
    [[nodiscard]] constexpr T* operator->() noexcept { return &value_; }
};

} // namespace cxgen

/* Global namespace alias for convenience in generated code */
using cxgen::Optional;
