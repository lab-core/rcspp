// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <chrono>  // NOLINT(build/c++11)
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>  // NOLINT(build/c++11)
#include <sstream>
#include <string>
#include <utility>

namespace rcspp {

enum class LogLevel : int { Trace = 0, Debug, Info, Warn, Error, Fatal };

class Logger {
    public:
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        static Logger& instance() {
            static Logger inst;
            return inst;
        }

        // Initialize logger: set level, enable console, optional file path
        void init(LogLevel level = LogLevel::Info, bool to_console = true,
                  const std::string& file_path = {}) {
            std::scoped_lock<std::mutex> lock(mu_);
            level_ = level;
            to_console_ = to_console;
            if (!file_path.empty()) {
                file_stream_.open(file_path, std::ios::app);
                file_ok_ = file_stream_.good();
            } else {
                if (file_stream_.is_open()) {
                    file_stream_.close();
                }
                file_ok_ = false;
            }
        }

        void set_level(LogLevel level) {
            std::scoped_lock<std::mutex> lock(mu_);
            level_ = level;
        }

        LogLevel level() const { return level_; }

        template <typename... Args>
        void log(LogLevel lvl, Args&&... args) {
            if (static_cast<int>(lvl) < static_cast<int>(level_)) {
                return;
            }

            std::ostringstream msg_ss;
            (msg_ss << ... << std::forward<Args>(args));
            const std::string payload = msg_ss.str();

            const std::string header = make_header(lvl);

            std::scoped_lock<std::mutex> lock(mu_);
            if (to_console_) {
                std::cout << color_for(lvl) << header << payload << color_reset();
                std::cout.flush();
            }
            if (file_ok_) {
                file_stream_ << header << payload;
                file_stream_.flush();
            }
        }

        // Convenience helpers
        template <typename... Args>
        void trace(Args&&... a) {
            log(LogLevel::Trace, std::forward<Args>(a)...);
        }
        template <typename... Args>
        void debug(Args&&... a) {
            log(LogLevel::Debug, std::forward<Args>(a)...);
        }
        template <typename... Args>
        void info(Args&&... a) {
            log(LogLevel::Info, std::forward<Args>(a)...);
        }
        template <typename... Args>
        void warn(Args&&... a) {
            log(LogLevel::Warn, std::forward<Args>(a)...);
        }
        template <typename... Args>
        void error(Args&&... a) {
            log(LogLevel::Error, std::forward<Args>(a)...);
        }
        template <typename... Args>
        void fatal(Args&&... a) {
            log(LogLevel::Fatal, std::forward<Args>(a)...);
        }

    private:
        Logger() = default;
        ~Logger() {
            if (file_stream_.is_open()) {
                file_stream_.close();
            }
        }

        static std::string now_timestamp() {
            const auto tp = std::chrono::system_clock::now();
            const auto t = std::chrono::system_clock::to_time_t(tp);
            const auto ms = duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
            std::ostringstream ss;
            ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
               << std::setw(3) << ms.count();
            return ss.str();
        }

        static const char* level_name(LogLevel l) {
            switch (l) {
                case LogLevel::Trace:
                    return "TRACE";
                case LogLevel::Debug:
                    return "DEBUG";
                case LogLevel::Info:
                    return "INFO ";
                case LogLevel::Warn:
                    return "WARN ";
                case LogLevel::Error:
                    return "ERROR";
                case LogLevel::Fatal:
                    return "FATAL";
            }
            return "UNK  ";
        }

        static const char* color_for(LogLevel l) {
            switch (l) {
                case LogLevel::Trace:
                    return "\033[37m";  // light gray
                case LogLevel::Debug:
                    return "\033[36m";  // cyan
                case LogLevel::Info:
                    return "\033[32m";  // green
                case LogLevel::Warn:
                    return "\033[33m";  // yellow
                case LogLevel::Error:
                    return "\033[31m";  // red
                case LogLevel::Fatal:
                    return "\033[41;97m";  // white on red
            }
            return "";
        }

        static const char* color_reset() { return "\033[0m"; }

        static std::string make_header(LogLevel lvl) {
            std::ostringstream ss;
            ss << '[' << now_timestamp() << "]" << '[' << level_name(lvl) << "] ";
            return ss.str();
        }

        mutable std::mutex mu_;
        LogLevel level_ = LogLevel::Info;
        bool to_console_ = true;
        std::ofstream file_stream_;
        bool file_ok_ = false;
};

// Helper macros for convenient logging
#define LOG_TRACE(...) ::rcspp::Logger::instance().trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::rcspp::Logger::instance().debug(__VA_ARGS__)
#define LOG_INFO(...) ::rcspp::Logger::instance().info(__VA_ARGS__)
#define LOG_WARN(...) ::rcspp::Logger::instance().warn(__VA_ARGS__)
#define LOG_ERROR(...) ::rcspp::Logger::instance().error(__VA_ARGS__)
#define LOG_FATAL(...) ::rcspp::Logger::instance().fatal(__VA_ARGS__)

}  // namespace rcspp
