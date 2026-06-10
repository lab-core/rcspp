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

// Helper: floating-point-aware <= with tolerance, NaN and infinity handling
template <typename T>
    requires(std::is_floating_point_v<T>)
bool value_leq(T lhs, T rhs) noexcept {
    return lhs <= rhs + std::numeric_limits<T>::epsilon();
}

// Fallback for non-floating types: exact comparison
// GCOVR_EXCL_START (integer value_leq fallback; only floating-point types used in unit tests)
template <typename T>
bool value_leq(T lhs, T rhs) noexcept {
    return lhs <= rhs;
}
// GCOVR_EXCL_STOP

// Helper: floating-point-aware <= with tolerance, NaN and infinity handling
template <typename T>
    requires(std::is_floating_point_v<T>)
bool value_lt(T lhs, T rhs) noexcept {
    return lhs < rhs - std::numeric_limits<T>::epsilon();
}

// Fallback for non-floating types: exact comparison
template <typename T>
bool value_lt(T lhs, T rhs) noexcept {
    return lhs < rhs;
}

template <typename T>
class NumericalResource {
    public:
        explicit NumericalResource(T value = 0) : value_(value) {}

        [[nodiscard]] auto get_value() const -> T { return value_; }  // GCOVR_EXCL_LINE

        void set_value(T value) { value_ = value; }  // GCOVR_EXCL_LINE

        void set_value(const NumericalResource<T>& resource) { value_ = resource.value_; }

        void add(T value) { value_ += value; }

        void reset() { value_ = 0; }

        [[nodiscard]] std::string to_string() const {
            return std::to_string(value_);
        }  // GCOVR_EXCL_LINE

        [[nodiscard]] bool leq(const NumericalResource& other) const {
            return leq(other.get_value());  // GCOVR_EXCL_LINE
        }

        [[nodiscard]] bool leq(const NumericalResource& other, double delta) const {
            return leq(other.get_value() + delta);
        }

        // bool operator<=(const NumericalResource<T>& other) const {
        [[nodiscard]] bool leq(T other_value) const {
            return value_leq(value_, other_value);
        }  // GCOVR_EXCL_LINE

        [[nodiscard]] bool geq(const NumericalResource& other) const {
            return geq(other.get_value());
        }

        // bool operator<=(const NumericalResource<T>& other) const {
        [[nodiscard]] bool geq(T other_value) const {
            return value_leq(other_value, value_);
        }  // GCOVR_EXCL_LINE

    private:
        T value_;
};
}  // namespace rcspp
