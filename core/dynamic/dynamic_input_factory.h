#pragma once

#include "core/dynamic/dynamic_input.h"
#include "core/model/mass_properties_input.h"

namespace engine::dynamic
{

class DynamicInputFactory
{
public:
    static DynamicInput Create(const MassPropertiesInput& input);
};

} // namespace engine::dynamic