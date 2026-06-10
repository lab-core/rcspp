// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include "rcspp/rcspp.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "rcspp/utils/memory.hpp"

namespace py = pybind11;

void init_graph(py::module_&);
void init_resource(py::module_&);
void init_solution_pool(py::module_&);
void init_sigint_handler();

PYBIND11_MODULE(_core, m) {
    m.doc() = "RCSPP module";

    // Install the SIGINT handler once, from the main thread (import time).
    // This must happen before any background thread calls solve().
    init_sigint_handler();

    auto graph_submodule = m.def_submodule("graph", "Graph-related classes");
    init_graph(graph_submodule);

    auto resource_submodule = m.def_submodule("resource", "Resource-related classes");
    init_resource(resource_submodule);

    auto solution_pool_submodule = m.def_submodule("solution_pool", "Solution pool classes");
    init_solution_pool(solution_pool_submodule);

    auto logger_submodule = m.def_submodule("logger", "Logging control");

    py::enum_<rcspp::LogLevel>(logger_submodule, "LogLevel")
        .value("Trace", rcspp::LogLevel::Trace)
        .value("Debug", rcspp::LogLevel::Debug)
        .value("Info", rcspp::LogLevel::Info)
        .value("Warn", rcspp::LogLevel::Warn)
        .value("Error", rcspp::LogLevel::Error)
        .value("Fatal", rcspp::LogLevel::Fatal)
        .export_values();

    logger_submodule.def(
        "set_level",
        [](rcspp::LogLevel level) { rcspp::Logger::instance().set_level(level); },
        py::arg("level"),
        "Set the minimum log level.");

    logger_submodule.def(
        "get_level",
        []() { return rcspp::Logger::instance().level(); },
        "Return the current log level.");

    logger_submodule.def(
        "init",
        [](rcspp::LogLevel level, bool to_console, const std::string& file_path) {
            rcspp::Logger::init(level, to_console, file_path);
        },
        py::arg("level") = rcspp::LogLevel::Info,
        py::arg("to_console") = true,
        py::arg("file_path") = std::string{},
        "Initialize the logger (level, console output, optional log file).");

    // ── Memory helpers ────────────────────────────────────────────────────────
    m.def(
        "process_memory_bytes",
        []() { return rcspp::MemoryInfo::process_bytes(); },
        "Current process Resident Set Size (RSS) in bytes.");
    m.def(
        "available_memory_bytes",
        []() { return rcspp::MemoryInfo::available_system_bytes(); },
        "Available system RAM in bytes.");
}
