// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "customer.hpp"

Customer::Customer()
    : id(0),
      pos_x(0.0),
      pos_y(0.0),
      demand(0),
      ready_time(0),
      due_time(0),
      service_time(0),
      depot(false) {}

Customer::Customer(std::size_t id, double pos_x, double pos_y, int demand, int ready_time,
                   int due_time, int service_time, bool depot)
    : id(id),
      pos_x(pos_x),
      pos_y(pos_y),
      demand(demand),
      ready_time(ready_time),
      due_time(due_time),
      service_time(service_time),
      depot(depot) {}
