// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Runs a frozen copy of a labelling class and the library's class of the same name on the same
// instance, and compares everything each did: the extensions in order, the pool's counts, the
// result, and the labels left in the containers.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "util/golden_harness.hpp"
#include "util/random_instance.hpp"

namespace parity {

using Composed = rcspp::ResourceTypeComposition<rcspp::RealResource>;
using List = rcspp::LabelList<Composed>;
using Buckets = rcspp::LabelBuckets<rcspp::RealResource, rcspp::RealResource, Composed>;
using GraphType = rcspp::ResourceGraph<rcspp::RealResource>;
constexpr size_t kNone = std::numeric_limits<size_t>::max();
constexpr double kInf = std::numeric_limits<double>::infinity();

/// @brief One extension tried: the label extended and the arc.
struct TraceEntry {
        size_t label_id = 0;
        size_t arc_id = 0;
        bool operator==(const TraceEntry&) const = default;
};

/// @brief One stored label, as the pool's ids and the arcs it hangs from describe it.
struct LabelRecord {
        size_t node_pos = 0;
        size_t label_id = 0;
        size_t prev_id = kNone;
        size_t in_arc = kNone;
        size_t out_arc = kNone;
        uint64_t cost_bits = 0;
        bool operator==(const LabelRecord&) const = default;
};

/// @brief One returned solution.
struct SolutionRecord {
        uint64_t cost_bits = 0;
        std::vector<size_t> arcs;
        std::vector<size_t> nodes;
        bool operator==(const SolutionRecord&) const = default;
};

/// @brief Everything one solve did that a port must reproduce.
struct Observation {
        std::vector<TraceEntry> trace;
        size_t extended = 0;
        int64_t pool_created = 0;
        int64_t pool_reused = 0;
        size_t pool_available = 0;
        size_t pool_total = 0;
        bool ref_counts_consistent = true;
        rcspp::AlgorithmStatus status = rcspp::AlgorithmStatus::Complete;
        bool memory_pressure = false;
        bool has_bidirectional = false;
        bool bounded = false;
        std::string off_reason;
        size_t joined = 0;
        std::vector<SolutionRecord> solutions;
        /// A single-direction search's labels; a bidirectional search's forward labels.
        std::vector<LabelRecord> labels;
        /// A bidirectional search's backward labels.
        std::vector<LabelRecord> backward_labels;
};

/// @brief The bits of @p x, so that costs compare exactly.
inline uint64_t bits(double x) {
    uint64_t b = 0;
    std::memcpy(&b, &x, sizeof b);
    return b;
}

/// @brief Appends a record of every label in @p containers to @p out, node by node, in container
///        order.
template <typename ContainerType>
void record_labels(const std::vector<ContainerType>& containers, std::vector<LabelRecord>* out) {
    for (size_t pos = 0; pos < containers.size(); ++pos) {
        for (const auto* label : containers[pos].get_labels()) {
            out->push_back({pos,
                            label->id,
                            label->prev_label != nullptr ? label->prev_label->id : kNone,
                            label->get_in_arc() != nullptr ? label->get_in_arc()->id : kNone,
                            label->get_out_arc() != nullptr ? label->get_out_arc()->id : kNone,
                            bits(label->get_cost())});
        }
    }
}

/// @brief @p AlgorithmType with forced memory pressure, and readers for what it did.
///
/// @tparam AlgorithmType A labelling class: a frozen copy or the library's.
template <typename AlgorithmType>
class Observed : public AlgorithmType {
    public:
        using AlgorithmType::AlgorithmType;

        /// @brief Where the next solve's observations go (null: nowhere), and which memory checks
        ///        report pressure.
        void observe(Observation* into, std::vector<size_t> forced_pressure_checks) {
            observation_ = into;
            forced_ = std::move(forced_pressure_checks);
            if constexpr (requires { this->search_.observer.trace; }) {
                this->search_.observer.trace = into != nullptr ? &into->trace : nullptr;
            }
        }

