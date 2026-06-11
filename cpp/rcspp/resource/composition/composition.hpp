// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace rcspp {

/// @brief Compile-time index of @p ComponentType within the type list @p ComponentTypes.
///
/// Provides a `value` equal to the zero-based position of the first occurrence of
/// @p ComponentType in @p ComponentTypes, or `-1` if it is not present.
///
/// @tparam ComponentType The type to search for.
/// @tparam ComponentTypes The ordered list of types to search within.
template <typename ComponentType, typename... ComponentTypes>
struct ComponentTypeIndex;

/// @brief Base case: empty list — @p ComponentType was not found.
///
/// @tparam ComponentType The type being searched for.
template <typename ComponentType>
struct ComponentTypeIndex<ComponentType> {
        /// @brief Sentinel value indicating the type was not found.
        static constexpr int value = -1;
};

/// @brief Recursive case: compare @p ComponentType against the front of the list.
///
/// Returns 0 if @p ComponentType matches @p FrontComponentType; otherwise returns
/// one plus the index found in the remaining @p ComponentTypes (or -1 if not found).
///
/// @tparam ComponentType The type to search for.
/// @tparam FrontComponentType The first type in the current list.
/// @tparam ComponentTypes The remaining types in the list.
template <typename ComponentType, typename FrontComponentType, typename... ComponentTypes>
struct ComponentTypeIndex<ComponentType, FrontComponentType, ComponentTypes...> {
    private:
        static constexpr int next = ComponentTypeIndex<ComponentType, ComponentTypes...>::value;
        static constexpr int next_or_minus_one = (next == -1 ? -1 : 1 + next);

    public:
        /// @brief Zero-based index of @p ComponentType, or -1 if not present.
        static constexpr int value =
            std::is_same_v<ComponentType, FrontComponentType> ? 0 : next_or_minus_one;
};

/// @brief Convenience variable template for `ComponentTypeIndex<ComponentType,
/// ComponentTypes...>::value`.
///
/// Only participates in overload resolution when @p ComponentType is present in
/// @p ComponentTypes (i.e., the index is not -1).
///
/// @tparam ComponentType The type to locate.
/// @tparam ComponentTypes The ordered list of types to search within.
template <typename ComponentType, typename... ComponentTypes>
    requires(ComponentTypeIndex<ComponentType, ComponentTypes...>::value != -1)
inline constexpr int ComponentTypeIndex_v =
    ComponentTypeIndex<ComponentType, ComponentTypes...>::value;

/// @brief Trait that maps a @p ComponentType to its initializer tuple type.
///
/// The default implementation deduces the initializer as a one-element tuple
/// wrapping the decayed return type of `ComponentType::get_value()`.
/// Specializations may be provided for component types whose initializers
/// require multiple values.
///
/// @tparam ComponentType A type that exposes a `get_value()` member function.
template <typename ComponentType>
    requires requires { std::declval<ComponentType>().get_value(); }
struct ComponentInitializerTypeTuple {
        /// @brief The initializer tuple type for @p ComponentType.
        using type = std::tuple<std::decay_t<decltype(std::declval<ComponentType>().get_value())>>;
};

/// @brief Convenience alias for `ComponentInitializerTypeTuple<ComponentType>::type`.
///
/// @tparam ComponentType A type that satisfies the `ComponentInitializerTypeTuple` trait.
template <typename ComponentType>
using ComponentInitializerTypeTuple_t = typename ComponentInitializerTypeTuple<ComponentType>::type;

/// @brief Empty tag base class used to identify all `Composition` specializations.
struct CompositionTag {};

/// @brief Concept that checks whether @p T is any specialization of `Composition`.
///
/// @tparam T The type to test.
template <typename T>
concept IsComposition = std::derived_from<std::remove_cvref_t<T>, CompositionTag>;

/// @brief Heterogeneous container that holds one vector of `ComponentClass<BaseType>` per base
/// type.
///
/// Each entry in @p BaseTypes corresponds to a `std::vector<std::unique_ptr<ComponentClass<BT>>>`
/// stored in an internal tuple.  The class provides iteration helpers (`apply`,
/// `for_each_component`) that dispatch a callable across every component-type slot simultaneously.
///
/// @tparam ComponentClass A class template whose single template parameter is a base type.
/// @tparam BaseTypes      The ordered list of base types that parameterise @p ComponentClass.
template <template <typename> class ComponentClass, typename... BaseTypes>
class Composition : public CompositionTag {
    public:
        /// @brief Default constructor — creates an empty composition.
        Composition() = default;

