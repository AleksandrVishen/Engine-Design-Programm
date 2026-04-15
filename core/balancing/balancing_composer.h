#pragma once

#include "core/balancing/balancing_composed_result.h"
#include "core/balancing/balancing_result.h"
#include "core/dynamic/dynamic_result.h"

namespace engine::balancing
{

class BalancingComposer
{
public:
    BalancingComposedResult Compose(const engine::dynamic::DynamicResult& dynamicResult,
                                    const BalancingResult& balancingResult) const;
};

} // namespace engine::balancing