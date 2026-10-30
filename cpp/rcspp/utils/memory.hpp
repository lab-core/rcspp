// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "rcspp/utils/logger.hpp"

#if defined(__APPLE__) || defined(__MACH__)
#include <mach/mach.h>       // task_info, mach_task_self, host_statistics64, host_page_size
#include <mach/mach_host.h>  // HOST_VM_INFO64, vm_statistics64_data_t
#include <sys/sysctl.h>      // sysctlbyname
#elif defined(_WIN32)
// Suppress the min()/max() macros from <windows.h>; they otherwise clobber
// std::min / std::max / std::numeric_limits::max() used across the library.
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Exclude the GDI drawing API (<wingdi.h>), which declares a global Arc() function that
// collides with rcspp::Arc wherever that type is named at namespace scope under
// `using namespace rcspp` (e.g. the Python bindings). We do not use any GDI here.
#ifndef NOGDI
#define NOGDI
#endif
// windows.h must precede psapi.h: <psapi.h> declares PROCESS_MEMORY_COUNTERS /
// GetProcessMemoryInfo using types (DWORD, HANDLE, ...) that <windows.h> defines,
// and it does not include <windows.h> itself.  The NOLINT comments silence
// cpplint, but clang-format ignores them and would sort psapi.h first
// (alphabetical), breaking the build — so disable its include sorting here.
// clang-format off
#include <windows.h>  // NOLINT(build/include_order) — platform SDK header, must precede psapi.h
#include <psapi.h>    // NOLINT(build/include_order) — GetProcessMemoryInfo; needs windows.h first
// clang-format on
#else
// POSIX fallback: peak RSS via getrusage (available on Linux/macOS/BSDs)
#include <sys/resource.h>  // NOLINT(build/include_order) — POSIX header
#endif

namespace rcspp {

// ── Byte-unit helpers ─────────────────────────────────────────────────────────
/// One kibibyte in bytes (1 024 B).
constexpr size_t kKB = 1024ULL;
/// One mebibyte in bytes (1 024² B).
constexpr size_t kMB = 1024ULL * 1024ULL;
/// One gibibyte in bytes (1 024³ B).
constexpr size_t kGB = 1024ULL * 1024ULL * 1024ULL;

/// Default pressure threshold: prune queues when RSS reaches 80 % of the limit.
constexpr double kDefaultMemoryPressureFraction = 0.8;

// ── MemoryInfo ────────────────────────────────────────────────────────────────

/// @brief Utility struct providing cross-platform memory queries.
///
/// All methods are static and return byte counts.  They return 0 when the
/// relevant value cannot be determined on the current platform.
struct MemoryInfo {
        /// @brief Returns the current Resident Set Size (RSS) of this process in bytes.
        ///
        /// Platform-specific implementation:
        ///  - **Linux**: reads `VmRSS` from `/proc/self/status` (current RSS in kB × 1024).
        ///  - **macOS**: queries `TASK_VM_INFO` for `phys_footprint`, which matches
        ///    the value shown in Activity Monitor.
        ///  - **Windows**: `WorkingSetSize` from `GetProcessMemoryInfo`.
        ///  - **POSIX fallback**: returns `ru_maxrss` from `getrusage(RUSAGE_SELF)`.
        ///    Note that `ru_maxrss` is the *peak* RSS since process start, not the
        ///    current value; it is still useful as a conservative upper bound.
        ///  - **Unknown platform**: always returns 0.
        ///
        /// @return Current process RSS in bytes, or 0 if it cannot be determined.
        static size_t process_bytes() noexcept {
#if defined(__linux__)
            std::ifstream status("/proc/self/status");
            std::string line;
            while (std::getline(status, line)) {
                if (line.starts_with("VmRSS:")) {
                    const auto pos = line.find_first_of("0123456789");
                    if (pos != std::string::npos) {
                        return std::stoull(line.substr(pos)) * kKB;
                    }
                }
            }
            return 0;
#elif defined(__APPLE__) || defined(__MACH__)
            task_vm_info_data_t info{};
            mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
            // The Mach C API declares task_info() with a (task_info_t) parameter, which is
            // a void*. reinterpret_cast is the correct C++ way to satisfy that signature.
            const kern_return_t kr =
                task_info(mach_task_self(),
                          TASK_VM_INFO,
                          reinterpret_cast<task_info_t>(
                              &info),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                       // — required by Mach C API
                          &count);
            return (kr == KERN_SUCCESS) ? static_cast<size_t>(info.phys_footprint) : 0;
#elif defined(_WIN32)
            PROCESS_MEMORY_COUNTERS pmc{};
            if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                return static_cast<size_t>(pmc.WorkingSetSize);
            }
            return 0;
#else
            // POSIX fallback: ru_maxrss is the peak RSS.
            // On macOS it is in bytes; on Linux and most BSDs it is in kibibytes.
            // Here we are in the final else (neither Linux nor macOS), so treat as bytes.
            rusage usage{};
            return (getrusage(RUSAGE_SELF, &usage) == 0) ? static_cast<size_t>(usage.ru_maxrss) : 0;
#endif
        }

