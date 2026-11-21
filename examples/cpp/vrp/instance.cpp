// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "vrp/instance.hpp"

#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vrp {

namespace {

// The repository's instances, used when a path given is not found; set by the build.
#ifndef RCSPP_VRP_INSTANCES_DIRECTORY
#define RCSPP_VRP_INSTANCES_DIRECTORY "instances"
#endif

// A customer's line: number, x, y, demand, ready time, due time, service time.
constexpr size_t customer_fields = 7;

// The numbers of a line, or nothing when one of its fields is not a number.
std::optional<std::vector<double>> numbers_of(const std::string& line) {
    std::istringstream fields(line);
    std::vector<double> numbers;
    std::string field;
    while (fields >> field) {
        try {
            size_t read = 0;
            numbers.push_back(std::stod(field, &read));
            if (read != field.size()) {
                return std::nullopt;
            }
        } catch (const std::logic_error&) {  // not a number, or out of range
            return std::nullopt;
        }
    }
    return numbers;
}

// The file's name: what follows the last folder separator.
std::string file_name(const std::string& path) {
    const size_t separator = path.find_last_of("/\\");
    return separator == std::string::npos ? path : path.substr(separator + 1);
}

}  // namespace

double distance(const Customer& first, const Customer& second) {
    return std::hypot(second.x - first.x, second.y - first.y);
}

std::string find_instance(std::string_view path) {
    const std::string given(path);
    if (std::ifstream(given).good()) {
        return given;
    }

    std::string name = file_name(given);
    if (name.find('.') == std::string::npos) {
        name += ".txt";
    }
    const std::string directory = RCSPP_VRP_INSTANCES_DIRECTORY;
    std::string candidate = directory + "/" + name;
    if (std::ifstream(candidate).good()) {
        return candidate;
    }

    throw std::runtime_error("no instance '" + given + "', nor '" + name + "' in " + directory);
}

Instance read_solomon(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error(path + ": cannot be read");
    }

    Instance instance;
    instance.name = file_name(path);
    instance.name = instance.name.substr(0, instance.name.rfind('.'));
    bool vehicles_read = false;

    // The title line, then the numbers; other lines are headers.
    std::string line;
    std::getline(file, line);
    while (std::getline(file, line)) {
        const std::optional<std::vector<double>> numbers = numbers_of(line);
        if (!numbers.has_value()) {
            continue;
        }

        const std::vector<double>& values = *numbers;
        if (values.size() == 2 && !vehicles_read) {
            instance.vehicles = static_cast<int>(values[0]);
            instance.capacity = values[1];
            vehicles_read = true;
        } else if (values.size() == customer_fields) {
            std::istringstream fields(line);
            double number = 0.0;
            Customer customer;
            fields >> number >> customer.x >> customer.y >> customer.demand >>
                customer.ready_time >> customer.due_time >> customer.service_time;
            customer.number = static_cast<size_t>(number);
            instance.customers.push_back(customer);
        }
    }

    if (!vehicles_read || instance.customers.empty()) {
        throw std::runtime_error(path + ": no vehicle line or no customers");
    }
    for (size_t index = 0; index < instance.customers.size(); ++index) {
        if (instance.customers[index].number != index) {
            throw std::runtime_error(path +
                                     ": the customers must be numbered 0, 1, 2, ..., the depot 0");
        }
    }

    return instance;
}

}  // namespace vrp
