// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "rcspp/algorithm/algorithm.hpp"
#include "rcspp/algorithm/algorithm_params.hpp"
#include "rcspp/algorithm/container_direction.hpp"
#include "rcspp/algorithm/direction.hpp"
#include "rcspp/algorithm/labelling/bidirectional_labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/driven.hpp"
#include "rcspp/algorithm/labelling/labelling_algorithm.hpp"
#include "rcspp/algorithm/labelling/parts/admission/admission.hpp"
#include "rcspp/algorithm/labelling/parts/admission/dominance_admission.hpp"
#include "rcspp/algorithm/labelling/parts/interleave/interleave.hpp"
#include "rcspp/algorithm/labelling/parts/interleave/least_work_first.hpp"
#include "rcspp/algorithm/labelling/parts/observer/no_observer.hpp"
#include "rcspp/algorithm/labelling/parts/observer/observer.hpp"
#include "rcspp/algorithm/labelling/parts/retention/keep_at_nodes.hpp"
#include "rcspp/algorithm/labelling/parts/retention/retention.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/node_queues.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/pull_schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/schedule.hpp"
#include "rcspp/algorithm/labelling/parts/schedule/sweep_schedule.hpp"
#include "rcspp/algorithm/labelling/search.hpp"
#include "rcspp/algorithm/search_direction.hpp"
#include "rcspp/algorithm/solve_result.hpp"
#include "rcspp/label/label_list.hpp"
#include "rcspp/resource/concrete/numerical_resource.hpp"
#include "rcspp/utils/utils.hpp"

namespace rcspp::detail {

/// @brief Accepts any two-parameter template: a probe for a descriptor's member templates.
template <template <typename, typename> class>
struct TemplateProbe {};

/// @brief Whether @p Strategy names a class of its own,
///        `template <typename ResourceType, typename LabelContainerType> using algorithm = ...;`,
///        rather than a schedule, as the heuristics do. The class derives from
///        `Algorithm<ResourceType, LabelContainerType>` and is built from
///        `(ResourceFactory<ResourceType>*, AlgorithmParams<LabelContainerType>)`; it searches
///        forward only.
template <typename Strategy>
concept ClassStrategy = requires {
    typename TemplateProbe<Strategy::template algorithm>;
};  // NOLINT(whitespace/newline)

/// @brief Whether @p Strategy names none of its templated parts as a plain type: a plain
///        `using admission = ...;` (or retention, observer) would not be read, and the default
///        part would be used instead.
template <typename Strategy>
concept NoPlainTypeParts = !requires { typename Strategy::admission; } && !requires {
    typename Strategy::retention;
} && !requires { typename Strategy::observer; };  // NOLINT(whitespace/newline)

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

/// @brief Whether @p Strategy names a driver (`using driver = ...`), which runs its search several
///        times in one solve.
template <typename Strategy>
concept DrivenStrategy = requires { typename Strategy::driver; };  // NOLINT(whitespace/newline)

/// @brief The single-direction search @p Strategy builds in @p Direction: a
///        @ref LabellingAlgorithm, run by its driver when it names one (@ref Driven).
template <typename Strategy, typename ResourceType, typename LabelContainerType, typename Direction,
          typename PartsType>
consteval auto single_search_of() {
    using Search = LabellingAlgorithm<ResourceType, LabelContainerType, Direction, PartsType>;
    if constexpr (DrivenStrategy<Strategy>) {
        return std::type_identity<Driven<Search, typename Strategy::driver>>{};
    } else {
        return std::type_identity<Search>{};
    }
}

/// @brief The single-direction search @p Strategy builds in @p Direction (@ref single_search_of).
template <typename Strategy, typename ResourceType, typename LabelContainerType, typename Direction,
          typename PartsType>
using single_search_t =
    typename decltype(single_search_of<Strategy, ResourceType, LabelContainerType, Direction,
                                       PartsType>())::type;

/// @brief The parts of @p Strategy's search in @p Direction (see @ref search_parts_t).
///
/// A strategy that names no retention gets @c parts::KeepAtNodes itself, not @c retention_of's
/// alias template: an alias template is a different template argument, so
/// @c SearchParts<Schedule, Admission, alias, Observer> would be a different class from the exact
/// search's @c SearchParts<Schedule>, instantiated again and not equal to it.
template <typename Strategy, typename ResourceType, typename Direction>
consteval auto search_parts_of() {
    using Schedule = schedule_of_t<Strategy, ResourceType, Direction>;
    using Admission = admission_of_t<Strategy, ResourceType, Direction>;
    using Observer = observer_of_t<Strategy, ResourceType, Direction>;
    if constexpr (requires { typename TemplateProbe<Strategy::template retention>; }) {
        return std::type_identity<
            SearchParts<Schedule,
                        Admission,
                        retention_of<Strategy, ResourceType, Direction>::template type,
                        Observer>>{};
    } else {
        return std::type_identity<SearchParts<Schedule, Admission, parts::KeepAtNodes, Observer>>{};
    }
}

/// @brief The parts of @p Strategy's search in @p Direction: each part the descriptor names, given
///        the resource type and @p Direction, and the exact search's part for each it does not
///        name.
template <typename Strategy, typename ResourceType, typename Direction>
using search_parts_t =
    typename decltype(search_parts_of<Strategy, ResourceType, Direction>())::type;

/// @brief Whether @p Strategy searches in @p direction: forward always; backward and
///        bidirectionally unless it says otherwise (`backward = false`, `bidirectional = false`)
///        or it is a class of its own; never bidirectionally when it names a driver.
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
            if constexpr (ClassStrategy<Strategy> || DrivenStrategy<Strategy>) {
                return false;
            } else if constexpr (requires { Strategy::bidirectional; }) {
                return Strategy::bidirectional;
            } else {
                return true;
            }
    }
    return false;
}

