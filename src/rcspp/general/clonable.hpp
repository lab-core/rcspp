// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <typeinfo>

namespace rcspp {

template <class DerivedType, class BaseType, class ReturnType = BaseType>
class Clonable : public BaseType {
    public:
        [[nodiscard]] auto clone() const -> std::unique_ptr<ReturnType> override {
            return std::make_unique<DerivedType>(static_cast<DerivedType const&>(*this));
        }
};
}  // namespace rcspp
