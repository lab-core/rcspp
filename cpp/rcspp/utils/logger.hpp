// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <chrono>  // NOLINT(build/c++11)
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>  // NOLINT(build/c++11)
#include <sstream>
#include <string>
#include <utility>

namespace rcspp {

/// @brief Severity levels for the Logger, ordered from least to most severe.
///
/// A logger configured at level @c X will emit messages with level >= @c X.
enum class LogLevel : int { Trace = 0, Debug, Info, Warn, Error, Fatal };

/// @brief Thread-safe, singleton logger with optional console and file output.
///
/// Obtain the singleton via @ref instance().  Configure it once at program
/// start with @ref init() or @ref initialize(), then emit messages through
/// the level-specific helpers (@ref trace(), @ref debug(), …) or the
/// convenience macros (`LOG_INFO(…)`, etc.).
///
/// The logger is non-copyable and non-movable; access it exclusively through
/// the singleton.
class Logger {
    public:
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        Logger(Logger&&) = delete;
        Logger& operator=(Logger&&) = delete;

        /// @brief Returns the process-wide Logger singleton.
        ///
        /// @return Reference to the single Logger instance.
        static Logger& instance() {
            static Logger inst;
            return inst;
        }

        /// @brief Convenience static wrapper that delegates to @ref initialize().
        ///
        /// @param level       Minimum log level to emit (default: Info).
        /// @param to_console  When true, write log lines to stdout (default: true).
        /// @param file_path   Optional path to a log file; empty string disables
        ///                    file output (default: disabled).
        static void init(LogLevel level = LogLevel::Info, bool to_console = true,
                         const std::string& file_path = {}) {
            Logger::instance().initialize(std::move(level), to_console, file_path);
        }