        /// @brief Records the counts and the result of the solve that returned @p result.
        void capture(const rcspp::SolveResult& result) {
            auto& o = *observation_;
            o.extended = this->get_number_of_extended_labels();
            const auto& pool = this->get_label_pool();
            o.pool_created = pool.get_nb_created_labels();
            o.pool_reused = pool.get_nb_reused_labels();
            o.pool_available = pool.get_nb_available_labels();
            o.pool_total = pool.get_nb_total_labels();
            o.ref_counts_consistent = pool.check_ref_count_consistency();
            o.status = result.status;
            o.memory_pressure = result.memory_pressure_triggered;
            o.has_bidirectional = result.bidirectional.has_value();
            if (result.bidirectional) {
                o.bounded = result.bidirectional->half_way_used;
                o.off_reason = result.bidirectional->half_way_off_reason;
                o.joined = result.bidirectional->joined_paths;
            }
            for (const auto& s : result.solutions) {
                o.solutions.push_back({bits(s.cost), s.path_arc_ids, s.path_node_ids});
            }
        }

        /// @brief Records the labels left in the containers; only after a solve that kept them.
        void capture_labels() {
            record_labels(own_labels(), &observation_->labels);
            if constexpr (requires { this->get_backward_labels_by_node_pos(); }) {
                record_labels(this->get_backward_labels_by_node_pos(),
                              &observation_->backward_labels);
            }
        }

        /// @brief Calls @p visit(backward, node position, label) for every stored label.
        template <typename Callable>
        void visit_labels(const Callable& visit) const {
            const auto& own = own_labels();
            for (size_t pos = 0; pos < own.size(); ++pos) {
                for (const auto* label : own[pos].get_labels()) {
                    visit(false, pos, *label);
                }
            }
            if constexpr (requires { this->get_backward_labels_by_node_pos(); }) {
                const auto& back = this->get_backward_labels_by_node_pos();
                for (size_t pos = 0; pos < back.size(); ++pos) {
                    for (const auto* label : back[pos].get_labels()) {
                        visit(true, pos, *label);
                    }
                }
            }
        }

    protected:
        void initialize_labels() override {
            AlgorithmType::initialize_labels();
            if (!forced_.empty()) {
                this->memory_limit_.force_pressure_on_checks(forced_);
            }
        }

        /// @brief Where the current solve's observations go; null when none is observed.
        Observation* observation_ = nullptr;

    private:
        /// @brief The containers of a single-direction search, or a bidirectional search's forward
        ///        ones: a kernel search's, or a frozen copy's.
        decltype(auto) own_labels() const {
            if constexpr (requires { this->search_.labels.containers(); }) {
                return (this->search_.labels.containers());
            } else {
                return (this->non_dominated_labels_by_node_pos_);
            }
        }

        std::vector<size_t> forced_;
};

/// @brief @ref Observed, also recording every call of the virtual `extend_label(label, arc)`, as
///        (label id, arc id).
///
/// For the single-direction searches; the bidirectional search extends without that hook.
template <typename AlgorithmType>
class Traced : public Observed<AlgorithmType> {
    public:
        using Observed<AlgorithmType>::Observed;

    protected:
        void extend_label(rcspp::Label<Composed>* label, const rcspp::Arc<Composed>* arc) override {
            if (this->observation_ != nullptr) {
                this->observation_->trace.push_back({label->id, arc->id});
            }
            Observed<AlgorithmType>::extend_label(label, arc);
        }
};

/// @brief An observer that records every extension a kernel search tries, as (label id, arc id),
///        whatever its outcome: the trace a frozen copy's @ref Traced records.
struct TracingObserver {
        /// @brief Where the entries go; null records nothing.
        std::vector<TraceEntry>* trace = nullptr;

