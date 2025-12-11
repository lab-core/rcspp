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

// Composition class that can hold multiple types of components
template <typename... ComponentTypes>
class Composition {
    public:
        Composition() = default;

        explicit Composition(std::tuple<std::vector<std::unique_ptr<ComponentTypes>>...> components)
            : components_(std::move(components)) {}

        // Copy constructor
        Composition(const Composition& rhs_composition) {
            // Clone the resources contained in a single vector of resources.
            const auto clone_comp_vec_function = [&](auto& sing_comp_vec,
                                                     const auto& rhs_sing_comp_vec) -> auto {
                std::transform(rhs_sing_comp_vec.begin(),
                               rhs_sing_comp_vec.end(),
                               std::back_inserter(sing_comp_vec),
                               [](const auto& rhs_comp) { return rhs_comp->clone(); });
            };

            // Apply clone_comp_vec_function to each component of the tuple
            apply(rhs_composition, clone_comp_vec_function);
        }

        Composition(Composition&& rhs_composition) { swap(*this, rhs_composition); }

        ~Composition() = default;

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

        // apply func to each component
        template <typename Func>
        void for_each_component(Func&& func) {
            apply([&func](auto&& args_comp_vec) -> auto {
                std::for_each(args_comp_vec.begin(), args_comp_vec.end(), [&](auto& comp_ptr) {
                    func(*comp_ptr);
                });
            });
        }

        template <typename Func, typename Comp>
        void for_each_component(const Comp& rhs_composition, Func&& func) {
            apply(rhs_composition, [&func](auto&& args_comp_vec, auto&& args_rhs_comp_vec) -> auto {
                auto it = args_rhs_comp_vec.begin();
                std::for_each(args_comp_vec.begin(), args_comp_vec.end(), [&](auto& comp_ptr) {
                    func(*comp_ptr, **it);
                    ++it;
                });
            });
        }

        // apply func to each component type
        template <typename Func>
        void apply(Func&& func) {
            std::apply([&](auto&&... args_comp_vec) -> auto { (func(args_comp_vec), ...); },
                       components_);
        }

        template <typename Func, typename Comp>
            requires(std::derived_from<Comp, Composition>)
        void apply(const Comp& rhs_composition, Func&& func) {
            apply(rhs_composition.components_, func);
        }

        template <typename Func, typename Comp>
        void apply(const Comp& rhs_components, Func&& func) {
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

        template <typename Func>
        bool apply_and(Func&& func) {
            return std::apply(
                [&](auto&&... args_comp_vec) -> auto { return (func(args_comp_vec) && ...); },
                components_);
        }

        template <typename Func, typename Comp>
        bool apply_and(const Comp& rhs_composition, Func&& func) {
            return std::apply(
                [&](auto&&... args_comp_vec) -> auto {
                    return std::apply(
                        [&](auto&&... args_rhs_comp_vec) -> auto {
                            return (func(args_comp_vec, args_rhs_comp_vec) && ...);
                        },
                        rhs_composition.components_);
                },
                components_);
        }

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

        template <typename ComponentType>
        [[nodiscard]] auto get_components() -> auto& {
            constexpr size_t ComponentTypeIndex =
                ComponentTypeIndex_v<ComponentType, ComponentTypes...>;
            return get_components<ComponentTypeIndex>();
        }

        template <typename ComponentType>
        [[nodiscard]] auto get_components() const -> const auto& {
            constexpr size_t ComponentTypeIndex =
                ComponentTypeIndex_v<ComponentType, ComponentTypes...>;
            return get_components<ComponentTypeIndex>();
        }

        template <typename ComponentType>
        [[nodiscard]] auto get_components(size_t index) const -> const auto& {
            constexpr size_t ComponentTypeIndex =
                ComponentTypeIndex_v<ComponentType, ComponentTypes...>;
            return get_component<ComponentTypeIndex>(index);
        }

    protected:
        // Tuple of resource vectors in which each component of the tuple is associated with
        // a different type from the template arguments (i.e., Components...)
        std::tuple<std::vector<std::unique_ptr<ComponentTypes>>...> components_;
};
}  // namespace rcspp