        /// @brief Constructs a composition from an existing tuple of component vectors.
        ///
        /// @param components Tuple of per-type vectors to adopt (moved in).
        explicit Composition(
            std::tuple<std::vector<std::unique_ptr<ComponentClass<BaseTypes>>>...> components)
            : components_(std::move(components)) {}

        /// @brief Deep-copy constructor — clones every component via its `clone()` method.
        ///
        /// @param rhs_composition The composition to copy from.
        Composition(const Composition& rhs_composition) {
            // Apply clone_comp_vec_function to each component of the tuple
            this->apply(rhs_composition,
                        [&](auto& sing_comp_vec, const auto& rhs_sing_comp_vec) -> auto {
                            sing_comp_vec.reserve(rhs_sing_comp_vec.size());
                            std::transform(rhs_sing_comp_vec.begin(),
                                           rhs_sing_comp_vec.end(),
                                           std::back_inserter(sing_comp_vec),
                                           [](const auto& rhs_comp) { return rhs_comp->clone(); });
                        });
        }

        /// @brief Move constructor — transfers ownership of all components.
        ///
        /// @param rhs_composition The composition to move from.
        Composition(Composition&& rhs_composition) noexcept : components_() {
            swap(*this, rhs_composition);
        }

        /// @brief Virtual destructor.
        virtual ~Composition() = default;

        /// @brief Copy-and-swap assignment operator.
        ///
        /// @param rhs_composition The composition to assign from (passed by value).
        /// @return Reference to this composition after assignment.
        auto operator=(Composition rhs_composition) -> auto& {
            swap(*this, rhs_composition);
            return *this;
        }

        /// @brief Swaps the contents of two `Composition` objects.
        ///
        /// Enables the copy-and-swap idiom.
        ///
        /// @param first  First composition.
        /// @param second Second composition.
        friend void swap(Composition& first, Composition& second) {
            using std::swap;
            swap(first.components_, second.components_);
        }

        /// @brief Moves a component into the slot identified by @p ComponentTypeIndex.
        ///
        /// @tparam ComponentTypeIndex Index of the target component-type slot in the tuple.
        /// @tparam ComponentType      Concrete type of the component being added.
        /// @param component           Owning pointer to the component (moved in).
        /// @return Reference to the newly added component.
        template <size_t ComponentTypeIndex, typename ComponentType>
        auto add_component(std::unique_ptr<ComponentType> component) -> auto& {
            return *std::get<ComponentTypeIndex>(components_).emplace_back(std::move(component));
        }

        /// @brief Applies a unary callable to each per-type component vector (mutable).
        ///
        /// The callable receives a `std::vector<std::unique_ptr<ComponentClass<BT>>>&` for
        /// each base type @p BT in @p BaseTypes.
        ///
        /// @tparam Func Callable type with signature `void(auto&)`.
        /// @param func  The callable to invoke.
        template <typename Func>
        void apply(Func&& func) {
            std::apply([&](auto&&... args_comp_vec) -> auto { (func(args_comp_vec), ...); },
                       components_);
        }

        /// @brief Applies a unary callable to each per-type component vector (const).
        ///
        /// @tparam Func Callable type with signature `void(const auto&)`.
        /// @param func  The callable to invoke.
        template <typename Func>
        void apply(Func&& func) const {
            std::apply([&](const auto&... args_comp_vec) -> auto { (func(args_comp_vec), ...); },
                       components_);
        }

        /// @brief Applies a binary callable to paired component vectors of this and another
        /// composition (mutable overload, rvalue ref).
        ///
        /// @tparam Func Callable type with signature `void(auto&, auto&&)`.
        /// @tparam Comp A type satisfying `IsComposition`.
        /// @param rhs_composition The right-hand composition.
        /// @param func            The callable to invoke.
        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(Comp&& rhs_composition, Func&& func) {
            apply(rhs_composition.get_components(), func);
        }

