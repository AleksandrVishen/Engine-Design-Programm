#include "core/balancing/balancing_composer.h"

#include <cstddef>
#include <stdexcept>

namespace engine::balancing
{

namespace
{

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

void EnsureSameSampleCount(const std::vector<double>& dynamicAlphaDeg,
                           const std::vector<double>& balancingAlphaDeg)
{
    if (dynamicAlphaDeg.size() != balancingAlphaDeg.size())
    {
        throw std::runtime_error(
            "Невозможно сложить результаты динамики и уравновешивания: "
            "разное число угловых отсчетов.");
    }
}

void EnsureSameAngles(const std::vector<double>& dynamicAlphaDeg,
                      const std::vector<double>& balancingAlphaDeg)
{
    EnsureSameSampleCount(dynamicAlphaDeg, balancingAlphaDeg);

    for (std::size_t i = 0; i < dynamicAlphaDeg.size(); ++i)
    {
        if (dynamicAlphaDeg[i] != balancingAlphaDeg[i])
        {
            throw std::runtime_error(
                "Невозможно сложить результаты динамики и уравновешивания: "
                "массивы углов alphaDeg не совпадают.");
        }
    }
}

std::vector<Vec3> AddSeries(const std::vector<Vec3>& a, const std::vector<Vec3>& b)
{
    if (a.size() != b.size())
    {
        throw std::runtime_error(
            "Невозможно сложить векторные ряды: разная длина массивов.");
    }

    std::vector<Vec3> result(a.size());
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        result[i] = Add(a[i], b[i]);
    }
    return result;
}

std::vector<Vec3> CopySeries(const std::vector<Vec3>& source)
{
    return source;
}

} // namespace

BalancingComposedResult BalancingComposer::Compose(
    const engine::dynamic::DynamicResult& dynamicResult,
    const BalancingResult& balancingResult) const
{
    EnsureSameAngles(dynamicResult.alphaDeg, balancingResult.alphaDeg);

    BalancingComposedResult result;
    result.alphaDeg = dynamicResult.alphaDeg;

    result.sourceInertiaForce1 = CopySeries(dynamicResult.totalInertiaForce1);
    result.sourceInertiaForce2 = CopySeries(dynamicResult.totalInertiaForce2);

    // Критично: полная сила инерции должна браться из точного результата динамики,
    // а не пересобираться как F1 + F2.
    result.sourceInertiaForce = CopySeries(dynamicResult.totalInertiaForce);

    result.sourceInertiaMoment1 = CopySeries(dynamicResult.totalInertiaMoment1);
    result.sourceInertiaMoment2 = CopySeries(dynamicResult.totalInertiaMoment2);

    result.sourceCentrifugalForce = CopySeries(dynamicResult.totalCentrifugalForce);
    result.sourceCentrifugalMoment = CopySeries(dynamicResult.totalCentrifugalMoment);

    result.crankCounterweightCentrifugalForce =
        CopySeries(balancingResult.crankCounterweightCentrifugalForce);
    result.crankCounterweightCentrifugalMoment =
        CopySeries(balancingResult.crankCounterweightCentrifugalMoment);

    result.balancerInertiaForce1 = CopySeries(balancingResult.balancerInertiaForce1);
    result.balancerInertiaForce2 = CopySeries(balancingResult.balancerInertiaForce2);
    result.balancerInertiaMoment1 = CopySeries(balancingResult.balancerInertiaMoment1);
    result.balancerInertiaMoment2 = CopySeries(balancingResult.balancerInertiaMoment2);

    result.counterweightInertiaForce = AddSeries(
        balancingResult.balancerInertiaForce1,
        balancingResult.balancerInertiaForce2);
    result.counterweightInertiaMoment = AddSeries(
        balancingResult.balancerInertiaMoment1,
        balancingResult.balancerInertiaMoment2);

    result.counterweightCentrifugalForce =
        CopySeries(balancingResult.crankCounterweightCentrifugalForce);
    result.counterweightCentrifugalMoment =
        CopySeries(balancingResult.crankCounterweightCentrifugalMoment);

    result.residualInertiaForce1 = AddSeries(
        dynamicResult.totalInertiaForce1,
        balancingResult.balancerInertiaForce1);
    result.residualInertiaForce2 = AddSeries(
        dynamicResult.totalInertiaForce2,
        balancingResult.balancerInertiaForce2);

    // Критично: остаточная полная сила инерции тоже должна строиться
    // от точного полного ряда динамики.
    result.residualInertiaForce = AddSeries(
        dynamicResult.totalInertiaForce,
        result.counterweightInertiaForce);

    result.residualInertiaMoment1 = AddSeries(
        dynamicResult.totalInertiaMoment1,
        balancingResult.balancerInertiaMoment1);
    result.residualInertiaMoment2 = AddSeries(
        dynamicResult.totalInertiaMoment2,
        balancingResult.balancerInertiaMoment2);
    result.residualInertiaMoment = AddSeries(
        result.residualInertiaMoment1,
        result.residualInertiaMoment2);

    result.residualCentrifugalForce = AddSeries(
        dynamicResult.totalCentrifugalForce,
        balancingResult.crankCounterweightCentrifugalForce);
    result.residualCentrifugalMoment = AddSeries(
        dynamicResult.totalCentrifugalMoment,
        balancingResult.crankCounterweightCentrifugalMoment);

    result.totalForce = AddSeries(
        result.residualInertiaForce,
        result.residualCentrifugalForce);

    result.totalMoment = AddSeries(
        result.residualInertiaMoment,
        result.residualCentrifugalMoment);

    return result;
}

} // namespace engine::balancing