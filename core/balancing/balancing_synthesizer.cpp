#include "core/balancing/balancing_synthesizer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/balancing/balancing_equivalent_target.h"
#include "core/balancing/balancing_pipeline.h"
#include "core/balancing/balancing_scheme_library.h"

namespace engine::balancing
{

namespace
{

using engine::kinematic::Vec3;

struct RankedScore
{
    double primaryResidual = 0.0;
    double secondaryResidual = 0.0;
    double complexityPenalty = 0.0;
    double massPenalty = 0.0;
    double layoutPenalty = 0.0;
};

struct FastCandidateScore
{
    bool feasible = true;
    double primaryResidual = 0.0;
    double secondaryResidual = 0.0;
    double complexityPenalty = 0.0;
    double massPenalty = 0.0;
    double layoutPenalty = 0.0;
};

struct CandidateDescriptor
{
    SynthesisSchemeDescriptor scheme;
    RankedScore rankedScore;
    FastCandidateScore fastScore;
};

struct PendingCandidate
{
    EngineModel model;
    SynthesisSchemeDescriptor scheme;
    FastCandidateScore fastScore;
    std::string signature;
};

Vec3 Add(const Vec3& a, const Vec3& b)
{
    return { a.x + b.x, a.y + b.y, a.z + b.z };
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

int ComputeTotalCylinderCount(const EngineModel& model)
{
    int count = 0;
    for (const auto& shaft : model.shafts)
        count += static_cast<int>(shaft.cylinders.size());

    return count;
}

double ComputeCrankshaftLengthMm(const EngineModel& model)
{
    const int crankCount = model.kinematic.crankCountPerShaft;
    if (crankCount <= 0)
        return 0.0;

    const double mainL = model.kinematic.mainJournalLengthM * 1000.0;
    const double rodL = model.kinematic.rodJournalLengthM * 1000.0;
    const double webT = model.kinematic.webThicknessM * 1000.0;

    int interMainCount = 0;
    if (model.kinematic.supportType == SupportType::FullySupported)
    {
        interMainCount = std::max(0, crankCount - 1);
    }
    else
    {
        for (int i = 0; i < crankCount - 1; ++i)
        {
            if ((i % 2) == 1)
                ++interMainCount;
        }
    }

    return mainL + crankCount * (2.0 * webT + rodL) + interMainCount * mainL;
}

std::vector<double> BuildRadiusGrid(double maxRadiusMm)
{
    const double r = std::max(1.0, maxRadiusMm);
    return {
        0.25 * r,
        0.50 * r,
        0.75 * r,
        1.00 * r
    };
}

std::vector<double> BuildLengthGrid(double maxLengthMm)
{
    const double L = std::max(1.0, maxLengthMm);
    return {
        0.50 * L,
        1.00 * L
    };
}

void AddError(BalancingSynthesisResult& result, const std::string& text)
{
    result.errors.push_back({ text });
}

void AddWarning(BalancingSynthesisResult& result, const std::string& text)
{
    result.warnings.push_back({ text });
}

double ComputeTotalCounterweightMassKg(const EngineModel& model)
{
    double totalMassKg = 0.0;

    const auto& crankCw = model.balancing.crankCounterweights;
    if (crankCw.enabled && crankCw.massKg > 0.0 && crankCw.radiusMm > 0.0)
    {
        int crankCountTotal = 0;
        for (const auto& shaft : model.shafts)
            crankCountTotal += static_cast<int>(shaft.cranks.size());

        const int countPerCrank =
            (crankCw.countMode == CounterweightCountMode::TwoPerCrank) ? 2 : 1;

        totalMassKg += crankCountTotal * countPerCrank * crankCw.massKg;
    }

    for (const auto& shaft : model.balancing.balancerShafts)
    {
        totalMassKg += shaft.counterweightMassKg *
                       static_cast<double>(shaft.counterweights.size());
    }

    return totalMassKg;
}

BalancingSynthesisCandidateMetrics ComputeMetrics(
    const EngineModel& model,
    const BalancingPipelineResult& pipelineResult)
{
    BalancingSynthesisCandidateMetrics metrics;

    const auto& c = pipelineResult.composedResult;

    metrics.rmsFc = ComputeRmsMagnitude(c.residualCentrifugalForce);
    metrics.rmsMc = ComputeRmsMagnitude(c.residualCentrifugalMoment);

    metrics.rmsF1 = ComputeRmsMagnitude(c.residualInertiaForce1);
    metrics.rmsF2 = ComputeRmsMagnitude(c.residualInertiaForce2);
    metrics.rmsF = ComputeRmsMagnitude(c.residualInertiaForce);

    metrics.rmsM1 = ComputeRmsMagnitude(c.residualInertiaMoment1);
    metrics.rmsM2 = ComputeRmsMagnitude(c.residualInertiaMoment2);
    metrics.rmsM = ComputeRmsMagnitude(c.residualInertiaMoment);

    metrics.balancerShaftCount = static_cast<int>(model.balancing.balancerShafts.size());
    metrics.totalCounterweightMassKg = ComputeTotalCounterweightMassKg(model);

    return metrics;
}

double ComputeLayoutPreferencePenalty(const EngineModel& model, double crankRadiusMm)
{
    double penalty = 0.0;
    const double safeCrankRadiusMm = std::max(1.0, crankRadiusMm);

    for (const auto& shaft : model.balancing.balancerShafts)
    {
        if (shaft.axis != BalancerAxis::Z)
            penalty += 15.0;

        const double radiusRatio =
            std::max(0.0, shaft.counterweightRadiusMm) / safeCrankRadiusMm;

        penalty += 8.0 * radiusRatio;
        penalty += 0.01 * std::max(0.0, std::abs(shaft.originXMm));
        penalty += 0.005 * std::max(0.0, std::abs(shaft.originYMm));
    }

    return penalty;
}

RankedScore ComputeRankedScore(const EngineModel& model,
                               const BalancingSynthesisCandidateMetrics& m,
                               BalancingSynthesisGoalKind goal,
                               double crankRadiusMm)
{
    RankedScore ranked;

    ranked.complexityPenalty =
        50.0 * static_cast<double>(m.balancerShaftCount) +
        6.0 * static_cast<double>(model.balancing.crankCounterweights.enabled ? 1 : 0);

    ranked.massPenalty = 2.0 * m.totalCounterweightMassKg;
    ranked.layoutPenalty = ComputeLayoutPreferencePenalty(model, crankRadiusMm);

    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce:
        ranked.primaryResidual = m.rmsFc;
        ranked.secondaryResidual = m.rmsMc;
        break;

    case BalancingSynthesisGoalKind::CentrifugalMoment:
        ranked.primaryResidual = m.rmsMc;
        ranked.secondaryResidual = m.rmsFc;
        break;

    case BalancingSynthesisGoalKind::InertiaForceFirstOrder:
        ranked.primaryResidual = m.rmsF1;
        ranked.secondaryResidual = m.rmsM1;
        break;

    case BalancingSynthesisGoalKind::InertiaForceSecondOrder:
        ranked.primaryResidual = m.rmsF2;
        ranked.secondaryResidual = m.rmsM2;
        break;

    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder:
        ranked.primaryResidual = m.rmsM1;
        ranked.secondaryResidual = m.rmsF1;
        break;

    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder:
        ranked.primaryResidual = m.rmsM2;
        ranked.secondaryResidual = m.rmsF2;
        break;

    case BalancingSynthesisGoalKind::InertiaForceTotal:
        ranked.primaryResidual = m.rmsF;
        ranked.secondaryResidual = m.rmsM;
        break;

    case BalancingSynthesisGoalKind::InertiaMomentTotal:
        ranked.primaryResidual = m.rmsM;
        ranked.secondaryResidual = m.rmsF;
        break;

    case BalancingSynthesisGoalKind::Combined:
    default:
        ranked.primaryResidual = m.rmsFc + m.rmsF1 + m.rmsF2;
        ranked.secondaryResidual = m.rmsMc + m.rmsM1 + m.rmsM2;
        break;
    }

    return ranked;
}

double CollapseRankedScore(const RankedScore& ranked)
{
    return ranked.primaryResidual * 1000000.0 +
           ranked.secondaryResidual * 10000.0 +
           ranked.complexityPenalty * 100.0 +
           ranked.massPenalty * 10.0 +
           ranked.layoutPenalty;
}

std::string GoalCodeToString(BalancingSynthesisGoalKind goal)
{
    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce: return "Fc";
    case BalancingSynthesisGoalKind::CentrifugalMoment: return "Mc";
    case BalancingSynthesisGoalKind::InertiaForceFirstOrder: return "F1";
    case BalancingSynthesisGoalKind::InertiaForceSecondOrder: return "F2";
    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder: return "M1";
    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder: return "M2";
    case BalancingSynthesisGoalKind::InertiaForceTotal: return "F";
    case BalancingSynthesisGoalKind::InertiaMomentTotal: return "M";
    case BalancingSynthesisGoalKind::Combined:
    default: return "Combined";
    }
}

std::string BuildTitle(const BalancingSynthesisCandidateMetrics& m,
                       const RankedScore& ranked,
                       double score,
                       const SynthesisSchemeDescriptor& scheme,
                       BalancingSynthesisGoalKind goal)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << scheme.name
        << " | goal=" << GoalCodeToString(goal)
        << " | S=" << score
        << " | P=" << ranked.primaryResidual
        << " | S2=" << ranked.secondaryResidual
        << " | Fc=" << m.rmsFc
        << " | Mc=" << m.rmsMc
        << " | F1=" << m.rmsF1
        << " | F2=" << m.rmsF2
        << " | M1=" << m.rmsM1
        << " | M2=" << m.rmsM2
        << " | валов=" << m.balancerShaftCount;
    return oss.str();
}

