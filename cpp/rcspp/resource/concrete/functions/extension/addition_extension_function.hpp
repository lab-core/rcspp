// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <algorithm>
#include <optional>

#include "rcspp/general/clonable.hpp"
#include "rcspp/resource/base/extender.hpp"
#include "rcspp/resource/functions/extension/extension_function.hpp"

namespace rcspp {

template <typename ResourceType>
class AdditionExtensionFunction
    : public Clonable<AdditionExtensionFunction<ResourceType>, ExtensionFunction<ResourceType>> {
        using ValueType = std::decay_t<decltype(std::declval<ResourceType>().get_value())>;

    public:
        explicit AdditionExtensionFunction(std::optional<ValueType> min_value = std::nullopt)
            : min_value_(min_value) {}

        void extend(const ResourceType& resource, const ResourceType& extender_value,
                    ResourceType* extended_resource) override {
            auto sum_value = resource.get_value() + extender_value.get_value();
            if (min_value_.has_value()) {
                sum_value = std::max(min_value_.value(), sum_value);  // GCOVR_EXCL_LINE
            }
            extended_resource->set_value(sum_value);
        }

    private:
        std::optional<ValueType> min_value_;
};
}  // namespace rcspp
