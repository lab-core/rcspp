// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "rcspp/graph/graph.hpp"
#include "rcspp/resource/composition/resource_composition.hpp"
#include "rcspp/resource/resource_traits.hpp"
#include "rcspp/utils/logger.hpp"

namespace rcspp {

/// @brief Why a bidirectional solve is running without the half-way bound.
enum class HalfWayOff : unsigned {
    NoHalfWayPoint,      ///< the caller gave no `H` -- the default
    CriticalTypeAbsent,  ///< the critical resource type is not in the model
    NotAThreshold,       ///< the clock's backward values are not on the forward scale
    NotMonotone,         ///< the clock can decrease along an arc
    NotIncreasing,       ///< the clock is not an increasing part of the dominance order
    NoCeiling,           ///< the clock states no ceiling at a sink to seed backward labels with
    IndexOutOfRange,     ///< `critical_resource_index` names no component of the critical type
};

/// @brief Whether this process has not yet reported the bound being off for @p reason.
///
/// Tracked per process, not per algorithm, so a pricing loop that builds a new algorithm for
/// every solve warns only once.
///
/// @param reason Why the bound is off.
/// @return @c true the first time @p reason is reported.
inline bool first_report_of(HalfWayOff reason) {
    static std::atomic<unsigned> reported{0};
    const unsigned bit = 1U << static_cast<unsigned>(reason);
    return (reported.fetch_or(bit) & bit) == 0U;
}

/// @brief Decides where each direction's search stops.
///
/// Uses a critical resource that acts as a clock: monotone (never decreasing along an arc) and
/// bounded in `[0, R]`. Forward labels are discarded above the half-way point `H`, backward labels
/// below it. Monotonicity ensures each path crosses `H` on exactly one arc, so it is found exactly
/// once at the join; a non-monotone clock can silently lose paths. Cost is not a valid clock.
///
/// **`H` is fixed for the whole of one solve**: there is no setter, only @ref disable. Moving `H`
/// mid-search would break the exactly-once crossing. A half-way point that adapts does so
/// *between* solves (see @ref HalfWayController), and each solve builds a fresh policy from the
/// controller's current value.
class HalfWayPolicy {
    public:
        /// @brief Constructs the policy, deriving `H` when it is not given explicitly.
        ///
        /// @param half_way_point       The value `H`. When 0, derived as `R / 2` if
        ///                             @p resource_upper_bound is finite (unlike
        ///                             @c AlgorithmParams::half_way_point, where 0 means "off").
        /// @param resource_upper_bound `R`, the critical resource's maximum. With no explicit `H`
        ///                             and no finite positive `R`, the bound starts disabled.
        ///                             A bidirectional solve passes an infinite `R` when its
        ///                             `half_way_point` is 0, so there `H` is never derived.
        HalfWayPolicy(double half_way_point, double resource_upper_bound) {
            if (half_way_point > 0.0) {
                h_ = half_way_point;
            } else if (std::isfinite(resource_upper_bound) && resource_upper_bound > 0.0) {
                h_ = resource_upper_bound / 2.0;
            } else {
                // No explicit H and no finite R.
                h_ = 0.0;
                enabled_ = false;
            }
        }

        /// @brief Whether a forward label has passed the half-way point and should be discarded.
        ///
        /// @param critical_value The label's value on the critical resource.
        /// @return `true` when the label is beyond `H` and the bound is in force.
        [[nodiscard]] bool should_stop_forward(double critical_value) const {
            return enabled_ && critical_value > h_;
        }

        /// @brief Whether a backward label is below the half-way point and should be discarded.
        ///
        /// Note the `<`: backward labels are discarded *below* `H`, the mirror of forward.
        ///
        /// @param critical_value The label's value on the critical resource.
        /// @return `true` when the label is short of `H` and the bound is in force.
        [[nodiscard]] bool should_stop_backward(double critical_value) const {
            return enabled_ && critical_value < h_;
        }

        /// @brief Turns the bound off: both searches run to completion, correct but slower.
        void disable() { enabled_ = false; }

        /// @brief Whether the bound is in force.
        [[nodiscard]] bool enabled() const { return enabled_; }

        /// @brief The half-way point `H`.
        [[nodiscard]] double h() const { return h_; }