double ComputeCrankCounterweightMassKg(double rotatingMassKg,
                                       double crankRadiusMm,
                                       double counterweightRadiusMm,
                                       CounterweightCountMode mode)
{
    if (rotatingMassKg <= 0.0 || crankRadiusMm <= 0.0 || counterweightRadiusMm <= 0.0)
        return 0.0;

    const double balanceProduct = rotatingMassKg * crankRadiusMm;

    switch (mode)
    {
    case CounterweightCountMode::OnePerCrank:
        return balanceProduct / counterweightRadiusMm;

    case CounterweightCountMode::TwoPerCrank:
        return balanceProduct / (2.0 * counterweightRadiusMm);

    case CounterweightCountMode::Auto:
    default:
        return balanceProduct / counterweightRadiusMm;
    }
}

double ComputeBalancerCounterweightMassKg(double requiredForceEquivalentProductKgMm,
                                          double counterweightRadiusMm,
                                          int totalCounterweightCountForOrder)
{
    if (requiredForceEquivalentProductKgMm <= 0.0 ||
        counterweightRadiusMm <= 0.0 ||
        totalCounterweightCountForOrder <= 0)
    {
        return 0.0;
    }

    return requiredForceEquivalentProductKgMm /
           (counterweightRadiusMm * static_cast<double>(totalCounterweightCountForOrder));
}

bool IsReasonableCrankCounterweightMass(double massKg)
{
    return std::isfinite(massKg) && massKg > 0.0 && massKg <= 1000.0;
}

bool IsReasonableBalancerCounterweightMass(double massKg)
{
    return std::isfinite(massKg) && massKg > 0.0 && massKg <= 1000.0;
}

std::string AxisToString(BalancerAxis axis)
{
    switch (axis)
    {
    case BalancerAxis::X: return "X";
    case BalancerAxis::Y: return "Y";
    case BalancerAxis::Z:
    default: return "Z";
    }
}

std::string SpeedToString(BalancerSpeedRatio speed)
{
    switch (speed)
    {
    case BalancerSpeedRatio::Plus1W: return "+1w";
    case BalancerSpeedRatio::Minus1W: return "-1w";
    case BalancerSpeedRatio::Plus2W: return "+2w";
    case BalancerSpeedRatio::Minus2W:
    default: return "-2w";
    }
}

std::string DescribePrimaryMetric(BalancingSynthesisGoalKind goal,
                                  const BalancingSynthesisCandidateMetrics& m)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce:
        oss << "главная цель: Fc=" << m.rmsFc << ", вторично Mc=" << m.rmsMc;
        break;
    case BalancingSynthesisGoalKind::CentrifugalMoment:
        oss << "главная цель: Mc=" << m.rmsMc << ", вторично Fc=" << m.rmsFc;
        break;
    case BalancingSynthesisGoalKind::InertiaForceFirstOrder:
        oss << "главная цель: F1=" << m.rmsF1 << ", вторично M1=" << m.rmsM1;
        break;
    case BalancingSynthesisGoalKind::InertiaForceSecondOrder:
        oss << "главная цель: F2=" << m.rmsF2 << ", вторично M2=" << m.rmsM2;
        break;
    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder:
        oss << "главная цель: M1=" << m.rmsM1 << ", вторично F1=" << m.rmsF1;
        break;
    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder:
        oss << "главная цель: M2=" << m.rmsM2 << ", вторично F2=" << m.rmsF2;
        break;
    case BalancingSynthesisGoalKind::InertiaForceTotal:
        oss << "главная цель: F=" << m.rmsF << ", вторично M=" << m.rmsM;
        break;
    case BalancingSynthesisGoalKind::InertiaMomentTotal:
        oss << "главная цель: M=" << m.rmsM << ", вторично F=" << m.rmsF;
        break;
    case BalancingSynthesisGoalKind::Combined:
    default:
        oss << "главная цель: Fc+F1+F2, вторично Mc+M1+M2";
        break;
    }

    return oss.str();
}

