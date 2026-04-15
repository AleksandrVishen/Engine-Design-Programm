#pragma once

#include <vector>

#include "core/model/balancing_model.h"

namespace engine::balancing
{

enum class CrankCounterweightSide
{
    Left = 0,
    Right,
    Both
};

struct NormalizedCrankCounterweight
{
    bool enabled = false;

    int shaftNumber = 0;
    int crankNumber = 0;

    double phaseDeg = 0.0;
    double massKg = 0.0;
    double radiusMm = 0.0;

    // Реальное число противовесов на данном кривошипе после нормализации.
    int count = 0;

    // Для count == 1: Left или Right.
    // Для count == 2: Both.
    CrankCounterweightSide side = CrankCounterweightSide::Both;
};

struct NormalizedBalancerCounterweight
{
    int shaftIndex = 0;
    int counterweightIndex = 0;

    double originXMm = 0.0;
    double originYMm = 0.0;
    double originZMm = 0.0;

    BalancerAxis axis = BalancerAxis::Z;
    double lengthMm = 0.0;

    BalancerSpeedRatio speedRatio = BalancerSpeedRatio::Plus1W;
    double shaftPhaseDeg = 0.0;

    double massKg = 0.0;
    double radiusMm = 0.0;

    // Абсолютное положение центра массы вдоль оси вала в глобальных координатах.
    double centerXmm = 0.0;
    double centerYmm = 0.0;
    double centerZmm = 0.0;

    double positionAlongShaftMm = 0.0;
    double phaseDeg = 0.0;
};

struct NormalizedBalancingModel
{
    std::vector<NormalizedCrankCounterweight> crankCounterweights;
    std::vector<NormalizedBalancerCounterweight> balancerCounterweights;
};

} // namespace engine::balancing