        template <typename ResourceType>
        void on_extension(const rcspp::Label<ResourceType>& label,
                          const rcspp::Arc<ResourceType>& arc,
                          rcspp::parts::ExtensionOutcome /*outcome*/) {
            if (trace != nullptr) {
                trace->push_back({label.id, arc.id});
            }
        }
};

/// @brief A preset of the library's parts on @p ScheduleTemplate, with the tracing observer: the
///        preset of a public class with one more type argument.
template <template <typename> class ScheduleTemplate>
using TracedPreset =
    rcspp::detail::Preset<ScheduleTemplate<Composed>, rcspp::parts::DominanceAdmission,
                          rcspp::parts::KeepAtNodes, TracingObserver>;

/// @brief The library's single-direction search on @p ScheduleTemplate, in @p Direction, traced:
///        the parity side of a ported class.
template <template <typename> class ScheduleTemplate, typename LabelContainerType,
          typename Direction = rcspp::ForwardDirection>
using TracedLabellingAlgorithm =
    Observed<rcspp::detail::LabellingAlgorithm<Composed, LabelContainerType, Direction,
                                               TracedPreset<ScheduleTemplate>>>;

/// @brief An instance a parity run solves, and where its bidirectional clock is.
struct Case {
        std::string name;
        std::function<std::unique_ptr<GraphType>()> build;
        size_t clock_index = 1;
        double half_way_point = 25.0;
};

/// @brief Whether RCSPP_PARITY_FULL is set: every instance instead of a subset.
[[nodiscard]] inline bool full_sweep() {
    return std::getenv("RCSPP_PARITY_FULL") != nullptr;
}

/// @brief The golden instances and random ones, a subset unless RCSPP_PARITY_FULL is set.
///
/// Golden: seeds 1–4 at 8 and 11 nodes, seeds 9 and 10 (two sources and two sinks) and seed 13
/// (16 nodes); in full, all of them and seeds 15–18 at 25 and 40 nodes. Their clock is the time,
/// with H = 25. Random: 12 nodes, a time window, a binding capacity and costs of both signs, with
/// one and two sinks, seeds 1–2 (full: 1–6); their clock is the instance's, with H half its range.
inline std::vector<Case> cases() {
    namespace gl = golden_labelling;
    std::vector<Case> all;
    const bool full = full_sweep();
    std::vector<gl::Spec> specs = gl::specs();
    if (full) {
        for (uint32_t seed = 15; seed <= 18; ++seed) {
            specs.push_back({seed, 25, 1});
            specs.push_back({seed, 40, 1});
        }
    }
    for (const gl::Spec& spec : specs) {
        const bool in_default =
            spec.seed <= 4 || spec.seed == 9 || spec.seed == 10 || spec.seed == 13;
        if (!full && !in_default) {
            continue;
        }
        all.push_back({"golden s" + std::to_string(spec.seed) + " n" + std::to_string(spec.n) +
                           " t" + std::to_string(spec.terminals),
                       [spec] { return gl::build(gl::generate(spec)); },
                       1,
                       25.0});
    }
    const uint32_t last_seed = full ? 6 : 2;
    for (uint32_t seed = 1; seed <= last_seed; ++seed) {
        for (const size_t sinks : {size_t{1}, size_t{2}}) {
            test_util::InstanceConfig config;
            config.num_nodes = 12;
            config.with_time_window = true;
            config.with_capacity = true;
            config.capacity_binds = true;
            config.mixed_sign_costs = true;
            config.num_sinks = sinks;
            config.seed = seed;
            const auto probe = test_util::build_instance(config);
            const double h = probe.clock_is_usable ? probe.clock_upper_bound / 2.0 : 0.0;
            all.push_back({"random " + test_util::describe(config),
                           [config] { return test_util::build_instance(config).graph; },
                           probe.clock_index,
                           h});
        }
    }
    return all;
}

/// @brief One setting a parity run solves under.
template <typename LabelContainerType>
struct Setting {
        std::string name;
        rcspp::AlgorithmParams<LabelContainerType> params;
        double upper_bound = kInf;
        bool preprocess = false;
        std::vector<size_t> forced_pressure_checks;
        size_t solves = 1;
        bool half_way_off = false;
};

/// @brief The settings that apply to @p direction, on @p container; with buckets, only exact,
///        quota and pressure.
///
/// - `exact`: the defaults. `pre`: the solve preprocesses the graph.
/// - `quota`: 2 labels extended per node. `phases` (one direction only): and 3 phases.
/// - `dominated`: dominated solutions returned. `stop`: and the search stops after 3.
/// - `prune`: upper bound 0, pruned on. `early`: 30 iterations.
/// - `pressure`: pressure forced on the 3rd and 7th memory checks, one check per iteration, and
///   2 labels per node under pressure.
/// - `reuse`: three exact solves on one object, each with the previous best cost + 5 as its
///   upper bound. `reuse_cut`: three solves on one object under a quota of 1, so a solve leaves
///   labels it never extended.
/// - Bidirectional only: `early_nojoin` (no join after the early stop) and `half_way_off`.
template <typename LabelContainerType>
std::vector<Setting<LabelContainerType>> settings(rcspp::SearchDirection direction,
                                                  const LabelContainerType& container,
                                                  bool buckets = false) {
    using rcspp::SearchDirection;
    using SettingType = Setting<LabelContainerType>;
    const auto base = [&] {
        rcspp::AlgorithmParams<LabelContainerType> p(container);
        p.direction = direction;
        return p;
    };
    const bool bidirectional = direction == SearchDirection::Bidirectional;
    std::vector<SettingType> all;
    all.push_back({"exact", base()});
    {
        SettingType s{"quota", base()};
        s.params.max_extended_labels_per_node = 2;
        all.push_back(s);
    }
    {
        SettingType s{"pressure", base()};
        s.params.memory_check_interval = 1;
        s.params.memory_pressure_max_labels_per_node = 2;
        s.forced_pressure_checks = {3, 7};
        all.push_back(s);
    }
    if (buckets) {
        return all;
    }
    {
        SettingType s{"pre", base()};
        s.preprocess = true;
        all.push_back(s);
    }
    if (!bidirectional) {
        SettingType s{"phases", base()};
        s.params.max_extended_labels_per_node = 2;
        s.params.max_phases = 3;
        s.params.max_solutions = 1000;
        all.push_back(s);
    }
    {
        SettingType s{"dominated", base()};
        s.params.return_dominated_solutions = true;
        s.params.max_solutions = 1000;
        all.push_back(s);
    }
    {
        SettingType s{"stop", base()};
        s.params.return_dominated_solutions = true;
        s.params.max_solutions = 3;
        all.push_back(s);
    }
    {
        SettingType s{"prune", base()};
        s.params.prune_by_incumbent = true;
        s.upper_bound = 0.0;
        all.push_back(s);
    }
    {
        SettingType s{"early", base()};
        s.params.max_iterations = 30;
        all.push_back(s);
    }
    {
        SettingType s{"reuse", base()};
        s.solves = 3;
        all.push_back(s);
    }
    {
        SettingType s{"reuse_cut", base()};
        s.params.max_extended_labels_per_node = 1;
        s.solves = 3;
        all.push_back(s);
    }
    if (bidirectional) {
        {
            SettingType s{"early_nojoin", base()};
            s.params.max_iterations = 30;
            s.params.join_after_early_stop = false;
            all.push_back(s);
        }
        {
            SettingType s{"half_way_off", base()};
            s.half_way_off = true;
            all.push_back(s);
        }
    }
    return all;
}

/// @brief Solves @p c under @p s with an @p AlgorithmType (an @ref Observed or @ref Traced class),
///        releasing the labels after each solve when @p release, and returns one observation per
///        solve.
template <typename AlgorithmType, typename LabelContainerType>
std::vector<Observation> observe(const Case& c, const Setting<LabelContainerType>& s,
                                 bool release) {
    auto graph = c.build();
    rcspp::AlgorithmParams<LabelContainerType> params = s.params;
    params.release_after_solve = release;
    if (params.direction == rcspp::SearchDirection::Bidirectional) {
        params.critical_resource_index = c.clock_index;
        params.half_way_point = s.half_way_off ? 0.0 : c.half_way_point;
    }
    auto algorithm = std::make_unique<AlgorithmType>(&graph->get_resource_factory(), params);
    std::vector<Observation> runs(s.solves);
    double upper_bound = s.upper_bound;
    for (size_t k = 0; k < s.solves; ++k) {
        algorithm->observe(&runs[k], s.forced_pressure_checks);
        const rcspp::SolveResult result = graph->solve(algorithm.get(), upper_bound, s.preprocess);
        algorithm->capture(result);
        if (!release) {
            algorithm->capture_labels();
        }
        algorithm->observe(nullptr, {});
        if (!result.solutions.empty()) {
            upper_bound = result.solutions.front().cost + 5.0;
        }
    }
    return runs;
}

/// @brief Whether two observations agree; if not, where they first part: the trace, then the
///        counts and the result, then the solutions, then the labels.
inline ::testing::AssertionResult same(const Observation& a, const Observation& b) {
    const size_t common = std::min(a.trace.size(), b.trace.size());
    for (size_t i = 0; i < common; ++i) {
        if (!(a.trace[i] == b.trace[i])) {
            std::ostringstream out;
            out << "the extension traces part at entry " << i << ": reference (label "
                << a.trace[i].label_id << ", arc " << a.trace[i].arc_id << "), library (label "
                << b.trace[i].label_id << ", arc " << b.trace[i].arc_id << "); before it:";
            for (size_t j = i >= 5 ? i - 5 : 0; j < i; ++j) {
                out << " (" << a.trace[j].label_id << ", " << a.trace[j].arc_id << ")";
            }
            return ::testing::AssertionFailure() << out.str();
        }
    }
    if (a.trace.size() != b.trace.size()) {
        return ::testing::AssertionFailure()
               << "the extension traces agree for " << common
               << " entries, then one ends: " << a.trace.size() << " against " << b.trace.size();
    }
    const auto field = [](const char* name, auto x, auto y) {
        if (x == y) {
            return ::testing::AssertionSuccess();
        }
        return ::testing::AssertionFailure() << name << ": reference " << x << ", library " << y;
    };
    for (auto r : {field("extended", a.extended, b.extended),
                   field("pool created", a.pool_created, b.pool_created),
                   field("pool reused", a.pool_reused, b.pool_reused),
                   field("pool available", a.pool_available, b.pool_available),
                   field("pool total", a.pool_total, b.pool_total),
                   field("status", static_cast<int>(a.status), static_cast<int>(b.status)),
                   field("memory pressure", a.memory_pressure, b.memory_pressure),
                   field("bidirectional stats", a.has_bidirectional, b.has_bidirectional),
                   field("bounded", a.bounded, b.bounded),
                   field("joined", a.joined, b.joined),
                   field("solutions", a.solutions.size(), b.solutions.size()),
                   field("labels", a.labels.size(), b.labels.size()),
                   field("backward labels", a.backward_labels.size(), b.backward_labels.size())}) {
        if (!r) {
            return r;
        }
    }
    if (a.off_reason != b.off_reason) {
        return ::testing::AssertionFailure() << "half_way_off_reason: reference \"" << a.off_reason
                                             << "\", library \"" << b.off_reason << "\"";
    }
    for (size_t i = 0; i < a.solutions.size(); ++i) {
        if (!(a.solutions[i] == b.solutions[i])) {
            return ::testing::AssertionFailure() << "solution " << i << " differs";
        }
    }
    const auto first_label_difference = [](const std::vector<LabelRecord>& x,
                                           const std::vector<LabelRecord>& y,
                                           const char* which) {
        for (size_t i = 0; i < x.size(); ++i) {
            if (!(x[i] == y[i])) {
                return ::testing::AssertionFailure()
                       << which << " label " << i << " differs: reference (node " << x[i].node_pos
                       << ", id " << x[i].label_id << ", prev " << x[i].prev_id
                       << "), library (node " << y[i].node_pos << ", id " << y[i].label_id
                       << ", prev " << y[i].prev_id << ")";
            }
        }
        return ::testing::AssertionSuccess();
    };
    if (auto r = first_label_difference(a.labels, b.labels, "forward"); !r) {
        return r;
    }
    return first_label_difference(a.backward_labels, b.backward_labels, "backward");
}

/// @brief @ref same, solve by solve.
inline ::testing::AssertionResult same(const std::vector<Observation>& reference,
                                       const std::vector<Observation>& library) {
    if (reference.size() != library.size()) {
        return ::testing::AssertionFailure() << "the solve counts differ";
    }
    for (size_t k = 0; k < reference.size(); ++k) {
        if (auto r = same(reference[k], library[k]); !r) {
            return ::testing::AssertionFailure() << "solve " << k << ": " << r.message();
        }
    }
    return ::testing::AssertionSuccess();
}

/// @brief Runs @p ReferenceType and @p LibraryType over every case and setting of @p direction,
///        in both release modes, and expects them to agree, and the library's reference counts
///        to be consistent.
///
/// @return The number of (case, setting, release mode) runs compared.
template <typename ReferenceType, typename LibraryType, typename LabelContainerType>
size_t sweep(rcspp::SearchDirection direction, const LabelContainerType& container,
             bool buckets = false) {
    size_t compared = 0;
    for (const Case& c : cases()) {
        for (const auto& s : settings<LabelContainerType>(direction, container, buckets)) {
            for (const bool release : {false, true}) {
                const auto expected = observe<ReferenceType>(c, s, release);
                const auto actual = observe<LibraryType>(c, s, release);
                EXPECT_TRUE(same(expected, actual))
                    << c.name << ", setting " << s.name << (release ? ", released" : ", kept");
                for (const auto& o : actual) {
                    EXPECT_TRUE(o.ref_counts_consistent) << c.name << ", setting " << s.name;
                }
                ++compared;
            }
        }
    }
    EXPECT_GT(compared, 0U);
    return compared;
}

}  // namespace parity