    private:
        double h_ = 0.0;
        bool enabled_ = true;
};

/// @brief What one bidirectional solve reported, reduced to what @ref HalfWayController reads.
///
/// A plain struct rather than a `SolveResult` so the controller does not depend on the algorithm
/// layer, and so a test can state an observation directly. `half_way_observation()` in
/// `bidirectional_dominance_algorithm.hpp` builds one from a `SolveResult`.
struct HalfWayObservation {
        /// @brief Labels surviving in the forward containers when the solve ended.
        size_t forward_labels = 0;

        /// @brief Labels surviving in the backward containers when the solve ended.
        size_t backward_labels = 0;

        /// @brief Complete paths the join produced.
        size_t joined_paths = 0;

        /// @brief Solutions the solve returned, from any of its three sources.
        size_t solutions = 0;

        /// @brief Whether the half-way bound was in force. An unbounded solve says nothing
        ///        about where `H` should be.
        bool bounded = false;

        /// @brief Whether the search ran to completion untrimmed and untruncated.
        ///
        /// A truncated pass -- timed out, trimmed by memory pressure, capped by a per-node
        /// extension quota -- stops wherever the cap happened to bite, so its two label counts
        /// measure the cap rather than the split.
        bool exact = false;
};

/// @brief The tuning knobs of @ref HalfWayController. The defaults are RouteOpt's.
struct HalfWayControllerParams {
        /// @brief Relative imbalance tolerated before `H` moves: `|f - b| / min(f, b)`.
        ///
        /// A dead zone, not a continuous correction -- label counts are noisy from one pricing
        /// iteration to the next, and chasing that noise would move `H` on every solve.
        double dead_zone = 0.2;  // NOLINT(readability-magic-numbers)

        /// @brief The first multiplicative step: `H <- H * (1 -/+ step)`.
        double initial_step = 0.2;  // NOLINT(readability-magic-numbers)

        /// @brief The step is divided by this each time the direction of travel reverses.
        ///
        /// Two reversals in a row mean `H` has straddled the balance point; halving the step is
        /// what lets it settle there instead of oscillating around it.
        double step_decay = 2.0;  // NOLINT(readability-magic-numbers)

        /// @brief The smallest the step can decay to.
        ///
        /// Not in RouteOpt, whose step only ever halves. There the meet point is frozen once the
        /// root node's column generation ends, so a step that has decayed to nothing cannot hurt
        /// later; a caller here may keep the controller running for the whole loop, and without a
        /// floor one early run of reversals would pin `H` for good.
        double min_step = 0.025;  // NOLINT(readability-magic-numbers)

        /// @brief `H` is kept inside `[min_fraction, max_fraction] * R`, with `R = 2 * H0`.
        ///
        /// `R` is the clock's range as the algorithm understands it -- the same `[0, 2H]` reading
        /// `AlgorithmBaseParams::half_way_point` documents -- so the initial `H0` should be about
        /// half the real range for the clamp to mean anything. The clamp only guards against
        /// runaway: any `H` is correct, and one outside the range merely turns the solve into a
        /// one-directional search with extra work.
        double min_fraction = 0.05;  // NOLINT(readability-magic-numbers)

        /// @brief See @ref min_fraction.
        double max_fraction = 0.95;  // NOLINT(readability-magic-numbers)

