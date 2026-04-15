#pragma once

#include <string>
#include <vector>

#include "core/balancing/balancing_composed_result.h"
#include "core/balancing/balancing_input.h"
#include "core/balancing/balancing_model_builder.h"
#include "core/balancing/balancing_result.h"
#include "core/dynamic/dynamic_result.h"
#include "core/model/engine_model.h"

namespace engine::balancing
{

struct BalancingPipelineMessage
{
    std::string message;
};

struct BalancingPipelineResult
{
    bool ok = false;

    NormalizedBalancingModel normalizedModel;
    BalancingResult balancingResult;
    BalancingComposedResult composedResult;

    std::vector<BalancingPipelineMessage> errors;
    std::vector<BalancingPipelineMessage> warnings;
};

class BalancingPipeline
{
public:
    BalancingPipelineResult Run(const EngineModel& sourceModel,
                                const engine::dynamic::DynamicResult& dynamicResult,
                                const BalancingInput& input) const;
};

} // namespace engine::balancing