std::string DescribeStrengths(BalancingSynthesisGoalKind goal,
                              const BalancingSynthesisCandidateMetrics& m)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce:
    case BalancingSynthesisGoalKind::CentrifugalMoment:
        oss << "сильная сторона: без доп. валов";
        break;

    case BalancingSynthesisGoalKind::InertiaForceFirstOrder:
        oss << "сильная сторона: основной упор на 1-й порядок";
        break;

    case BalancingSynthesisGoalKind::InertiaForceSecondOrder:
        oss << "сильная сторона: основной упор на 2-й порядок";
        break;

    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder:
        oss << "сильная сторона: ориентирована на момент 1-го порядка";
        break;

    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder:
        oss << "сильная сторона: ориентирована на момент 2-го порядка";
        break;

    case BalancingSynthesisGoalKind::InertiaForceTotal:
        oss << "сильная сторона: суммарная сила инерции F=" << m.rmsF;
        break;

    case BalancingSynthesisGoalKind::InertiaMomentTotal:
        oss << "сильная сторона: суммарный момент инерции M=" << m.rmsM;
        break;

    case BalancingSynthesisGoalKind::Combined:
    default:
        oss << "сильная сторона: комбинированное покрытие Fc/F1/F2 ";
        break;
    }

    return oss.str();
}

std::string DescribeWeaknesses(BalancingSynthesisGoalKind goal,
                               const BalancingSynthesisCandidateMetrics& m)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce:
        oss << "слабое место: моментная часть Mc=" << m.rmsMc;
        break;
    case BalancingSynthesisGoalKind::CentrifugalMoment:
        oss << "слабое место: силовая часть Fc=" << m.rmsFc;
        break;
    case BalancingSynthesisGoalKind::InertiaForceFirstOrder:
        oss << "слабое место: момент 1-го порядка M1=" << m.rmsM1;
        break;
    case BalancingSynthesisGoalKind::InertiaForceSecondOrder:
        oss << "слабое место: момент 2-го порядка M2=" << m.rmsM2;
        break;
    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder:
        oss << "слабое место: сила 1-го порядка F1=" << m.rmsF1;
        break;
    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder:
        oss << "слабое место: сила 2-го порядка F2=" << m.rmsF2;
        break;
    case BalancingSynthesisGoalKind::InertiaForceTotal:
        oss << "слабое место: суммарный момент M=" << m.rmsM;
        break;
    case BalancingSynthesisGoalKind::InertiaMomentTotal:
        oss << "слабое место: суммарная сила F=" << m.rmsF;
        break;
    case BalancingSynthesisGoalKind::Combined:
    default:
        oss << "слабое место: моментные остатки Mc/M1/M2="
            << m.rmsMc << "/" << m.rmsM1 << "/" << m.rmsM2;
        break;
    }

    return oss.str();
}

std::string DescribeComplexity(const BalancingSynthesisCandidateMetrics& m,
                               const EngineModel& model,
                               const RankedScore& ranked)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    const int crankWeightsUsed = model.balancing.crankCounterweights.enabled ? 1 : 0;

    oss << "конструктивная сложность: доп. валов=" << m.balancerShaftCount
        << ", противовесы на продолжении щёк=" << crankWeightsUsed
        << ", Σm=" << m.totalCounterweightMassKg
        << " кг, penalty(C/M/L)="
        << ranked.complexityPenalty << "/"
        << ranked.massPenalty << "/"
        << ranked.layoutPenalty;

    return oss.str();
}

std::string DescribeDecisionReason(const CandidateDescriptor& descriptor,
                                   double score)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    oss << "выбран выше других, потому что лексикографически лучше по "
        << "primary=" << descriptor.rankedScore.primaryResidual
        << ", secondary=" << descriptor.rankedScore.secondaryResidual
        << ", complexity=" << descriptor.rankedScore.complexityPenalty
        << ", mass=" << descriptor.rankedScore.massPenalty
        << ", layout=" << descriptor.rankedScore.layoutPenalty
        << ", итоговый score=" << score;

    return oss.str();
}

BalancingSynthesisCandidateExplanation BuildExplanation(
    BalancingSynthesisGoalKind goal,
    const EngineModel& model,
    const BalancingSynthesisCandidateMetrics& metrics,
    const CandidateDescriptor& descriptor,
    double score)
{
    BalancingSynthesisCandidateExplanation explanation;
    explanation.goalSummary = DescribePrimaryMetric(goal, metrics);
    explanation.strengths = DescribeStrengths(goal, metrics);
    explanation.weaknesses = DescribeWeaknesses(goal, metrics);
    explanation.decisionReason = DescribeDecisionReason(descriptor, score);
    explanation.complexitySummary = DescribeComplexity(metrics, model, descriptor.rankedScore);
    return explanation;
}

std::string BuildDescription(const EngineModel& model,
                             const BalancingSynthesisCandidateMetrics& m,
                             const CandidateDescriptor& descriptor,
                             const BalancingSynthesisCandidateExplanation& explanation)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    oss << "Схема=" << descriptor.scheme.name
        << " | " << explanation.goalSummary
        << " | " << explanation.strengths
        << " | " << explanation.weaknesses
        << " | " << explanation.complexitySummary;

    oss << " | щеки: m=" << model.balancing.crankCounterweights.massKg
        << " кг, r=" << model.balancing.crankCounterweights.radiusMm
        << " мм, режим=";

    switch (model.balancing.crankCounterweights.countMode)
    {
    case CounterweightCountMode::Auto: oss << "Auto"; break;
    case CounterweightCountMode::OnePerCrank: oss << "1"; break;
    case CounterweightCountMode::TwoPerCrank: oss << "2"; break;
    }

    oss << " | доп. валов=" << model.balancing.balancerShafts.size();

    if (!model.balancing.balancerShafts.empty())
    {
        oss << " | [";
        for (std::size_t i = 0; i < model.balancing.balancerShafts.size(); ++i)
        {
            const auto& shaft = model.balancing.balancerShafts[i];
            if (i > 0)
                oss << "; ";

            oss << "ось=" << AxisToString(shaft.axis)
                << ", speed=" << SpeedToString(shaft.speedRatio)
                << ", m=" << shaft.counterweightMassKg
                << ", r=" << shaft.counterweightRadiusMm
                << ", L=" << shaft.lengthMm
                << ", n=" << shaft.counterweights.size();
        }
        oss << "]";
    }

    oss << " | " << explanation.decisionReason;
    return oss.str();
}

double RoundForSignature(double value, double step)
{
    if (!std::isfinite(value) || step <= 0.0)
        return value;

    return std::round(value / step) * step;
}

double NormalizePhaseDeg(double phaseDeg)
{
    double normalized = std::fmod(phaseDeg, 360.0);
    if (normalized < 0.0)
        normalized += 360.0;

    if (std::abs(normalized - 360.0) < 1e-9)
        normalized = 0.0;

    return normalized;
}

std::string BuildCanonicalCounterweightSignature(const BalancerCounterweightSpec& cw)
{
    const double posMm = RoundForSignature(cw.positionAlongShaftMm, 0.001);
    const double phaseDeg = RoundForSignature(NormalizePhaseDeg(cw.phaseDeg), 0.001);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << posMm << ':' << phaseDeg;
    return oss.str();
}

