// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>

struct Customer {
        Customer();

        Customer(std::size_t id, double pos_x, double pos_y, int demand, int ready_time,
                 int due_time, int service_time, bool depot);

        size_t id;
        double pos_x;
        double pos_y;
        int demand;
        int ready_time;
        int due_time;
        int service_time;
        bool depot;
};