/// @brief Checks one direction's parts (@c SearchParts) against their concepts.
template <typename PartsType, typename ResourceType, typename ContainerType>
constexpr void check_parts() {
    static_assert(parts::Schedule<typename PartsType::schedule, ResourceType>,
                  "the strategy's schedule does not satisfy rcspp::parts::Schedule: it needs "
                  "initialize(context), push(stored), a const size(), prepare_next_phase(context), "
                  "trim(context, limit), release_stored_aside(context, erase) and clear()");
    static_assert(
        parts::LabelSchedule<typename PartsType::schedule, ResourceType> ||
            parts::PullingSchedule<typename PartsType::schedule, ResourceType>,
        "the strategy's schedule is neither a label schedule (next(context)) nor a pulling "
        "schedule (visit_next_node(context, extend), current(), remove_current(position))");
    using Retention = typename PartsType::template retention<ContainerType>;
    static_assert(parts::Retention<Retention, ResourceType, ContainerType>,
                  "the strategy's retention does not satisfy rcspp::parts::Retention: it needs "
                  "reset(number_of_nodes, prototype), clear(), node(pos), store(pos, label), "
                  "erase(position), labels_at(pos), containers() and a static constexpr bool "
                  "records_at_take");
    static_assert(parts::Admission<typename PartsType::admission,
                                   ResourceType,
                                   decltype(std::declval<Retention&>().node(size_t{0}))>,
                  "the strategy's admission does not satisfy rcspp::parts::Admission: it needs "
                  "a const member function bool admit(candidate, node, context) and a static "
                  "function bool is_exact(context)");
    static_assert(parts::Observer<typename PartsType::observer, ResourceType>,
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
        static_assert(NoPlainTypeParts<Strategy>,
                      "a strategy's admission, retention and observer are templates on the "
                      "resource type and the direction: template <typename ResourceType, "
                      "typename Direction> using admission = ...; a plain `using admission = "
                      "...;` would be ignored");
        using Forward = search_parts_t<Strategy, ResourceType, ForwardDirection>;
        using Backward = search_parts_t<Strategy, ResourceType, BackwardDirection>;
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
                "a const member function bool should_step_forward(forward, backward)");
        }
    }
}

/// @brief The message of a refused container: it has no form that compares backward.
template <typename LabelContainerType>
[[nodiscard]] std::string no_backward_container_message(SearchDirection direction) {
    return template_name<LabelContainerType>() + " cannot compare backward: a " +
           std::string(to_string(direction)) +
           " search keeps its labels in the container its params hold, turned backward, so the "
           "container must declare its own with_direction<TargetDirection>()";
}

/// @brief Builds the algorithm that searches for @p Strategy in `params.direction`.
///
/// A labelling strategy becomes a @ref LabellingAlgorithm forward or backward, run by its driver
/// when it names one (@ref Driven), and a @ref BidirectionalLabellingAlgorithm bidirectionally,
/// with each direction's parts (@ref search_parts_t); a class strategy builds its own class,
/// forward only.
///
/// @tparam Strategy             The descriptor (@c rcspp::strategy, or one of one's own).
/// @tparam ResourceType         The resource type carried by labels.
/// @tparam LabelContainerType   The per-node container of the params.
/// @tparam CriticalResourceType The type of the critical resource (@c critical_resource_index),
///                              whose value decides where a bidirectional search's two halves
///                              stop (its half-way point).
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
        throw std::invalid_argument(template_name<Strategy>() + " cannot search " +
                                    std::string(to_string(direction)) +
                                    ": it declares backward = false or bidirectional = false, "
                                    "or is a class of its own (forward only)");
    }
    using R = ResourceType;
    using LC = LabelContainerType;
    if constexpr (ClassStrategy<Strategy>) {
        using Class = typename Strategy::template algorithm<R, LC>;
        static_assert(std::derived_from<Class, Algorithm<R, LC>> &&
                          std::constructible_from<Class, ResourceFactory<R>*, AlgorithmParams<LC>>,
                      "a strategy's algorithm<ResourceType, LabelContainerType> derives from "
                      "rcspp::Algorithm<ResourceType, LabelContainerType> and is constructed from "
                      "(ResourceFactory<ResourceType>*, AlgorithmParams<LabelContainerType>)");
        return std::make_unique<Class>(resource_factory, std::move(params));
    } else {
        check_strategy<Strategy, R>();
        using Forward = search_parts_t<Strategy, R, ForwardDirection>;
        switch (direction) {
            case SearchDirection::Forward:
                return std::make_unique<
                    single_search_t<Strategy, R, LC, ForwardDirection, Forward>>(resource_factory,
                                                                                 std::move(params));
            case SearchDirection::Backward:
                if constexpr (searches<Strategy>(SearchDirection::Backward) &&
                              has_directed_container_v<LC, BackwardDirection>) {
                    using Backward = search_parts_t<Strategy, R, BackwardDirection>;
                    return std::make_unique<
                        single_search_t<Strategy, R, LC, BackwardDirection, Backward>>(
                        resource_factory,
                        std::move(params));
                } else {
                    throw std::invalid_argument(no_backward_container_message<LC>(direction));
                }
            case SearchDirection::Bidirectional:
                if constexpr (searches<Strategy>(SearchDirection::Bidirectional) &&
                              has_directed_container_v<LC, BackwardDirection>) {
                    using Backward = search_parts_t<Strategy, R, BackwardDirection>;
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
                    throw std::invalid_argument(no_backward_container_message<LC>(direction));
                }
        }
    }
    throw std::invalid_argument("unknown search direction");
}

}  // namespace rcspp::detail
