// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <string>

#include "instance.hpp"

class InstanceReader {
    public:
        InstanceReader(std::string file_path);

        [[nodiscard]] Instance read() const;

        [[nodiscard]] static std::map<size_t, double> read_duals(
            const std::string& duals_file_path);

    private:
        std::string file_path_;
};
