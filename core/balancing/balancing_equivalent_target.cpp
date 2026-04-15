#include "core/balancing/balancing_equivalent_target.h"

#include <algorithm>
#include <cmath>

namespace engine::balancing
{

namespace
{

using engine::dynamic::Vec3;

struct ScalarHarmonicProjection
{
    double c = 0.0;
    double s = 0.0;
    double amplitude = 0.0;
    double phaseDeg = 0.0;
};

int SpeedOrderLocal(BalancerSpeedRatio ratio)
{
    switch (ratio)
    {
    case BalancerSpeedRatio::Plus1W:
    case BalancerSpeedRatio::Minus1W:
        return 1;

    case BalancerSpeedRatio::Plus2W:
    case BalancerSpeedRatio::Minus2W:
    default:
        return 2;
    }
}

double Magnitude(const Vec3& v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

double ComputeRmsMagnitude(const std::vector<Vec3>& series)
{
    if (series.empty())
        return 0.0;

    double sumSq = 0.0;
    for (const auto& v : series)
    {
        const double mag = Magnitude(v);
        sumSq += mag * mag;
    }

    return std::sqrt(sumSq / static_cast<double>(series.size()));
}

ScalarHarmonicProjection ProjectScalarSeries(
    const std::vector<double>& alphaDeg,
    const std::vector<double>& values,
    int order)
{
    ScalarHarmonicProjection projection;

    const std::size_t n = alphaDeg.size();
    if (n == 0 || values.size() != n || order <= 0)
        return projection;

    constexpr double kPi = 3.14159265358979323846;

    double c = 0.0;
    double s = 0.0;

    for (std::size_t i = 0; i < n; ++i)
    {
        const double theta = order * alphaDeg[i] * kPi / 180.0;
        c += values[i] * std::cos(theta);
        s += values[i] * std::sin(theta);
    }

    const double scale = 2.0 / static_cast<double>(n);
    projection.c = c * scale;
    projection.s = s * scale;
    projection.amplitude = std::sqrt(projection.c * projection.c + projection.s * projection.s);

    if (projection.amplitude > 0.0)
    {
        const double phaseRad = std::atan2(-projection.s, projection.c);
        projection.phaseDeg = phaseRad * 180.0 / kPi;
    }

    return projection;
}

double ComputeDominantComponentPhaseDeg(
    const std::vector<double>& alphaDeg,
    const std::vector<Vec3>& series,
    int order)
{
    if (alphaDeg.empty() || series.size() != alphaDeg.size() || order <= 0)
        return 0.0;

    std::vector<double> xs(series.size(), 0.0);
    std::vector<double> ys(series.size(), 0.0);
    std::vector<double> zs(series.size(), 0.0);

    for (std::size_t i = 0; i < series.size(); ++i)
    {
        xs[i] = series[i].x;
        ys[i] = series[i].y;
        zs[i] = series[i].z;
    }

    const auto px = ProjectScalarSeries(alphaDeg, xs, order);
    const auto py = ProjectScalarSeries(alphaDeg, ys, order);
    const auto pz = ProjectScalarSeries(alphaDeg, zs, order);

    if (px.amplitude >= py.amplitude && px.amplitude >= pz.amplitude)
        return px.phaseDeg;
    if (py.amplitude >= pz.amplitude)
        return py.phaseDeg;
    return pz.phaseDeg;
}

double ComputeEquivalentForceProductKgMm(
    double amplitudeN,
    double omegaMain)
{
    if (amplitudeN <= 0.0 || std::abs(omegaMain) < 1e-12)
        return 0.0;

    const double productKgM = amplitudeN / (omegaMain * omegaMain);
    return productKgM * 1000.0;
}

double ComputeEquivalentMomentAuthorityKgMm2(
    double amplitudeNm,
    double omegaMain)
{
    if (amplitudeNm <= 0.0 || std::abs(omegaMain) < 1e-12)
        return 0.0;

    const double authorityKgM2 = amplitudeNm / (omegaMain * omegaMain);
    return authorityKgM2 * 1000000.0;
}

HarmonicBalanceTarget BuildOrderTarget(
    const std::vector<double>& alphaDeg,
    const std::vector<Vec3>& forceSeries,
    const std::vector<Vec3>& momentSeries,
    int order,
    double omegaMain)
{
    HarmonicBalanceTarget target;
    target.order = order;

    target.forceRmsN = ComputeRmsMagnitude(forceSeries);
    target.momentRmsNm = ComputeRmsMagnitude(momentSeries);

    target.forceAmplitudeN = target.forceRmsN * std::sqrt(2.0);
    target.momentAmplitudeNm = target.momentRmsNm * std::sqrt(2.0);

    target.forceEquivalentProductKgMm =
        ComputeEquivalentForceProductKgMm(target.forceAmplitudeN, omegaMain);
    target.momentEquivalentAuthorityKgMm2 =
        ComputeEquivalentMomentAuthorityKgMm2(target.momentAmplitudeNm, omegaMain);

    target.forceDominantPhaseDeg =
        ComputeDominantComponentPhaseDeg(alphaDeg, forceSeries, order);
    target.momentDominantPhaseDeg =
        ComputeDominantComponentPhaseDeg(alphaDeg, momentSeries, order);

    return target;
}

double ComputeShaftMomentArmMm(const BalancerShaftSpec& shaft)
{
    const double centerX = shaft.originXMm;
    const double centerY = shaft.originYMm;
    const double centerZ = shaft.originZMm + 0.5 * shaft.lengthMm;

    return std::sqrt(centerX * centerX + centerY * centerY + centerZ * centerZ);
}

double ComputeCounterweightSpreadMm(const BalancerShaftSpec& shaft)
{
    if (shaft.counterweights.empty())
        return 0.0;

    double minPos = shaft.counterweights.front().positionAlongShaftMm;
    double maxPos = minPos;
    for (const auto& cw : shaft.counterweights)
    {
        minPos = std::min(minPos, cw.positionAlongShaftMm);
        maxPos = std::max(maxPos, cw.positionAlongShaftMm);
    }

    return maxPos - minPos;
}

} // namespace

EquivalentBalanceTarget BuildEquivalentBalanceTarget(
    const EngineModel& model,
    const engine::dynamic::DynamicResult& dynamicResult,
    const MassPropertiesInput& massInput)
{
    EquivalentBalanceTarget target;

    const double omegaMain =
        model.kinematic.rpm * 2.0 * 3.14159265358979323846 / 60.0;

    target.crankForceProductKgMm =
        std::max(0.0, massInput.rotatingMassKg) *
        std::max(0.0, model.kinematic.crankRadiusM * 1000.0);

    target.crankMomentAuthorityKgMm2 =
        target.crankForceProductKgMm *
        std::max(1.0, 0.5 * model.kinematic.crankRadiusM * 1000.0);

    target.order1 = BuildOrderTarget(
        dynamicResult.alphaDeg,
        dynamicResult.totalInertiaForce1,
        dynamicResult.totalInertiaMoment1,
        1,
        omegaMain);

    target.order2 = BuildOrderTarget(
        dynamicResult.alphaDeg,
        dynamicResult.totalInertiaForce2,
        dynamicResult.totalInertiaMoment2,
        2,
        omegaMain);

    target.baselineFc = ComputeRmsMagnitude(dynamicResult.totalCentrifugalForce);
    target.baselineMc = ComputeRmsMagnitude(dynamicResult.totalCentrifugalMoment);
    target.baselineF1 = ComputeRmsMagnitude(dynamicResult.totalInertiaForce1);
    target.baselineF2 = ComputeRmsMagnitude(dynamicResult.totalInertiaForce2);
    target.baselineF = ComputeRmsMagnitude(dynamicResult.totalInertiaForce);
    target.baselineM1 = ComputeRmsMagnitude(dynamicResult.totalInertiaMoment1);
    target.baselineM2 = ComputeRmsMagnitude(dynamicResult.totalInertiaMoment2);
    target.baselineM = ComputeRmsMagnitude(dynamicResult.totalInertiaMoment1) +
                       ComputeRmsMagnitude(dynamicResult.totalInertiaMoment2);

    return target;
}

double ComputeCrankForceEquivalentProduct(const EngineModel& model)
{
    const auto& cw = model.balancing.crankCounterweights;
    if (!cw.enabled || cw.massKg <= 0.0 || cw.radiusMm <= 0.0)
        return 0.0;

    int crankCountTotal = 0;
    for (const auto& shaft : model.shafts)
        crankCountTotal += static_cast<int>(shaft.cranks.size());

    int countPerCrank = 1;
    if (cw.countMode == CounterweightCountMode::TwoPerCrank)
        countPerCrank = 2;

    return static_cast<double>(crankCountTotal * countPerCrank) * cw.massKg * cw.radiusMm;
}

double ComputeBalancerForceEquivalentProduct(const EngineModel& model, int order)
{
    double sum = 0.0;

    for (const auto& shaft : model.balancing.balancerShafts)
    {
        if (!shaft.enabled)
            continue;

        if (SpeedOrderLocal(shaft.speedRatio) != order)
            continue;

        const double count = static_cast<double>(shaft.counterweights.size());
        sum += count * shaft.counterweightMassKg * shaft.counterweightRadiusMm;
    }

    return sum;
}

double ComputeMomentAuthorityEstimate(const EngineModel& model, int order)
{
    double sum = 0.0;

    for (const auto& shaft : model.balancing.balancerShafts)
    {
        if (!shaft.enabled)
            continue;

        if (SpeedOrderLocal(shaft.speedRatio) != order)
            continue;

        const double forceEquivalent =
            static_cast<double>(shaft.counterweights.size()) *
            shaft.counterweightMassKg *
            shaft.counterweightRadiusMm;

        const double armMm = ComputeShaftMomentArmMm(shaft);
        const double spreadMm = ComputeCounterweightSpreadMm(shaft);

        sum += forceEquivalent * (0.5 * armMm + 0.5 * spreadMm);
    }

    return sum;
}

} // namespace engine::balancing