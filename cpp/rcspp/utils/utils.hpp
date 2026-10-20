// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstdlib>
#include <string>
#include <string_view>
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

/// @brief Returns @p name without its `rcspp::` qualifiers.
///
/// @param name A demangled type name.
/// @return     @p name with every `rcspp::` removed.
inline std::string without_rcspp_namespace(std::string name) {
    constexpr std::string_view kNamespace = "rcspp::";
    for (auto pos = name.find(kNamespace); pos != std::string::npos;
         pos = name.find(kNamespace, pos)) {
        name.erase(pos, kNamespace.size());
    }
    return name;
}

/// @brief Returns the demangled dynamic type name of @p obj, without its `rcspp::` qualifiers.
///
/// Error messages name the user's class this way: the library namespace only adds noise.
///
/// @tparam T   Type of the object (deduced).
/// @param  obj Object whose dynamic type name to return.
/// @return     Demangled type name, e.g. `"TimeWindowFeasibilityFunction<RealResource>"`.
template <typename T>
inline std::string short_type_name(const T& obj) {
    return without_rcspp_namespace(demangle(obj));
}

/// @brief Returns the demangled name of type @p T, without its `rcspp::` qualifiers.
///
/// For a message about a type when no object of it exists.
///
/// @tparam T The type to name.
/// @return   Demangled type name.
template <typename T>
inline std::string short_type_name() {
    return without_rcspp_namespace(demangle(typeid(T)));
}

/// @brief Returns @p name with every template argument list (`<...>`) removed.
///
/// @param name A type name.
/// @return     e.g. `"PullingDominanceAlgorithm"` for `"PullingDominanceAlgorithm<A, B>"`.
inline std::string without_template_arguments(const std::string& name) {
    std::string result;
    int depth = 0;
    for (const char c : name) {
        if (c == '<') {
            ++depth;
        } else if (c == '>') {
            --depth;
        } else if (depth == 0) {
            result += c;
        }
    }
    return result;
}

/// @brief Returns the name of type @p T without its `rcspp::` qualifiers or template arguments,
///        e.g. `"PullingDominanceAlgorithm"`.
///
/// @tparam T The type to name.
/// @return   The short class name.
template <typename T>
inline std::string template_name() {
    return without_template_arguments(short_type_name<T>());
}

/// @brief Returns the name of the dynamic type of @p obj without its `rcspp::` qualifiers or
///        template arguments.
///
/// @tparam T   Type of the object (deduced).
/// @param  obj Object whose dynamic type to name.
/// @return     The short class name.
template <typename T>
inline std::string template_name(const T& obj) {
    return without_template_arguments(short_type_name(obj));
}
}  // namespace rcspp
