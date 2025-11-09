// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <optional>

#include "rcspp/resource/base/resource_base.hpp"

namespace rcspp {

template <typename T>
class NumResource : public ResourceBase<NumResource<T>> {
    public:
        explicit NumResource(T value = 0) : value_(value) {}

        [[nodiscard]] auto get_value() const -> T { return value_; }

        void set_value(T value) { value_ = value; }

        void reset() override { value_ = 0; }

    private:
        T value_;
};

// Type aliases for common numeric resource types
using RealResource = NumResource<double>;
using IntResource = NumResource<int>;
using LongResource = NumResource<int64_t>;
using SizeTResource = NumResource<size_t>;
}  // namespace rcspp
