#pragma once

#include "core/balancing/balancing_input.h"
#include "core/kinematic/kinematic_result.h"
#include "core/model/mass_properties_input.h"

namespace engine::balancing
{

class BalancingInputFactory
{
public:
    static BalancingInput Create(const engine::kinematic::KinematicResult& kinematic,
                                 const MassPropertiesInput& input);
};

} // namespace engine::balancing