        /// @brief Throws `std::invalid_argument` naming the first knob that is out of range.
        void check() const {
            if (!(dead_zone >= 0.0)) {
                throw std::invalid_argument("HalfWayControllerParams: dead_zone must be >= 0");
            }
            if (!(min_step > 0.0 && min_step <= initial_step && initial_step < 1.0)) {
                throw std::invalid_argument(
                    "HalfWayControllerParams: need 0 < min_step <= initial_step < 1");
            }
            if (!(step_decay >= 1.0)) {
                throw std::invalid_argument("HalfWayControllerParams: step_decay must be >= 1");
            }
            if (!(min_fraction > 0.0 && min_fraction < max_fraction && max_fraction < 1.0)) {
                throw std::invalid_argument(
                    "HalfWayControllerParams: need 0 < min_fraction < max_fraction < 1");
            }
        }
};

/// @brief What @ref HalfWayController::update did with an observation.
enum class HalfWayMove {
    /// The two searches were balanced within the dead zone; `H` stays.
    Unchanged,
    /// The observation was not trustworthy -- unbounded, inexact, or empty; nothing was learned.
    Skipped,
    /// The controller is frozen; nothing was learned.
    Frozen,
    /// The forward search did too much of the work; `H` moved down.
    Down,
    /// The backward search did too much of the work; `H` moved up.
    Up,
    /// Nothing crossed `H`, so the split bought nothing; `H` moved toward the clock's centre.
    TowardCentre,
};

/// @brief Human-readable name of a @ref HalfWayMove.
[[nodiscard]] inline std::string to_string(HalfWayMove move) {
    switch (move) {
        case HalfWayMove::Unchanged:
            return "unchanged";
        case HalfWayMove::Skipped:
            return "skipped";
        case HalfWayMove::Frozen:
            return "frozen";
        case HalfWayMove::Down:
            return "down";
        case HalfWayMove::Up:
            return "up";
        case HalfWayMove::TowardCentre:
            return "toward_centre";
    }
    return "unknown";
}

/// @brief Moves the half-way point `H` between successive solves, from how each one went.
///
/// A bidirectional solve with a static `H` can spend nearly all its effort in one direction:
/// measured on `R201_25` with `H = R/2`, the forward search kept 2 to 10 times the backward
/// search's labels at every iteration of a column generation. This is the feedback loop that
/// corrects it, after RouteOpt's `adjustResourceMeetPointInPricing` (You & Yang 2025; the idea is
/// Righini & Salani 2006's). **It never runs inside a solve** -- @ref HalfWayPolicy holds `H` fixed
/// for that -- so it cannot affect correctness, only how the work is split.
///
/// After each solve, @ref update reads the two surviving-label counts `f` and `b`:
///
///  1. **Guard.** Learn only from an exact solve with the bound in force. A truncated pass biases
///     the counts toward wherever the cap bit, and an unbounded one did not use `H` at all.
///  2. **Imbalance.** When `|f - b| / min(f, b)` exceeds @c dead_zone, move multiplicatively away
///     from the heavier side: `H <- H * (1 - step)` when forward is heavier, `H * (1 + step)`
///     when backward is.
///  3. **Zero-join guard**, only when the counts are balanced. No join, yet solutions were found:
///     every answer came from a search that ran all the way to a terminal, so nothing crossed `H`
///     and the split bought nothing. Balanced counts cannot see that, so it is its own trigger,
///     and it moves `H` toward the clock's centre -- never past it. Not in RouteOpt. It runs
///     *after* the imbalance rule deliberately: an imbalance already says which way to move, and
///     given precedence this guard pins `H` at its starting point, which is the centre.
///  4. **Damp on reversal.** Whenever the direction of travel flips, divide the step by
///     @c step_decay -- never below @c min_step, which RouteOpt does not have.
///
/// RouteOpt adapts during the root node's column generation and then **freezes** the meet point
/// for the whole branch-and-bound tree; @ref set_frozen is that switch.
class HalfWayController {
    public:
        /// @brief An unseeded controller: @ref seeded is false and @ref update does nothing.
        HalfWayController() = default;

        /// @brief Seeds the controller at @p initial_h.
        ///
        /// @param initial_h `H0`, which also fixes the range `R = 2 * H0` that `H` is clamped to.
        ///                  Must be positive: a zero `H` is the "bound off" sentinel, and there is
        ///                  nothing to adapt when the bound is off.
        /// @param params    The tuning knobs; validated here.
        /// @throws std::invalid_argument when @p initial_h is not positive and finite, or when
        ///         @p params is out of range.
        explicit HalfWayController(double initial_h, HalfWayControllerParams params = {})
            : params_(params), initial_h_(initial_h) {
            if (!(initial_h > 0.0) || !std::isfinite(initial_h)) {
                throw std::invalid_argument(
                    "HalfWayController: the initial half-way point must be positive and finite");
            }
            params_.check();
            reset();
        }

        /// @brief Folds one solve's observation into `H`, and says what it did.
        ///
        /// @param observation What the solve reported.
        /// @return The move made; @ref h is the value the next solve should use.
        HalfWayMove update(const HalfWayObservation& observation) {
            if (!seeded()) {
                return HalfWayMove::Skipped;
            }
            last_move_ = decide(observation);
            ++observations_;
            if (last_move_ != HalfWayMove::Unchanged && last_move_ != HalfWayMove::Skipped &&
                last_move_ != HalfWayMove::Frozen) {
                ++moves_;
            }
            return last_move_;
        }

        /// @brief Returns `H` and the step to their initial values and forgets the direction.
        ///
        /// For a caller that starts pricing a different problem with the same controller -- a new
        /// branch-and-bound node whose subproblem differs enough that what was learned no longer
        /// applies. Does not change @ref frozen.
        void reset() {
            h_ = initial_h_;
            step_ = params_.initial_step;
            last_direction_ = 0;
            last_move_ = HalfWayMove::Unchanged;
            observations_ = 0;
            moves_ = 0;
        }