std::string BuildCanonicalShaftSignature(const BalancerShaftSpec& shaft)
{
    std::vector<std::string> counterweightSignatures;
    counterweightSignatures.reserve(shaft.counterweights.size());

    for (const auto& cw : shaft.counterweights)
        counterweightSignatures.push_back(BuildCanonicalCounterweightSignature(cw));

    std::sort(counterweightSignatures.begin(), counterweightSignatures.end());

    const double originX = RoundForSignature(shaft.originXMm, 0.001);
    const double originY = RoundForSignature(shaft.originYMm, 0.001);
    const double originZ = RoundForSignature(shaft.originZMm, 0.001);
    const double lengthMm = RoundForSignature(shaft.lengthMm, 0.001);
    const double massKg = RoundForSignature(shaft.counterweightMassKg, 0.000001);
    const double radiusMm = RoundForSignature(shaft.counterweightRadiusMm, 0.001);
    const double shaftPhaseDeg = RoundForSignature(NormalizePhaseDeg(shaft.shaftPhaseDeg), 0.001);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    oss << static_cast<int>(shaft.enabled)
        << ':' << static_cast<int>(shaft.axis)
        << ':' << static_cast<int>(shaft.speedRatio)
        << ':' << originX
        << ':' << originY
        << ':' << originZ
        << ':' << lengthMm
        << ':' << massKg
        << ':' << radiusMm
        << ':' << shaftPhaseDeg
        << ":cw[";

    for (std::size_t i = 0; i < counterweightSignatures.size(); ++i)
    {
        if (i > 0)
            oss << ';';
        oss << counterweightSignatures[i];
    }

    oss << ']';
    return oss.str();
}

std::string BuildCandidateSignature(const EngineModel& model)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);

    const auto& crank = model.balancing.crankCounterweights;
    const double crankMassKg = RoundForSignature(crank.massKg, 0.000001);
    const double crankRadiusMm = RoundForSignature(crank.radiusMm, 0.001);

    oss << "crank:"
        << static_cast<int>(crank.enabled)
        << ':' << crankMassKg
        << ':' << crankRadiusMm
        << ':' << static_cast<int>(crank.countMode);

    std::vector<std::string> shaftSignatures;
    shaftSignatures.reserve(model.balancing.balancerShafts.size());

    for (const auto& shaft : model.balancing.balancerShafts)
        shaftSignatures.push_back(BuildCanonicalShaftSignature(shaft));

    std::sort(shaftSignatures.begin(), shaftSignatures.end());

    for (const auto& shaftSignature : shaftSignatures)
        oss << "|shaft:" << shaftSignature;

    return oss.str();
}

int CompareFastScores(const FastCandidateScore& a, const FastCandidateScore& b)
{
    const auto keyA = std::make_tuple(
        a.primaryResidual,
        a.secondaryResidual,
        a.complexityPenalty,
        a.massPenalty,
        a.layoutPenalty);

    const auto keyB = std::make_tuple(
        b.primaryResidual,
        b.secondaryResidual,
        b.complexityPenalty,
        b.massPenalty,
        b.layoutPenalty);

    if (keyA < keyB)
        return -1;
    if (keyB < keyA)
        return 1;
    return 0;
}

bool IsBetterPendingCandidate(const PendingCandidate& candidate,
                              const PendingCandidate& currentBest)
{
    const int cmp = CompareFastScores(candidate.fastScore, currentBest.fastScore);
    if (cmp != 0)
        return cmp < 0;

    if (candidate.scheme.balancerShaftCount != currentBest.scheme.balancerShaftCount)
        return candidate.scheme.balancerShaftCount < currentBest.scheme.balancerShaftCount;

    if (candidate.scheme.usesCrankCounterweights != currentBest.scheme.usesCrankCounterweights)
        return !candidate.scheme.usesCrankCounterweights && currentBest.scheme.usesCrankCounterweights;

    return candidate.scheme.name < currentBest.scheme.name;
}

void KeepBestCandidate(BalancingSynthesisResult& result,
                       BalancingSynthesisCandidate&& candidate,
                       int maxVariantsToReturn)
{
    result.candidates.push_back(std::move(candidate));

    std::sort(result.candidates.begin(),
              result.candidates.end(),
              [](const BalancingSynthesisCandidate& a,
                 const BalancingSynthesisCandidate& b)
              {
                  return a.score < b.score;
              });

    if (static_cast<int>(result.candidates.size()) > maxVariantsToReturn)
        result.candidates.resize(static_cast<std::size_t>(maxVariantsToReturn));
}

double SafeCoverageResidual(double available, double required)
{
    if (required <= 0.0)
        return 0.0;

    const double ratio = std::clamp(available / required, 0.0, 1.5);
    if (ratio >= 1.0)
        return 0.0;

    return 1.0 - ratio;
}

