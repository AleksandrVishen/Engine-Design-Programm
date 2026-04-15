#pragma once

namespace engine::dynamic
{

struct DynamicInput
{
    double reciprocatingMassKg = 0.0;
    double rotatingMassKg = 0.0;

    double referenceX_M = 0.0;
    double referenceY_M = 0.0;
    double referenceZ_M = 0.0;
};

} // namespace engine::dynamic