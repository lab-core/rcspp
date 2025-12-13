// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace rcspp {

// compute index of first type in ResourceTypes... that is ResourceType
// Base template
template <typename ComponentType, typename... ComponentTypes>
struct ComponentTypeIndex;

// default case: not found
template <typename ComponentType>
struct ComponentTypeIndex<ComponentType> {
        static constexpr int value = -1;
};

// recursive case: either found at current index (ResourceType == ResourceType1)
// or continue searching in ResourceTypes...
template <typename ComponentType, typename FrontComponentType, typename... ComponentTypes>
struct ComponentTypeIndex<ComponentType, FrontComponentType, ComponentTypes...> {
    private:
        static constexpr int next = ComponentTypeIndex<ComponentType, ComponentTypes...>::value;
        static constexpr int next_or_minus_one = (next == -1 ? -1 : 1 + next);

    public:
        static constexpr int value =
            std::is_same_v<ComponentType, FrontComponentType> ? 0 : next_or_minus_one;
};

// Retrieve the value of the associated template
template <typename ComponentType, typename... ComponentTypes>
    requires(ComponentTypeIndex<ComponentType, ComponentTypes...>::value != -1)
inline constexpr int ComponentTypeIndex_v =
    ComponentTypeIndex<ComponentType, ComponentTypes...>::value;

// ComponentType to ComponentInitializerTypeTuple
// Extracts the initializer type tuple for a given ComponentType
// Default implementation deduces the value type from ComponentType::get_value(), if any
// Specializations can be provided for more complex ComponentType that have initializer with several
// values
// default trait (can be specialized)
template <typename ComponentType>
    requires requires { std::declval<ComponentType>().get_value(); }
struct ComponentInitializerTypeTuple {
        using type = std::tuple<std::decay_t<decltype(std::declval<ComponentType>().get_value())>>;
};

// convenience alias
template <typename ComponentType>
using ComponentInitializerTypeTuple_t = typename ComponentInitializerTypeTuple<ComponentType>::type;

// tag base to identify all Composition specializations
struct CompositionTag {};

// concept to test for any Composition<...>
template <typename T>
concept IsComposition = std::derived_from<std::remove_cvref_t<T>, CompositionTag>;

// Composition class that can hold multiple types of components
template <template <typename> class ComponentClass, typename... BaseTypes>
class Composition : public CompositionTag {
    public:
        Composition() = default;

        explicit Composition(
            std::tuple<std::vector<std::unique_ptr<ComponentClass<BaseTypes>>>...> components)
            : components_(std::move(components)) {}

        // Copy constructor
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

        Composition(Composition&& rhs_composition) { swap(*this, rhs_composition); }

        virtual ~Composition() = default;

        auto operator=(Composition rhs_composition) -> auto& {
            swap(*this, rhs_composition);
            return *this;
        }

        // To implement the copy-and-swap idiom
        friend void swap(Composition& first, Composition& second) {
            using std::swap;
            swap(first.components_, second.components_);
        }

        // Move an existing component to the composition
        template <size_t ComponentTypeIndex, typename ComponentType>
        auto add_component(std::unique_ptr<ComponentType> component) -> auto& {
            return *std::get<ComponentTypeIndex>(components_).emplace_back(std::move(component));
        }

        // apply func to each component type
        template <typename Func>
        void apply(Func&& func) {
            std::apply([&](auto&&... args_comp_vec) -> auto { (func(args_comp_vec), ...); },
                       components_);
        }

        template <typename Func>
        void apply(Func&& func) const {
            std::apply([&](const auto&... args_comp_vec) -> auto { (func(args_comp_vec), ...); },
                       components_);
        }

        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(Comp&& rhs_composition, Func&& func) {
            apply(rhs_composition.get_components(), func);
        }

        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(Comp&& rhs_composition, Func&& func) const {
            apply(rhs_composition.get_components(), func);
        }

        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(const Comp& rhs_composition, Func&& func) {
            apply(rhs_composition.get_components(), func);
        }

        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        void apply(const Comp& rhs_composition, Func&& func) const {
            apply(rhs_composition.get_components(), func);
        }

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

        template <typename Func, typename Comp, typename Comp2>
            requires(IsComposition<Comp> && IsComposition<Comp2>)
        void apply(const Comp& rhs_composition, const Comp2& rhs_composition2, Func&& func) {
            apply(rhs_composition, rhs_composition2.get_components(), func);
        }

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

        // apply func to each component type and return bool AND of results
        template <typename Func>
        bool apply_and(Func&& func) const {
            return std::apply(
                [&](const auto&... args_comp_vec) -> auto { return (func(args_comp_vec) && ...); },
                components_);
        }

        template <typename Func, typename Comp>
            requires(IsComposition<Comp>)
        bool apply_and(const Comp& rhs_composition, Func&& func) const {
            return apply_and(rhs_composition.get_components(), func);
        }

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

        // apply func to each component
        template <typename Func>
        void for_each_component(Func&& func) {
            apply([&func](auto&& args_comp_vec) -> auto {
                std::for_each(args_comp_vec.begin(), args_comp_vec.end(), [&](auto&& comp_ptr) {
                    func(*comp_ptr);
                });
            });
        }

        template <typename Func>
        void for_each_component(Func&& func) const {
            apply([&func](const auto& args_comp_vec) -> auto {
                std::for_each(args_comp_vec.begin(),
                              args_comp_vec.end(),
                              [&](const auto& comp_ptr) { func(*comp_ptr); });
            });
        }

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

        // Getters for components

        [[nodiscard]] auto get_components() -> auto& { return components_; }

        [[nodiscard]] auto get_components() const -> auto& { return components_; }

        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_components() -> auto& {
            return std::get<ComponentTypeIndex>(components_);
        }

        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_components() const -> const auto& {
            return std::get<ComponentTypeIndex>(components_);
        }

        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_component(size_t index) const -> const auto& {
            return *(std::get<ComponentTypeIndex>(components_).at(index));
        }

        template <size_t ComponentTypeIndex>
        [[nodiscard]] auto get_component(size_t index) -> auto& {
            return *(std::get<ComponentTypeIndex>(components_).at(index));
        }

        template <typename BaseType>
        [[nodiscard]] auto get_components() -> auto& {
            constexpr size_t ComponentTypeIndex = ComponentTypeIndex_v<BaseType, BaseTypes...>;
            return get_components<ComponentTypeIndex>();
        }

        template <typename BaseType>
        [[nodiscard]] auto get_components() const -> const auto& {
            constexpr size_t ComponentTypeIndex = ComponentTypeIndex_v<BaseType, BaseTypes...>;
            return get_components<ComponentTypeIndex>();
        }

        template <typename BaseType>
        [[nodiscard]] auto get_component(size_t index) -> auto& {
            constexpr size_t ComponentTypeIndex = ComponentTypeIndex_v<BaseType, BaseTypes...>;
            return get_component<ComponentTypeIndex>(index);
        }

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
