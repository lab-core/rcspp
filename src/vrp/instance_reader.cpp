// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "instance_reader.hpp"

#include <fstream>
#include <iostream>

InstanceReader::InstanceReader(std::string file_path) : file_path_(std::move(file_path)) {}

Instance InstanceReader::read() const {
    std::cout << "InstanceReader::read()\n";

    int nb_vehicles = 0;
    int capacity = 0;
    std::string instance_name;

    std::cout << "file_path_=" << file_path_ << std::endl;

    std::ifstream file(file_path_);

    // First line contains the name of the instance.
    std::getline(file, instance_name);

    // Skip lines 2 to 4.
    std::string line;
    std::getline(file, line);
    std::getline(file, line);
    std::getline(file, line);

    // Line 5 contains the number of vehicles and the vehicle capacity.
    file >> nb_vehicles >> capacity;

    Instance instance(nb_vehicles, capacity, instance_name);

    // Skip lines 6 to 9.
    std::getline(file, line);
    std::getline(file, line);
    std::getline(file, line);
    std::getline(file, line);

    // The remaining lines information about customers.
    int customer_id = 0;
    double pos_x = 0;
    double pos_y = 0;
    int demand = 0;
    int ready_time = 0;
    int due_time = 0;
    int service_time = 0;

    while (file >> customer_id >> pos_x >> pos_y >> demand >> ready_time >> due_time >>
           service_time) {
        bool depot = false;
        if (customer_id == 0) {
            depot = true;
        }

        instance.add_customer(customer_id,
                              pos_x,
                              pos_y,
                              demand,
                              ready_time,
                              due_time,
                              service_time,
                              depot);
    }

    std::cout << "nb_customers: " << instance.get_customers_by_id().size() << std::endl;

    return instance;
}

std::map<size_t, double> InstanceReader::read_duals(const std::string& duals_file_path) {
    std::cout << __FUNCTION__ << std::endl;

    std::map<size_t, double> dual_by_var_id;

    std::ifstream file(duals_file_path);

    int var_id = 0;
    double dual_value = 0.0;

    while (file >> var_id >> dual_value) {
        dual_by_var_id.emplace(var_id, dual_value);
    }

    return dual_by_var_id;
}
