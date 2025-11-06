// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "customer.hpp"

class Instance {
  public:
    Instance(int nb_vehicles, int capacity, std::optional<std::string> name);

    const Customer& add_customer(int id, double pos_x, double pos_y, int demand, int ready_time,
                                 int due_time, int service_time, bool depot = false);

    [[nodiscard]] const std::map<size_t, Customer>& get_customers_by_id() const;

    [[nodiscard]] const Customer& get_customer(size_t id) const;

    [[nodiscard]] const Customer& get_depot_customer() const;

    [[nodiscard]] const std::vector<size_t>& get_demand_customers_id() const;

    [[nodiscard]] int get_nb_vehicles() const;

    [[nodiscard]] int get_capacity() const;

  private:
    int nb_vehicles_;
    int capacity_;
    std::optional<std::string> name_;
    std::map<size_t, Customer> customers_by_id_;

    size_t depot_customer_id_{0};
    std::vector<size_t> demand_customers_id_;
};