FastCandidateScore EvaluateFast(const EngineModel& candidateModel,
                                const EquivalentBalanceTarget& target,
                                const SynthesisSchemeDescriptor& scheme,
                                BalancingSynthesisGoalKind goal,
                                double crankRadiusMm)
{
    FastCandidateScore score;

    score.complexityPenalty =
        25.0 * static_cast<double>(candidateModel.balancing.balancerShafts.size()) +
        4.0 * static_cast<double>(candidateModel.balancing.crankCounterweights.enabled ? 1 : 0);

    score.massPenalty = ComputeTotalCounterweightMassKg(candidateModel);
    score.layoutPenalty = ComputeLayoutPreferencePenalty(candidateModel, crankRadiusMm);

    const double crankProduct = ComputeCrankForceEquivalentProduct(candidateModel);
    const double order1Product = ComputeBalancerForceEquivalentProduct(candidateModel, 1);
    const double order2Product = ComputeBalancerForceEquivalentProduct(candidateModel, 2);

    const double moment1Authority = ComputeMomentAuthorityEstimate(candidateModel, 1);
    const double moment2Authority = ComputeMomentAuthorityEstimate(candidateModel, 2);
    const double centrifugalMomentAuthority = crankProduct * std::max(1.0, 0.5 * crankRadiusMm);

    const double reqForce1 = target.order1.forceEquivalentProductKgMm;
    const double reqForce2 = target.order2.forceEquivalentProductKgMm;
    const double reqMoment1 = target.order1.momentEquivalentAuthorityKgMm2;
    const double reqMoment2 = target.order2.momentEquivalentAuthorityKgMm2;
    const double reqForceC = target.crankForceProductKgMm;
    const double reqMomentC = target.crankMomentAuthorityKgMm2;

    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce:
        score.feasible = crankProduct > 0.0;
        score.primaryResidual = SafeCoverageResidual(crankProduct, reqForceC);
        score.secondaryResidual = SafeCoverageResidual(centrifugalMomentAuthority, reqMomentC);
        break;

    case BalancingSynthesisGoalKind::CentrifugalMoment:
        score.feasible = crankProduct > 0.0;
        score.primaryResidual = SafeCoverageResidual(centrifugalMomentAuthority, reqMomentC);
        score.secondaryResidual = SafeCoverageResidual(crankProduct, reqForceC);
        break;

    case BalancingSynthesisGoalKind::InertiaForceFirstOrder:
        score.feasible = order1Product > 0.0;
        score.primaryResidual = SafeCoverageResidual(order1Product, reqForce1);
        score.secondaryResidual = SafeCoverageResidual(moment1Authority, reqMoment1);
        break;

    case BalancingSynthesisGoalKind::InertiaForceSecondOrder:
        score.feasible = order2Product > 0.0;
        score.primaryResidual = SafeCoverageResidual(order2Product, reqForce2);
        score.secondaryResidual = SafeCoverageResidual(moment2Authority, reqMoment2);
        break;

    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder:
        score.feasible = moment1Authority > 0.0;
        score.primaryResidual = SafeCoverageResidual(moment1Authority, reqMoment1);
        score.secondaryResidual = SafeCoverageResidual(order1Product, reqForce1);
        break;

    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder:
        score.feasible = moment2Authority > 0.0;
        score.primaryResidual = SafeCoverageResidual(moment2Authority, reqMoment2);
        score.secondaryResidual = SafeCoverageResidual(order2Product, reqForce2);
        break;

    case BalancingSynthesisGoalKind::InertiaForceTotal:
        score.feasible = (order1Product > 0.0) || (order2Product > 0.0);
        score.primaryResidual =
            0.5 * SafeCoverageResidual(order1Product, reqForce1) +
            0.5 * SafeCoverageResidual(order2Product, reqForce2);
        score.secondaryResidual =
            0.5 * SafeCoverageResidual(moment1Authority, reqMoment1) +
            0.5 * SafeCoverageResidual(moment2Authority, reqMoment2);
        break;

    case BalancingSynthesisGoalKind::InertiaMomentTotal:
        score.feasible = (moment1Authority > 0.0) || (moment2Authority > 0.0);
        score.primaryResidual =
            0.5 * SafeCoverageResidual(moment1Authority, reqMoment1) +
            0.5 * SafeCoverageResidual(moment2Authority, reqMoment2);
        score.secondaryResidual =
            0.5 * SafeCoverageResidual(order1Product, reqForce1) +
            0.5 * SafeCoverageResidual(order2Product, reqForce2);
        break;

    case BalancingSynthesisGoalKind::Combined:
    default:
        score.feasible = (crankProduct > 0.0) || (order1Product > 0.0) || (order2Product > 0.0);
        score.primaryResidual =
            (1.0 / 3.0) * SafeCoverageResidual(crankProduct, reqForceC) +
            (1.0 / 3.0) * SafeCoverageResidual(order1Product, reqForce1) +
            (1.0 / 3.0) * SafeCoverageResidual(order2Product, reqForce2);
        score.secondaryResidual =
            (1.0 / 3.0) * SafeCoverageResidual(centrifugalMomentAuthority, reqMomentC) +
            (1.0 / 3.0) * SafeCoverageResidual(moment1Authority, reqMoment1) +
            (1.0 / 3.0) * SafeCoverageResidual(moment2Authority, reqMoment2);
        break;
    }

    if (scheme.usesCrankCounterweights && candidateModel.balancing.crankCounterweights.enabled)
        score.primaryResidual *= 0.95;

    if (scheme.balancerShaftCount >= 2)
        score.secondaryResidual *= 0.95;

    return score;
}

bool PassFastFilter(const FastCandidateScore& fastScore,
                    const BalancingSynthesisGoalKind goal,
                    const SynthesisSchemeDescriptor& scheme)
{
    if (!fastScore.feasible)
        return false;

    if (goal == BalancingSynthesisGoalKind::CentrifugalForce ||
        goal == BalancingSynthesisGoalKind::CentrifugalMoment)
    {
        if (scheme.balancerShaftCount > 0 && !scheme.usesCrankCounterweights)
            return false;
    }

    if (fastScore.primaryResidual > 0.80)
        return false;

    if (fastScore.complexityPenalty > 140.0)
        return false;

    return true;
}

EngineModel MakeBaseModel(const EngineModel& sourceModel)
{
    EngineModel model = sourceModel;
    model.balancing.crankCounterweights.enabled = false;
    model.balancing.crankCounterweights.massKg = 0.0;
    model.balancing.crankCounterweights.radiusMm = 0.0;
    model.balancing.crankCounterweights.countMode = CounterweightCountMode::Auto;
    model.balancing.crankCounterweights.entries.clear();
    model.balancing.balancerShafts.clear();
    return model;
}

BalancerShaftSpec MakeBalancerShaft(const CrankshaftSpec& anchor,
                                    BalancerAxis axis,
                                    BalancerSpeedRatio speedRatio,
                                    double lengthMm,
                                    double massKg,
                                    double radiusMm,
                                    const CounterweightPattern& pattern,
                                    double offsetMm)
{
    BalancerShaftSpec shaft;
    shaft.enabled = true;

    shaft.originXMm = anchor.originXMm;
    shaft.originYMm = anchor.originYMm;
    shaft.originZMm = anchor.originZMm;

    switch (axis)
    {
    case BalancerAxis::X:
        shaft.originYMm += offsetMm;
        break;

    case BalancerAxis::Y:
        shaft.originXMm += offsetMm;
        break;

    case BalancerAxis::Z:
    default:
        shaft.originXMm += offsetMm;
        break;
    }

    shaft.axis = axis;
    shaft.lengthMm = lengthMm;
    shaft.speedRatio = speedRatio;
    shaft.shaftPhaseDeg = 0.0;
    shaft.counterweightMassKg = massKg;
    shaft.counterweightRadiusMm = radiusMm;

    shaft.counterweights.clear();
    shaft.counterweights.reserve(pattern.phasesDeg.size());

    for (std::size_t i = 0; i < pattern.phasesDeg.size(); ++i)
    {
        BalancerCounterweightSpec cw;
        const double normalizedPosition =
            pattern.normalizedPositions[std::min(i, pattern.normalizedPositions.size() - 1)];

        cw.positionAlongShaftMm = normalizedPosition * lengthMm;
        cw.phaseDeg = pattern.phasesDeg[i];
        shaft.counterweights.push_back(cw);
    }

    return shaft;
}

void AddMixedCrankCounterweightsIfReasonable(EngineModel& model,
                                             double rotatingMassKg,
                                             double crankRadiusMm,
                                             double radiusMm)
{
    model.balancing.crankCounterweights.enabled = true;
    model.balancing.crankCounterweights.radiusMm = radiusMm;
    model.balancing.crankCounterweights.countMode = CounterweightCountMode::TwoPerCrank;
    model.balancing.crankCounterweights.massKg =
        ComputeCrankCounterweightMassKg(
            rotatingMassKg,
            crankRadiusMm,
            radiusMm,
            model.balancing.crankCounterweights.countMode);
}