        /// @brief Stops @ref update from moving `H`, or lets it resume.
        void set_frozen(bool frozen) { frozen_ = frozen; }

        /// @brief Whether @ref update is currently a no-op.
        [[nodiscard]] bool frozen() const { return frozen_; }

        /// @brief Whether the controller was given an initial `H`.
        [[nodiscard]] bool seeded() const { return initial_h_ > 0.0; }

        /// @brief The half-way point the next solve should use.
        [[nodiscard]] double h() const { return h_; }

        /// @brief The `H0` the controller was seeded with.
        [[nodiscard]] double initial_h() const { return initial_h_; }

        /// @brief The clock's range as the controller understands it, `R = 2 * H0`.
        [[nodiscard]] double range() const { return 2.0 * initial_h_; }

        /// @brief The current multiplicative step.
        [[nodiscard]] double step() const { return step_; }

        /// @brief What the most recent @ref update did.
        [[nodiscard]] HalfWayMove last_move() const { return last_move_; }

        /// @brief How many observations @ref update has been given since the last @ref reset.
        [[nodiscard]] size_t observations() const { return observations_; }

        /// @brief How many of them actually moved `H`.
        [[nodiscard]] size_t moves() const { return moves_; }

        /// @brief The tuning knobs in use.
        [[nodiscard]] const HalfWayControllerParams& params() const { return params_; }

    private:
        /// @brief The whole decision, without the bookkeeping @ref update wraps round it.
        HalfWayMove decide(const HalfWayObservation& observation) {
            if (frozen_) {
                return HalfWayMove::Frozen;
            }
            if (!observation.bounded || !observation.exact || observation.forward_labels == 0 ||
                observation.backward_labels == 0) {
                return HalfWayMove::Skipped;
            }

            const auto forward = static_cast<double>(observation.forward_labels);
            const auto backward = static_cast<double>(observation.backward_labels);
            const double imbalance = std::abs(forward - backward) / std::min(forward, backward);
            if (imbalance > params_.dead_zone) {
                // The heavier search stops earlier: forward stops above H, so lowering H shortens
                // it.
                const int direction = forward > backward ? -1 : 1;
                h_ = stepped(direction);
                return direction < 0 ? HalfWayMove::Down : HalfWayMove::Up;
            }

            // Balanced counts, checked only once the counts have had their say. Given precedence,
            // this guard pinned H: the controller starts at H0 = R/2, which IS the centre, so on
            // every solve with no join it answered "already central, unchanged" and the imbalance
            // rule never ran -- measured on the equivalence sweep at up to 4.3x more forward
            // labels than backward with H stuck. An imbalance already says which way to go.
            if (observation.joined_paths == 0 && observation.solutions > 0) {
                const double centre = range() / 2.0;
                if (std::abs(h_ - centre) <= params_.dead_zone * centre) {
                    return HalfWayMove::Unchanged;
                }
                const int direction = h_ < centre ? 1 : -1;
                const double moved = stepped(direction);
                // Toward the centre, never past it: overshooting would only hand the next solve a
                // misplaced H on the other side.
                h_ = direction > 0 ? std::min(moved, centre) : std::max(moved, centre);
                return HalfWayMove::TowardCentre;
            }
            return HalfWayMove::Unchanged;
        }

        /// @brief `H` after one damped, clamped multiplicative step in @p direction (+1 up, -1
        ///        down). Updates the step and the remembered direction; the caller assigns `H`.
        [[nodiscard]] double stepped(int direction) {
            if (last_direction_ != 0 && direction != last_direction_) {
                step_ = std::max(step_ / params_.step_decay, params_.min_step);
            }
            last_direction_ = direction;
            const double moved = h_ * (1.0 + (direction * step_));
            return std::clamp(moved,
                              params_.min_fraction * range(),
                              params_.max_fraction * range());
        }

