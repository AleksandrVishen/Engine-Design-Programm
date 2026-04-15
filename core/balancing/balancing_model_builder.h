#pragma once

#include <string>
#include <vector>

#include "core/balancing/normalized_balancing_model.h"
#include "core/model/engine_model.h"

namespace engine::balancing
{

struct BalancingBuildMessage
{
    std::string message;
};

struct BalancingBuildResult
{
    bool ok = false;
    NormalizedBalancingModel model;

    std::vector<BalancingBuildMessage> errors;
    std::vector<BalancingBuildMessage> warnings;
};

class BalancingModelBuilder
{
public:
    static BalancingBuildResult Build(const EngineModel& source);
};

} // namespace engine::balancing