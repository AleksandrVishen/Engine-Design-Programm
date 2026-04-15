#pragma once

#include "core/kinematic/normalized_kinematic_model.h"
#include "core/kinematic/kinematic_result.h"

namespace engine::kinematic
{

class KinematicSolver
{
public:
    static KinematicResult Solve(const NormalizedKinematicModel& model);
};

} // namespace engine::kinematic