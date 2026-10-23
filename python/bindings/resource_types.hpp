// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Resource type registry — add new types here only. Register only the types a bound graph uses
// (graph_mix2.cpp, graph_mix3.cpp): the others' function classes cannot be used from Python.
// Format: X(py_name, scalar_type, CppResourceType)
//   py_name         — used in Python method names (add_<py_name>_resource) and C++ binding names
//   scalar_type     — the underlying scalar / element type
//   CppResourceType — the full C++ resource type (from resource_traits.hpp)

// clang-format off
#define RCSPP_NUMERICAL_RESOURCES(X)             \
    X(real,          double,        RealResource) \
    X(int,           int,           IntResource)

// The signed subset, for functions that require a signed value type (e.g. CapacityExtensionFunction,
// which subtracts when extending backwards).
#define RCSPP_SIGNED_NUMERICAL_RESOURCES(X)       \
    X(real,          double,        RealResource) \
    X(int,           int,           IntResource)

#define RCSPP_SET_RESOURCES(X)                    \
    X(real_set,   double,        RealSetResource)  \
    X(int_set,    int,           IntSetResource)

#define RCSPP_BITSET_RESOURCES(X)                 \
    X(bitset,     unsigned int,  UIntBitsetResource)

#define RCSPP_CONTAINER_RESOURCES(X) \
    RCSPP_SET_RESOURCES(X)           \
    RCSPP_BITSET_RESOURCES(X)

#define RCSPP_ALL_RESOURCES(X)   \
    RCSPP_NUMERICAL_RESOURCES(X) \
    RCSPP_CONTAINER_RESOURCES(X)
// clang-format on
