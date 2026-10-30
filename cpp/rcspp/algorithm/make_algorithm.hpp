// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/label_buckets.hpp"
#include "rcspp/algorithm/labelling/bidirectional_labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/node_schedules.hpp"
#include "rcspp/algorithm/labelling/policies.hpp"
#include "rcspp/algorithm/labelling/schedules.hpp"
#include "rcspp/algorithm/labelling/search.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/utils/utils.hpp"

namespace rcspp::detail {

/// @brief Accepts any two-parameter template: a probe for a descriptor's member templates.
template <template <typename, typename> class>
struct TemplateProbe {};

/// @brief Whether @p Strategy names a class of its own (`algorithm<R, LC>`), as the heuristics
///        do, rather than a schedule.
template <typename Strategy>
concept ClassStrategy = requires {
    typename TemplateProbe<Strategy::template algorithm>;
};  // NOLINT(whitespace/newline)

/// @brief The schedule @p Strategy names, given the resource type and @p Direction.
template <typename Strategy, typename ResourceType, typename Direction>
using schedule_of_t = typename Strategy::template schedule<ResourceType, Direction>;

/// @brief The admission rule @p Strategy names in @p Direction, or dominance.
template <typename Strategy, typename ResourceType, typename Direction>
consteval auto admission_of() {
    if constexpr (requires { typename TemplateProbe<Strategy::template admission>; }) {
        return std::type_identity<typename Strategy::template admission<ResourceType, Direction>>{};
    } else {
        return std::type_identity<parts::DominanceAdmission>{};
    }
}

/// @brief The admission rule @p Strategy names in @p Direction, or dominance.
template <typename Strategy, typename ResourceType, typename Direction>
using admission_of_t = typename decltype(admission_of<Strategy, ResourceType, Direction>())::type;

/// @brief The retention @p Strategy names in @p Direction, as a template on the container type.
template <typename Strategy, typename ResourceType, typename Direction>
struct retention_of {
        template <typename ContainerType>
        using type = typename Strategy::template retention<ResourceType,
                                                           Direction>::template type<ContainerType>;
};

/// @brief The observer @p Strategy names in @p Direction, or none.
template <typename Strategy, typename ResourceType, typename Direction>
consteval auto observer_of() {
    if constexpr (requires { typename TemplateProbe<Strategy::template observer>; }) {
        return std::type_identity<typename Strategy::template observer<ResourceType, Direction>>{};
    } else {
        return std::type_identity<parts::NoObserver>{};
    }
}

/// @brief The observer @p Strategy names in @p Direction, or none.
template <typename Strategy, typename ResourceType, typename Direction>
using observer_of_t = typename decltype(observer_of<Strategy, ResourceType, Direction>())::type;

/// @brief The interleave @p Strategy names for a bidirectional search, or least work first.
template <typename Strategy>
consteval auto interleave_of() {
    if constexpr (requires { typename Strategy::interleave; }) {
        return std::type_identity<typename Strategy::interleave>{};
    } else {
        return std::type_identity<parts::LeastWorkFirst>{};
    }
}

/// @brief The interleave @p Strategy names for a bidirectional search, or least work first.
template <typename Strategy>
using interleave_of_t = typename decltype(interleave_of<Strategy>())::type;

/// @brief The parts of @p Strategy's search in @p Direction (see @ref preset_t).
///
/// A strategy that names no retention gets @c parts::KeepAtNodes itself, not an alias of it, so
/// that its preset is the exact search's.
template <typename Strategy, typename ResourceType, typename Direction>
consteval auto preset_of() {
    using Schedule = schedule_of_t<Strategy, ResourceType, Direction>;
    using Admission = admission_of_t<Strategy, ResourceType, Direction>;
    using Observer = observer_of_t<Strategy, ResourceType, Direction>;
    if constexpr (requires { typename TemplateProbe<Strategy::template retention>; }) {
        return std::type_identity<
            Preset<Schedule,
                   Admission,
                   retention_of<Strategy, ResourceType, Direction>::template type,
                   Observer>>{};
    } else {
        return std::type_identity<Preset<Schedule, Admission, parts::KeepAtNodes, Observer>>{};
    }
}

/// @brief The parts of @p Strategy's search in @p Direction: each part the descriptor names, given
///        the resource type and @p Direction, and the exact search's part for each it does not
///        name.
template <typename Strategy, typename ResourceType, typename Direction>
using preset_t = typename decltype(preset_of<Strategy, ResourceType, Direction>())::type;

/// @brief Whether @p Strategy searches in @p direction: forward always; backward and
///        bidirectionally unless it says otherwise (`backward = false`, `bidirectional = false`)
///        or it is a class of its own.
template <typename Strategy>
constexpr bool searches(SearchDirection direction) {
    switch (direction) {
        case SearchDirection::Forward:
            return true;
        case SearchDirection::Backward:
            if constexpr (ClassStrategy<Strategy>) {
                return false;
            } else if constexpr (requires { Strategy::backward; }) {
                return Strategy::backward;
            } else {
                return true;
            }
        case SearchDirection::Bidirectional:
            if constexpr (ClassStrategy<Strategy>) {
                return false;
            } else if constexpr (requires { Strategy::bidirectional; }) {
                return Strategy::bidirectional;
            } else {
                return true;
            }
    }
    return false;
}

/// @brief Checks the parts of one direction's preset against their concepts.
template <typename PresetType, typename ResourceType, typename ContainerType>
constexpr void check_parts() {
    static_assert(parts::Schedule<typename PresetType::schedule, ResourceType>,
                  "the strategy's schedule does not satisfy rcspp::parts::Schedule: it needs "
                  "initialize, push, size, prepare_next_phase, trim, release_stored_aside and "
                  "clear");
    static_assert(parts::LabelSchedule<typename PresetType::schedule, ResourceType> ||
                      parts::NodeSchedule<typename PresetType::schedule, ResourceType>,
                  "the strategy's schedule hands out neither a label at a time (next) nor a node "
                  "at a time (visit_next_node, current, erase_current)");
    using Retention = typename PresetType::template retention<ContainerType>;
    static_assert(parts::Retention<Retention, ResourceType>,
                  "the strategy's retention does not satisfy rcspp::parts::Retention: it needs "
                  "node, store, erase and collects_at_take");
    static_assert(parts::Admission<typename PresetType::admission,
                                   ResourceType,
                                   decltype(std::declval<Retention&>().node(size_t{0}))>,
                  "the strategy's admission does not satisfy rcspp::parts::Admission: it needs "
                  "admit(candidate, node, ctx) and exact(ctx)");
    static_assert(parts::Observer<typename PresetType::observer, ResourceType>,
                  "the strategy's observer does not satisfy rcspp::parts::Observer: it needs "
                  "on_extension(label, arc, outcome)");
}

/// @brief Checks each part of @p Strategy against its concept, in each direction it searches, and
///        names the first that fails.
///
/// A class strategy has no parts to check.
template <typename Strategy, typename ResourceType>
constexpr void check_strategy() {
    if constexpr (!ClassStrategy<Strategy>) {
        static_assert(
            requires { typename TemplateProbe<Strategy::template schedule>; },
            "a strategy names a schedule: template <typename ResourceType, typename "
            "Direction> using schedule = ...; or a class of its own, as `algorithm`");
        using Forward = preset_t<Strategy, ResourceType, ForwardDirection>;
        using Backward = preset_t<Strategy, ResourceType, BackwardDirection>;
        using ForwardList = LabelList<ResourceType>;
        using BackwardList = LabelList<ResourceType, BackwardDirection>;
        check_parts<Forward, ResourceType, ForwardList>();
        if constexpr (searches<Strategy>(SearchDirection::Backward) ||
                      searches<Strategy>(SearchDirection::Bidirectional)) {
            check_parts<Backward, ResourceType, BackwardList>();
        }
        if constexpr (searches<Strategy>(SearchDirection::Bidirectional)) {
            static_assert(
                parts::Interleave<interleave_of_t<Strategy>,
                                  Search<ResourceType, ForwardDirection, ForwardList, Forward>,
                                  Search<ResourceType, BackwardDirection, BackwardList, Backward>>,
                "the strategy's interleave does not satisfy rcspp::parts::Interleave: it needs "
                "forward_next(forward, backward)");
        }
    }
}

/// @brief The message of a refused container: it has no form that compares backward.
template <typename LabelContainerType>
[[nodiscard]] std::string no_backward_container(SearchDirection direction) {
    return template_name<LabelContainerType>() + " cannot compare backward: a " +
           std::string(to_string(direction)) +
           " search keeps its labels in the container its params hold, turned backward, so the "
           "container must declare its own with_direction<TargetDirection>()";
}

/// @brief Builds the algorithm that searches for @p Strategy in `params.direction`.
///
/// A labelling strategy becomes a @ref LabellingAlgorithm forward or backward, and a
/// @ref BidirectionalLabellingAlgorithm bidirectionally, with each direction's parts
/// (@ref preset_t); a class strategy builds its own class, forward only.
///
/// @tparam Strategy             The descriptor (@c rcspp::strategy, or one of one's own).
/// @tparam ResourceType         The resource type carried by labels.
/// @tparam LabelContainerType   The per-node container of the params.
/// @tparam CriticalResourceType The type of the bidirectional search's clock.
/// @param resource_factory The graph's resource factory.
/// @param params           The algorithm's params; their direction picks the class.
/// @return The algorithm, behind its base class.
/// @throws std::invalid_argument when @p Strategy cannot search in that direction, or a
///         backward or bidirectional search is asked for with a container that has no backward
///         form.
template <typename Strategy, typename ResourceType, typename LabelContainerType,
          typename CriticalResourceType = RealResource>
std::unique_ptr<Algorithm<ResourceType, LabelContainerType>> make_algorithm(
    ResourceFactory<ResourceType>* resource_factory, AlgorithmParams<LabelContainerType> params) {
    const SearchDirection direction = params.direction;
    if (!searches<Strategy>(direction)) {
        throw std::invalid_argument(template_name<Strategy>() + " does not support the " +
                                    std::string(to_string(direction)) + " direction");
    }
    using R = ResourceType;
    using LC = LabelContainerType;
    if constexpr (ClassStrategy<Strategy>) {
        using Class = typename Strategy::template algorithm<R, LC>;
        return std::make_unique<Class>(resource_factory, std::move(params));
    } else {
        check_strategy<Strategy, R>();
        using Forward = preset_t<Strategy, R, ForwardDirection>;
        switch (direction) {
            case SearchDirection::Forward:
                return std::make_unique<LabellingAlgorithm<R, LC, ForwardDirection, Forward>>(
                    resource_factory,
                    std::move(params));
            case SearchDirection::Backward:
                if constexpr (searches<Strategy>(SearchDirection::Backward) &&
                              has_directed_container_v<LC, BackwardDirection>) {
                    using Backward = preset_t<Strategy, R, BackwardDirection>;
                    return std::make_unique<LabellingAlgorithm<R, LC, BackwardDirection, Backward>>(
                        resource_factory,
                        std::move(params));
                } else {
                    throw std::invalid_argument(no_backward_container<LC>(direction));
                }
            case SearchDirection::Bidirectional:
                if constexpr (searches<Strategy>(SearchDirection::Bidirectional) &&
                              has_directed_container_v<LC, BackwardDirection>) {
                    using Backward = preset_t<Strategy, R, BackwardDirection>;
                    return std::make_unique<
                        BidirectionalLabellingAlgorithm<R,
                                                        LC,
                                                        CriticalResourceType,
                                                        Forward,
                                                        Backward,
                                                        interleave_of_t<Strategy>>>(
                        resource_factory,
                        std::move(params));
                } else {
                    throw std::invalid_argument(no_backward_container<LC>(direction));
                }
        }
    }
    throw std::invalid_argument("unknown search direction");
}

}  // namespace rcspp::detail