void CollectCandidate(std::map<std::string, PendingCandidate>& pendingCandidates,
                      const EquivalentBalanceTarget& target,
                      const EngineModel& candidateModel,
                      BalancingSynthesisGoalKind goal,
                      double crankRadiusMm,
                      const SynthesisSchemeDescriptor& scheme)
{
    const FastCandidateScore fastScore =
        EvaluateFast(candidateModel, target, scheme, goal, crankRadiusMm);

    if (!PassFastFilter(fastScore, goal, scheme))
        return;

    PendingCandidate pending;
    pending.model = candidateModel;
    pending.scheme = scheme;
    pending.fastScore = fastScore;
    pending.signature = BuildCandidateSignature(candidateModel);

    auto it = pendingCandidates.find(pending.signature);
    if (it == pendingCandidates.end())
    {
        pendingCandidates.emplace(pending.signature, std::move(pending));
        return;
    }

    if (IsBetterPendingCandidate(pending, it->second))
        it->second = std::move(pending);
}

void EvaluatePendingCandidate(BalancingSynthesisResult& synthesisResult,
                              const PendingCandidate& pending,
                              const engine::dynamic::DynamicResult& dynamicResult,
                              const MassPropertiesInput& massInput,
                              BalancingSynthesisGoalKind goal,
                              const BalancingSynthesisConstraints& constraints,
                              double crankRadiusMm)
{
    BalancingInput input;
    input.alphaDeg = dynamicResult.alphaDeg;
    input.rpm = pending.model.kinematic.rpm;
    input.referenceX_M = massInput.referenceXmm / 1000.0;
    input.referenceY_M = massInput.referenceYmm / 1000.0;
    input.referenceZ_M = massInput.referenceZmm / 1000.0;

    BalancingPipeline pipeline;
    BalancingPipelineResult pipelineResult = pipeline.Run(pending.model, dynamicResult, input);
    if (!pipelineResult.ok)
        return;

    BalancingSynthesisCandidate candidate;
    candidate.model = pending.model;
    candidate.pipelineResult = pipelineResult;
    candidate.metrics = ComputeMetrics(pending.model, pipelineResult);

    CandidateDescriptor descriptor;
    descriptor.scheme = pending.scheme;
    descriptor.fastScore = pending.fastScore;
    descriptor.rankedScore = ComputeRankedScore(pending.model, candidate.metrics, goal, crankRadiusMm);

    candidate.score = CollapseRankedScore(descriptor.rankedScore);
    candidate.explanation = BuildExplanation(goal, pending.model, candidate.metrics, descriptor, candidate.score);
    candidate.title = BuildTitle(candidate.metrics, descriptor.rankedScore, candidate.score, pending.scheme, goal);
    candidate.description = BuildDescription(pending.model, candidate.metrics, descriptor, candidate.explanation);

    KeepBestCandidate(synthesisResult, std::move(candidate), constraints.maxVariantsToReturn);
}

