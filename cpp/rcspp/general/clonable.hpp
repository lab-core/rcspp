// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <typeinfo>

namespace rcspp {

/// @brief CRTP mixin that provides a type-safe @c clone() implementation for polymorphic
/// hierarchies.
///
/// Inherit from @c Clonable to automatically implement the @c clone() method declared in
/// @p BaseType. The returned @c unique_ptr<ReturnType> always points to a freshly copy-constructed
/// @p DerivedType, preserving the full derived-class state.
///
/// Example usage:
/// @code
/// class MyResource : public Clonable<MyResource, Resource<MyType>> { ... };
/// @endcode
///
/// @tparam DerivedType The concrete class that inherits from this mixin. Used for the
/// copy-construction.
/// @tparam BaseType The abstract base class that declares the virtual @c clone() method.
/// @tparam ReturnType The type returned by @c clone() (defaults to @p BaseType). Useful when the
///         base hierarchy uses a covariant return type different from @p BaseType.
template <class DerivedType, class BaseType, class ReturnType = BaseType>
class Clonable : public BaseType {
    public:
        /// @brief Creates a deep copy of this object as the concrete @p DerivedType.
        ///
        /// @return A @c unique_ptr<ReturnType> owning a newly copy-constructed @p DerivedType.
        [[nodiscard]] auto clone() const -> std::unique_ptr<ReturnType> override {
            return std::make_unique<DerivedType>(static_cast<DerivedType const&>(*this));
        }
};
}  // namespace rcspp