        HalfWayControllerParams params_;
        double initial_h_ = 0.0;
        double h_ = 0.0;
        double step_ = 0.0;
        int last_direction_ = 0;
        bool frozen_ = false;
        HalfWayMove last_move_ = HalfWayMove::Unchanged;
        size_t observations_ = 0;
        size_t moves_ = 0;
};

/// @brief Checks that the chosen critical resource really is monotone along every arc.
///
/// For each arc and probe value `x`, extends a resource whose critical component is `x` and
/// requires the result to be at least `x`. Raw arc consumptions are not enough: an extension that
/// clamps up to a time window can be monotone despite a negative stored value.
///
/// @tparam CriticalRC    The critical resource's type.
/// @tparam ResourceTypes The graph's resource pack.
/// @param graph                   The graph whose arcs are probed.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @param probes                  Values to probe with, e.g. `{0, R/2}`.
/// @return `true` when every arc is monotone on the critical slot.
template <typename CriticalRC, typename... ResourceTypes>
[[nodiscard]] bool critical_resource_is_monotone(
    const Graph<ResourceTypeComposition<ResourceTypes...>>& graph, size_t critical_resource_index,
    std::span<const double> probes) {
    // Type absent from the pack: report "not monotone" so the caller disables the bound.
    if constexpr (ComponentTypeIndex<CriticalRC, ResourceTypes...>::value == -1) {
        return false;
    } else {
        using Composed = Resource<ResourceTypeComposition<ResourceTypes...>>;
        // Two scratch resources, cloned once and rebound per arc: reset() adopts a node's
        // function objects without cloning them, as a pooled label does.
        std::unique_ptr<Composed> source;
        std::unique_ptr<Composed> extended;
        bool monotone = true;
        graph.for_each_arc([&](const auto& arc) {
            if (!monotone || arc.extender == nullptr || arc.origin->resource == nullptr ||
                arc.destination->resource == nullptr) {
                return;
            }
            if (source == nullptr) {
                source = std::make_unique<Composed>(*arc.origin->resource);
                extended = std::make_unique<Composed>(*arc.destination->resource);
            }
            for (const double probe : probes) {
                source->reset(*arc.origin->resource);
                extended->reset(*arc.destination->resource);

                auto& source_component =
                    source->template get_component<CriticalRC>(critical_resource_index);
                // Cast to the component's value type, which may be integral.
                using CriticalValueType =
                    std::decay_t<decltype(source_component.get_value().get_value())>;
                source_component.set_value(static_cast<CriticalValueType>(probe));

                arc.extender->extend(*source, extended.get());

                const auto& extended_component =
                    extended->template get_component<CriticalRC>(critical_resource_index);
                const auto after = static_cast<double>(extended_component.get_value().get_value());
                if (after < probe) {
                    monotone = false;
                    return;
                }
            }
        });
        return monotone;
    }
}

/// @brief Checks that the chosen critical resource takes part in the dominance order, increasing.
///
/// The bound requires `X dominates Y => X.clock <= Y.clock`; otherwise a dominator past `H` can
/// evict a label the join needed. `ValueDominanceFunction` satisfies this;
/// `TrivialDominanceFunction` and higher-is-better orders do not.
///
/// @tparam CriticalRC    The critical resource's type.
/// @tparam ResourceTypes The graph's resource pack.
/// @param graph                   The graph whose nodes carry the function objects.
/// @param critical_resource_index Position of the critical resource within its type slot.
/// @return `true` when the clock's dominance is increasing; `false` otherwise, including when the
///         type is absent or the graph has no usable node.
template <typename CriticalRC, typename... ResourceTypes>
[[nodiscard]] bool critical_resource_dominance_is_increasing(
    const Graph<ResourceTypeComposition<ResourceTypes...>>& graph, size_t critical_resource_index) {
    if constexpr (ComponentTypeIndex<CriticalRC, ResourceTypes...>::value == -1) {
        return false;
    } else {
        const std::vector<size_t> node_ids = graph.get_node_ids();
        if (node_ids.empty()) {
            return false;
        }
        const auto* node = graph.get_node(node_ids.front());
        if (node == nullptr || node->resource == nullptr) {
            return false;
        }

        // Copies clone the function objects.
        Resource<ResourceTypeComposition<ResourceTypes...>> low(*node->resource);
        Resource<ResourceTypeComposition<ResourceTypes...>> high(*node->resource);

        auto& low_component = low.template get_component<CriticalRC>(critical_resource_index);
        auto& high_component = high.template get_component<CriticalRC>(critical_resource_index);
        using CriticalValueType = std::decay_t<decltype(low_component.get_value().get_value())>;
        low_component.set_value(static_cast<CriticalValueType>(0));
        high_component.set_value(static_cast<CriticalValueType>(1));

        // Probe values one apart, well outside any comparison tolerance.
        return (low_component <= high_component) && !(high_component <= low_component);
    }
}

}  // namespace rcspp
