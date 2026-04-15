#include "core/balancing/balancing_scheme_library.h"

namespace engine::balancing
{

bool GoalUsesCrankCounterweights(BalancingSynthesisGoalKind goal)
{
    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce:
    case BalancingSynthesisGoalKind::CentrifugalMoment:
    case BalancingSynthesisGoalKind::Combined:
        return true;

    default:
        return false;
    }
}

bool IsOrderCompatible(BalancingSynthesisGoalKind goal, int order)
{
    switch (goal)
    {
    case BalancingSynthesisGoalKind::InertiaForceFirstOrder:
    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder:
        return order == 1;

    case BalancingSynthesisGoalKind::InertiaForceSecondOrder:
    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder:
        return order == 2;

    case BalancingSynthesisGoalKind::CentrifugalForce:
    case BalancingSynthesisGoalKind::CentrifugalMoment:
        return false;

    case BalancingSynthesisGoalKind::InertiaForceTotal:
    case BalancingSynthesisGoalKind::InertiaMomentTotal:
    case BalancingSynthesisGoalKind::Combined:
    default:
        return true;
    }
}

BalancerSpeedRatio OppositeSpeed(BalancerSpeedRatio ratio)
{
    switch (ratio)
    {
    case BalancerSpeedRatio::Plus1W: return BalancerSpeedRatio::Minus1W;
    case BalancerSpeedRatio::Minus1W: return BalancerSpeedRatio::Plus1W;
    case BalancerSpeedRatio::Plus2W: return BalancerSpeedRatio::Minus2W;
    case BalancerSpeedRatio::Minus2W:
    default: return BalancerSpeedRatio::Plus2W;
    }
}

int SpeedOrder(BalancerSpeedRatio ratio)
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

std::vector<CounterweightPattern> BuildCounterweightPatterns(int counterweightCount)
{
    switch (counterweightCount)
    {
    case 1:
        return {
            { CounterweightPatternKind::SingleCenter, "Single_0", { 0.50 }, { 0.0 } },
            { CounterweightPatternKind::SingleCenter, "Single_90", { 0.50 }, { 90.0 } },
            { CounterweightPatternKind::SingleCenter, "Single_180", { 0.50 }, { 180.0 } },
            { CounterweightPatternKind::SingleCenter, "Single_270", { 0.50 }, { 270.0 } }
        };

    case 2:
        return {
            { CounterweightPatternKind::PairOpposed, "PairOpposed_X", { 0.25, 0.75 }, { 0.0, 180.0 } },
            { CounterweightPatternKind::PairOpposed, "PairOpposed_Y", { 0.25, 0.75 }, { 90.0, 270.0 } },
            { CounterweightPatternKind::PairInline, "PairInline_X", { 0.25, 0.75 }, { 0.0, 0.0 } },
            { CounterweightPatternKind::PairInline, "PairInline_Y", { 0.25, 0.75 }, { 90.0, 90.0 } }
        };

    case 3:
        return {
            { CounterweightPatternKind::Triple120, "Triple120_X", { 0.15, 0.50, 0.85 }, { 0.0, 120.0, 240.0 } },
            { CounterweightPatternKind::Triple120, "Triple120_Y", { 0.15, 0.50, 0.85 }, { 90.0, 210.0, 330.0 } },
            { CounterweightPatternKind::TripleOpposed, "TripleOpposed_X", { 0.20, 0.50, 0.80 }, { 0.0, 180.0, 0.0 } }
        };

    case 4:
    default:
        return {
            { CounterweightPatternKind::Cross4, "Cross4", { 0.125, 0.375, 0.625, 0.875 }, { 0.0, 90.0, 180.0, 270.0 } },
            { CounterweightPatternKind::OpposedQuad, "OpposedQuad_X", { 0.125, 0.375, 0.625, 0.875 }, { 0.0, 180.0, 0.0, 180.0 } },
            { CounterweightPatternKind::OpposedQuad, "OpposedQuad_Y", { 0.125, 0.375, 0.625, 0.875 }, { 90.0, 270.0, 90.0, 270.0 } }
        };
    }
}

std::vector<SynthesisSchemeDescriptor> BuildSchemeLibrary(
    BalancingSynthesisGoalKind goal,
    const BalancingSynthesisConstraints& constraints)
{
    std::vector<SynthesisSchemeDescriptor> schemes;

    schemes.push_back({
        SynthesisSchemeKind::Base,
        "Base",
        0,
        false,
        false,
        false
    });

    if (GoalUsesCrankCounterweights(goal) || goal == BalancingSynthesisGoalKind::Combined)
    {
        schemes.push_back({
            SynthesisSchemeKind::CrankOnly,
            "CrankOnly",
            0,
            true,
            false,
            false
        });
    }

    if (constraints.maxBalancerShaftCount >= 1)
    {
        schemes.push_back({
            SynthesisSchemeKind::SingleBalancer,
            "SingleBalancer",
            1,
            false,
            true,
            true
        });

        schemes.push_back({
            SynthesisSchemeKind::CrankPlusSingleBalancer,
            "CrankPlusSingleBalancer",
            1,
            true,
            true,
            true
        });
    }

    if (constraints.maxBalancerShaftCount >= 2)
    {
        schemes.push_back({
            SynthesisSchemeKind::SymmetricPairSameOrder,
            "PairSameOrder",
            2,
            false,
            true,
            true
        });

        if (goal != BalancingSynthesisGoalKind::CentrifugalForce &&
            goal != BalancingSynthesisGoalKind::CentrifugalMoment)
        {
            schemes.push_back({
                SynthesisSchemeKind::MixedPairOrder1And2,
                "PairMixed1and2",
                2,
                false,
                true,
                true
            });
        }

        schemes.push_back({
            SynthesisSchemeKind::CrankPlusPair,
            "CrankPlusPair",
            2,
            true,
            true,
            true
        });
    }

    if (constraints.maxBalancerShaftCount >= 3 &&
        goal != BalancingSynthesisGoalKind::CentrifugalForce &&
        goal != BalancingSynthesisGoalKind::CentrifugalMoment)
    {
        schemes.push_back({
            SynthesisSchemeKind::TriplePairPlusSingle,
            "TriplePairPlusSingle",
            3,
            false,
            true,
            true
        });
    }

    if (constraints.maxBalancerShaftCount >= 4)
    {
        schemes.push_back({
            SynthesisSchemeKind::FourBalancersSameOrder,
            "FourSameOrder",
            4,
            false,
            true,
            true
        });

        if (goal != BalancingSynthesisGoalKind::CentrifugalForce &&
            goal != BalancingSynthesisGoalKind::CentrifugalMoment)
        {
            schemes.push_back({
                SynthesisSchemeKind::FourBalancersTwoOrders,
                "FourTwoOrders",
                4,
                false,
                true,
                true
            });
        }
    }

    return schemes;
}

} // namespace engine::balancing