        /// @brief Applies a binary callable to paired component vectors of this and another
        /// composition (const overload, rvalue ref).
        ///
        /// @tparam Func Callable type with signature `void(const auto&, auto&&)`.
        /// @tparam Comp A type satisfying `IsComposition`.
        /// @param rhs_composition The right-hand composition.
        /// @param func            The callable to invoke.
        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(Comp&& rhs_composition, Func&& func) const {
            apply(rhs_composition.get_components(), func);
        }

        /// @brief Applies a binary callable to paired component vectors of this and another
        /// composition (mutable overload, const lvalue ref).
        ///
        /// @tparam Func Callable type with signature `void(auto&, const auto&)`.
        /// @tparam Comp A type satisfying `IsComposition`.
        /// @param rhs_composition The right-hand composition.
        /// @param func            The callable to invoke.
        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(const Comp& rhs_composition, Func&& func) {
            apply(rhs_composition.get_components(), func);
        }

        /// @brief Applies a binary callable to paired component vectors of this and another
        /// composition (const overload, const lvalue ref).
        ///
        /// @tparam Func Callable type with signature `void(const auto&, const auto&)`.
        /// @tparam Comp A type satisfying `IsComposition`.
        /// @param rhs_composition The right-hand composition.
        /// @param func            The callable to invoke.
        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(const Comp& rhs_composition, Func&& func) const {
            apply(rhs_composition.get_components(), func);
        }

        /// @brief Applies a binary callable to paired component vectors using a raw tuple of
        /// right-hand vectors (mutable overload, forwarding ref).
        ///
        /// @tparam Func Callable type with signature `void(auto&, auto&&)`.
        /// @tparam Comp Raw tuple type compatible with the component-vector tuple.
        /// @param rhs_components Forwarding-ref to the right-hand component tuple.
        /// @param func           The callable to invoke.
        template <typename Func, typename Comp>
        void apply(Comp&& rhs_components, Func&& func) {
            std::apply(
                [&](auto&&... args_comp_vec) -> auto {
                    std::apply(
                        [&](auto&&... args_rhs_comp_vec) -> auto {
                            (func(args_comp_vec, args_rhs_comp_vec), ...);
                        },
                        rhs_components);
                },
                components_);
        }

        /// @brief Applies a binary callable to paired component vectors using a raw tuple of
        /// right-hand vectors (const overload, forwarding ref).
        ///
        /// @tparam Func Callable type with signature `void(const auto&, auto&&)`.
        /// @tparam Comp Raw tuple type compatible with the component-vector tuple.
        /// @param rhs_components Forwarding-ref to the right-hand component tuple.
        /// @param func           The callable to invoke.
        template <typename Func, typename Comp>
        void apply(Comp&& rhs_components, Func&& func) const {
            std::apply(
                [&](const auto&... args_comp_vec) -> auto {
                    std::apply(
                        [&](auto&&... args_rhs_comp_vec) -> auto {
                            (func(args_comp_vec, args_rhs_comp_vec), ...);
                        },
                        rhs_components);
                },
                components_);
        }

        /// @brief Applies a binary callable to paired component vectors using a raw tuple of
        /// right-hand vectors (mutable overload, const lvalue ref).
        ///
        /// @tparam Func Callable type with signature `void(auto&, const auto&)`.
        /// @tparam Comp Raw tuple type compatible with the component-vector tuple.
        /// @param rhs_components Const lvalue ref to the right-hand component tuple.
        /// @param func           The callable to invoke.
        template <typename Func, typename Comp>
        void apply(const Comp& rhs_components, Func&& func) {
            std::apply(
                [&](auto&&... args_comp_vec) -> auto {
                    std::apply(
                        [&](const auto&... args_rhs_comp_vec) -> auto {
                            (func(args_comp_vec, args_rhs_comp_vec), ...);
                        },
                        rhs_components);
                },
                components_);
        }

        /// @brief Applies a binary callable to paired component vectors using a raw tuple of
        /// right-hand vectors (const overload, const lvalue ref).
        ///
        /// @tparam Func Callable type with signature `void(const auto&, const auto&)`.
        /// @tparam Comp Raw tuple type compatible with the component-vector tuple.
        /// @param rhs_components Const lvalue ref to the right-hand component tuple.
        /// @param func           The callable to invoke.
        template <typename Func, typename Comp>
        void apply(const Comp& rhs_components, Func&& func) const {
            std::apply(
                [&](const auto&... args_comp_vec) -> auto {
                    std::apply(
                        [&](const auto&... args_rhs_comp_vec) -> auto {
                            (func(args_comp_vec, args_rhs_comp_vec), ...);
                        },
                        rhs_components);
                },
                components_);
        }

