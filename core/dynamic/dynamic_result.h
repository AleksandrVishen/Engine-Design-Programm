#pragma once

#include <vector>

#include "core/kinematic/kinematic_result.h"

namespace engine::dynamic
{

using Vec3 = engine::kinematic::Vec3;

struct CylinderDynamicSeries
{
    int cylinderNumber = 0;
    int shaftNumber = 0;
    int crankNumber = 0;

    engine::kinematic::CylinderLinkType linkType =
        engine::kinematic::CylinderLinkType::Main;

    // Поступательно движущиеся массы
    std::vector<Vec3> inertiaForce;
    std::vector<Vec3> inertiaForce1;
    std::vector<Vec3> inertiaForce2;

    std::vector<Vec3> inertiaMoment1;
    std::vector<Vec3> inertiaMoment2;

    // Вращающиеся массы
    std::vector<Vec3> centrifugalForce;
    std::vector<Vec3> centrifugalMoment;
};

struct DynamicResult
{
    std::vector<double> alphaDeg;
    std::vector<CylinderDynamicSeries> cylinders;

    std::vector<Vec3> totalInertiaForce;
    std::vector<Vec3> totalInertiaForce1;
    std::vector<Vec3> totalInertiaForce2;

    std::vector<Vec3> totalInertiaMoment1;
    std::vector<Vec3> totalInertiaMoment2;

    std::vector<Vec3> totalCentrifugalForce;
    std::vector<Vec3> totalCentrifugalMoment;
};

} // namespace engine::dynamic