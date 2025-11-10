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
            // Prefer a safe runtime downcast: if *this is indeed a DerivedType,
            // dynamic_cast returns a non-null pointer and we can copy-construct.
            if (auto derived_ptr = dynamic_cast<const DerivedType*>(this)) {
                return std::make_unique<DerivedType>(*derived_ptr);
            }
            // If the dynamic cast fails, signal an error. Throwing std::bad_cast is
            // appropriate because the caller expected a DerivedType instance.
            throw std::bad_cast();
        }
};
}  // namespace rcspp
