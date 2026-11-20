// MEASUREMENT ONLY. The C++ VRP example of examples/cpp/vrp (copied in old/vrp, unchanged but
// for VRP::solve_timed, a copy of its template solve with a timer around each step), one exact
// pricing algorithm (SimpleDominanceAlgorithm, default parameters), single-customer start, its
// own Gurobi master. Prints one JSON line.
//
// Usage: old-vrp <instance file>

#include <cstdio>
#include <string>

#include "instance_reader.hpp"
#include "rcspp/rcspp.hpp"
#include "vrp.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: old-vrp <instance file>\n");
        return 2;
    }
    // The example's benchmarks log at Info (one line per iteration); the layer prints nothing.
    Logger::init(LogLevel::Warn);

    Timer total(true);
    Timer graph(true);
    InstanceReader reader(argv[1]);
    auto instance = reader.read();
    VRP vrp(instance);  // builds the graph
    graph.stop();

    AlgorithmParams<LabelList<ResourceType>> params;
    const auto r = vrp.solve_timed(params);
    total.stop();

    const double rest = r.loop_s - r.setup_s - r.master_s - r.duals_s - r.search_s - r.add_s;
    std::printf(
        "RESULT {\"side\": \"old\", \"lp\": %.12f, \"iterations\": %d, \"columns\": %zu, "
        "\"proven\": %s, \"graph_s\": %.6f, \"setup_s\": %.6f, \"master_s\": %.6f, "
        "\"duals_s\": %.6f, \"search_s\": %.6f, \"add_s\": %.6f, \"rest_s\": %.6f, "
        "\"loop_s\": %.6f, \"total_s\": %.6f}\n",
        r.lp_cost, r.iterations, r.columns, r.proven_optimal ? "true" : "false",
        graph.elapsed_seconds(), r.setup_s, r.master_s, r.duals_s, r.search_s, r.add_s, rest,
        r.loop_s, total.elapsed_seconds());
    return 0;
}
