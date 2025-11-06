// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <filesystem>
#include <string>

#include "instance.hpp"

namespace fs = std::filesystem;

// returns a stable string with the canonical/absolute path of the root directory
inline std::string file_parent_dir(const std::string file_path, unsigned levels) {
  fs::path p(file_path);
  try {
    p = fs::canonical(p);
  } catch (...) {
    if (!p.is_absolute()) {
      p = fs::absolute(p);
    }
  }
  for (unsigned i = 0; i < levels && p.has_parent_path(); ++i) {
    p = p.parent_path();
  }
  return p.string();
}

class InstanceReader {
  public:
    InstanceReader(std::string file_path);

    [[nodiscard]] Instance read() const;

    [[nodiscard]] static std::map<size_t, double> read_duals(const std::string& duals_file_path);

  private:
    std::string file_path_;
};
