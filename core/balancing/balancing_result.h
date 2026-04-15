#pragma once

#include <vector>

#include "core/balancing/normalized_balancing_model.h"
#include "core/kinematic/kinematic_result.h"

namespace engine::balancing
{

using Vec3 = engine::kinematic::Vec3;

struct CrankCounterweightSeries
{
    int shaftNumber = 0;
    int crankNumber = 0;

    int count = 0;
    CrankCounterweightSide side = CrankCounterweightSide::Both;

    std::vector<Vec3> centrifugalForce;
    std::vector<Vec3> centrifugalMoment;
};

struct BalancerCounterweightSeries
{
    int shaftIndex = 0;
    int counterweightIndex = 0;

    BalancerAxis axis = BalancerAxis::Z;
    BalancerSpeedRatio speedRatio = BalancerSpeedRatio::Plus1W;

    std::vector<Vec3> inertiaForce;
    std::vector<Vec3> inertiaMoment;
};

struct BalancingResult
{
    std::vector<double> alphaDeg;

    std::vector<CrankCounterweightSeries> crankCounterweights;
    std::vector<BalancerCounterweightSeries> balancerCounterweights;

    // Вклад противовесов на продолжении щек коленчатого вала
    std::vector<Vec3> crankCounterweightCentrifugalForce;
    std::vector<Vec3> crankCounterweightCentrifugalMoment;

    // Вклад противовесов на дополнительных валах в уравновешивание поступательных масс
    std::vector<Vec3> balancerInertiaForce1;
    std::vector<Vec3> balancerInertiaForce2;
    std::vector<Vec3> balancerInertiaMoment1;
    std::vector<Vec3> balancerInertiaMoment2;

    // Суммарные вклады противовесов по каналам
    std::vector<Vec3> totalInertiaForce;
    std::vector<Vec3> totalInertiaMoment;
    std::vector<Vec3> totalCentrifugalForce;
    std::vector<Vec3> totalCentrifugalMoment;
};

} // namespace engine::balancing