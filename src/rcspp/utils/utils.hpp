// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cxxabi.h>

#include <cstdlib>
#include <string>
#include <typeinfo>

namespace rcspp {
inline std::string demangle(const std::type_info& ti) {
    int status = 0;
    char* s = abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status);
    std::string result = (status == 0 && s != nullptr) ? s : ti.name();
    std::free(s);
    return result;
}

template <typename T>
inline std::string demangle(const T& obj) {
    return demangle(typeid(obj));
}
}  // namespace rcspp
