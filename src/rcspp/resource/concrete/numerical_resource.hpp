// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <limits>
#include <optional>

#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

// Helper: floating-point-aware <= with tolerance, NaN and infinity handling
template <typename T>
    requires(std::is_floating_point_v<T>)
bool value_leq(T lhs, T rhs) noexcept {
    return lhs <= rhs + std::numeric_limits<T>::epsilon();
}

// Fallback for non-floating types: exact comparison
template <typename T>
bool value_leq(T lhs, T rhs) noexcept {
    return lhs <= rhs;
}

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
class NumericalResource : public ResourceBase<NumericalResource<T>> {
    public:
        explicit NumericalResource(T value = 0) : value_(value) {}

        [[nodiscard]] auto get_value() const -> T { return value_; }

        void set_value(T value) { value_ = value; }

        void set_value(const NumericalResource<T>& resource) { value_ = resource.value_; }

        void add(T value) { value_ += value; }

        void reset() override { value_ = 0; }

        [[nodiscard]] bool leq(const NumericalResource<T>& other) const {
            return leq(other.get_value());
        }

        // bool operator<=(const NumericalResource<T>& other) const {
        [[nodiscard]] bool leq(T other_value) const {
            return value_ <= other_value;
            return value_leq(value_, other_value);
        }

        [[nodiscard]] bool geq(const NumericalResource<T>& other) const {
            return geq(other.get_value());
        }

        // bool operator<=(const NumericalResource<T>& other) const {
        [[nodiscard]] bool geq(T other_value) const {
            return value_ >= other_value;
            return value_leq(other_value, value_);
        }

    private:
        T value_;
};
}  // namespace rcspp
