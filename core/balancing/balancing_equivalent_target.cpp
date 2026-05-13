#include "core/balancing/balancing_equivalent_target.h"

#include <algorithm>
#include <cmath>
#include <complex>

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

std::complex<double> HarmonicPhasor(const ScalarHarmonicProjection& p)
{
    return { p.c, -p.s };
}

/// Для гармоники Re(Pa e^{iθ}), Re(Pb e^{iθ}) — максимум √(Fx²+Fy²) по θ за оборот (эллипс в плоскости).
double PeakMagnitudeHarmonicPair(const ScalarHarmonicProjection& pa, const ScalarHarmonicProjection& pb)
{
    const std::complex<double> pha = HarmonicPhasor(pa);
    const std::complex<double> phb = HarmonicPhasor(pb);

    constexpr int kSamples = 36;
    constexpr double kTwoPi = 2.0 * 3.14159265358979323846;

    double peak = 0.0;
    for (int k = 0; k < kSamples; ++k)
    {
        const double theta = kTwoPi * static_cast<double>(k) / static_cast<double>(kSamples);
        const std::complex<double> ei{ std::cos(theta), std::sin(theta) };
        const double fa = std::real(pha * ei);
        const double fb = std::real(phb * ei);
        peak = std::max(peak, std::hypot(fa, fb));
    }

    return peak;
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

    const std::size_t n = alphaDeg.size();
    std::vector<double> fx(n, 0.0);
    std::vector<double> fy(n, 0.0);
    std::vector<double> fz(n, 0.0);
    std::vector<double> mx(n, 0.0);
    std::vector<double> my(n, 0.0);
    std::vector<double> mz(n, 0.0);

    for (std::size_t i = 0; i < n; ++i)
    {
        fx[i] = forceSeries[i].x;
        fy[i] = forceSeries[i].y;
        fz[i] = forceSeries[i].z;
        mx[i] = momentSeries[i].x;
        my[i] = momentSeries[i].y;
        mz[i] = momentSeries[i].z;
    }

    const auto pfx = ProjectScalarSeries(alphaDeg, fx, order);
    const auto pfy = ProjectScalarSeries(alphaDeg, fy, order);
    const auto pfz = ProjectScalarSeries(alphaDeg, fz, order);
    const auto pmx = ProjectScalarSeries(alphaDeg, mx, order);
    const auto pmy = ProjectScalarSeries(alphaDeg, my, order);
    const auto pmz = ProjectScalarSeries(alphaDeg, mz, order);

    target.forceHarmonicAmpX = pfx.amplitude;
    target.forceHarmonicAmpY = pfy.amplitude;
    target.forceHarmonicAmpZ = pfz.amplitude;
    target.momentHarmonicAmpX = pmx.amplitude;
    target.momentHarmonicAmpY = pmy.amplitude;
    target.momentHarmonicAmpZ = pmz.amplitude;

    target.forceVectorPeakXY = PeakMagnitudeHarmonicPair(pfx, pfy);
    target.forceVectorPeakXZ = PeakMagnitudeHarmonicPair(pfx, pfz);
    target.forceVectorPeakYZ = PeakMagnitudeHarmonicPair(pfy, pfz);

    target.momentVectorPeakXY = PeakMagnitudeHarmonicPair(pmx, pmy);
    target.momentVectorPeakXZ = PeakMagnitudeHarmonicPair(pmx, pmz);
    target.momentVectorPeakYZ = PeakMagnitudeHarmonicPair(pmy, pmz);

    if (pfx.amplitude >= pfy.amplitude && pfx.amplitude >= pfz.amplitude)
        target.forceDominantPhaseDeg = pfx.phaseDeg;
    else if (pfy.amplitude >= pfz.amplitude)
        target.forceDominantPhaseDeg = pfy.phaseDeg;
    else
        target.forceDominantPhaseDeg = pfz.phaseDeg;

    if (pmx.amplitude >= pmy.amplitude && pmx.amplitude >= pmz.amplitude)
        target.momentDominantPhaseDeg = pmx.phaseDeg;
    else if (pmy.amplitude >= pmz.amplitude)
        target.momentDominantPhaseDeg = pmy.phaseDeg;
    else
        target.momentDominantPhaseDeg = pmz.phaseDeg;

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
    constexpr double kPi = 3.14159265358979323846;

    double sum = 0.0;

    for (const auto& shaft : model.balancing.balancerShafts)
    {
        if (!shaft.enabled)
            continue;

        if (SpeedOrderLocal(shaft.speedRatio) != order)
            continue;

        const double m = shaft.counterweightMassKg;
        const double r = shaft.counterweightRadiusMm;
        if (m <= 0.0 || r <= 0.0 || shaft.counterweights.empty())
            continue;

        double re = 0.0;
        double im = 0.0;
        for (const auto& cw : shaft.counterweights)
        {
            const double rad = cw.phaseDeg * kPi / 180.0;
            re += std::cos(rad);
            im += std::sin(rad);
        }

        const double phaseSumMag = std::hypot(re, im);
        sum += m * r * phaseSumMag;
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