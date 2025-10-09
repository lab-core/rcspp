// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <boost/graph/adjacency_list.hpp>

struct VertexPropertiesVRPTW {
        VertexPropertiesVRPTW(size_t id_ = 0, double ready_time_ = 0, double due_time_ = 0,
                              double capacity_ = 0)
            : id(id_), ready_time(ready_time_), due_time(due_time_), capacity(capacity_) {}
        size_t id;
        double ready_time;
        double due_time;
        double capacity;
};

struct EdgePropertiesVRPTW {
        EdgePropertiesVRPTW(size_t id_ = 0, double cost_ = 0, double time_ = 0, double demand_ = 0)
            : id(id_), cost(cost_), time(time_), demand(demand_) {}
        size_t id;
        double cost;
        double time;
        double demand;
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, VertexPropertiesVRPTW,
                              EdgePropertiesVRPTW>
    GraphVRPTW;

struct ResourceContainerVRPTW {
        ResourceContainerVRPTW(double cost_ = 0, double time_ = 0, double demand_ = 0)
            : cost(cost_), time(time_), demand(demand_) {}
        ResourceContainerVRPTW& operator=(const ResourceContainerVRPTW& other) {
            if (this == &other) return *this;
            this->~ResourceContainerVRPTW();
            new (this) ResourceContainerVRPTW(other);
            return *this;
        }
        double cost;
        double time;
        double demand;
};

// ResourceExtensionFunction model
class ResourceExtensionFunctionVRPTW {
    public:
        inline bool operator()(const GraphVRPTW& graph, ResourceContainerVRPTW& new_res_cont,
                               const ResourceContainerVRPTW& old_res_cont,
                               boost::graph_traits<GraphVRPTW>::edge_descriptor edge_desc) const {
            const EdgePropertiesVRPTW& arc_prop = get(boost::edge_bundle, graph)[edge_desc];
            const VertexPropertiesVRPTW& vert_prop =
                get(boost::vertex_bundle, graph)[target(edge_desc, graph)];

            // Extension
            new_res_cont.cost = old_res_cont.cost + arc_prop.cost;

            new_res_cont.time = old_res_cont.time + arc_prop.time;
            if (vert_prop.ready_time > new_res_cont.time) {
                new_res_cont.time = vert_prop.ready_time;
            }

            new_res_cont.demand = old_res_cont.demand + arc_prop.demand;

            // Feasibility
            bool feasible = true;

            if (new_res_cont.time > vert_prop.due_time) {
                feasible = false;
            }

            if (new_res_cont.demand > vert_prop.capacity) {
                feasible = false;
            }

            return feasible;
        }
};

// DominanceFunction model
class DominanceFunctionVRPTW {
    public:
        inline bool operator()(const ResourceContainerVRPTW& res_cont_lhs,
                               const ResourceContainerVRPTW& res_cont_rhs) const {
            // must be "<=" here!!!
            // must NOT be "<"!!!

            return res_cont_lhs.cost <= res_cont_rhs.cost &&
                   res_cont_lhs.time <= res_cont_rhs.time &&
                   res_cont_lhs.demand <= res_cont_rhs.demand;
            // this is not a contradiction to the documentation
            // the documentation says:
            // "A label $l_1$ dominates a label $l_2$ if and only if both are resident
            // at the same vertex, and if, for each resource, the resource consumption
            // of $l_1$ is less than or equal to the resource consumption of $l_2$,
            // and if there is at least one resource where $l_1$ has a lower resource
            // consumption than $l_2$."
            // one can think of a new label with a resource consumption equal to that
            // of an old label as being dominated by that old label, because the new
            // one will have a higher number and is created at a later point in time,
            // so one can implicitly use the number or the creation time as a resource
            // for tie-breaking
        }
};

bool operator==(const ResourceContainerVRPTW& res_cont_lhs,
                const ResourceContainerVRPTW& res_cont_rhs);

bool operator<(const ResourceContainerVRPTW& res_cont_lhs,
               const ResourceContainerVRPTW& res_cont_rhs);
