#pragma once

#include <vector>

#include "core/balancing/balancing_result.h"
#include "core/dynamic/dynamic_result.h"

namespace engine::balancing
{

using Vec3 = engine::kinematic::Vec3;

struct BalancingComposedResult
{
    std::vector<double> alphaDeg;

    // Исходные силы и моменты двигателя без противовесов
    std::vector<Vec3> sourceInertiaForce;
    std::vector<Vec3> sourceInertiaForce1;
    std::vector<Vec3> sourceInertiaForce2;

    std::vector<Vec3> sourceInertiaMoment1;
    std::vector<Vec3> sourceInertiaMoment2;

    std::vector<Vec3> sourceCentrifugalForce;
    std::vector<Vec3> sourceCentrifugalMoment;

    // Вклад противовесов
    std::vector<Vec3> crankCounterweightCentrifugalForce;
    std::vector<Vec3> crankCounterweightCentrifugalMoment;

    std::vector<Vec3> balancerInertiaForce1;
    std::vector<Vec3> balancerInertiaForce2;
    std::vector<Vec3> balancerInertiaMoment1;
    std::vector<Vec3> balancerInertiaMoment2;

    std::vector<Vec3> counterweightInertiaForce;
    std::vector<Vec3> counterweightInertiaMoment;
    std::vector<Vec3> counterweightCentrifugalForce;
    std::vector<Vec3> counterweightCentrifugalMoment;

    // Остаточные величины после установки противовесов
    std::vector<Vec3> residualInertiaForce1;
    std::vector<Vec3> residualInertiaForce2;
    std::vector<Vec3> residualInertiaForce;

    std::vector<Vec3> residualInertiaMoment1;
    std::vector<Vec3> residualInertiaMoment2;
    std::vector<Vec3> residualInertiaMoment;

    std::vector<Vec3> residualCentrifugalForce;
    std::vector<Vec3> residualCentrifugalMoment;

    // Полные итоги после уравновешивания
    std::vector<Vec3> totalForce;
    std::vector<Vec3> totalMoment;
};

} // namespace engine::balancing