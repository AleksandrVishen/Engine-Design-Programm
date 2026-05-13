#include "core/kinematic/kinematic_solver.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace engine::kinematic
{

namespace
{
constexpr double kPi = 3.14159265358979323846;

struct Vec2
{
    double x = 0.0;
    double y = 0.0;
};

engine::kinematic::Vec3 ToVec3(const Vec2& v)
{
    return { v.x, v.y, 0.0 };
}

bool HasIntermediateMainJournal(SupportType supportType, int leftCrankIndexZeroBased)
{
    if (supportType == SupportType::FullySupported)
        return true;

    return (leftCrankIndexZeroBased % 2) == 1;
}

double ComputeCrankPinCenterZ_M(double shaftOriginZMm,
                                double mainJournalLengthM,
                                double rodJournalLengthM,
                                double webThicknessM,
                                SupportType supportType,
                                int crankCount,
                                int crankNumber)
{
    const double shaftOriginZ_M = shaftOriginZMm / 1000.0;

    double cursorZ_M = 0.5 * mainJournalLengthM;

    for (int i = 0; i < crankCount; ++i)
    {
        cursorZ_M += webThicknessM;

        const double pinStartZ_M = cursorZ_M;
        const double pinEndZ_M = pinStartZ_M + rodJournalLengthM;
        const double pinCenterZ_M = 0.5 * (pinStartZ_M + pinEndZ_M);

        if ((i + 1) == crankNumber)
            return shaftOriginZ_M + pinCenterZ_M;

        cursorZ_M = pinEndZ_M;
        cursorZ_M += webThicknessM;

        if (i < crankCount - 1 && HasIntermediateMainJournal(supportType, i))
            cursorZ_M += mainJournalLengthM;
    }

    return shaftOriginZ_M;
}

struct HarmonicDecomposition
{
    std::vector<double> firstOrder;
    std::vector<double> secondOrder;
};

struct MainMechanismInstant
{
    Vec2 crankPin;
    Vec2 mainPistonPin;
    Vec2 mainRodDir;
    double mainAxisCoordinate = 0.0;
};

struct SliderCrankAnalyticalInstant
{
    double displacementM = 0.0;
    double velocityMps = 0.0;
    double accelerationMps2 = 0.0;
    double accelerationFirstOrderMps2 = 0.0;
    double accelerationSecondOrderMps2 = 0.0;
    double rodAngleRad = 0.0;
    double rodAngularVelocityRadS = 0.0;
    double rodAngularAccelerationRadS2 = 0.0;
};

struct SeriesBuffers
{
    std::vector<double> axisCoordinates;
    std::vector<double> displacements;
    std::vector<double> velocities;
    std::vector<double> accelerations;

    std::vector<double> accelerationsFirstOrder;
    std::vector<double> accelerationsSecondOrder;

    std::vector<double> rodAngles;
    std::vector<double> rodAngularVelocities;
    std::vector<double> rodAngularAccelerations;

    engine::kinematic::Vec3 cylinderAxisUnitGlobal;
    std::vector<engine::kinematic::Vec3> forceLinePointGlobalM;

    std::vector<engine::kinematic::Vec3> crankPinPointGlobalM;
    std::vector<engine::kinematic::Vec3> crankRadiusVectorGlobalM;
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

bool IsCenteredMainRodAnalyticalCase(
    double crankRadiusM,
    double rodLengthM,
    double deaxialMm)
{
    constexpr double kDeaxialEpsM = 1e-9;

    if (crankRadiusM <= 0.0 || rodLengthM <= 0.0)
        return false;

    const double lambda = crankRadiusM / rodLengthM;
    if (lambda <= 0.0 || lambda >= 1.0)
        return false;

    return std::abs(MmToM(deaxialMm)) <= kDeaxialEpsM;
}

SliderCrankAnalyticalInstant ComputeCenteredSliderCrankAnalyticalInstant(
    double alphaDeg,
    double phaseDeg,
    double axisTiltDeg,
    double crankRadiusM,
    double rodLengthM,
    double rpm)
{
    SliderCrankAnalyticalInstant instant;

    if (crankRadiusM <= 0.0 || rodLengthM <= 0.0)
        return instant;

    const double lambda = crankRadiusM / rodLengthM;
    if (lambda <= 0.0 || lambda >= 1.0)
        return instant;

    constexpr double kEps = 1e-12;
    const double theta = DegToRad(alphaDeg + phaseDeg - axisTiltDeg);
    const double omega = RpmToOmega(rpm);
    const double omega2 = omega * omega;

    const double sinTheta = std::sin(theta);
    const double cosTheta = std::cos(theta);
    const double sin2Theta = std::sin(2.0 * theta);
    const double cos2Theta = std::cos(2.0 * theta);

    instant.displacementM = crankRadiusM *
        ((1.0 - cosTheta) + (lambda * 0.25) * (1.0 - cos2Theta));
    instant.velocityMps = crankRadiusM * omega *
        (sinTheta + 0.5 * lambda * sin2Theta);
    instant.accelerationFirstOrderMps2 = crankRadiusM * omega2 * cosTheta;
    instant.accelerationSecondOrderMps2 = crankRadiusM * omega2 * lambda * cos2Theta;
    instant.accelerationMps2 =
        instant.accelerationFirstOrderMps2 + instant.accelerationSecondOrderMps2;

    const double asinArg = std::clamp(lambda * sinTheta, -1.0, 1.0);
    const double denom2 = std::max(kEps, 1.0 - asinArg * asinArg);
    const double denom = std::sqrt(denom2);
    const double denom32 = denom2 * denom;

    instant.rodAngleRad = std::asin(asinArg);
    instant.rodAngularVelocityRadS = omega * lambda * cosTheta / denom;
    instant.rodAngularAccelerationRadS2 = omega2 *
        ((-lambda * sinTheta * (1.0 - lambda * lambda * sinTheta * sinTheta)) +
         (lambda * lambda * lambda * sinTheta * cosTheta * cosTheta)) / denom32;

    if (!std::isfinite(instant.displacementM) ||
        !std::isfinite(instant.velocityMps) ||
        !std::isfinite(instant.accelerationMps2) ||
        !std::isfinite(instant.accelerationFirstOrderMps2) ||
        !std::isfinite(instant.accelerationSecondOrderMps2) ||
        !std::isfinite(instant.rodAngleRad) ||
        !std::isfinite(instant.rodAngularVelocityRadS) ||
        !std::isfinite(instant.rodAngularAccelerationRadS2))
    {
        return SliderCrankAnalyticalInstant{};
    }

    return instant;
}

double NormalizeAngleDiff(double angle)
{
    while (angle > kPi)
        angle -= 2.0 * kPi;
    while (angle < -kPi)
        angle += 2.0 * kPi;
    return angle;
}

Vec2 operator+(const Vec2& a, const Vec2& b)
{
    return { a.x + b.x, a.y + b.y };
}

Vec2 operator-(const Vec2& a, const Vec2& b)
{
    return { a.x - b.x, a.y - b.y };
}

Vec2 operator*(const Vec2& v, double s)
{
    return { v.x * s, v.y * s };
}

Vec2 operator*(double s, const Vec2& v)
{
    return { v.x * s, v.y * s };
}

double Dot(const Vec2& a, const Vec2& b)
{
    return a.x * b.x + a.y * b.y;
}

double CrossZ(const Vec2& a, const Vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

double Length(const Vec2& v)
{
    return std::sqrt(Dot(v, v));
}

Vec2 Normalize(const Vec2& v)
{
    const double len = Length(v);
    if (len <= std::numeric_limits<double>::epsilon())
        return { 0.0, 1.0 };

    return { v.x / len, v.y / len };
}

Vec2 MakeAxisUnit(double axisTiltDeg)
{
    const double phi = DegToRad(axisTiltDeg);
    return { std::sin(phi), std::cos(phi) };
}

Vec2 MakeAxisNormal(double axisTiltDeg)
{
    const double phi = DegToRad(axisTiltDeg);
    return { std::cos(phi), -std::sin(phi) };
}

Vec2 ComputeCrankPinPoint(
    double alphaDeg,
    double phaseDeg,
    double crankRadiusM)
{
    const double theta = DegToRad(alphaDeg + phaseDeg);
    return {
        crankRadiusM * std::sin(theta),
        crankRadiusM * std::cos(theta)
    };
}

double ComputeAxisCoordinateFromPoint(
    const Vec2& basePoint,
    double rodLengthM,
    double axisTiltDeg,
    double deaxialMm)
{
    const double l = rodLengthM;
    const double e = MmToM(deaxialMm);

    const Vec2 u = MakeAxisUnit(axisTiltDeg);
    const Vec2 n = MakeAxisNormal(axisTiltDeg);

    const double ax = basePoint.x - e * n.x;
    const double ay = basePoint.y - e * n.y;

    const double a_u = ax * u.x + ay * u.y;
    const double a_n = ax * n.x + ay * n.y;

    const double underRoot = l * l - a_n * a_n;
    const double rootValue = (underRoot > 0.0) ? std::sqrt(underRoot) : 0.0;

    return a_u + rootValue;
}

Vec2 ComputePistonPinPointFromAxisCoordinate(
    double axisCoordinate,
    double axisTiltDeg,
    double deaxialMm)
{
    const double e = MmToM(deaxialMm);
    const Vec2 u = MakeAxisUnit(axisTiltDeg);
    const Vec2 n = MakeAxisNormal(axisTiltDeg);

    return n * e + u * axisCoordinate;
}

double ComputeRodAngleFromPoints(
    const Vec2& basePoint,
    const Vec2& pistonPinPoint,
    double axisTiltDeg)
{
    const Vec2 rodVec = pistonPinPoint - basePoint;
    const Vec2 rodUnit = Normalize(rodVec);
    const Vec2 axisUnit = MakeAxisUnit(axisTiltDeg);

    return std::atan2(CrossZ(axisUnit, rodUnit), Dot(axisUnit, rodUnit));
}

double ComputeMainAxisCoordinate(
    double alphaDeg,
    double phaseDeg,
    double crankRadiusM,
    double rodLengthM,
    double axisTiltDeg,
    double deaxialMm)
{
    const Vec2 crankPin = ComputeCrankPinPoint(alphaDeg, phaseDeg, crankRadiusM);

    return ComputeAxisCoordinateFromPoint(
        crankPin,
        rodLengthM,
        axisTiltDeg,
        deaxialMm);
}

MainMechanismInstant ComputeMainMechanismInstant(
    double alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double deaxialMm)
{
    MainMechanismInstant instant;

    instant.crankPin = ComputeCrankPinPoint(alphaDeg, phaseDeg, mainCrankRadiusM);

    instant.mainAxisCoordinate = ComputeMainAxisCoordinate(
        alphaDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        deaxialMm);

    instant.mainPistonPin = ComputePistonPinPointFromAxisCoordinate(
        instant.mainAxisCoordinate,
        mainAxisTiltDeg,
        deaxialMm);

    instant.mainRodDir = Normalize(instant.mainPistonPin - instant.crankPin);
    return instant;
}

Vec2 ComputeArticulatedAttachPointFromMain(
    const MainMechanismInstant& mainInstant,
    double articulatedRadiusM,
    double gammaDeg)
{
    const Vec2 sideDir = Normalize({ mainInstant.mainRodDir.y, -mainInstant.mainRodDir.x });
    const double gamma = DegToRad(gammaDeg);

    const Vec2 articulatedRadiusDir = Normalize(
        mainInstant.mainRodDir * std::cos(gamma) +
        sideDir * std::sin(gamma));

    return mainInstant.crankPin + articulatedRadiusDir * articulatedRadiusM;
}

double ComputeArticulatedAxisCoordinate(
    double alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double articulatedRadiusM,
    double articulatedRodLengthM,
    double articulatedAxisTiltDeg,
    double gammaDeg,
    double deaxialMm)
{
    const MainMechanismInstant mainInstant = ComputeMainMechanismInstant(
        alphaDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        deaxialMm);

    const Vec2 attachPoint = ComputeArticulatedAttachPointFromMain(
        mainInstant,
        articulatedRadiusM,
        gammaDeg);

    return ComputeAxisCoordinateFromPoint(
        attachPoint,
        articulatedRodLengthM,
        articulatedAxisTiltDeg,
        deaxialMm);
}

double ComputeVelocityFromAxisCoordinate(
    bool isArticulated,
    double alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double articulatedRadiusM,
    double articulatedRodLengthM,
    double articulatedAxisTiltDeg,
    double gammaDeg,
    double deaxialMm,
    double rpm)
{
    const double hDeg = 1e-4;

    const auto eval = [&](double alpha) -> double
    {
        if (isArticulated)
        {
            return ComputeArticulatedAxisCoordinate(
                alpha,
                phaseDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                mainAxisTiltDeg,
                articulatedRadiusM,
                articulatedRodLengthM,
                articulatedAxisTiltDeg,
                gammaDeg,
                deaxialMm);
        }

        return ComputeMainAxisCoordinate(
            alpha,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            deaxialMm);
    };

    const double s1 = eval(alphaDeg - hDeg);
    const double s2 = eval(alphaDeg + hDeg);

    const double deltaAlphaRad = DegToRad(2.0 * hDeg);
    const double omega = RpmToOmega(rpm);

    if (std::abs(deltaAlphaRad) < 1e-12)
        return 0.0;

    const double dsDtheta = (s2 - s1) / deltaAlphaRad;
    return -dsDtheta * omega;
}

double ComputeAccelerationFromAxisCoordinate(
    bool isArticulated,
    double alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double articulatedRadiusM,
    double articulatedRodLengthM,
    double articulatedAxisTiltDeg,
    double gammaDeg,
    double deaxialMm,
    double rpm)
{
    const double hDeg = 1e-4;

    const double v1 = ComputeVelocityFromAxisCoordinate(
        isArticulated,
        alphaDeg - hDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        articulatedRadiusM,
        articulatedRodLengthM,
        articulatedAxisTiltDeg,
        gammaDeg,
        deaxialMm,
        rpm);

    const double v2 = ComputeVelocityFromAxisCoordinate(
        isArticulated,
        alphaDeg + hDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        articulatedRadiusM,
        articulatedRodLengthM,
        articulatedAxisTiltDeg,
        gammaDeg,
        deaxialMm,
        rpm);

    const double deltaAlphaRad = DegToRad(2.0 * hDeg);
    const double omega = RpmToOmega(rpm);

    if (std::abs(omega) < 1e-12)
        return 0.0;

    const double dt = deltaAlphaRad / omega;
    if (std::abs(dt) < 1e-12)
        return 0.0;

    return (v2 - v1) / dt;
}

double ComputeRodAngle(
    bool isArticulated,
    double alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double articulatedRadiusM,
    double articulatedRodLengthM,
    double articulatedAxisTiltDeg,
    double gammaDeg,
    double deaxialMm)
{
    if (isArticulated)
    {
        const MainMechanismInstant mainInstant = ComputeMainMechanismInstant(
            alphaDeg,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            deaxialMm);

        const Vec2 attachPoint = ComputeArticulatedAttachPointFromMain(
            mainInstant,
            articulatedRadiusM,
            gammaDeg);

        const double axisCoordinate = ComputeArticulatedAxisCoordinate(
            alphaDeg,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            articulatedRadiusM,
            articulatedRodLengthM,
            articulatedAxisTiltDeg,
            gammaDeg,
            deaxialMm);

        const Vec2 pistonPin = ComputePistonPinPointFromAxisCoordinate(
            axisCoordinate,
            articulatedAxisTiltDeg,
            deaxialMm);

        return ComputeRodAngleFromPoints(attachPoint, pistonPin, articulatedAxisTiltDeg);
    }

    const Vec2 basePoint = ComputeCrankPinPoint(
        alphaDeg,
        phaseDeg,
        mainCrankRadiusM);

    const double axisCoordinate = ComputeMainAxisCoordinate(
        alphaDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        deaxialMm);

    const Vec2 pistonPin = ComputePistonPinPointFromAxisCoordinate(
        axisCoordinate,
        mainAxisTiltDeg,
        deaxialMm);

    return ComputeRodAngleFromPoints(basePoint, pistonPin, mainAxisTiltDeg);
}

double ComputeRodAngularVelocity(
    bool isArticulated,
    double alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double articulatedRadiusM,
    double articulatedRodLengthM,
    double articulatedAxisTiltDeg,
    double gammaDeg,
    double deaxialMm,
    double rpm)
{
    const double hDeg = 1e-4;

    const double phi1 = ComputeRodAngle(
        isArticulated,
        alphaDeg - hDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        articulatedRadiusM,
        articulatedRodLengthM,
        articulatedAxisTiltDeg,
        gammaDeg,
        deaxialMm);

    const double phi2 = ComputeRodAngle(
        isArticulated,
        alphaDeg + hDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        articulatedRadiusM,
        articulatedRodLengthM,
        articulatedAxisTiltDeg,
        gammaDeg,
        deaxialMm);

    const double dPhi = NormalizeAngleDiff(phi2 - phi1);
    const double deltaAlphaRad = DegToRad(2.0 * hDeg);
    const double omega = RpmToOmega(rpm);

    if (std::abs(deltaAlphaRad) < 1e-12)
        return 0.0;

    return (dPhi / deltaAlphaRad) * omega;
}

double ComputeRodAngularAcceleration(
    bool isArticulated,
    double alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double articulatedRadiusM,
    double articulatedRodLengthM,
    double articulatedAxisTiltDeg,
    double gammaDeg,
    double deaxialMm,
    double rpm)
{
    const double hDeg = 1e-4;

    const double w1 = ComputeRodAngularVelocity(
        isArticulated,
        alphaDeg - hDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        articulatedRadiusM,
        articulatedRodLengthM,
        articulatedAxisTiltDeg,
        gammaDeg,
        deaxialMm,
        rpm);

    const double w2 = ComputeRodAngularVelocity(
        isArticulated,
        alphaDeg + hDeg,
        phaseDeg,
        mainCrankRadiusM,
        mainRodLengthM,
        mainAxisTiltDeg,
        articulatedRadiusM,
        articulatedRodLengthM,
        articulatedAxisTiltDeg,
        gammaDeg,
        deaxialMm,
        rpm);

    const double deltaAlphaRad = DegToRad(2.0 * hDeg);
    const double omega = RpmToOmega(rpm);

    if (std::abs(omega) < 1e-12)
        return 0.0;

    const double dt = deltaAlphaRad / omega;
    if (std::abs(dt) < 1e-12)
        return 0.0;

    return (w2 - w1) / dt;
}

HarmonicDecomposition DecomposeAccelerationIntoFirstSecondOrder(
    const std::vector<double>& alphaDeg,
    double phaseDeg,
    const std::vector<double>& totalAcceleration)
{
    HarmonicDecomposition result;
    result.firstOrder.assign(alphaDeg.size(), 0.0);
    result.secondOrder.assign(alphaDeg.size(), 0.0);

    const std::size_t n = alphaDeg.size();
    if (n == 0 || totalAcceleration.size() != n)
        return result;

    double a1c = 0.0;
    double a1s = 0.0;
    double a2c = 0.0;
    double a2s = 0.0;

    for (std::size_t i = 0; i < n; ++i)
    {
        const double theta = DegToRad(alphaDeg[i] + phaseDeg);
        const double a = totalAcceleration[i];

        a1c += a * std::cos(theta);
        a1s += a * std::sin(theta);
        a2c += a * std::cos(2.0 * theta);
        a2s += a * std::sin(2.0 * theta);
    }

    const double scale = 2.0 / static_cast<double>(n);
    a1c *= scale;
    a1s *= scale;
    a2c *= scale;
    a2s *= scale;

    for (std::size_t i = 0; i < n; ++i)
    {
        const double theta = DegToRad(alphaDeg[i] + phaseDeg);

        result.firstOrder[i] =
            a1c * std::cos(theta) +
            a1s * std::sin(theta);

        result.secondOrder[i] =
            a2c * std::cos(2.0 * theta) +
            a2s * std::sin(2.0 * theta);
    }

    return result;
}

SeriesBuffers ComputeMainSeries(
    const std::vector<double>& alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double deaxialMm,
    double rpm,
    double crankPinCenterZ_M)
{
    SeriesBuffers buffers;
    buffers.axisCoordinates.reserve(alphaDeg.size());
    buffers.displacements.reserve(alphaDeg.size());
    buffers.velocities.reserve(alphaDeg.size());
    buffers.accelerations.reserve(alphaDeg.size());
    buffers.accelerationsFirstOrder.reserve(alphaDeg.size());
    buffers.accelerationsSecondOrder.reserve(alphaDeg.size());
    buffers.rodAngles.reserve(alphaDeg.size());
    buffers.rodAngularVelocities.reserve(alphaDeg.size());
    buffers.rodAngularAccelerations.reserve(alphaDeg.size());

    const Vec2 axisUnit2 = MakeAxisUnit(mainAxisTiltDeg);
    buffers.cylinderAxisUnitGlobal = ToVec3(axisUnit2);
    buffers.forceLinePointGlobalM.reserve(alphaDeg.size());
    buffers.crankPinPointGlobalM.reserve(alphaDeg.size());
    buffers.crankRadiusVectorGlobalM.reserve(alphaDeg.size());

    const bool useAnalyticalPath = IsCenteredMainRodAnalyticalCase(
        mainCrankRadiusM,
        mainRodLengthM,
        deaxialMm);

    if (useAnalyticalPath)
    {
        for (double alpha : alphaDeg)
        {
            const auto instant = ComputeCenteredSliderCrankAnalyticalInstant(
                alpha,
                phaseDeg,
                mainAxisTiltDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                rpm);

            const Vec2 crankPin = ComputeCrankPinPoint(
                alpha,
                phaseDeg,
                mainCrankRadiusM);

            const double axisCoordinate =
                mainRodLengthM + mainCrankRadiusM - instant.displacementM;
            const Vec2 pistonPin = ComputePistonPinPointFromAxisCoordinate(
                axisCoordinate,
                mainAxisTiltDeg,
                deaxialMm);

            // Analytical centered slider-crank kinematics provides scalar motion along
            // the local cylinder axis. The cylinder axis tilt gamma is still applied when
            // placing piston, crank and force-line points in the global coordinate system.
            buffers.axisCoordinates.push_back(axisCoordinate);
            buffers.displacements.push_back(instant.displacementM);
            buffers.velocities.push_back(instant.velocityMps);
            buffers.accelerations.push_back(instant.accelerationMps2);
            buffers.accelerationsFirstOrder.push_back(instant.accelerationFirstOrderMps2);
            buffers.accelerationsSecondOrder.push_back(instant.accelerationSecondOrderMps2);
            buffers.rodAngles.push_back(instant.rodAngleRad);
            buffers.rodAngularVelocities.push_back(instant.rodAngularVelocityRadS);
            buffers.rodAngularAccelerations.push_back(instant.rodAngularAccelerationRadS2);

            buffers.forceLinePointGlobalM.push_back({
                pistonPin.x,
                pistonPin.y,
                crankPinCenterZ_M
            });
            buffers.crankPinPointGlobalM.push_back({
                crankPin.x,
                crankPin.y,
                crankPinCenterZ_M
            });
            buffers.crankRadiusVectorGlobalM.push_back({
                crankPin.x,
                crankPin.y,
                0.0
            });
        }
    }
    else
    {
        // Geometric fallback path.
        // The piston position is obtained from the existing geometric constraint
        // solution. Velocity, acceleration and rod angular derivatives in this path
        // are still finite-difference based and therefore should not be labeled as
        // fully analytical kinematics.
        double sAxisMax = -std::numeric_limits<double>::infinity();

        for (double alpha : alphaDeg)
        {
            const double sAxis = ComputeMainAxisCoordinate(
                alpha,
                phaseDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                mainAxisTiltDeg,
                deaxialMm);

            buffers.axisCoordinates.push_back(sAxis);
            if (sAxis > sAxisMax)
                sAxisMax = sAxis;
        }

        for (std::size_t i = 0; i < alphaDeg.size(); ++i)
        {
            const double alpha = alphaDeg[i];
            const double s = sAxisMax - buffers.axisCoordinates[i];

            const Vec2 crankPin = ComputeCrankPinPoint(
                alpha,
                phaseDeg,
                mainCrankRadiusM);

            const Vec2 pistonPin = ComputePistonPinPointFromAxisCoordinate(
                buffers.axisCoordinates[i],
                mainAxisTiltDeg,
                deaxialMm);

            buffers.forceLinePointGlobalM.push_back({
                pistonPin.x,
                pistonPin.y,
                crankPinCenterZ_M
            });

            buffers.crankPinPointGlobalM.push_back({
                crankPin.x,
                crankPin.y,
                crankPinCenterZ_M
            });

            buffers.crankRadiusVectorGlobalM.push_back({
                crankPin.x,
                crankPin.y,
                0.0
            });

            const double v = ComputeVelocityFromAxisCoordinate(
                false,
                alpha,
                phaseDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                mainAxisTiltDeg,
                0.0,
                0.0,
                0.0,
                0.0,
                deaxialMm,
                rpm);

            const double a = ComputeAccelerationFromAxisCoordinate(
                false,
                alpha,
                phaseDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                mainAxisTiltDeg,
                0.0,
                0.0,
                0.0,
                0.0,
                deaxialMm,
                rpm);

            const double phi = ComputeRodAngle(
                false,
                alpha,
                phaseDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                mainAxisTiltDeg,
                0.0,
                0.0,
                0.0,
                0.0,
                deaxialMm);

            const double wRod = ComputeRodAngularVelocity(
                false,
                alpha,
                phaseDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                mainAxisTiltDeg,
                0.0,
                0.0,
                0.0,
                0.0,
                deaxialMm,
                rpm);

            const double eRod = ComputeRodAngularAcceleration(
                false,
                alpha,
                phaseDeg,
                mainCrankRadiusM,
                mainRodLengthM,
                mainAxisTiltDeg,
                0.0,
                0.0,
                0.0,
                0.0,
                deaxialMm,
                rpm);

            buffers.displacements.push_back(s);
            buffers.velocities.push_back(v);
            buffers.accelerations.push_back(a);
            buffers.rodAngles.push_back(phi);
            buffers.rodAngularVelocities.push_back(wRod);
            buffers.rodAngularAccelerations.push_back(eRod);
        }

        const auto harmonic = DecomposeAccelerationIntoFirstSecondOrder(
            alphaDeg,
            phaseDeg,
            buffers.accelerations);
        buffers.accelerationsFirstOrder = harmonic.firstOrder;
        buffers.accelerationsSecondOrder = harmonic.secondOrder;
    }

    return buffers;
}

SeriesBuffers ComputeArticulatedSeries(
    const std::vector<double>& alphaDeg,
    double phaseDeg,
    double mainCrankRadiusM,
    double mainRodLengthM,
    double mainAxisTiltDeg,
    double articulatedRadiusM,
    double articulatedRodLengthM,
    double articulatedAxisTiltDeg,
    double gammaDeg,
    double deaxialMm,
    double rpm,
    double crankPinCenterZ_M)
{
    SeriesBuffers buffers;
    buffers.axisCoordinates.reserve(alphaDeg.size());
    buffers.displacements.reserve(alphaDeg.size());
    buffers.velocities.reserve(alphaDeg.size());
    buffers.accelerations.reserve(alphaDeg.size());
    buffers.accelerationsFirstOrder.reserve(alphaDeg.size());
    buffers.accelerationsSecondOrder.reserve(alphaDeg.size());
    buffers.rodAngles.reserve(alphaDeg.size());
    buffers.rodAngularVelocities.reserve(alphaDeg.size());
    buffers.rodAngularAccelerations.reserve(alphaDeg.size());

    const Vec2 axisUnit2 = MakeAxisUnit(articulatedAxisTiltDeg);
    buffers.cylinderAxisUnitGlobal = ToVec3(axisUnit2);
    buffers.forceLinePointGlobalM.reserve(alphaDeg.size());
    buffers.crankPinPointGlobalM.reserve(alphaDeg.size());
    buffers.crankRadiusVectorGlobalM.reserve(alphaDeg.size());

    double sAxisMax = -std::numeric_limits<double>::infinity();

    for (double alpha : alphaDeg)
    {
        const double sAxis = ComputeArticulatedAxisCoordinate(
            alpha,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            articulatedRadiusM,
            articulatedRodLengthM,
            articulatedAxisTiltDeg,
            gammaDeg,
            deaxialMm);

        buffers.axisCoordinates.push_back(sAxis);
        if (sAxis > sAxisMax)
            sAxisMax = sAxis;
    }

    for (std::size_t i = 0; i < alphaDeg.size(); ++i)
    {
        const double alpha = alphaDeg[i];
        const double s = sAxisMax - buffers.axisCoordinates[i];

        const Vec2 crankPin = ComputeCrankPinPoint(
            alpha,
            phaseDeg,
            mainCrankRadiusM);

        const Vec2 pistonPin = ComputePistonPinPointFromAxisCoordinate(
            buffers.axisCoordinates[i],
            articulatedAxisTiltDeg,
            deaxialMm);

        buffers.forceLinePointGlobalM.push_back({
            pistonPin.x,
            pistonPin.y,
            crankPinCenterZ_M
        });

        buffers.crankPinPointGlobalM.push_back({
            crankPin.x,
            crankPin.y,
            crankPinCenterZ_M
        });

        buffers.crankRadiusVectorGlobalM.push_back({
            crankPin.x,
            crankPin.y,
            0.0
        });

        const double v = ComputeVelocityFromAxisCoordinate(
            true,
            alpha,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            articulatedRadiusM,
            articulatedRodLengthM,
            articulatedAxisTiltDeg,
            gammaDeg,
            deaxialMm,
            rpm);

        const double a = ComputeAccelerationFromAxisCoordinate(
            true,
            alpha,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            articulatedRadiusM,
            articulatedRodLengthM,
            articulatedAxisTiltDeg,
            gammaDeg,
            deaxialMm,
            rpm);

        const double phi = ComputeRodAngle(
            true,
            alpha,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            articulatedRadiusM,
            articulatedRodLengthM,
            articulatedAxisTiltDeg,
            gammaDeg,
            deaxialMm);

        const double wRod = ComputeRodAngularVelocity(
            true,
            alpha,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            articulatedRadiusM,
            articulatedRodLengthM,
            articulatedAxisTiltDeg,
            gammaDeg,
            deaxialMm,
            rpm);

        const double eRod = ComputeRodAngularAcceleration(
            true,
            alpha,
            phaseDeg,
            mainCrankRadiusM,
            mainRodLengthM,
            mainAxisTiltDeg,
            articulatedRadiusM,
            articulatedRodLengthM,
            articulatedAxisTiltDeg,
            gammaDeg,
            deaxialMm,
            rpm);

        buffers.displacements.push_back(s);
        buffers.velocities.push_back(v);
        buffers.accelerations.push_back(a);

        buffers.rodAngles.push_back(phi);
        buffers.rodAngularVelocities.push_back(wRod);
        buffers.rodAngularAccelerations.push_back(eRod);
    }

    const auto harmonic = DecomposeAccelerationIntoFirstSecondOrder(
        alphaDeg,
        phaseDeg,
        buffers.accelerations);

    buffers.accelerationsFirstOrder = harmonic.firstOrder;
    buffers.accelerationsSecondOrder = harmonic.secondOrder;

    return buffers;
}

} // namespace

KinematicResult KinematicSolver::Solve(const NormalizedKinematicModel& model)
{
    KinematicResult result;
    result.rpm = model.rpm;

    if (model.alphaStepDeg <= 0.0)
        return result;

    for (double alpha = model.alphaStartDeg; alpha <= model.alphaEndDeg + 1e-9; alpha += model.alphaStepDeg)
    {
        result.alphaDeg.push_back(alpha);
    }

    for (const auto& shaft : model.shafts)
    {
        for (const auto& throwItem : shaft.throws)
        {
            std::vector<const NormalizedCylinderLink*> mainLinks;
            std::vector<const NormalizedCylinderLink*> articulatedLinks;

            for (const auto& link : throwItem.links)
            {
                if (link.linkType == CylinderLinkType::Main)
                {
                    mainLinks.push_back(&link);
                }
                else if (link.linkType == CylinderLinkType::Articulated)
                {
                    articulatedLinks.push_back(&link);
                }
            }

            const NormalizedCylinderLink* masterLink =
                mainLinks.empty() ? nullptr : mainLinks.front();

            const int crankCount = static_cast<int>(shaft.throws.size());
            const double crankPinCenterZ_M = ComputeCrankPinCenterZ_M(
                shaft.originZMm,
                model.mainJournalLengthM,
                model.rodJournalLengthM,
                model.webThicknessM,
                model.supportType,
                crankCount,
                throwItem.crankNumber);

            for (const auto* mainLink : mainLinks)
            {
                if (mainLink == nullptr)
                    continue;

                const auto buffers = ComputeMainSeries(
                    result.alphaDeg,
                    throwItem.phaseDeg,
                    model.mainCrankRadiusM,
                    mainLink->mainRodLengthM,
                    mainLink->axisTiltDeg,
                    model.deaxialMm,
                    model.rpm,
                    crankPinCenterZ_M);

                CylinderKinematicSeries series;
                series.cylinderNumber = mainLink->cylinderNumber;
                series.shaftNumber = shaft.shaftNumber;
                series.crankNumber = throwItem.crankNumber;
                series.linkType = mainLink->linkType;

                series.displacementM = buffers.displacements;
                series.velocityMps = buffers.velocities;
                series.accelerationMps2 = buffers.accelerations;

                series.accelerationFirstOrderMps2 = buffers.accelerationsFirstOrder;
                series.accelerationSecondOrderMps2 = buffers.accelerationsSecondOrder;

                series.rodAngleRad = buffers.rodAngles;
                series.rodAngularVelocityRadS = buffers.rodAngularVelocities;
                series.rodAngularAccelerationRadS2 = buffers.rodAngularAccelerations;

                series.cylinderAxisUnitGlobal = buffers.cylinderAxisUnitGlobal;
                series.forceLinePointGlobalM = buffers.forceLinePointGlobalM;
                series.crankPinPointGlobalM = buffers.crankPinPointGlobalM;
                series.crankRadiusVectorGlobalM = buffers.crankRadiusVectorGlobalM;

                result.cylinders.push_back(std::move(series));
            }

            for (const auto* articulatedLink : articulatedLinks)
            {
                if (articulatedLink == nullptr || masterLink == nullptr)
                    continue;

                const double gammaDeg =
                    articulatedLink->axisTiltDeg - masterLink->axisTiltDeg;

                const auto buffers = ComputeArticulatedSeries(
                    result.alphaDeg,
                    throwItem.phaseDeg,
                    model.mainCrankRadiusM,
                    masterLink->mainRodLengthM,
                    masterLink->axisTiltDeg,
                    articulatedLink->articulatedCrankRadiusM,
                    articulatedLink->articulatedRodLengthM,
                    articulatedLink->axisTiltDeg,
                    gammaDeg,
                    model.deaxialMm,
                    model.rpm,
                    crankPinCenterZ_M);

                CylinderKinematicSeries series;
                series.cylinderNumber = articulatedLink->cylinderNumber;
                series.shaftNumber = shaft.shaftNumber;
                series.crankNumber = throwItem.crankNumber;
                series.linkType = articulatedLink->linkType;

                series.displacementM = buffers.displacements;
                series.velocityMps = buffers.velocities;
                series.accelerationMps2 = buffers.accelerations;

                series.accelerationFirstOrderMps2 = buffers.accelerationsFirstOrder;
                series.accelerationSecondOrderMps2 = buffers.accelerationsSecondOrder;

                series.rodAngleRad = buffers.rodAngles;
                series.rodAngularVelocityRadS = buffers.rodAngularVelocities;
                series.rodAngularAccelerationRadS2 = buffers.rodAngularAccelerations;

                series.cylinderAxisUnitGlobal = buffers.cylinderAxisUnitGlobal;
                series.forceLinePointGlobalM = buffers.forceLinePointGlobalM;
                series.crankPinPointGlobalM = buffers.crankPinPointGlobalM;
                series.crankRadiusVectorGlobalM = buffers.crankRadiusVectorGlobalM;

                result.cylinders.push_back(std::move(series));
            }
        }
    }

    return result;
}

} // namespace engine::kinematic