// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// The VRPTW pricing subproblem, configurable for the heuristic experiments.
//
// `VRPSubproblem` and `VRPSubproblemNg` fix their model; the heuristic study needs the same model
// with or without a route memory, and with dominance relaxed on one resource at a time (the most
// common way to make a labeling pricer heuristic). One three-slot pack serves every variant, as in
// the VRP example: without a route memory the bitset slot simply holds no component.
//
// Built from `VRPSubproblemNg`'s construction, so the optima match both existing harnesses: the
// cost in real slot 0, time windows in real slot 1 (the bidirectional clock), the demand as an int
// capacity, and optionally an ng memory in the bitset slot.

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/rcspp.hpp"
#include "vrp/instance.hpp"

namespace heuristics {

using PricingGraph =
    rcspp::ResourceGraph<rcspp::RealResource, rcspp::IntResource, rcspp::SizeTBitsetResource>;
using PricingRC = rcspp::ResourceTypeComposition<rcspp::RealResource, rcspp::IntResource,
                                                 rcspp::SizeTBitsetResource>;
using PricingLC = rcspp::LabelList<PricingRC>;

/// @brief Which route relaxation the model prices over.
enum class Relaxation {
    None,  ///< resource-feasible routes; cycles allowed
    Ng,    ///< ng-route, through `presets::add_ng_path_resource`
};

/// @brief The model's knobs.
struct ModelOptions {
        Relaxation relaxation = Relaxation::None;

        /// @brief Nearest customers in each ng-neighbourhood; read under `Relaxation::Ng` only.
        size_t ng_size = 8;

        /// @brief Drop the demand from the dominance order (`TrivialDominanceFunction`).
        bool relax_load_dominance = false;

        /// @brief Drop the ng memory from the dominance order; read under `Relaxation::Ng` only.
        bool relax_ng_dominance = false;

        /// @brief A short tag for a results table.
        [[nodiscard]] std::string tag() const {
            std::string text =
                relaxation == Relaxation::Ng ? "ng" + std::to_string(ng_size) : std::string("none");
            if (relax_load_dominance) {
                text += "+relax_load";
            }
            if (relax_ng_dominance && relaxation == Relaxation::Ng) {
                text += "+relax_ng";
            }
            return text;
        }
};

/// @brief One pricing subproblem: an instance, a dual vector, and a model variant.
///
/// The graph is built once, from the duals, and then shared by every algorithm run on it. A
/// solve restores the arcs its preprocessing removed, so runs do not disturb one another; the
/// node order the first solve chooses is kept by the rest, which keeps them comparable.
class PricingModel {
    public:
        /// @brief Builds the graph.
        ///
        /// @param instance The VRPTW instance.
        /// @param duals    The duals, by customer id; a customer absent from the map has dual 0.
        /// @param options  The model variant.
        PricingModel(const Instance& instance, const std::map<size_t, double>& duals,
                     ModelOptions options)
            : options_(options),
              horizon_(static_cast<double>(instance.get_depot_customer().due_time)),
              graph_(std::make_unique<PricingGraph>()) {
            build(instance, duals);
        }

        [[nodiscard]] PricingGraph& graph() { return *graph_; }

        /// @brief The depot's closing time; the static half-way point is half of it.
        [[nodiscard]] double horizon() const { return horizon_; }

        [[nodiscard]] const ModelOptions& options() const { return options_; }

    private:
        ModelOptions options_;
        double horizon_;
        std::unique_ptr<PricingGraph> graph_;

        static double distance(const Customer& a, const Customer& b) {
            return std::sqrt(std::pow(b.pos_x - a.pos_x, 2) + std::pow(b.pos_y - a.pos_y, 2));
        }

