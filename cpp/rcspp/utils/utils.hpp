// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstdlib>
#include <string>
#include <typeinfo>

#if defined(__GNUC__) || (defined(__clang__) && !defined(_MSC_VER))
#include <cxxabi.h>
#define RCSPP_HAS_CXA_DEMANGLE 1
#else
#define RCSPP_HAS_CXA_DEMANGLE 0
#endif

namespace rcspp {

/// @brief Returns a human-readable type name for the given `std::type_info`.
///
/// On GCC and Clang (non-MSVC), uses `abi::__cxa_demangle` to convert the
/// internal mangled name into the familiar C++ spelling
/// (e.g., `"std::vector<int>"`).  On other toolchains the raw `ti.name()`
/// string is returned unchanged.
///
/// @param ti  The `std::type_info` object whose name to demangle.
/// @return    Demangled type name, or the raw mangled name if demangling is
///            unavailable.
inline std::string demangle(const std::type_info& ti) {
#if RCSPP_HAS_CXA_DEMANGLE
    int status = 0;
    char* s = abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status);
    std::string result = (status == 0 && s != nullptr) ? s : ti.name();
    std::free(s);
    return result;
#else
    // No demangling available, return the mangled name
    return ti.name();
#endif
}

/// @brief Returns a human-readable type name for the dynamic type of @p obj.
///
/// Equivalent to calling `demangle(typeid(obj))`.  For polymorphic types,
/// the most-derived type name is returned.
///
/// @tparam T   Type of the object (deduced).
/// @param  obj Object whose dynamic type name to demangle.
/// @return     Demangled type name string.
template <typename T>
inline std::string demangle(const T& obj) {
    return demangle(typeid(obj));
}
}  // namespace rcspp
