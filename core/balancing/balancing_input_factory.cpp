#include "core/balancing/balancing_input_factory.h"

namespace engine::balancing
{

namespace
{

double MmToM(double valueMm)
{
    return valueMm / 1000.0;
}

} // namespace

BalancingInput BalancingInputFactory::Create(
    const engine::kinematic::KinematicResult& kinematic,
    const MassPropertiesInput& input)
{
    BalancingInput balancingInput;
    balancingInput.alphaDeg = kinematic.alphaDeg;
    balancingInput.rpm = kinematic.rpm;

    balancingInput.referenceX_M = MmToM(input.referenceXmm);
    balancingInput.referenceY_M = MmToM(input.referenceYmm);
    balancingInput.referenceZ_M = MmToM(input.referenceZmm);

    return balancingInput;
}

} // namespace engine::balancing