        void build(const Instance& instance, const std::map<size_t, double>& duals) {
            using namespace rcspp;  // NOLINT(google-build-using-namespace)
            const auto& customers = instance.get_customers_by_id();
            const size_t sink_id = customers.size();

            // Time windows, exactly as the existing harnesses set them.
            std::map<size_t, std::pair<double, double>> windows;
            for (const auto& [id, customer] : customers) {
                windows.emplace(id,
                                std::pair<double, double>{static_cast<double>(customer.ready_time),
                                                          static_cast<double>(customer.due_time)});
            }
            constexpr double kOpen = std::numeric_limits<double>::max() / 2;  // prevents overflow
            windows.insert_or_assign(0, std::pair<double, double>{0.0, kOpen});
            windows.insert_or_assign(sink_id, std::pair<double, double>{0.0, kOpen});

            presets::add_cost_resource<RealResource>(*graph_);
            presets::add_window_resource<RealResource>(*graph_, windows);

            // The capacity preset's expansion, with the dominance function selectable.
            const int capacity = instance.get_capacity();
            const auto caps =
                make_node_bounds(0, capacity, std::map<size_t, std::pair<int, int>>{});
            std::unique_ptr<DominanceFunction<IntResource>> load_dominance;
            if (options_.relax_load_dominance) {
                load_dominance = std::make_unique<TrivialDominanceFunction<IntResource>>();
            } else {
                load_dominance = std::make_unique<ValueDominanceFunction<IntResource>>();
            }
            graph_->add_resource<IntResource>(
                std::make_unique<CapacityExtensionFunction<IntResource>>(caps),
                std::make_unique<MinMaxFeasibilityFunction<IntResource>>(caps),
                std::make_unique<TrivialCostFunction<IntResource>>(),
                std::move(load_dominance));

            if (options_.relaxation == Relaxation::Ng) {
                add_ng_memory(customers, sink_id);
            }

            for (const auto& [id, customer] : customers) {
                graph_->add_node(id, customer.depot);
                if (customer.depot) {
                    depot_id_ = id;
                    graph_->add_node(sink_id, false, true);
                }
            }

            for (const auto& [origin_id, origin] : customers) {
                for (const auto& [destination_id, destination] : customers) {
                    if (origin_id != destination_id) {
                        add_arc(origin_id, destination_id, origin, destination, duals);
                    }
                }
                add_arc(origin_id, sink_id, origin, customers.at(depot_id_), duals);
            }
        }

        /// @brief The ng preset's expansion, with the dominance function selectable.
        void add_ng_memory(const std::map<size_t, Customer>& customers, size_t sink_id) {
            using namespace rcspp;  // NOLINT(google-build-using-namespace)
            std::map<size_t, std::set<size_t>> neighborhoods;
            for (const auto& [id, customer] : customers) {
                std::set<size_t> neighborhood;
                if (!customer.depot && options_.ng_size > 0) {
                    std::vector<std::pair<double, size_t>> by_distance;
                    for (const auto& [other_id, other] : customers) {
                        if (other_id != id && !other.depot) {
                            by_distance.emplace_back(distance(customer, other), other_id);
                        }
                    }
                    std::ranges::sort(by_distance);
                    const size_t take = std::min(options_.ng_size, by_distance.size());
                    for (size_t i = 0; i < take; ++i) {
                        neighborhood.insert(by_distance[i].second);
                    }
                }
                neighborhoods[id] = std::move(neighborhood);
            }
            neighborhoods[sink_id] = {};

            std::map<size_t, std::set<size_t>> forbidden_by_node;
            for (const auto& [node_id, neighborhood] : neighborhoods) {
                forbidden_by_node[node_id] = {node_id};
                for (const size_t member : neighborhood) {
                    forbidden_by_node[member] = {member};
                }
            }

            std::unique_ptr<DominanceFunction<SizeTBitsetResource>> ng_dominance;
            if (options_.relax_ng_dominance) {
                ng_dominance = std::make_unique<TrivialDominanceFunction<SizeTBitsetResource>>();
            } else {
                ng_dominance = std::make_unique<InclusionDominanceFunction<SizeTBitsetResource>>();
            }
            graph_->add_resource<SizeTBitsetResource>(
                std::make_unique<NgPathExtensionFunction<SizeTBitsetResource, size_t>>(
                    std::move(neighborhoods)),
                std::make_unique<IntersectionFeasibilityFunction<SizeTBitsetResource, size_t>>(
                    std::move(forbidden_by_node),
                    /*forbidden=*/true),
                std::make_unique<TrivialCostFunction<SizeTBitsetResource>>(),
                std::move(ng_dominance));
        }

        void add_arc(size_t origin_id, size_t destination_id, const Customer& origin,
                     const Customer& destination, const std::map<size_t, double>& duals) {
            using namespace rcspp;  // NOLINT(google-build-using-namespace)
            const double dist = distance(origin, destination);
            double pi = 0.0;
            if (!origin.depot) {
                if (const auto it = duals.find(origin_id); it != duals.end()) {
                    pi = it->second;
                }
            }
            double reduced_cost = dist - pi;
            if (origin.depot && destination.depot) {
                reduced_cost = std::numeric_limits<double>::infinity();
            }
            const double time = origin.service_time + dist;
            const int demand = destination.demand;

            if (options_.relaxation == Relaxation::Ng) {
                graph_->add_arc<RealResource, RealResource, IntResource, SizeTBitsetResource>(
                    std::make_tuple(std::make_tuple(reduced_cost),
                                    std::make_tuple(time),
                                    std::make_tuple(demand),
                                    std::make_tuple(std::set<size_t>{})),
                    origin_id,
                    destination_id,
                    dist,
                    {Row(origin_id, 1.0)});
            } else {
                graph_->add_arc<RealResource, RealResource, IntResource>(
                    {reduced_cost, time, demand},
                    origin_id,
                    destination_id,
                    dist,
                    {Row(origin_id, 1.0)});
            }
        }

        size_t depot_id_ = 0;
};

}  // namespace heuristics
