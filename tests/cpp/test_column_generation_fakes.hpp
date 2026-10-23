// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

// Test doubles for the column generation layer: a backend whose LP answers are scripted by the
// test, and pricers whose answers are scripted too. They let the loop be tested without an LP
// solver or a graph.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/column_generation/pricer.hpp"
#include "rcspp/column_generation/solver_backend.hpp"
#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation::testing {

/// @brief Shorthand for an infinite bound.
inline constexpr double infinity = std::numeric_limits<double>::infinity();

/// @brief A backend that records what it is given and answers what the test scripts.
class ScriptedBackend final : public SolverBackend {
    public:
        /// @brief A column as the backend received it.
        struct ReceivedColumn {
                ColumnId id = 0;
                double cost = 0.0;
                double lower = 0.0;
                double upper = 0.0;
                std::vector<std::pair<std::uint32_t, double>> entries;
        };

        [[nodiscard]] size_t num_rows() const override { return lower.size(); }

        void row_bounds(std::span<double> lower_out, std::span<double> upper_out) const override {
            for (size_t row = 0; row < lower.size(); ++row) {
                lower_out[row] = lower[row];
                upper_out[row] = upper[row];
            }
        }

        void add_rows(const RowBatch& rows) override {
            for (size_t row = 0; row < rows.lower.size(); ++row) {
                lower.push_back(rows.lower[row]);
                upper.push_back(rows.upper[row]);
                names.push_back(rows.names.empty() ? std::string() : rows.names[row]);
            }
        }

        void add_columns(const ColumnBatch& batch) override {
            ++add_columns_calls;
            for (size_t column = 0; column < batch.size(); ++column) {
                ReceivedColumn received{batch.ids[column],
                                        batch.cost[column],
                                        batch.lower[column],
                                        batch.upper[column],
                                        {}};
                for (std::uint32_t entry = batch.starts[column]; entry < batch.starts[column + 1];
                     ++entry) {
                    received.entries.emplace_back(batch.row_indices[entry], batch.values[entry]);
                }
                columns.push_back(std::move(received));
            }
        }

        SolverStatus solve_relaxation(const RelaxationParams& params) override {
            relaxation_calls.push_back(params);
            return on_solve ? on_solve(*this) : SolverStatus::Optimal;
        }

        [[nodiscard]] double objective() const override { return objective_value; }

        void duals(std::span<double> out) const override {
            for (size_t row = 0; row < out.size(); ++row) {
                out[row] = row < dual_values.size() ? dual_values[row] : 0.0;
            }
        }

        void primal(std::span<const ColumnId> ids, std::span<double> out) const override {
            for (size_t index = 0; index < ids.size(); ++index) {
                const auto found = values.find(ids[index]);
                out[index] = found == values.end() ? 0.0 : found->second;
            }
        }

        SolverStatus solve_integer(const IntegerParams& params) override {
            integer_calls.push_back(params);
            return SolverStatus::Optimal;
        }

        void set_option(std::string_view name, const OptionValue& value) override {
            options.emplace_back(std::string(name), value);
        }

        [[nodiscard]] Capabilities capabilities() const override { return capabilities_value; }

        // Rows.
        std::vector<double> lower;
        std::vector<double> upper;
        std::vector<std::string> names;

        // What was received.
        std::vector<ReceivedColumn> columns;
        size_t add_columns_calls = 0;
        std::vector<RelaxationParams> relaxation_calls;
        std::vector<IntegerParams> integer_calls;
        std::vector<std::pair<std::string, OptionValue>> options;

        // The script: called by every LP solve, it sets the answers below.
        std::function<SolverStatus(ScriptedBackend&)> on_solve;
        double objective_value = 0.0;
        std::vector<double> dual_values;
        std::unordered_map<ColumnId, double> values;
        Capabilities capabilities_value{.add_rows = true, .integer_solve = true};
};

/// @brief The smallest backend: a wrapped model whose rows already exist, so it keeps the base
///        class's @c add_rows and reports no capability.
class WrappedModelBackend final : public SolverBackend {
    public:
        [[nodiscard]] size_t num_rows() const override { return 0; }
        void row_bounds(std::span<double> /*lower*/, std::span<double> /*upper*/) const override {}
        void add_columns(const ColumnBatch& /*columns*/) override {}
        SolverStatus solve_relaxation(const RelaxationParams& /*params*/) override {
            return SolverStatus::Optimal;
        }
        [[nodiscard]] double objective() const override { return 0.0; }
        void duals(std::span<double> /*out*/) const override {}
        void primal(std::span<const ColumnId> /*ids*/, std::span<double> /*out*/) const override {}
        SolverStatus solve_integer(const IntegerParams& /*params*/) override {
            return SolverStatus::Optimal;
        }
        void set_option(std::string_view /*name*/, const OptionValue& /*value*/) override {}
        [[nodiscard]] Capabilities capabilities() const override { return {}; }
};

/// @brief A pricer that records each call and answers what the test scripts.
///
/// Keeps the base class's @c update and @c cost_scale.
class ScriptedPricer final : public Pricer {
    public:
        using Respond =
            std::function<PricingResult(std::span<const double>, const PricingRequest&)>;

        explicit ScriptedPricer(Respond respond_function = {})
            : respond(std::move(respond_function)) {}

        PricingResult price(std::span<const double> duals, const PricingRequest& request) override {
            requests.push_back(request);
            seen_duals.emplace_back(duals.begin(), duals.end());
            if (respond) {
                return respond(duals, request);
            }
            return proven_none();
        }

        /// @brief No column, proven by an exact stage.
        static PricingResult proven_none() {
            return PricingResult{{}, PricingStatus::Optimal, 0.0, true};
        }

        Respond respond;
        std::vector<PricingRequest> requests;
        std::vector<std::vector<double>> seen_duals;
};

/// @brief A pricer that finds nothing, with a chosen cost scale, and counts updates.
class ScaledPricer final : public Pricer {
    public:
        explicit ScaledPricer(double scale) : scale_(scale) {}

        PricingResult price(std::span<const double> /*duals*/,
                            const PricingRequest& /*request*/) override {
            return ScriptedPricer::proven_none();
        }

        void update(const PricingChanges& /*changes*/) override { ++updates; }

        [[nodiscard]] double cost_scale() const override { return scale_; }

        int updates = 0;

    private:
        double scale_;
};

/// @brief A path whose master column has cost @p cost and the entries @p rows.
inline Solution make_path(double cost, std::vector<Row> rows, double reduced_cost = -1.0) {
    return Solution(reduced_cost, {}, {}, Column{cost, std::move(rows)});
}

}  // namespace rcspp::column_generation::testing
