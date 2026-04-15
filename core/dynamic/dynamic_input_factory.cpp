#include "core/dynamic/dynamic_input_factory.h"

namespace engine::dynamic
{

namespace
{

double MmToM(double valueMm)
{
    return valueMm / 1000.0;
}

} // namespace

DynamicInput DynamicInputFactory::Create(const MassPropertiesInput& input)
{
    DynamicInput dynInput;
    dynInput.reciprocatingMassKg = input.reciprocatingMassKg;
    dynInput.rotatingMassKg = input.rotatingMassKg;

    dynInput.referenceX_M = MmToM(input.referenceXmm);
    dynInput.referenceY_M = MmToM(input.referenceYmm);
    dynInput.referenceZ_M = MmToM(input.referenceZmm);

    return dynInput;
}

} // namespace engine::dynamic