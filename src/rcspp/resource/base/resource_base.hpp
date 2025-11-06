// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <concepts>
#include <memory>
#include <utility>

namespace rcspp {

template <typename ResourceType>
class ResourceBase {
  public:
    ResourceBase() = default;

    virtual ~ResourceBase() = default;

    [[nodiscard]] virtual auto clone() const -> std::unique_ptr<ResourceType> {
      return std::make_unique<ResourceType>(static_cast<ResourceType const&>(*this));
    }

    virtual void reset() = 0;
};
}  // namespace rcspp
