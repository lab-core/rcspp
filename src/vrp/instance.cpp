// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "instance.hpp"

#include <iostream>
#include <optional>

Instance::Instance(int nb_vehicles, int capacity, std::optional<std::string> name)
    : nb_vehicles_(nb_vehicles),
      capacity_(capacity),
      name_(std::move(name)),
      depot_customer_id_(0) {}

const Customer& Instance::add_customer(int customer_id, double pos_x, double pos_y, int demand,
                                       int ready_time, int due_time, int service_time, bool depot) {
    std::cout << __FUNCTION__ << std::endl;
    std::cout << customer_id << ", " << pos_x << ", " << pos_y << ", " << demand << ", "
              << ready_time << ", " << due_time << ", " << service_time << ", " << depot
              << std::endl;

    if (depot) {
        depot_customer_id_ = customer_id;
    } else {
        demand_customers_id_.push_back(customer_id);
    }

    customers_by_id_.emplace(
        customer_id,
        Customer(customer_id, pos_x, pos_y, demand, ready_time, due_time, service_time, depot));

    return customers_by_id_.at(customer_id);
}

const std::map<size_t, Customer>& Instance::get_customers_by_id() const {
    std::cout << __FUNCTION__ << std::endl;

    return customers_by_id_;
}

const Customer& Instance::get_customer(size_t id) const {
    return customers_by_id_.at(id);
}

const Customer& Instance::get_depot_customer() const {
    std::cout << "depot_customer_id_=" << depot_customer_id_ << std::endl;

    return customers_by_id_.at(depot_customer_id_);
}

const std::vector<size_t>& Instance::get_demand_customers_id() const {
    return demand_customers_id_;
}

int Instance::get_nb_vehicles() const {
    return nb_vehicles_;
}

int Instance::get_capacity() const {
    return capacity_;
}
