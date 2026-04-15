#include "core/balancing/balancing_calculator.h"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>

namespace engine::balancing
{

namespace
{

constexpr double kPi = 3.14159265358979323846;

struct CrankGlobalGeometry
{
    double originX_M = 0.0;
    double originY_M = 0.0;

    double leftCheekZ_M = 0.0;
    double rightCheekZ_M = 0.0;
};

double DegToRad(double deg)
{
    return deg * kPi / 180.0;
}

double MmToM(double mm)
{
    return mm * 1e-3;
}

double RpmToOmega(double rpm)
{
    return 2.0 * kPi * rpm / 60.0;
}

Vec3 MakeVec3(double x, double y, double z)
{
    return {x, y, z};
}

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(const Vec3& a, const Vec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Scale(const Vec3& v, double s)
{
    return {v.x * s, v.y * s, v.z * s};
}

Vec3 Cross(const Vec3& a, const Vec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

bool HasIntermediateMainJournal(SupportType supportType, int leftCrankIndexZeroBased)
{
    if (supportType == SupportType::FullySupported)
    {
        return true;
    }

    return (leftCrankIndexZeroBased % 2) == 1;
}

CrankGlobalGeometry ComputeCrankGlobalGeometry(
    const EngineModel& sourceModel,
    std::size_t shaftIndex,
    std::size_t crankIndexWithinShaft)
{
    const auto& shaft = sourceModel.shafts[shaftIndex];
    const auto& kin = sourceModel.kinematic;

    CrankGlobalGeometry geometry;
    geometry.originX_M = MmToM(shaft.originXMm);
    geometry.originY_M = MmToM(shaft.originYMm);

    const double shaftOriginZ_M = MmToM(shaft.originZMm);

    double cursorZ_M = 0.5 * kin.mainJournalLengthM;

    for (std::size_t i = 0; i < shaft.cranks.size(); ++i)
    {
        cursorZ_M += kin.webThicknessM;

        const double pinStartZ_M = cursorZ_M;
        const double pinEndZ_M = pinStartZ_M + kin.rodJournalLengthM;

        const double leftCheekCenterZ_M = pinStartZ_M - 0.5 * kin.webThicknessM;
        const double rightCheekCenterZ_M = pinEndZ_M + 0.5 * kin.webThicknessM;

        if (i == crankIndexWithinShaft)
        {
            geometry.leftCheekZ_M = shaftOriginZ_M + leftCheekCenterZ_M;
            geometry.rightCheekZ_M = shaftOriginZ_M + rightCheekCenterZ_M;
            return geometry;
        }

        cursorZ_M = pinEndZ_M;
        cursorZ_M += kin.webThicknessM;

        if (i < shaft.cranks.size() - 1 && HasIntermediateMainJournal(kin.supportType, static_cast<int>(i)))
        {
            cursorZ_M += kin.mainJournalLengthM;
        }
    }

    throw std::runtime_error("Не удалось вычислить геометрию кривошипа.");
}

int SpeedRatioValue(BalancerSpeedRatio ratio)
{
    switch (ratio)
    {
    case BalancerSpeedRatio::Plus1W:
        return 1;
    case BalancerSpeedRatio::Plus2W:
        return 2;
    case BalancerSpeedRatio::Minus1W:
        return -1;
    case BalancerSpeedRatio::Minus2W:
        return -2;
    default:
        return 1;
    }
}

Vec3 MakeCounterweightRadiusVectorForCrank(double radiusM, double alphaDeg, double phaseDeg)
{
    const double theta = DegToRad(alphaDeg + phaseDeg + 180.0);

    return {
        radiusM * std::sin(theta),
        radiusM * std::cos(theta),
        0.0
    };
}

Vec3 MakeBalancerRadiusVector(
    BalancerAxis axis,
    double radiusM,
    double alphaDeg,
    double shaftPhaseDeg,
    double counterweightPhaseDeg,
    BalancerSpeedRatio speedRatio)
{
    const int ratio = SpeedRatioValue(speedRatio);
    const double theta = DegToRad(static_cast<double>(ratio) * alphaDeg +
                                  shaftPhaseDeg +
                                  counterweightPhaseDeg);

    switch (axis)
    {
    case BalancerAxis::X:
        return {
            0.0,
            radiusM * std::cos(theta),
            radiusM * std::sin(theta)
        };

    case BalancerAxis::Y:
        return {
            radiusM * std::sin(theta),
            0.0,
            radiusM * std::cos(theta)
        };

    case BalancerAxis::Z:
    default:
        return {
            radiusM * std::sin(theta),
            radiusM * std::cos(theta),
            0.0
        };
    }
}

Vec3 MakeCentrifugalForce(const Vec3& radiusVectorM, double massKg, double omegaRadPerSec)
{
    return Scale(radiusVectorM, massKg * omegaRadPerSec * omegaRadPerSec);
}

Vec3 ComputeMomentFromReference(const Vec3& centerGlobalM,
                                const Vec3& forceN,
                                const Vec3& referencePointM)
{
    const Vec3 arm = Sub(centerGlobalM, referencePointM);
    return Cross(arm, forceN);
}

std::unordered_map<long long, CrankGlobalGeometry> BuildCrankGeometryMap(const EngineModel& sourceModel)
{
    std::unordered_map<long long, CrankGlobalGeometry> map;

    for (std::size_t shaftIndex = 0; shaftIndex < sourceModel.shafts.size(); ++shaftIndex)
    {
        const auto& shaft = sourceModel.shafts[shaftIndex];

        for (std::size_t crankIndex = 0; crankIndex < shaft.cranks.size(); ++crankIndex)
        {
            const auto& crank = shaft.cranks[crankIndex];
            const long long key =
                (static_cast<long long>(shaft.shaftNumber) << 32) |
                static_cast<unsigned int>(crank.crankNumber);

            map.emplace(key, ComputeCrankGlobalGeometry(sourceModel, shaftIndex, crankIndex));
        }
    }

    return map;
}

long long MakeCrankKey(int shaftNumber, int crankNumber)
{
    return (static_cast<long long>(shaftNumber) << 32) |
           static_cast<unsigned int>(crankNumber);
}

} // namespace

BalancingResult BalancingCalculator::Calculate(const EngineModel& sourceModel,
                                               const NormalizedBalancingModel& model,
                                               const BalancingInput& input) const
{
    BalancingResult result;
    result.alphaDeg = input.alphaDeg;

    const std::size_t sampleCount = input.alphaDeg.size();

    result.crankCounterweightCentrifugalForce.assign(sampleCount, {});
    result.crankCounterweightCentrifugalMoment.assign(sampleCount, {});

    result.balancerInertiaForce1.assign(sampleCount, {});
    result.balancerInertiaForce2.assign(sampleCount, {});
    result.balancerInertiaMoment1.assign(sampleCount, {});
    result.balancerInertiaMoment2.assign(sampleCount, {});

    result.totalInertiaForce.assign(sampleCount, {});
    result.totalInertiaMoment.assign(sampleCount, {});
    result.totalCentrifugalForce.assign(sampleCount, {});
    result.totalCentrifugalMoment.assign(sampleCount, {});

    const double omegaMain = RpmToOmega(input.rpm);
    const Vec3 referencePointM =
        MakeVec3(input.referenceX_M, input.referenceY_M, input.referenceZ_M);

    const auto crankGeometryMap = BuildCrankGeometryMap(sourceModel);

    result.crankCounterweights.reserve(model.crankCounterweights.size());
    for (const auto& cw : model.crankCounterweights)
    {
        CrankCounterweightSeries series;
        series.shaftNumber = cw.shaftNumber;
        series.crankNumber = cw.crankNumber;
        series.count = cw.count;
        series.side = cw.side;
        series.centrifugalForce.assign(sampleCount, {});
        series.centrifugalMoment.assign(sampleCount, {});

        const auto geometryIt = crankGeometryMap.find(MakeCrankKey(cw.shaftNumber, cw.crankNumber));
        if (geometryIt == crankGeometryMap.end())
        {
            throw std::runtime_error("Не найдена геометрия кривошипа для противовеса.");
        }

        const CrankGlobalGeometry& geometry = geometryIt->second;
        const double radiusM = MmToM(cw.radiusMm);

        for (std::size_t i = 0; i < sampleCount; ++i)
        {
            const double alphaDeg = input.alphaDeg[i];
            const Vec3 radiusVectorM =
                MakeCounterweightRadiusVectorForCrank(radiusM, alphaDeg, cw.phaseDeg);

            Vec3 totalForceAtAngle{};
            Vec3 totalMomentAtAngle{};

            if (cw.count == 1)
            {
                const double z_M =
                    (cw.side == CrankCounterweightSide::Left)
                        ? geometry.leftCheekZ_M
                        : geometry.rightCheekZ_M;

                const Vec3 centerGlobalM = MakeVec3(
                    geometry.originX_M + radiusVectorM.x,
                    geometry.originY_M + radiusVectorM.y,
                    z_M);

                const Vec3 forceN = MakeCentrifugalForce(radiusVectorM, cw.massKg, omegaMain);
                const Vec3 momentNm = ComputeMomentFromReference(centerGlobalM, forceN, referencePointM);

                totalForceAtAngle = Add(totalForceAtAngle, forceN);
                totalMomentAtAngle = Add(totalMomentAtAngle, momentNm);
            }
            else if (cw.count == 2)
            {
                const Vec3 leftCenterGlobalM = MakeVec3(
                    geometry.originX_M + radiusVectorM.x,
                    geometry.originY_M + radiusVectorM.y,
                    geometry.leftCheekZ_M);

                const Vec3 rightCenterGlobalM = MakeVec3(
                    geometry.originX_M + radiusVectorM.x,
                    geometry.originY_M + radiusVectorM.y,
                    geometry.rightCheekZ_M);

                const Vec3 leftForceN = MakeCentrifugalForce(radiusVectorM, cw.massKg, omegaMain);
                const Vec3 rightForceN = MakeCentrifugalForce(radiusVectorM, cw.massKg, omegaMain);

                const Vec3 leftMomentNm =
                    ComputeMomentFromReference(leftCenterGlobalM, leftForceN, referencePointM);
                const Vec3 rightMomentNm =
                    ComputeMomentFromReference(rightCenterGlobalM, rightForceN, referencePointM);

                totalForceAtAngle = Add(leftForceN, rightForceN);
                totalMomentAtAngle = Add(leftMomentNm, rightMomentNm);
            }

            series.centrifugalForce[i] = totalForceAtAngle;
            series.centrifugalMoment[i] = totalMomentAtAngle;

            result.crankCounterweightCentrifugalForce[i] =
                Add(result.crankCounterweightCentrifugalForce[i], totalForceAtAngle);
            result.crankCounterweightCentrifugalMoment[i] =
                Add(result.crankCounterweightCentrifugalMoment[i], totalMomentAtAngle);
        }

        result.crankCounterweights.push_back(std::move(series));
    }

    result.balancerCounterweights.reserve(model.balancerCounterweights.size());
    for (const auto& cw : model.balancerCounterweights)
    {
        BalancerCounterweightSeries series;
        series.shaftIndex = cw.shaftIndex;
        series.counterweightIndex = cw.counterweightIndex;
        series.axis = cw.axis;
        series.speedRatio = cw.speedRatio;
        series.inertiaForce.assign(sampleCount, {});
        series.inertiaMoment.assign(sampleCount, {});

        const double radiusM = MmToM(cw.radiusMm);
        const double baseOmega =
            static_cast<double>(std::abs(SpeedRatioValue(cw.speedRatio))) * omegaMain;

        for (std::size_t i = 0; i < sampleCount; ++i)
        {
            const double alphaDeg = input.alphaDeg[i];

            const Vec3 radiusVectorM = MakeBalancerRadiusVector(
                cw.axis,
                radiusM,
                alphaDeg,
                cw.shaftPhaseDeg,
                cw.phaseDeg,
                cw.speedRatio);

            const Vec3 centerGlobalM = MakeVec3(
                MmToM(cw.centerXmm) + radiusVectorM.x,
                MmToM(cw.centerYmm) + radiusVectorM.y,
                MmToM(cw.centerZmm) + radiusVectorM.z);

            const Vec3 forceN = MakeCentrifugalForce(radiusVectorM, - cw.massKg, baseOmega);
            const Vec3 momentNm = ComputeMomentFromReference(centerGlobalM, forceN, referencePointM);

            series.inertiaForce[i] = forceN;
            series.inertiaMoment[i] = momentNm;

            const int order = std::abs(SpeedRatioValue(cw.speedRatio));

            if (order == 1)
            {
                result.balancerInertiaForce1[i] =
                    Add(result.balancerInertiaForce1[i], forceN);
                result.balancerInertiaMoment1[i] =
                    Add(result.balancerInertiaMoment1[i], momentNm);
            }
            else if (order == 2)
            {
                result.balancerInertiaForce2[i] =
                    Add(result.balancerInertiaForce2[i], forceN);
                result.balancerInertiaMoment2[i] =
                    Add(result.balancerInertiaMoment2[i], momentNm);
            }
            else
            {
                throw std::runtime_error("Поддерживаются только дополнительные валы 1w и 2w.");
            }
        }

        result.balancerCounterweights.push_back(std::move(series));
    }

    for (std::size_t i = 0; i < sampleCount; ++i)
    {
        result.totalInertiaForce[i] = Add(
            result.balancerInertiaForce1[i],
            result.balancerInertiaForce2[i]);

        result.totalInertiaMoment[i] = Add(
            result.balancerInertiaMoment1[i],
            result.balancerInertiaMoment2[i]);

        result.totalCentrifugalForce[i] =
            result.crankCounterweightCentrifugalForce[i];

        result.totalCentrifugalMoment[i] =
            result.crankCounterweightCentrifugalMoment[i];
    }

    return result;
}

} // namespace engine::balancing