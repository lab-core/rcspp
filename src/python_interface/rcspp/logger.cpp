// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#define PYBIND11_USE_SMART_HOLDER_AS_DEFAULT
#include <pybind11/pybind11.h>

#include "rcspp/utils/logger.hpp"

namespace py = pybind11;
using namespace rcspp;

void init_logger(py::module_& m) {
    // Créer un sous-module "logger"
    auto logger_module = m.def_submodule("logger", "Logger utilities");
    
    // Définir l'énumération LogLevel
    py::enum_<LogLevel>(logger_module, "LogLevel")
        .value("Trace", LogLevel::Trace)
        .value("Debug", LogLevel::Debug)
        .value("Info", LogLevel::Info)
        .value("Warn", LogLevel::Warn)
        .value("Error", LogLevel::Error)
        .value("Fatal", LogLevel::Fatal);

    // Exposer les fonctions statiques comme des fonctions module-level
    logger_module.def("init", [](LogLevel level = LogLevel::Info, 
                                 bool to_console = true, 
                                 const std::string& file_path = "") {
        Logger::init(level, to_console, file_path);
    },
        py::arg("level") = LogLevel::Info,
        py::arg("to_console") = true,
        py::arg("file_path") = "");
    
    logger_module.def("set_level", [](LogLevel level) {
        Logger::instance().set_level(level);
    },
        py::arg("level"));
    
    logger_module.def("level", []() {
        return Logger::instance().level();
    });
    
    logger_module.def("is_level_active", [](LogLevel level) {
        return Logger::instance().is_level_active(level);
    },
        py::arg("level"));
}