        /// @brief Applies a ternary callable to component-vector triples from this and two other
        /// compositions (mutable, both rhs as `IsComposition`).
        ///
        /// @tparam Func  Callable type with signature `void(auto&, const auto&, const auto&)`.
        /// @tparam Comp  A type satisfying `IsComposition`.
        /// @tparam Comp2 A type satisfying `IsComposition`.
        /// @param rhs_composition  First right-hand composition.
        /// @param rhs_composition2 Second right-hand composition.
        /// @param func             The callable to invoke.
        template <typename Func, typename Comp, typename Comp2>
            requires(IsComposition<Comp> && IsComposition<Comp2>)
        void apply(const Comp& rhs_composition, const Comp2& rhs_composition2, Func&& func) {
            apply(rhs_composition, rhs_composition2.get_components(), func);
        }

        /// @brief Applies a ternary callable to component-vector triples from this, one
        /// composition, and one raw component tuple (mutable overload).
        ///
        /// @tparam Func  Callable type with signature `void(auto&, const auto&, const auto&)`.
        /// @tparam Comp  A type satisfying `IsComposition`.
        /// @tparam Comp2 Raw tuple type compatible with the component-vector tuple.
        /// @param rhs_composition   Right-hand composition.
        /// @param rhs_components2   Right-hand raw component tuple.
        /// @param func              The callable to invoke.
        template <typename Func, typename Comp, typename Comp2>
            requires(IsComposition<Comp>)
        void apply(const Comp& rhs_composition, const Comp2& rhs_components2, Func&& func) {
            std::apply(
                [&](auto&&... args_comp_vec) -> auto {
                    std::apply(
                        [&](const auto&... args_rhs_comp_vec) -> auto {
                            std::apply(
                                [&](const auto&... args_rhs_comp2_vec) -> auto {
                                    (func(args_comp_vec, args_rhs_comp_vec, args_rhs_comp2_vec),
                                     ...);
                                },
                                rhs_components2);
                        },
                        rhs_composition.get_components());
                },
                components_);
        }

        /// @brief Applies a ternary callable to component-vector triples from this, one
        /// composition, and one raw component tuple (const overload).
        ///
        /// @tparam Func  Callable type with signature `void(const auto&, const auto&, const
        /// auto&)`.
        /// @tparam Comp  A type satisfying `IsComposition`.
        /// @tparam Comp2 Raw tuple type compatible with the component-vector tuple.
        /// @param rhs_composition   Right-hand composition.
        /// @param rhs_composition2  Right-hand raw component tuple.
        /// @param func              The callable to invoke.
        template <typename Func, typename Comp, typename Comp2>
            requires(IsComposition<Comp>)
        void apply(const Comp& rhs_composition, const Comp2& rhs_composition2, Func&& func) const {
            std::apply(
                [&](const auto&... args_comp_vec) -> auto {
                    std::apply(
                        [&](const auto&... args_rhs_comp_vec) -> auto {
                            std::apply(
                                [&](const auto&... args_rhs_comp2_vec) -> auto {
                                    (func(args_comp_vec, args_rhs_comp_vec, args_rhs_comp2_vec),
                                     ...);
                                },
                                rhs_composition2.get_components());
                        },
                        rhs_composition.get_components());
                },
                components_);
        }

        /// @brief Applies a unary predicate to each component vector and returns the logical AND.
        ///
        /// Short-circuits on the first `false` result.
        ///
        /// @tparam Func Callable type with signature `bool(const auto&)`.
        /// @param func  The predicate to invoke.
        /// @return `true` if and only if @p func returns `true` for every component-type slot.
        template <typename Func>
        bool apply_and(Func&& func) const {
            return std::apply(
                [&](const auto&... args_comp_vec) -> auto { return (func(args_comp_vec) && ...); },
                components_);
        }

