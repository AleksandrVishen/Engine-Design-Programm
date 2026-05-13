#pragma once

#include "core/balancing/balancing_synthesis.h"
#include "core/dynamic/dynamic_result.h"
#include "core/model/engine_model.h"
#include "core/model/mass_properties_input.h"

namespace engine::balancing
{

struct HarmonicBalanceTarget
{
    int order = 0;

    double forceAmplitudeN = 0.0;
    double forceRmsN = 0.0;
    double forceEquivalentProductKgMm = 0.0;
    double forceDominantPhaseDeg = 0.0;

    /// Амплитуда гармоники порядка `order` для проекций вектора силы на X/Y/Z (Н), из DFT по углу коленвала.
    double forceHarmonicAmpX = 0.0;
    double forceHarmonicAmpY = 0.0;
    double forceHarmonicAmpZ = 0.0;

    double momentAmplitudeNm = 0.0;
    double momentRmsNm = 0.0;
    double momentEquivalentAuthorityKgMm2 = 0.0;
    double momentDominantPhaseDeg = 0.0;

    /// То же для проекций момента (Н·м).
    double momentHarmonicAmpX = 0.0;
    double momentHarmonicAmpY = 0.0;
    double momentHarmonicAmpZ = 0.0;

    /// Пик модуля вектора (Fx,Fy) гармоники порядка `order` по углу коленвала (Н); плоскость ⊥ Z.
    double forceVectorPeakXY = 0.0;
    /// Пик |(Fx,Fz)|; плоскость ⊥ Y.
    double forceVectorPeakXZ = 0.0;
    /// Пик |(Fy,Fz)|; плоскость ⊥ X.
    double forceVectorPeakYZ = 0.0;

    double momentVectorPeakXY = 0.0;
    double momentVectorPeakXZ = 0.0;
    double momentVectorPeakYZ = 0.0;
};

struct EquivalentBalanceTarget
{
    double crankForceProductKgMm = 0.0;
    double crankMomentAuthorityKgMm2 = 0.0;

    HarmonicBalanceTarget order1;
    HarmonicBalanceTarget order2;

    double baselineFc = 0.0;
    double baselineMc = 0.0;
    double baselineF1 = 0.0;
    double baselineF2 = 0.0;
    double baselineF = 0.0;
    double baselineM1 = 0.0;
    double baselineM2 = 0.0;
    double baselineM = 0.0;
};

EquivalentBalanceTarget BuildEquivalentBalanceTarget(
    const EngineModel& model,
    const engine::dynamic::DynamicResult& dynamicResult,
    const MassPropertiesInput& massInput);

double ComputeCrankForceEquivalentProduct(const EngineModel& model);

/// Сумма по доп. валам величины |Σ m r e^{iφ}| (одинаковые m,r по противовесам на валу):
/// учитывает взаимное гашение сил 1ω/2ω (например два противовеса на 180° дают 0, а не 2mr).
double ComputeBalancerForceEquivalentProduct(const EngineModel& model, int order);
double ComputeMomentAuthorityEstimate(const EngineModel& model, int order);

} // namespace engine::balancing