#pragma once

#include <vector>

namespace engine::balancing
{

struct BalancingInput
{
    // Углы поворота главного коленчатого вала, градусы.
    // Должны совпадать с разверткой, по которой позже будет сравнение с динамикой.
    std::vector<double> alphaDeg;

    // Частота вращения главного коленчатого вала.
    double rpm = 0.0;

    // Точка, относительно которой считаются моменты.
    double referenceX_M = 0.0;
    double referenceY_M = 0.0;
    double referenceZ_M = 0.0;
};

} // namespace engine::balancing