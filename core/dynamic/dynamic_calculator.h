#pragma once

#include "core/dynamic/dynamic_input.h"
#include "core/dynamic/dynamic_result.h"
#include "core/kinematic/kinematic_result.h"

namespace engine::dynamic
{

class DynamicCalculator
{
public:
    DynamicResult Calculate(const engine::kinematic::KinematicResult& kinematic,
                            const DynamicInput& input) const;
};

} // namespace engine::dynamic