void GenerateForScheme(std::map<std::string, PendingCandidate>& pendingCandidates,
                       const SynthesisSchemeDescriptor& scheme,
                       const EngineModel& sourceModel,
                       const EngineModel& baselineModel,
                       const EquivalentBalanceTarget& target,
                       BalancingSynthesisGoalKind goal,
                       const BalancingSynthesisConstraints& constraints,
                       double crankRadiusMm,
                       double rotatingMassKg,
                       const std::vector<double>& radiusGrid,
                       const std::vector<double>& lengthGrid)
{
    const std::vector<BalancerAxis> axes = {
        BalancerAxis::Z,
        BalancerAxis::X,
        BalancerAxis::Y
    };

    const double offsetBaseMm = std::max(1.0, crankRadiusMm);
    const int maxCounterweightsPerShaft = std::min(4, constraints.maxCounterweightsPerBalancerShaft);

    if (scheme.kind == SynthesisSchemeKind::Base)
    {
        CollectCandidate(pendingCandidates,
                         target,
                         baselineModel,
                         goal,
                         crankRadiusMm,
                         scheme);
        return;
    }

    if (scheme.kind == SynthesisSchemeKind::CrankOnly)
    {
        for (double radiusMm : radiusGrid)
        {
            for (CounterweightCountMode mode : {
                     CounterweightCountMode::OnePerCrank,
                     CounterweightCountMode::TwoPerCrank })
            {
                const double massKg =
                    ComputeCrankCounterweightMassKg(
                        rotatingMassKg,
                        crankRadiusMm,
                        radiusMm,
                        mode);

                if (!IsReasonableCrankCounterweightMass(massKg))
                    continue;

                EngineModel model = MakeBaseModel(sourceModel);
                model.balancing.crankCounterweights.enabled = true;
                model.balancing.crankCounterweights.massKg = massKg;
                model.balancing.crankCounterweights.radiusMm = radiusMm;
                model.balancing.crankCounterweights.countMode = mode;

                CollectCandidate(pendingCandidates,
                                 target,
                                 model,
                                 goal,
                                 crankRadiusMm,
                                 scheme);
            }
        }
        return;
    }

    for (const auto& anchor : sourceModel.shafts)
    {
        for (BalancerAxis axis : axes)
        {
            for (double lengthMm : lengthGrid)
            {
                for (double radiusMm : radiusGrid)
                {
                    for (int counterweightCount = 1;
                         counterweightCount <= maxCounterweightsPerShaft;
                         ++counterweightCount)
                    {
                        const auto patterns = BuildCounterweightPatterns(counterweightCount);

                        for (const auto& pattern : patterns)
                        {
                            switch (scheme.kind)
                            {
                            case SynthesisSchemeKind::SingleBalancer:
                            case SynthesisSchemeKind::CrankPlusSingleBalancer:
                            {
                                for (BalancerSpeedRatio speed : {
                                         BalancerSpeedRatio::Plus1W,
                                         BalancerSpeedRatio::Minus1W,
                                         BalancerSpeedRatio::Plus2W,
                                         BalancerSpeedRatio::Minus2W })
                                {
                                    if (!IsOrderCompatible(goal, SpeedOrder(speed)) &&
                                        goal != BalancingSynthesisGoalKind::Combined &&
                                        goal != BalancingSynthesisGoalKind::InertiaForceTotal &&
                                        goal != BalancingSynthesisGoalKind::InertiaMomentTotal)
                                    {
                                        continue;
                                    }

                                    const double requiredForceProductKgMm =
                                        (SpeedOrder(speed) == 1)
                                            ? target.order1.forceEquivalentProductKgMm
                                            : target.order2.forceEquivalentProductKgMm;

                                    const double massKg =
                                        ComputeBalancerCounterweightMassKg(
                                            requiredForceProductKgMm,
                                            radiusMm,
                                            counterweightCount);

                                    if (!IsReasonableBalancerCounterweightMass(massKg))
                                        continue;

                                    EngineModel model = MakeBaseModel(sourceModel);
                                    model.balancing.balancerShafts.push_back(
                                        MakeBalancerShaft(anchor,
                                                          axis,
                                                          speed,
                                                          lengthMm,
                                                          massKg,
                                                          radiusMm,
                                                          pattern,
                                                          +offsetBaseMm));

                                    if (scheme.usesCrankCounterweights)
                                    {
                                        AddMixedCrankCounterweightsIfReasonable(model,
                                                                                rotatingMassKg,
                                                                                crankRadiusMm,
                                                                                radiusGrid[2]);
                                        if (!IsReasonableCrankCounterweightMass(model.balancing.crankCounterweights.massKg))
                                            continue;
                                    }

                                    CollectCandidate(pendingCandidates,
                                                     target,
                                                     model,
                                                     goal,
                                                     crankRadiusMm,
                                                     scheme);
                                }
                                break;
                            }

                            case SynthesisSchemeKind::SymmetricPairSameOrder:
                            case SynthesisSchemeKind::CrankPlusPair:
                            {
                                for (BalancerSpeedRatio speedA : {
                                         BalancerSpeedRatio::Plus1W,
                                         BalancerSpeedRatio::Plus2W })
                                {
                                    if (!IsOrderCompatible(goal, SpeedOrder(speedA)) &&
                                        goal != BalancingSynthesisGoalKind::Combined &&
                                        goal != BalancingSynthesisGoalKind::InertiaForceTotal &&
                                        goal != BalancingSynthesisGoalKind::InertiaMomentTotal)
                                    {
                                        continue;
                                    }

                                    const BalancerSpeedRatio speedB = OppositeSpeed(speedA);
                                    const int totalCounterweightCount = 2 * counterweightCount;

                                    const double requiredForceProductKgMm =
                                        (SpeedOrder(speedA) == 1)
                                            ? target.order1.forceEquivalentProductKgMm
                                            : target.order2.forceEquivalentProductKgMm;

                                    const double massKg =
                                        ComputeBalancerCounterweightMassKg(
                                            requiredForceProductKgMm,
                                            radiusMm,
                                            totalCounterweightCount);

                                    if (!IsReasonableBalancerCounterweightMass(massKg))
                                        continue;

                                    EngineModel model = MakeBaseModel(sourceModel);
                                    model.balancing.balancerShafts.push_back(
                                        MakeBalancerShaft(anchor, axis, speedA, lengthMm, massKg, radiusMm,
                                                          pattern, +offsetBaseMm));
                                    model.balancing.balancerShafts.push_back(
                                        MakeBalancerShaft(anchor, axis, speedB, lengthMm, massKg, radiusMm,
                                                          pattern, -offsetBaseMm));

                                    if (scheme.usesCrankCounterweights)
                                    {
                                        AddMixedCrankCounterweightsIfReasonable(model,
                                                                                rotatingMassKg,
                                                                                crankRadiusMm,
                                                                                radiusGrid[2]);
                                        if (!IsReasonableCrankCounterweightMass(model.balancing.crankCounterweights.massKg))
                                            continue;
                                    }

                                    CollectCandidate(pendingCandidates,
                                                     target,
                                                     model,
                                                     goal,
                                                     crankRadiusMm,
                                                     scheme);
                                }
                                break;
                            }

                            case SynthesisSchemeKind::MixedPairOrder1And2:
                            {
                                for (BalancerSpeedRatio speed1 : {
                                         BalancerSpeedRatio::Plus1W,
                                         BalancerSpeedRatio::Minus1W })
                                {
                                    for (BalancerSpeedRatio speed2 : {
                                             BalancerSpeedRatio::Plus2W,
                                             BalancerSpeedRatio::Minus2W })
                                    {
                                        const double mass1 =
                                            ComputeBalancerCounterweightMassKg(
                                                target.order1.forceEquivalentProductKgMm,
                                                radiusMm,
                                                counterweightCount);

                                        const double mass2 =
                                            ComputeBalancerCounterweightMassKg(
                                                target.order2.forceEquivalentProductKgMm,
                                                radiusMm,
                                                counterweightCount);

                                        if (!IsReasonableBalancerCounterweightMass(mass1) ||
                                            !IsReasonableBalancerCounterweightMass(mass2))
                                        {
                                            continue;
                                        }

                                        EngineModel model = MakeBaseModel(sourceModel);
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, speed1, lengthMm, mass1, radiusMm,
                                                              pattern, -offsetBaseMm));
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, speed2, lengthMm, mass2, radiusMm,
                                                              pattern, +offsetBaseMm));

                                        CollectCandidate(pendingCandidates,
                                                         target,
                                                         model,
                                                         goal,
                                                         crankRadiusMm,
                                                         scheme);
                                    }
                                }
                                break;
                            }

                            case SynthesisSchemeKind::TriplePairPlusSingle:
                            {
                                for (BalancerSpeedRatio speed1 : {
                                         BalancerSpeedRatio::Plus1W,
                                         BalancerSpeedRatio::Minus1W })
                                {
                                    for (BalancerSpeedRatio speed2 : {
                                             BalancerSpeedRatio::Plus2W,
                                             BalancerSpeedRatio::Minus2W })
                                    {
                                        const int countOrder1 = 2 * counterweightCount;
                                        const int countOrder2 = 1 * counterweightCount;

                                        const double mass1 =
                                            ComputeBalancerCounterweightMassKg(
                                                target.order1.forceEquivalentProductKgMm,
                                                radiusMm,
                                                countOrder1);

                                        const double mass2 =
                                            ComputeBalancerCounterweightMassKg(
                                                target.order2.forceEquivalentProductKgMm,
                                                radiusMm,
                                                countOrder2);

                                        if (!IsReasonableBalancerCounterweightMass(mass1) ||
                                            !IsReasonableBalancerCounterweightMass(mass2))
                                        {
                                            continue;
                                        }

                                        EngineModel model = MakeBaseModel(sourceModel);
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, speed1, lengthMm, mass1, radiusMm,
                                                              pattern, -offsetBaseMm));
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, OppositeSpeed(speed1), lengthMm, mass1, radiusMm,
                                                              pattern, +offsetBaseMm));
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, speed2, lengthMm, mass2, radiusMm,
                                                              pattern, 0.0));

                                        CollectCandidate(pendingCandidates,
                                                         target,
                                                         model,
                                                         goal,
                                                         crankRadiusMm,
                                                         scheme);
                                    }
                                }
                                break;
                            }

                            case SynthesisSchemeKind::FourBalancersSameOrder:
                            {
                                for (BalancerSpeedRatio speed : {
                                         BalancerSpeedRatio::Plus1W,
                                         BalancerSpeedRatio::Plus2W })
                                {
                                    if (!IsOrderCompatible(goal, SpeedOrder(speed)) &&
                                        goal != BalancingSynthesisGoalKind::Combined &&
                                        goal != BalancingSynthesisGoalKind::InertiaForceTotal &&
                                        goal != BalancingSynthesisGoalKind::InertiaMomentTotal)
                                    {
                                        continue;
                                    }

                                    const BalancerSpeedRatio opposite = OppositeSpeed(speed);
                                    const int totalCounterweightCount = 4 * counterweightCount;

                                    const double requiredForceProductKgMm =
                                        (SpeedOrder(speed) == 1)
                                            ? target.order1.forceEquivalentProductKgMm
                                            : target.order2.forceEquivalentProductKgMm;

                                    const double massKg =
                                        ComputeBalancerCounterweightMassKg(
                                            requiredForceProductKgMm,
                                            radiusMm,
                                            totalCounterweightCount);

                                    if (!IsReasonableBalancerCounterweightMass(massKg))
                                        continue;

                                    EngineModel model = MakeBaseModel(sourceModel);
                                    model.balancing.balancerShafts.push_back(
                                        MakeBalancerShaft(anchor, axis, speed, lengthMm, massKg, radiusMm,
                                                          pattern, -1.5 * offsetBaseMm));
                                    model.balancing.balancerShafts.push_back(
                                        MakeBalancerShaft(anchor, axis, opposite, lengthMm, massKg, radiusMm,
                                                          pattern, -0.5 * offsetBaseMm));
                                    model.balancing.balancerShafts.push_back(
                                        MakeBalancerShaft(anchor, axis, speed, lengthMm, massKg, radiusMm,
                                                          pattern, +0.5 * offsetBaseMm));
                                    model.balancing.balancerShafts.push_back(
                                        MakeBalancerShaft(anchor, axis, opposite, lengthMm, massKg, radiusMm,
                                                          pattern, +1.5 * offsetBaseMm));

                                    CollectCandidate(pendingCandidates,
                                                     target,
                                                     model,
                                                     goal,
                                                     crankRadiusMm,
                                                     scheme);
                                }
                                break;
                            }

                            case SynthesisSchemeKind::FourBalancersTwoOrders:
                            {
                                for (BalancerSpeedRatio speed1 : {
                                         BalancerSpeedRatio::Plus1W,
                                         BalancerSpeedRatio::Minus1W })
                                {
                                    for (BalancerSpeedRatio speed2 : {
                                             BalancerSpeedRatio::Plus2W,
                                             BalancerSpeedRatio::Minus2W })
                                    {
                                        const int countOrder1 = 2 * counterweightCount;
                                        const int countOrder2 = 2 * counterweightCount;

                                        const double mass1 =
                                            ComputeBalancerCounterweightMassKg(
                                                target.order1.forceEquivalentProductKgMm,
                                                radiusMm,
                                                countOrder1);

                                        const double mass2 =
                                            ComputeBalancerCounterweightMassKg(
                                                target.order2.forceEquivalentProductKgMm,
                                                radiusMm,
                                                countOrder2);

                                        if (!IsReasonableBalancerCounterweightMass(mass1) ||
                                            !IsReasonableBalancerCounterweightMass(mass2))
                                        {
                                            continue;
                                        }

                                        EngineModel model = MakeBaseModel(sourceModel);
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, speed1, lengthMm, mass1, radiusMm,
                                                              pattern, -1.5 * offsetBaseMm));
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, OppositeSpeed(speed1), lengthMm, mass1, radiusMm,
                                                              pattern, -0.5 * offsetBaseMm));
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, speed2, lengthMm, mass2, radiusMm,
                                                              pattern, +0.5 * offsetBaseMm));
                                        model.balancing.balancerShafts.push_back(
                                            MakeBalancerShaft(anchor, axis, OppositeSpeed(speed2), lengthMm, mass2, radiusMm,
                                                              pattern, +1.5 * offsetBaseMm));

                                        CollectCandidate(pendingCandidates,
                                                         target,
                                                         model,
                                                         goal,
                                                         crankRadiusMm,
                                                         scheme);
                                    }
                                }
                                break;
                            }

                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace

