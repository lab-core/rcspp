// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType>
class AdditionExtensionFunction
    : public Clonable<AdditionExtensionFunction<ResourceType>, ExtensionFunction<ResourceType>> {
        using ValueType =
            std::decay_t<decltype(std::declval<Resource<ResourceType>>().get_value())>;

    public:
        explicit AdditionExtensionFunction(std::optional<ValueType> min_value = std::nullopt)
            : min_value_(min_value) {}

        void extend(const Resource<ResourceType>& resource, const Extender<ResourceType>& extender,
                    Resource<ResourceType>* extended_resource) override {
            auto sum_value = resource.get_value() + extender.get_value();
            if (min_value_.has_value()) {
                sum_value = std::max(min_value_.value(), sum_value);
            }
            extended_resource->set_value(sum_value);
        }

    private:
        std::optional<ValueType> min_value_;
};
}  // namespace rcspp