        /// @brief Applies a binary predicate to paired component vectors and returns the logical
        /// AND.
        ///
        /// Delegates to the raw-tuple overload via `rhs_composition.get_components()`.
        ///
        /// @tparam Func Callable type with signature `bool(const auto&, const auto&)`.
        /// @tparam Comp A type satisfying `IsComposition`.
        /// @param rhs_composition The right-hand composition.
        /// @param func            The predicate to invoke.
        /// @return `true` if and only if @p func returns `true` for every paired slot.
        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        bool apply_and(const Comp& rhs_composition, Func&& func) const {
            return apply_and(rhs_composition.get_components(), func);
        }

        /// @brief Applies a binary predicate to paired component-vector slots and returns the
        /// logical AND (raw-tuple overload).
        ///
        /// @tparam Func Callable type with signature `bool(const auto&, const auto&)`.
        /// @tparam Comp Raw tuple type compatible with the component-vector tuple.
        /// @param rhs_components The right-hand component tuple.
        /// @param func           The predicate to invoke.
        /// @return `true` if and only if @p func returns `true` for every paired slot.
        template <typename Func, typename Comp>
        bool apply_and(const Comp& rhs_components, Func&& func) const {
            return std::apply(
                [&](const auto&... args_comp_vec) -> auto {
                    return std::apply(
                        [&](const auto&... args_rhs_comp_vec) -> auto {
                            return (func(args_comp_vec, args_rhs_comp_vec) && ...);
                        },
                        rhs_components);
                },
                components_);
        }

        /// @brief Applies a unary callable to every individual component (mutable).
        ///
        /// Iterates over all per-type vectors and invokes @p func on each dereferenced component.
        ///
        /// @tparam Func Callable type with signature `void(ComponentClass<BT>&)`.
        /// @param func  The callable to invoke.
        template <typename Func>
        void for_each_component(Func&& func) {
            apply([&func](auto&& args_comp_vec) -> auto {
                std::for_each(args_comp_vec.begin(), args_comp_vec.end(), [&](auto&& comp_ptr) {
                    func(*comp_ptr);
                });
            });
        }

        /// @brief Applies a unary callable to every individual component (const).
        ///
        /// @tparam Func Callable type with signature `void(const ComponentClass<BT>&)`.
        /// @param func  The callable to invoke.
        template <typename Func>
        void for_each_component(Func&& func) const {
            apply([&func](const auto& args_comp_vec) -> auto {
                std::for_each(args_comp_vec.begin(),
                              args_comp_vec.end(),
                              [&](const auto& comp_ptr) { func(*comp_ptr); });
            });
        }

        /// @brief Applies a binary callable to each paired component from this and another
        /// composition (mutable).
        ///
        /// Both compositions must have the same number of components in each type slot.
        ///
        /// @tparam Func Callable type with signature `void(ComponentClass<BT>&, const
        ///              ComponentClass<BT>&)`.
        /// @tparam Comp Any composition or tuple whose per-type vectors pair with this one.
        /// @param rhs_composition The right-hand composition to pair with.
        /// @param func            The callable to invoke.
        template <typename Func, typename Comp>
        void for_each_component(const Comp& rhs_composition, Func&& func) {
            apply(
                rhs_composition,
                [&func](auto&& args_comp_vec, const auto& args_rhs_comp_vec) -> auto {
                    auto it = args_rhs_comp_vec.begin();
                    std::for_each(args_comp_vec.begin(), args_comp_vec.end(), [&](auto&& comp_ptr) {
                        func(*comp_ptr, *it);
                        ++it;
                    });
                });
        }

        /// @brief Applies a binary callable to each paired component from this and another
        /// composition (const).
        ///
        /// @tparam Func Callable type with signature `void(const ComponentClass<BT>&, const
        ///              ComponentClass<BT>&)`.
        /// @tparam Comp Any composition or tuple whose per-type vectors pair with this one.
        /// @param rhs_composition The right-hand composition to pair with.
        /// @param func            The callable to invoke.
        template <typename Func, typename Comp>
        void for_each_component(const Comp& rhs_composition, Func&& func) const {
            apply(rhs_composition,
                  [&func](const auto& args_comp_vec, const auto& args_rhs_comp_vec) -> auto {
                      auto it = args_rhs_comp_vec.begin();
                      std::for_each(args_comp_vec.begin(),
                                    args_comp_vec.end(),
                                    [&](const auto& comp_ptr) {
                                        func(*comp_ptr, *it);
                                        ++it;
                                    });
                  });
        }