        /// @brief Returns the amount of available physical RAM in bytes.
        ///
        /// "Available" means memory the OS can hand to new allocations without swapping:
        ///  - **Linux**: `MemAvailable` from `/proc/meminfo` (free + easily reclaimable).
        ///  - **macOS**: `(free + inactive) pages × page_size` from `HOST_VM_INFO64`.
        ///  - **Windows**: `ullAvailPhys` from `GlobalMemoryStatusEx`.
        ///  - **Unknown platform**: always returns 0.
        ///
        /// @return Available system RAM in bytes, or 0 if it cannot be determined.
        static size_t available_system_bytes() noexcept {
#if defined(__linux__)
            std::ifstream meminfo("/proc/meminfo");
            std::string line;
            while (std::getline(meminfo, line)) {
                if (line.starts_with("MemAvailable:")) {
                    const auto pos = line.find_first_of("0123456789");
                    if (pos != std::string::npos) {
                        return std::stoull(line.substr(pos)) * kKB;
                    }
                }
            }
            return 0;
#elif defined(__APPLE__) || defined(__MACH__)
            vm_statistics64_data_t vm_stat{};
            mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
            const mach_port_t host = mach_host_self();
            // Same Mach C API reinterpret_cast requirement as process_bytes().
            const kern_return_t kr = host_statistics64(
                host,
                HOST_VM_INFO64,
                reinterpret_cast<host_info64_t>(
                    &vm_stat),  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
                                // — required by Mach C API
                &count);
            if (kr != KERN_SUCCESS) {
                return 0;
            }
            vm_size_t page_size = 0;
            host_page_size(host, &page_size);
            return static_cast<size_t>(vm_stat.free_count + vm_stat.inactive_count) *
                   static_cast<size_t>(page_size);
#elif defined(_WIN32)
            MEMORYSTATUSEX status{};
            status.dwLength = sizeof(status);
            return GlobalMemoryStatusEx(&status) ? static_cast<size_t>(status.ullAvailPhys) : 0;
#else
            return 0;
#endif
        }

        /// @brief Returns the total physical RAM installed in the system, in bytes.
        ///
        /// Unlike @ref available_system_bytes(), this value is fixed for the lifetime
        /// of the machine and is not affected by other running processes.  Use it when
        /// you want a stable, predictable limit such as "never exceed 80% of this
        /// machine's 16 GiB".
        ///
        ///  - **Linux**: `MemTotal` from `/proc/meminfo`.
        ///  - **macOS**: `hw.memsize` sysctl key.
        ///  - **Windows**: `ullTotalPhys` from `GlobalMemoryStatusEx`.
        ///  - **Unknown platform**: always returns 0.
        ///
        /// @return Total installed physical RAM in bytes, or 0 if it cannot be determined.
        static size_t total_system_bytes() noexcept {
#if defined(__linux__)
            std::ifstream meminfo("/proc/meminfo");
            std::string line;
            while (std::getline(meminfo, line)) {
                if (line.starts_with("MemTotal:")) {
                    const auto pos = line.find_first_of("0123456789");
                    if (pos != std::string::npos) {
                        return std::stoull(line.substr(pos)) * kKB;
                    }
                }
            }
            return 0;
#elif defined(__APPLE__) || defined(__MACH__)
            int64_t mem = 0;
            size_t len = sizeof(mem);
            // sysctlbyname("hw.memsize") returns total physical RAM on macOS.
            return (sysctlbyname("hw.memsize", &mem, &len, nullptr, 0) == 0)
                       ? static_cast<size_t>(mem)
                       : 0;
#elif defined(_WIN32)
            MEMORYSTATUSEX status{};
            status.dwLength = sizeof(status);
            return GlobalMemoryStatusEx(&status) ? static_cast<size_t>(status.ullTotalPhys) : 0;
#else
            return 0;
#endif
        }
};