BalancingSynthesisResult BalancingSynthesizer::Generate(
    const EngineModel& sourceModel,
    const engine::dynamic::DynamicResult& dynamicResult,
    const MassPropertiesInput& massInput,
    BalancingSynthesisGoalKind goal,
    const BalancingSynthesisConstraints& constraints) const
{
    BalancingSynthesisResult result;

    if (sourceModel.shafts.empty())
    {
        AddError(result, "В модели двигателя отсутствуют коленчатые валы.");
        return result;
    }

    if (dynamicResult.alphaDeg.empty())
    {
        AddError(result, "Отсутствуют результаты динамики для автоподбора.");
        return result;
    }

    if (constraints.maxBalancerShaftCount <= 0)
    {
        AddError(result, "Максимальное число дополнительных валов должно быть больше нуля.");
        return result;
    }

    if (constraints.maxCounterweightsPerBalancerShaft <= 0)
    {
        AddError(result, "Максимальное число противовесов на дополнительном валу должно быть больше нуля.");
        return result;
    }

    const int totalCylinderCount = ComputeTotalCylinderCount(sourceModel);
    if (totalCylinderCount <= 0)
    {
        AddError(result, "В модели двигателя отсутствуют цилиндры.");
        return result;
    }

    const double crankRadiusMm = std::max(1.0, sourceModel.kinematic.crankRadiusM * 1000.0);
    const double rotatingMassKg = std::max(0.0, massInput.rotatingMassKg);

    const double maxRadiusMm = std::max(1.0, crankRadiusMm);
    const double maxLengthMm = std::max(1.0, ComputeCrankshaftLengthMm(sourceModel));

    const std::vector<double> radiusGrid = BuildRadiusGrid(maxRadiusMm);
    const std::vector<double> lengthGrid = BuildLengthGrid(maxLengthMm);

    const EngineModel baselineModel = MakeBaseModel(sourceModel);
    const EquivalentBalanceTarget target =
        BuildEquivalentBalanceTarget(sourceModel, dynamicResult, massInput);

    std::map<std::string, PendingCandidate> pendingCandidates;
    const auto schemes = BuildSchemeLibrary(goal, constraints);

    for (const auto& scheme : schemes)
    {
        GenerateForScheme(pendingCandidates,
                          scheme,
                          sourceModel,
                          baselineModel,
                          target,
                          goal,
                          constraints,
                          crankRadiusMm,
                          rotatingMassKg,
                          radiusGrid,
                          lengthGrid);
    }

    for (const auto& [signature, pending] : pendingCandidates)
    {
        (void)signature;
        EvaluatePendingCandidate(result,
                                 pending,
                                 dynamicResult,
                                 massInput,
                                 goal,
                                 constraints,
                                 crankRadiusMm);
    }

    if (result.candidates.empty())
    {
        AddError(result, "Не удалось построить ни одного допустимого варианта уравновешивания.");
        result.ok = false;
        return result;
    }

    AddWarning(result,
               "Автоподбор теперь действительно двухэтапный: сначала формируются и канонизируются кандидаты, по каждой сигнатуре остаётся лучший fast-score, и только потом выполняется точный pipeline.");

    if (constraints.maxCounterweightsPerBalancerShaft >= 3)
    {
        AddWarning(result, "Для 3 и 4 противовесов на дополнительном валу используются типовые шаблоны размещения. Быстрая оценка использует эквивалентные цели, извлечённые из динамики, итоговая проверка выполняется полным pipeline.");
    }

    result.ok = true;
    return result;
}

} // namespace engine::balancing