        /// @brief Applies a ternary callable to each component triple from this and two other
        /// compositions (mutable).
        ///
        /// All three compositions must have the same number of components per type slot.
        ///
        /// @tparam Func  Callable type with signature
        ///               `void(ComponentClass<BT>&, const ComponentClass<BT>&,
        ///                     const ComponentClass<BT>&)`.
        /// @tparam Comp1 First right-hand composition type.
        /// @tparam Comp2 Second right-hand composition type.
        /// @param rhs1  First right-hand composition.
        /// @param rhs2  Second right-hand composition.
        /// @param func  The callable to invoke.
        template <typename Func, typename Comp1, typename Comp2>
        void for_each_component(const Comp1& rhs1, const Comp2& rhs2, Func&& func) {
            apply(rhs1, rhs2, [&func](auto& vec, const auto& rhs1_vec, const auto& rhs2_vec) {
                auto it1 = rhs1_vec.begin();
                auto it2 = rhs2_vec.begin();
                for (auto& ptr : vec) {
                    func(*ptr, **it1, **it2);
                    ++it1;
                    ++it2;
                }
            });
        }

        /// @brief Applies a ternary callable to each component triple from this and two other
        /// compositions (const).
        ///
        /// @tparam Func  Callable type with signature
        ///               `void(const ComponentClass<BT>&, const ComponentClass<BT>&,
        ///                     const ComponentClass<BT>&)`.
        /// @tparam Comp1 First right-hand composition type.
        /// @tparam Comp2 Second right-hand composition type.
        /// @param rhs1  First right-hand composition.
        /// @param rhs2  Second right-hand composition.
        /// @param func  The callable to invoke.
        template <typename Func, typename Comp1, typename Comp2>
        void for_each_component(const Comp1& rhs1, const Comp2& rhs2, Func&& func) const {
            apply(rhs1, rhs2, [&func](const auto& vec, const auto& rhs1_vec, const auto& rhs2_vec) {
                auto it1 = rhs1_vec.begin();
                auto it2 = rhs2_vec.begin();
                for (const auto& ptr : vec) {
                    func(*ptr, **it1, **it2);
                    ++it1;
                    ++it2;
                }
            });
        }

        /// @brief Applies a unary predicate to every component and returns `true` if all pass.
        ///
        /// @tparam Func Callable type with signature `bool(const ComponentClass<BT>&)`.
        /// @param func  The predicate to invoke.
        /// @return `true` if and only if @p func returns `true` for every component.
        template <typename Func>
        bool for_each_component_and(Func&& func) const {
            return apply_and([&func](const auto& comp_vec) {
                return std::all_of(comp_vec.begin(), comp_vec.end(), [&](const auto& comp_ptr) {
                    return func(*comp_ptr);
                });
            });
        }

        /// @brief Applies a binary predicate to each paired component and returns `true` if all
        /// pass.
        ///
        /// Both compositions must have the same number of components per type slot.
        ///
        /// @tparam Func Callable type with signature
        ///              `bool(const ComponentClass<BT>&, const ComponentClass<BT>&)`.
        /// @tparam Comp Any composition or tuple whose per-type vectors pair with this one.
        /// @param rhs   The right-hand composition to pair with.
        /// @param func  The predicate to invoke.
        /// @return `true` if and only if @p func returns `true` for every paired component.
        template <typename Func, typename Comp>
        bool for_each_component_and(const Comp& rhs, Func&& func) const {
            return apply_and(rhs, [&func](const auto& comp_vec, const auto& rhs_comp_vec) {
                auto it = rhs_comp_vec.begin();
                return std::all_of(comp_vec.begin(), comp_vec.end(), [&](const auto& comp_ptr) {
                    return func(*comp_ptr, **it++);
                });
            });
        }

        /// @brief Returns a comma-separated string representation of all component values.
        ///
        /// @return A string of the form `"v1, v2, ..."` derived from each component's
        ///         `get_value().to_string()`.
        [[nodiscard]] std::string to_string() const {
            std::string result;
            for_each_component([&](auto&& c) { result += c.get_value().to_string() + ", "; });

            if (result.size() > 2) {
                result.resize(result.size() - 2);
            }

            return result;
        }