// ── MemoryLimitHelper ─────────────────────────────────────────────────────────

/// @brief What a periodic memory check found.
enum class MemoryState {
    Ok,             ///< below the pressure threshold, or not checked on this iteration
    UnderPressure,  ///< at or above the pressure threshold, below the limit
    Exceeded,       ///< at or above the limit
};

/// @brief Encapsulates memory-limit state and checks for a single solve pass.
///
/// Call @ref resolve() once at the start of solve() with the relevant
/// configuration values.  Then call @ref check() on each iteration of the main loop; it reads
/// the process's memory only every @ref check_interval iterations.
struct MemoryLimitHelper {
        /// @brief Derive and store the effective byte limit from plain configuration values.
        ///
        /// Priority (highest first):
        ///  1. @p max_memory_gb — explicit GiB value (non-zero wins immediately).
        ///  2. @p limit_to_available_ram — fraction of currently-available RAM
        ///     (fluctuates with other processes).
        ///  3. @p limit_to_total_ram — fraction of total physical RAM (fixed
        ///     hardware constant, always reproducible).
        ///  4. No limit (@ref effective_limit = 0).
        ///
        /// Logs the resolved limit at INFO level when a non-zero limit is set.
        ///
        /// @param max_memory_gb         Explicit hard limit in gibibytes (0 = disabled).
        /// @param limit_to_available_ram  Derive limit from currently-available RAM.
        /// @param limit_to_total_ram      Derive limit from total physical RAM.
        /// @param memory_limit_fraction   Fraction to apply in the RAM-relative modes.
        /// @param pressure_fraction_in    RSS fraction that triggers pressure pruning.
        /// @param check_interval_in       Iterations between two checks by @ref check(); 0 means
        ///                                @ref check() never reads the memory.
        void resolve(double max_memory_gb, bool limit_to_available_ram, bool limit_to_total_ram,
                     double memory_limit_fraction, double pressure_fraction_in,
                     size_t check_interval_in = 0) {
            forced_pressure_checks_.clear();
            pressure_checks_made_ = 0;
            pressure_fraction = pressure_fraction_in;
            check_interval = check_interval_in;
            if (max_memory_gb > 0.0) {
                effective_limit = static_cast<size_t>(max_memory_gb * kGB);
                // LOG_TRACE: resolve() is called on every G.solve() invocation,
                // so anything above TRACE produces thousands of lines per run.
                LOG_TRACE("Memory limit: ", max_memory_gb, " GB (explicit).\n");
            } else if (limit_to_available_ram) {
                const size_t ref = MemoryInfo::available_system_bytes();
                if (ref > 0) {
                    effective_limit =
                        static_cast<size_t>(static_cast<double>(ref) * memory_limit_fraction);
                    LOG_TRACE("Memory limit: ",
                              effective_limit / kMB,
                              " MB (",
                              static_cast<int>(memory_limit_fraction * 100.0),
                              "% of ",
                              ref / kMB,
                              " MB available).\n");
                } else {
                    LOG_WARN(
                        "Cannot determine available system RAM; "
                        "memory limit disabled.\n");
                    effective_limit = 0;
                }
            } else if (limit_to_total_ram) {
                const size_t ref = MemoryInfo::total_system_bytes();
                if (ref > 0) {
                    effective_limit =
                        static_cast<size_t>(static_cast<double>(ref) * memory_limit_fraction);
                    LOG_TRACE("Memory limit: ",
                              effective_limit / kMB,
                              " MB (",
                              static_cast<int>(memory_limit_fraction * 100.0),
                              "% of ",
                              ref / kMB,
                              " MB total).\n");
                } else {
                    LOG_WARN(
                        "Cannot determine total system RAM; "
                        "memory limit disabled.\n");
                    effective_limit = 0;
                }
            } else {
                effective_limit = 0;
            }
        }

