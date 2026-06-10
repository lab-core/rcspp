// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Resource type registry — add new types here only.
// Format: X(py_name, scalar_type, CppResourceType)
//   py_name         — used in Python method names (add_<py_name>_resource) and C++ binding names
//   scalar_type     — the underlying scalar / element type
//   CppResourceType — the full C++ resource type (from resource_traits.hpp)

// clang-format off
#define RCSPP_NUMERICAL_RESOURCES(X)             \
    X(real,          double,        RealResource) \
    X(int,           int,           IntResource)  \
    X(uint,          unsigned int,  UIntResource)

#define RCSPP_SET_RESOURCES(X)                    \
    X(real_set,   double,        RealSetResource)  \
    X(int_set,    int,           IntSetResource)   \
    X(uint_set,   unsigned int,  UIntSetResource)  \
    X(size_t_set, size_t,        SizeTSetResource)

#define RCSPP_BITSET_RESOURCES(X)                        \
    X(uint_bitset,   unsigned int,  UIntBitsetResource)   \
    X(size_t_bitset, size_t,        SizeTBitsetResource)

#define RCSPP_CONTAINER_RESOURCES(X) \
    RCSPP_SET_RESOURCES(X)           \
    RCSPP_BITSET_RESOURCES(X)

#define RCSPP_ALL_RESOURCES(X)   \
    RCSPP_NUMERICAL_RESOURCES(X) \
    RCSPP_CONTAINER_RESOURCES(X)
// clang-format on
