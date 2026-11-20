// MEASUREMENT ONLY: the VRP's graph for the C++ example layer, and a pricer wrapper that times
// the graph pricer. Shared by new_main.cpp and replay_main.cpp.
#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <span>
#include <utility>
#include <vector>

#include "instance.hpp"
#include "rcspp/column_generation.hpp"
#include "rcspp/rcspp.hpp"

namespace layer_vrp {

using namespace rcspp;
using namespace rcspp::column_generation;

using VrpGraph = ResourceGraph<RealResource, IntResource, SizeTSetResource, SizeTBitsetResource>;
using VrpPricer = GraphPricer<RealResource, IntResource, SizeTSetResource, SizeTBitsetResource>;

inline double distance(const Customer& a, const Customer& b) {
    return std::sqrt(std::pow(b.pos_x - a.pos_x, 2) + std::pow(b.pos_y - a.pos_y, 2));
}

// The old example's construct_resource_graph under RouteRelaxation::None (same resources, nodes,
// arcs and arc order), except that the arc out of customer i covers row i - 1 and the arcs out of
// the depot cover none.
inline void build_graph(const Instance& instance, VrpGraph& graph) {
    const auto& customers = instance.get_customers_by_id();
    const size_t sink = customers.size();
    constexpr double open = std::numeric_limits<double>::max() / 2;

    std::map<size_t, std::pair<double, double>> windows;
    for (const auto& [id, customer] : customers) {
        windows.emplace(id, std::pair<double, double>{customer.ready_time, customer.due_time});
    }
    windows.insert_or_assign(0, std::pair<double, double>{0, open});
    windows.insert_or_assign(sink, std::pair<double, double>{0, open});

    presets::add_cost_resource<RealResource>(graph);
    presets::add_window_resource<RealResource>(graph, windows);
    presets::add_capacity_resource<IntResource>(graph, instance.get_capacity());

    size_t depot = 0;
    for (const auto& [id, customer] : customers) {
        graph.add_node(id, customer.depot);
        if (customer.depot) {
            depot = id;
            graph.add_node(sink, false, true);
        }
    }

    auto add_arc = [&](size_t from, size_t to, const Customer& origin, const Customer& target) {
        const double length = distance(origin, target);
        double reduced_cost = length;
        if (origin.depot && target.depot) {
            reduced_cost = std::numeric_limits<double>::infinity();
        }
        const double time = origin.service_time + length;
        const int demand = target.demand;
        std::vector<Row> rows;
        if (!origin.depot) {
            rows.push_back(Row(from - 1, 1.0));
        }
        graph.add_arc<RealResource, RealResource, IntResource>({reduced_cost, time, demand}, from,
                                                               to, length, rows);
    };

    for (const auto& [from, origin] : customers) {
        for (const auto& [to, target] : customers) {
            if (!target.depot && from != to) {
                add_arc(from, to, origin, target);
            }
        }
        add_arc(from, sink, origin, customers.at(depot));
    }
}

// Depot, customer, depot for every customer.
inline std::vector<Solution> single_customer_routes(const Instance& instance) {
    const auto& customers = instance.get_customers_by_id();
    const auto& depot = instance.get_depot_customer();
    const size_t sink = customers.size();
    std::vector<Solution> routes;
    for (size_t id = 1; id < customers.size(); ++id) {
        const auto& customer = customers.at(id);
        Column column{.cost = distance(depot, customer) + distance(customer, depot),
                      .rows = {Row(id - 1, 1.0)}};
        routes.emplace_back(0.0, std::vector<size_t>{depot.id, id, sink}, std::vector<size_t>{},
                            std::move(column));
    }
    return routes;
}

// Times the graph pricer's own work: reduced costs, search, keeping the paths.
class TimedPricer final : public Pricer {
    public:
        explicit TimedPricer(VrpPricer& inner) : inner_(inner) {}
        PricingResult price(std::span<const double> duals, const PricingRequest& request) override {
            timer_.start();
            PricingResult result = inner_.price(duals, request);
            timer_.stop();
            return result;
        }
        [[nodiscard]] double cost_scale() const override { return inner_.cost_scale(); }
        [[nodiscard]] double seconds() const { return timer_.elapsed_seconds(); }

    private:
        VrpPricer& inner_;
        Timer timer_;
};

}  // namespace layer_vrp