        /// @brief Returns true when the process RSS has reached the effective limit.
        [[nodiscard]] bool is_exceeded() const noexcept {
            if (effective_limit == 0 || !forced_pressure_checks_.empty()) {
                return false;
            }
            return MemoryInfo::process_bytes() >= effective_limit;
        }

        /// @brief Returns true when the process RSS has reached the pressure threshold.
        ///
        /// The threshold is @ref pressure_fraction × @ref effective_limit.
        [[nodiscard]] bool is_under_pressure() const noexcept {
            if (effective_limit == 0) {
                return false;
            }
            return MemoryInfo::process_bytes() >= pressure_threshold();
        }

        /// @brief The memory state at main-loop iteration @p iteration.
        ///
        /// Reads the process's memory once, and only every @ref check_interval iterations, never
        /// at iteration 0. Otherwise, and when there is no limit, the answer is @c Ok.
        ///
        /// @param iteration The main loop's iteration count.
        /// @return What the check found.
        [[nodiscard]] MemoryState check(size_t iteration) const noexcept {
            if (effective_limit == 0 || check_interval == 0 || iteration == 0 ||
                iteration % check_interval != 0) {
                return MemoryState::Ok;
            }
            if (!forced_pressure_checks_.empty()) {
                ++pressure_checks_made_;
                return std::ranges::find(forced_pressure_checks_, pressure_checks_made_) !=
                               forced_pressure_checks_.end()
                           ? MemoryState::UnderPressure
                           : MemoryState::Ok;
            }
            const size_t bytes = MemoryInfo::process_bytes();
            if (bytes >= effective_limit) {
                return MemoryState::Exceeded;
            }
            return bytes >= pressure_threshold() ? MemoryState::UnderPressure : MemoryState::Ok;
        }

        /// @brief Tests only: until the next @ref resolve, the limit is never exceeded and
        ///        @ref check reports pressure on exactly the listed checks.
        ///
        /// A check is a call of @ref check on an iteration it reads the memory at, numbered from
        /// 1. A non-empty list also sets @ref effective_limit, since @ref check reads nothing
        /// without a limit.
        ///
        /// @param checks The 1-based numbers of the checks that report pressure; empty turns the
        ///               forcing off.
        void force_pressure_on_checks(std::vector<size_t> checks) {
            forced_pressure_checks_ = std::move(checks);
            pressure_checks_made_ = 0;
            if (!forced_pressure_checks_.empty()) {
                effective_limit = std::numeric_limits<size_t>::max();
            }
        }

        /// @brief Effective byte cap; 0 means unlimited.
        size_t effective_limit = 0;

        /// @brief Fraction of @ref effective_limit that triggers pressure pruning.
        double pressure_fraction = kDefaultMemoryPressureFraction;

        /// @brief Iterations between two reads by @ref check(); 0 means it never reads.
        size_t check_interval = 0;

    private:
        /// @brief The byte count at which pressure pruning starts.
        [[nodiscard]] size_t pressure_threshold() const noexcept {
            return static_cast<size_t>(static_cast<double>(effective_limit) * pressure_fraction);
        }

        /// @brief The checks @ref force_pressure_on_checks asked for; empty when none is forced.
        std::vector<size_t> forced_pressure_checks_;
        /// @brief The checks @ref check has made since the forcing was set (@ref check is const).
        mutable size_t pressure_checks_made_ = 0;
};

}  // namespace rcspp
