#pragma once

#include <vector>

#include "core/kinematic/normalized_kinematic_model.h"

namespace engine::kinematic
{

struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct CylinderKinematicSeries
{
    int cylinderNumber = 0;
    int shaftNumber = 0;
    int crankNumber = 0;

    CylinderLinkType linkType = CylinderLinkType::Main;

    std::vector<double> displacementM;
    std::vector<double> velocityMps;
    std::vector<double> accelerationMps2;

    std::vector<double> accelerationFirstOrderMps2;
    std::vector<double> accelerationSecondOrderMps2;

    std::vector<double> rodAngleRad;
    std::vector<double> rodAngularVelocityRadS;
    std::vector<double> rodAngularAccelerationRadS2;

    Vec3 cylinderAxisUnitGlobal;
    std::vector<Vec3> forceLinePointGlobalM;

    // Для центробежной силы вращающихся масс
    std::vector<Vec3> crankPinPointGlobalM;
    std::vector<Vec3> crankRadiusVectorGlobalM;
};

struct KinematicResult
{
    double rpm = 0.0;

    std::vector<double> alphaDeg;
    std::vector<CylinderKinematicSeries> cylinders;
};

} // namespace engine::kinematic