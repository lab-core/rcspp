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

template <typename T>
inline std::string demangle(const T& obj) {
    return demangle(typeid(obj));
}
}  // namespace rcspp
