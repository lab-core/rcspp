// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <concepts>
#include <limits>
#include <optional>
#include <string>

#include "rcspp/resource/base/resource_type.hpp"

namespace rcspp {

/// @brief Floating-point-aware less-than-or-equal comparison with epsilon tolerance.
///
/// Returns true when @p lhs <= @p rhs + epsilon, handling the imprecision inherent
/// in floating-point arithmetic.
///
/// @tparam T A floating-point type.
/// @param lhs Left-hand side operand.
/// @param rhs Right-hand side operand.
/// @return True if @p lhs is considered less than or equal to @p rhs within tolerance.
template <typename T>
    requires(std::is_floating_point_v<T>)
bool value_leq(T lhs, T rhs) noexcept {
    return lhs <= rhs + std::numeric_limits<T>::epsilon();
}

/// @brief Exact less-than-or-equal comparison for non-floating-point types.
///
/// @tparam T Any non-floating-point comparable type.
/// @param lhs Left-hand side operand.
/// @param rhs Right-hand side operand.
/// @return True if @p lhs <= @p rhs.
template <typename T>
bool value_leq(T lhs, T rhs) noexcept {
    return lhs <= rhs;
}

/// @brief Floating-point-aware strict less-than comparison with epsilon tolerance.
///
/// Returns true when @p lhs < @p rhs - epsilon, guarding against false positives
/// caused by floating-point rounding.
///
/// @tparam T A floating-point type.
/// @param lhs Left-hand side operand.
/// @param rhs Right-hand side operand.
/// @return True if @p lhs is strictly less than @p rhs beyond epsilon tolerance.
template <typename T>
    requires(std::is_floating_point_v<T>)
bool value_lt(T lhs, T rhs) noexcept {
    return lhs < rhs - std::numeric_limits<T>::epsilon();
}

/// @brief Exact strict less-than comparison for non-floating-point types.
///
/// @tparam T Any non-floating-point comparable type.
/// @param lhs Left-hand side operand.
/// @param rhs Right-hand side operand.
/// @return True if @p lhs < @p rhs.
template <typename T>
bool value_lt(T lhs, T rhs) noexcept {
    return lhs < rhs;
}

/// @brief A resource that holds a single numeric value of type @p T.
///
/// Provides arithmetic helpers (add, reset) and comparison utilities (leq, geq)
/// used by the RCSPP labelling algorithm to track scalar resources such as cost,
/// time, or capacity along a path.
///
/// @tparam T The numeric value type (e.g. int, double).
template <typename T>
class NumericalResource {
    public:
        /// @brief Constructs a NumericalResource with the given initial value.
        ///
        /// @param value Initial scalar value; defaults to 0.
        explicit NumericalResource(T value = 0) : value_(value) {}

        /// @brief Returns the current scalar value held by this resource.
        ///
        /// @return The stored value.
        [[nodiscard]] auto get_value() const -> T { return value_; }

        /// @brief Sets the resource value to @p value.
        ///
        /// @param value New value to store.
        void set_value(T value) { value_ = value; }

        /// @brief Copies the value from another NumericalResource.
        ///
        /// @param resource Source resource whose value is copied.
        void set_value(const NumericalResource<T>& resource) { value_ = resource.value_; }

        /// @brief Adds @p value to the current resource value.
        ///
        /// @param value Amount to add.
        void add(T value) { value_ += value; }

        /// @brief Resets the resource value to zero.
        void reset() { value_ = 0; }

        /// @brief Returns a string representation of the current value.
        ///
        /// @return Human-readable string of the stored value.
        [[nodiscard]] std::string to_string() const { return std::to_string(value_); }

        /// @brief Returns true if this resource value is less than or equal to @p other's value.
        ///
        /// @param other Resource to compare against.
        /// @return True if this->value_ <= other.value_.
        [[nodiscard]] bool leq(const NumericalResource& other) const {
            return leq(other.get_value());
        }

        /// @brief Returns true if this value is less than or equal to @p other's value plus
        /// @p delta.
        ///
        /// Useful for bound checks with a tolerance or slack term.
        ///
        /// @param other Resource to compare against.
        /// @param delta Slack added to the right-hand side before comparison.
        /// @return True if this->value_ <= other.value_ + delta.
        [[nodiscard]] bool leq(const NumericalResource& other, double delta) const {
            return leq(other.get_value() + delta);
        }

        // bool operator<=(const NumericalResource<T>& other) const {
        /// @brief Returns true if this value is less than or equal to @p other_value.
        ///
        /// Uses value_leq which applies epsilon tolerance for floating-point types.
        ///
        /// @param other_value Scalar value to compare against.
        /// @return True if this->value_ <= other_value.
        [[nodiscard]] bool leq(T other_value) const { return value_leq(value_, other_value); }

        /// @brief Returns true if this resource value is greater than or equal to @p other's
        /// value.
        ///
        /// @param other Resource to compare against.
        /// @return True if this->value_ >= other.value_.
        [[nodiscard]] bool geq(const NumericalResource& other) const {
            return geq(other.get_value());
        }

        // bool operator<=(const NumericalResource<T>& other) const {
        /// @brief Returns true if this value is greater than or equal to @p other_value.
        ///
        /// Uses value_leq which applies epsilon tolerance for floating-point types.
        ///
        /// @param other_value Scalar value to compare against.
        /// @return True if this->value_ >= other_value.
        [[nodiscard]] bool geq(T other_value) const { return value_leq(other_value, value_); }

    private:
        T value_;
};
}  // namespace rcspp
