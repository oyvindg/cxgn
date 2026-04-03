/**
 * @file Array.hpp
 * @brief Constexpr array view for cxgn generated code.
 *
 * A lightweight, constexpr-friendly array view that references
 * static backing storage. Used by cxgn to generate Array<T> fields.
 */

#pragma once

#include <cstddef>

namespace cxgn {

/**
 * @brief Constexpr array view over static data.
 *
 * @tparam T Element type
 *
 * This is a non-owning view designed for constexpr contexts.
 * The backing data must have static storage duration.
 *
 * @code
 * static constexpr int data[] = {1, 2, 3};
 * constexpr Array<int> arr{data, 3};
 * static_assert(arr[0] == 1);
 * static_assert(arr.size() == 3);
 * @endcode
 */
template<typename T>
struct Array {
    const T* data_ = nullptr;
    std::size_t size_ = 0;

    constexpr Array() noexcept = default;
    constexpr Array(const T* data, std::size_t size) noexcept
        : data_(data), size_(size) {}

    /**
     * @brief Get number of elements.
     */
    [[nodiscard]] constexpr std::size_t size() const noexcept { return size_; }

    /**
     * @brief Check if array is empty.
     */
    [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }

    /**
     * @brief Access element by index.
     */
    [[nodiscard]] constexpr const T& operator[](std::size_t index) const noexcept {
        return data_[index];
    }

    /**
     * @brief Get pointer to data.
     */
    [[nodiscard]] constexpr const T* data() const noexcept { return data_; }

    /**
     * @brief Iterator support.
     */
    [[nodiscard]] constexpr const T* begin() const noexcept { return data_; }
    [[nodiscard]] constexpr const T* end() const noexcept { return data_ + size_; }

    /**
     * @brief Get first element.
     */
    [[nodiscard]] constexpr const T& front() const noexcept { return data_[0]; }

    /**
     * @brief Get last element.
     */
    [[nodiscard]] constexpr const T& back() const noexcept { return data_[size_ - 1]; }
};

} // namespace cxgn

/* Global namespace alias for convenience in generated code */
using cxgn::Array;
