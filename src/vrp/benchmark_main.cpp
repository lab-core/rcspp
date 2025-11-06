// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include <iostream>

#include "cg/subproblem/boost/boost_subproblem.hpp"
#include "instance.hpp"
#include "instance_reader.hpp"
#include "rcspp/algorithm/pulling_dominance_algorithm_iterators.hpp"
#include "rcspp/algorithm/pushing_dominance_algorithm_iterators.hpp"
#include "rcspp/algorithm/simple_dominance_algorithm_iterators.hpp"
#include "rcspp/resource/resource_graph.hpp"
#include "solution_output.hpp"
#include "vrp.hpp"

std::string print_timer_table(const std::string& instance, const std::vector<Timer>& timers,
                              const std::vector<std::string>& labels, bool print_headers = true,
                              size_t metric_w = 15,     // NOLINT(readability-magic-numbers)
                              size_t min_col_w = 10) {  // NOLINT(readability-magic-numbers)
  std::ostringstream out;
  if (timers.size() != labels.size()) {
    out << "print_timer_table_flipped: mismatch between timers and labels sizes\n";
    return out.str();
  }

  // prepare formatted metric strings and compute column widths
  std::vector<std::string> secs_str(timers.size());
  std::vector<std::string> hms_str(timers.size());
  for (size_t i = 0; i < timers.size(); ++i) {
    std::ostringstream tmp;
    double s = timers[i].elapsed_seconds();
    tmp << std::fixed << std::setprecision(3) << (std::isfinite(s) ? s : NAN);
    secs_str[i] = tmp.str();
    hms_str[i] = timers[i].elapsed_to_hms();
  }

  const std::string metric_label = "Instance";
  // compute column widths for each label column
  std::vector<size_t> col_w(timers.size());
  for (size_t i = 0; i < timers.size(); ++i) {
    col_w[i] = std::max({labels[i].size(), secs_str[i].size(), hms_str[i].size(), min_col_w});
  }

  // total width for separator
  size_t total_w = metric_w + 1;  // metric column + space
  for (size_t w : col_w) {
    total_w += (w + 1);  // each column + space
  }

  // header row: metric label + labels as column headers
  if (print_headers) {
    out << std::left << std::setw(metric_w) << metric_label << " ";
    for (size_t i = 0; i < labels.size(); ++i) {
      out << std::right << std::setw(static_cast<int>(col_w[i])) << labels[i] << " ";
    }
    out << "\n" << std::string(total_w, '-') << "\n";
  }

  // // Seconds row
  // out << std::left << std::setw(metric_w) << "Seconds" << " ";
  // for (size_t i = 0; i < secs_str.size(); ++i) {
  //   out << std::right << std::setw(static_cast<int>(col_w[i])) << secs_str[i] << " ";
  // }
  // out << "\n";

  // HH:MM:SS row
  // out << std::left << std::setw(metric_w) << "HH:MM:SS" << " ";
  out << std::left << std::setw(metric_w) << instance << " ";
  for (size_t i = 0; i < hms_str.size(); ++i) {
    out << std::right << std::setw(static_cast<int>(col_w[i])) << hms_str[i] << " ";
  }
  out << "\n";

  return out.str();
}

int main(int argc, char* argv[]) {
  Logger::init(LogLevel::Info);

  LOG_TRACE(__FUNCTION__, '\n');

  // std::vector<std::string> instance_names = {"toy"};
  std::vector<std::string> instance_names = {"C101", "C102", "R101", "R102", "RC101", "RC102"};
  std::vector<std::string> labels = {"Boost", "Simple", "Pushing", "Pulling"};
  std::string root_dir = file_parent_dir(__FILE__, 3);

  std::vector<Timer> total_timers;
  std::string stats;
  bool first_instance = true;
  for (const auto& instance_name : instance_names) {
    std::string instance_path = root_dir + "/instances/" + instance_name + ".txt";

    LOG_INFO("Instance: ", instance_path, '\n');
    InstanceReader instance_reader(instance_path);

    auto instance = instance_reader.read();
    VRP vrp(instance);
    vrp.sort_nodes();

    Timer timer(true);
    auto timers = vrp.solve<SimpleDominanceAlgorithmIterators,
                            PushingDominanceAlgorithmIterators,
                            PullingDominanceAlgorithmIterators>();
    timer.stop();

    if (first_instance) {
      total_timers = timers;
    } else {
      for (size_t i = 0; i < timers.size(); ++i) {
        total_timers[i] += timers[i];
      }
    }
    auto instance_stats = print_timer_table(instance_name, timers, labels, first_instance);
    first_instance = false;
    stats += instance_stats;

    LOG_INFO("Instance: ", instance_name, '\n');
    LOG_INFO('\n', instance_stats, '\n');
  }

  auto total_stats = print_timer_table("Total", total_timers, labels, false);
  LOG_INFO('\n', std::string(80, '='), '\n', stats, std::string(80, '='), '\n', total_stats);

  return 0;
}
