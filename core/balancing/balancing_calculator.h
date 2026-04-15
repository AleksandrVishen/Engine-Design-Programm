#pragma once

#include "core/balancing/balancing_input.h"
#include "core/balancing/balancing_result.h"
#include "core/balancing/normalized_balancing_model.h"
#include "core/model/engine_model.h"

namespace engine::balancing
{

class BalancingCalculator
{
public:
    BalancingResult Calculate(const EngineModel& sourceModel,
                              const NormalizedBalancingModel& model,
                              const BalancingInput& input) const;
};

} // namespace engine::balancing