        /// @brief Configure the logger's level and output destinations.
        ///
        /// Thread-safe.  Can be called multiple times to reconfigure.  If
        /// @p file_path is non-empty the file is opened in append mode; an
        /// existing open file is closed first.
        ///
        /// @param level       Minimum log level to emit (default: Info).
        /// @param to_console  When true, write log lines to stdout (default: true).
        /// @param file_path   Optional path to a log file; empty string disables
        ///                    file output (default: disabled).
        void initialize(LogLevel level = LogLevel::Info, bool to_console = true,
                        const std::string& file_path = {}) {
            std::scoped_lock<std::mutex> lock(mu_);
            level_ = std::move(level);
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

        /// @brief Sets the minimum log level.  Thread-safe.
        ///
        /// Messages below @p level are silently discarded after this call.
        ///
        /// @param level  New minimum level.
        void set_level(LogLevel level) {
            std::scoped_lock<std::mutex> lock(mu_);
            level_ = std::move(level);
        }

        /// @brief Returns the current minimum log level.
        ///
        /// @return The configured LogLevel threshold.
        LogLevel level() const { return level_; }

        /// @brief Returns whether the given level would be emitted.
        ///
        /// @param lvl  The level to test.
        /// @return     True when @p lvl >= the current threshold.
        bool is_level_active(const LogLevel& lvl) const { return lvl >= level_; }

        /// @brief Formats and emits a log message at the specified level.
        ///
        /// All @p args are streamed into an `std::ostringstream` using
        /// `operator<<`, so any type that supports that operator is accepted.
        /// The call is a no-op when @p lvl is below the current threshold.
        ///
        /// @tparam Args     Variadic argument types, each streamable to ostream.
        /// @param  lvl      Severity level of this message.
        /// @param  args     Message parts concatenated in order.
        template <typename... Args>
        void log(const LogLevel& lvl, Args&&... args) {
            if (!is_level_active(lvl)) {
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

        /// @brief Emits a TRACE-level message.
        ///
        /// @tparam Args  Variadic argument types, each streamable to ostream.
        /// @param  a     Message parts concatenated in order.
        template <typename... Args>
        void trace(Args&&... a) {
            log(LogLevel::Trace, std::forward<Args>(a)...);
        }

        /// @brief Emits a DEBUG-level message.
        ///
        /// @tparam Args  Variadic argument types, each streamable to ostream.
        /// @param  a     Message parts concatenated in order.
        template <typename... Args>
        void debug(Args&&... a) {
            log(LogLevel::Debug, std::forward<Args>(a)...);
        }

        /// @brief Emits an INFO-level message.
        ///
        /// @tparam Args  Variadic argument types, each streamable to ostream.
        /// @param  a     Message parts concatenated in order.
        template <typename... Args>
        void info(Args&&... a) {
            log(LogLevel::Info, std::forward<Args>(a)...);
        }

        /// @brief Emits a WARN-level message.
        ///
        /// @tparam Args  Variadic argument types, each streamable to ostream.
        /// @param  a     Message parts concatenated in order.
        template <typename... Args>
        void warn(Args&&... a) {
            log(LogLevel::Warn, std::forward<Args>(a)...);
        }

        /// @brief Emits an ERROR-level message.
        ///
        /// @tparam Args  Variadic argument types, each streamable to ostream.
        /// @param  a     Message parts concatenated in order.
        template <typename... Args>
        void error(Args&&... a) {
            log(LogLevel::Error, std::forward<Args>(a)...);
        }

        /// @brief Emits a FATAL-level message.
        ///
        /// @tparam Args  Variadic argument types, each streamable to ostream.
        /// @param  a     Message parts concatenated in order.
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
            const auto ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
            std::ostringstream ss;
            std::tm tm_buf;
            std::tm* tm_ptr = nullptr;
#if defined(_MSC_VER)
            // MSVC: use thread-safe localtime_s
            localtime_s(&tm_buf, &t);
            tm_ptr = &tm_buf;
#else
            // POSIX: use thread-safe localtime_r
            localtime_r(&t, &tm_buf);
            tm_ptr = &tm_buf;
#endif
            ss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0')
               << std::setw(3) << ms.count();
            return ss.str();
        }

        static const char* level_name(const LogLevel& l) {
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

        static const char* color_for(const LogLevel& l) {
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

        static std::string make_header(const LogLevel& lvl) {
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

/// @brief Emit a TRACE-level log message via the global Logger singleton.
#define LOG_TRACE(...) ::rcspp::Logger::instance().trace(__VA_ARGS__)
/// @brief Emit a DEBUG-level log message via the global Logger singleton.
#define LOG_DEBUG(...) ::rcspp::Logger::instance().debug(__VA_ARGS__)
/// @brief Emit an INFO-level log message via the global Logger singleton.
#define LOG_INFO(...) ::rcspp::Logger::instance().info(__VA_ARGS__)
/// @brief Emit a WARN-level log message via the global Logger singleton.
#define LOG_WARN(...) ::rcspp::Logger::instance().warn(__VA_ARGS__)
/// @brief Emit an ERROR-level log message via the global Logger singleton.
#define LOG_ERROR(...) ::rcspp::Logger::instance().error(__VA_ARGS__)
/// @brief Emit a FATAL-level log message via the global Logger singleton.
#define LOG_FATAL(...) ::rcspp::Logger::instance().fatal(__VA_ARGS__)

/// @brief Returns true when TRACE messages are currently enabled.
#define LOG_TRACE_ACTIVE() ::rcspp::Logger::instance().is_level_active(::rcspp::LogLevel::Trace)
/// @brief Returns true when DEBUG messages are currently enabled.
#define LOG_DEBUG_ACTIVE() ::rcspp::Logger::instance().is_level_active(::rcspp::LogLevel::Debug)
/// @brief Returns true when INFO messages are currently enabled.
#define LOG_INFO_ACTIVE() ::rcspp::Logger::instance().is_level_active(::rcspp::LogLevel::Info)
/// @brief Returns true when WARN messages are currently enabled.
#define LOG_WARN_ACTIVE() ::rcspp::Logger::instance().is_level_active(::rcspp::LogLevel::Warn)
/// @brief Returns true when ERROR messages are currently enabled.
#define LOG_ERROR_ACTIVE() ::rcspp::Logger::instance().is_level_active(::rcspp::LogLevel::Error)
/// @brief Returns true when FATAL messages are currently enabled.
#define LOG_FATAL_ACTIVE() ::rcspp::Logger::instance().is_level_active(::rcspp::LogLevel::Fatal)

}  // namespace rcspp
