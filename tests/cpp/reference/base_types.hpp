// The two lines of cpp/rcspp/algorithm/algorithm.hpp that define LabelIteratorPair, at 9c80f174,
// for the parity tests: the library drops the alias later. Do not edit.
// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <list>
#include <utility>

#include "rcspp/label/label.hpp"

namespace rcspp::reference {

template <typename ResourceType>
using LabelIteratorPair =
    std::pair<Label<ResourceType>*, typename std::list<Label<ResourceType>*>::iterator>;

}  // namespace rcspp::reference
