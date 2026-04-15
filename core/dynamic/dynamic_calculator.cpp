#include "core/dynamic/dynamic_calculator.h"

#include <cmath>
#include <set>
#include <stdexcept>
#include <utility>

namespace engine::dynamic
{
namespace
{

using Vec3 = engine::kinematic::Vec3;

Vec3 MakeVec3(double x, double y, double z)
{
    return { x, y, z };
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vec3 Sub(const Vec3& a, const Vec3& b)
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

Vec3 Scale(const Vec3& v, double s)
{
    return { v.x * s, v.y * s, v.z * s };
}

double Length(const Vec3& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

bool IsFinite(const Vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

double RpmToOmega(double rpm)
{
    return 2.0 * 3.14159265358979323846 * rpm / 60.0;
}

void ValidateInput(const engine::kinematic::KinematicResult& kinematic,
                   const DynamicInput& input)
{
    if (input.reciprocatingMassKg < 0.0)
        throw std::runtime_error("DynamicCalculator: reciprocating mass must be non-negative.");

    if (input.rotatingMassKg < 0.0)
        throw std::runtime_error("DynamicCalculator: rotating mass must be non-negative.");

    if (!std::isfinite(input.referenceX_M) ||
        !std::isfinite(input.referenceY_M) ||
        !std::isfinite(input.referenceZ_M))
    {
        throw std::runtime_error("DynamicCalculator: reference point contains non-finite values.");
    }

    if (!std::isfinite(kinematic.rpm) || kinematic.rpm < 0.0)
        throw std::runtime_error("DynamicCalculator: kinematic rpm must be finite and non-negative.");

    const std::size_t sampleCount = kinematic.alphaDeg.size();

    for (const auto& cylinder : kinematic.cylinders)
    {
        if (cylinder.accelerationMps2.size() != sampleCount)
            throw std::runtime_error("DynamicCalculator: acceleration series size mismatch.");

        if (cylinder.accelerationFirstOrderMps2.size() != sampleCount)
            throw std::runtime_error("DynamicCalculator: first-order acceleration series size mismatch.");

        if (cylinder.accelerationSecondOrderMps2.size() != sampleCount)
            throw std::runtime_error("DynamicCalculator: second-order acceleration series size mismatch.");

        if (cylinder.forceLinePointGlobalM.size() != sampleCount)
            throw std::runtime_error("DynamicCalculator: force line point series size mismatch.");

        if (cylinder.crankPinPointGlobalM.size() != sampleCount)
            throw std::runtime_error("DynamicCalculator: crank pin point series size mismatch.");

        if (cylinder.crankRadiusVectorGlobalM.size() != sampleCount)
            throw std::runtime_error("DynamicCalculator: crank radius vector series size mismatch.");

        if (!IsFinite(cylinder.cylinderAxisUnitGlobal))
            throw std::runtime_error("DynamicCalculator: cylinder axis contains non-finite values.");

        const double axisLength = Length(cylinder.cylinderAxisUnitGlobal);
        if (axisLength < 1e-12)
            throw std::runtime_error("DynamicCalculator: cylinder axis vector is zero.");

        if (std::abs(axisLength - 1.0) > 1e-6)
            throw std::runtime_error("DynamicCalculator: cylinder axis vector is not normalized.");
    }
}

} // namespace

DynamicResult DynamicCalculator::Calculate(
    const engine::kinematic::KinematicResult& kinematic,
    const DynamicInput& input) const
{
    ValidateInput(kinematic, input);

    DynamicResult result;
    result.alphaDeg = kinematic.alphaDeg;

    const std::size_t sampleCount = kinematic.alphaDeg.size();
    const Vec3 referencePoint = MakeVec3(input.referenceX_M, input.referenceY_M, input.referenceZ_M);

    const double omega = RpmToOmega(kinematic.rpm);
    const double omega2 = omega * omega;

    result.totalInertiaForce.assign(sampleCount, MakeVec3(0.0, 0.0, 0.0));
    result.totalInertiaForce1.assign(sampleCount, MakeVec3(0.0, 0.0, 0.0));
    result.totalInertiaForce2.assign(sampleCount, MakeVec3(0.0, 0.0, 0.0));

    result.totalInertiaMoment1.assign(sampleCount, MakeVec3(0.0, 0.0, 0.0));
    result.totalInertiaMoment2.assign(sampleCount, MakeVec3(0.0, 0.0, 0.0));

    result.totalCentrifugalForce.assign(sampleCount, MakeVec3(0.0, 0.0, 0.0));
    result.totalCentrifugalMoment.assign(sampleCount, MakeVec3(0.0, 0.0, 0.0));

    result.cylinders.reserve(kinematic.cylinders.size());

    std::set<std::pair<int, int>> processedCranks;

    for (const auto& kinCylinder : kinematic.cylinders)
    {
        CylinderDynamicSeries dynCylinder;
        dynCylinder.cylinderNumber = kinCylinder.cylinderNumber;
        dynCylinder.shaftNumber = kinCylinder.shaftNumber;
        dynCylinder.crankNumber = kinCylinder.crankNumber;
        dynCylinder.linkType = kinCylinder.linkType;

        dynCylinder.inertiaForce.reserve(sampleCount);
        dynCylinder.inertiaForce1.reserve(sampleCount);
        dynCylinder.inertiaForce2.reserve(sampleCount);

        dynCylinder.inertiaMoment1.reserve(sampleCount);
        dynCylinder.inertiaMoment2.reserve(sampleCount);

        dynCylinder.centrifugalForce.reserve(sampleCount);
        dynCylinder.centrifugalMoment.reserve(sampleCount);

        const Vec3 axis = kinCylinder.cylinderAxisUnitGlobal;
        const std::pair<int, int> crankKey{ kinCylinder.shaftNumber, kinCylinder.crankNumber };
        const bool isFirstCylinderForCrank = processedCranks.insert(crankKey).second;

        for (std::size_t i = 0; i < sampleCount; ++i)
        {
            const double a  = kinCylinder.accelerationMps2[i];
            const double a1 = kinCylinder.accelerationFirstOrderMps2[i];
            const double a2 = kinCylinder.accelerationSecondOrderMps2[i];

            // Поступательные массы — на цилиндр
            const Vec3 force  = Scale(axis, -input.reciprocatingMassKg * a);
            const Vec3 force1 = Scale(axis, -input.reciprocatingMassKg * a1);
            const Vec3 force2 = Scale(axis, -input.reciprocatingMassKg * a2);

            dynCylinder.inertiaForce.push_back(force);
            dynCylinder.inertiaForce1.push_back(force1);
            dynCylinder.inertiaForce2.push_back(force2);

            const Vec3 point = kinCylinder.forceLinePointGlobalM[i];
            const Vec3 radiusVector = Sub(point, referencePoint);

            const Vec3 moment1 = Cross(radiusVector, force1);
            const Vec3 moment2 = Cross(radiusVector, force2);

            dynCylinder.inertiaMoment1.push_back(moment1);
            dynCylinder.inertiaMoment2.push_back(moment2);

            result.totalInertiaForce[i]  = Add(result.totalInertiaForce[i],  force);
            result.totalInertiaForce1[i] = Add(result.totalInertiaForce1[i], force1);
            result.totalInertiaForce2[i] = Add(result.totalInertiaForce2[i], force2);

            result.totalInertiaMoment1[i] = Add(result.totalInertiaMoment1[i], moment1);
            result.totalInertiaMoment2[i] = Add(result.totalInertiaMoment2[i], moment2);

            // Вращающиеся массы — на кривошип, а не на цилиндр.
            if (isFirstCylinderForCrank)
            {
                const Vec3 centrifugalForce =
                    Scale(kinCylinder.crankRadiusVectorGlobalM[i], input.rotatingMassKg * omega2);

                dynCylinder.centrifugalForce.push_back(centrifugalForce);

                const Vec3 crankPinPoint = kinCylinder.crankPinPointGlobalM[i];
                const Vec3 crankPinRadiusToRef = Sub(crankPinPoint, referencePoint);

                const Vec3 centrifugalMoment = Cross(crankPinRadiusToRef, centrifugalForce);
                dynCylinder.centrifugalMoment.push_back(centrifugalMoment);

                result.totalCentrifugalForce[i] =
                    Add(result.totalCentrifugalForce[i], centrifugalForce);

                result.totalCentrifugalMoment[i] =
                    Add(result.totalCentrifugalMoment[i], centrifugalMoment);
            }
            else
            {
                dynCylinder.centrifugalForce.push_back(MakeVec3(0.0, 0.0, 0.0));
                dynCylinder.centrifugalMoment.push_back(MakeVec3(0.0, 0.0, 0.0));
            }
        }

        result.cylinders.push_back(std::move(dynCylinder));
    }

    return result;
}

} // namespace engine::dynamic