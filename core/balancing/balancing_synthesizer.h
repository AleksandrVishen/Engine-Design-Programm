#pragma once

#include "core/balancing/balancing_synthesis.h"
#include "core/dynamic/dynamic_result.h"
#include "core/model/mass_properties_input.h"

namespace engine::balancing
{

class BalancingSynthesizer
{
public:
    BalancingSynthesisResult Generate(
        const EngineModel& sourceModel,
        const engine::dynamic::DynamicResult& dynamicResult,
        const MassPropertiesInput& massInput,
        BalancingSynthesisGoalKind goal,
        const BalancingSynthesisConstraints& constraints = {}) const;
};

} // namespace engine::balancing