        /// @brief Returns a mutable reference to the full component tuple.
        ///
        /// @return Reference to the internal tuple of per-type component vectors.
        [[nodiscard]] auto get_components() -> auto& { return components_; }

        /// @brief Returns a const reference to the full component tuple.
        ///
        /// @return Const reference to the internal tuple of per-type component vectors.
        [[nodiscard]] auto get_components() const -> auto& { return components_; }

        /// @brief Returns a mutable reference to the component vector at the given index.
        ///
        /// @tparam ComponentTypeIndex Zero-based index of the desired type slot.
        /// @return Reference to `std::vector<std::unique_ptr<ComponentClass<BT>>>` at that slot.
        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_components() -> auto& {
            return std::get<ComponentTypeIndex>(components_);
        }

        /// @brief Returns a const reference to the component vector at the given index.
        ///
        /// @tparam ComponentTypeIndex Zero-based index of the desired type slot.
        /// @return Const reference to `std::vector<std::unique_ptr<ComponentClass<BT>>>` at that
        ///         slot.
        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_components() const -> const auto& {
            return std::get<ComponentTypeIndex>(components_);
        }

        /// @brief Returns a const reference to a single component by type-slot index and position.
        ///
        /// @tparam ComponentTypeIndex Zero-based index of the desired type slot.
        /// @param index               Position within that type-slot's vector.
        /// @return Const reference to the component at the given position.
        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_component(size_t index) const -> const auto& {
            return *(std::get<ComponentTypeIndex>(components_).at(index));
        }

        /// @brief Returns a mutable reference to a single component by type-slot index and
        /// position.
        ///
        /// @tparam ComponentTypeIndex Zero-based index of the desired type slot.
        /// @param index               Position within that type-slot's vector.
        /// @return Mutable reference to the component at the given position.
        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_component(size_t index) -> auto& {
            return *(std::get<ComponentTypeIndex>(components_).at(index));
        }

        /// @brief Returns a mutable reference to the component vector for a given base type.
        ///
        /// The type-slot index is resolved at compile time via `ComponentTypeIndex_v`.
        ///
        /// @tparam BaseType The base type whose slot should be retrieved.
        /// @return Reference to `std::vector<std::unique_ptr<ComponentClass<BaseType>>>`.
        template <typename BaseType>
        [[nodiscard]] auto get_components() -> auto& {
            constexpr size_t ComponentTypeIndex = ComponentTypeIndex_v<BaseType, BaseTypes...>;
            return get_components<ComponentTypeIndex>();
        }

        /// @brief Returns a const reference to the component vector for a given base type.
        ///
        /// @tparam BaseType The base type whose slot should be retrieved.
        /// @return Const reference to `std::vector<std::unique_ptr<ComponentClass<BaseType>>>`.
        template <typename BaseType>
        [[nodiscard]] auto get_components() const -> const auto& {
            constexpr size_t ComponentTypeIndex = ComponentTypeIndex_v<BaseType, BaseTypes...>;
            return get_components<ComponentTypeIndex>();
        }

        /// @brief Returns a mutable reference to a single component by base type and position.
        ///
        /// @tparam BaseType The base type that identifies the type slot.
        /// @param index     Position within that type-slot's vector.
        /// @return Mutable reference to the component at the given position.
        template <typename BaseType>
        [[nodiscard]] auto get_component(size_t index) -> auto& {
            constexpr size_t ComponentTypeIndex = ComponentTypeIndex_v<BaseType, BaseTypes...>;
            return get_component<ComponentTypeIndex>(index);
        }

        /// @brief Returns a const reference to a single component by base type and position.
        ///
        /// @tparam BaseType The base type that identifies the type slot.
        /// @param index     Position within that type-slot's vector.
        /// @return Const reference to the component at the given position.
        template <typename BaseType>
        [[nodiscard]] auto get_component(size_t index) const -> const auto& {
            constexpr size_t ComponentTypeIndex = ComponentTypeIndex_v<BaseType, BaseTypes...>;
            return get_component<ComponentTypeIndex>(index);
        }

    private:
        // Tuple of resource vectors in which each component of the tuple is associated with
        // a different type from the template arguments (i.e., Components...)
        std::tuple<std::vector<std::unique_ptr<ComponentClass<BaseTypes>>>...> components_;
};

}  // namespace rcspp
