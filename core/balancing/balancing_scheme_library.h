#pragma once

#include <string>
#include <vector>

#include "core/balancing/balancing_synthesis.h"
#include "core/model/balancing_model.h"

namespace engine::balancing
{

enum class CounterweightPatternKind
{
    SingleCenter = 0,
    PairOpposed,
    PairInline,
    Triple120,
    TripleOpposed,
    Cross4,
    OpposedQuad
};

struct CounterweightPattern
{
    CounterweightPatternKind kind = CounterweightPatternKind::SingleCenter;
    std::string name;
    std::vector<double> normalizedPositions;
    std::vector<double> phasesDeg;
};

enum class SynthesisSchemeKind
{
    None = 0,
    Base,
    CrankOnly,
    SingleBalancer,
    SymmetricPairSameOrder,
    MixedPairOrder1And2,
    TriplePairPlusSingle,
    FourBalancersSameOrder,
    FourBalancersTwoOrders,
    CrankPlusSingleBalancer,
    CrankPlusPair
};

struct SynthesisSchemeDescriptor
{
    SynthesisSchemeKind kind = SynthesisSchemeKind::None;
    std::string name;
    int balancerShaftCount = 0;
    bool usesCrankCounterweights = false;
    bool allowsOrder1 = false;
    bool allowsOrder2 = false;
};

bool GoalUsesCrankCounterweights(BalancingSynthesisGoalKind goal);
bool IsOrderCompatible(BalancingSynthesisGoalKind goal, int order);
BalancerSpeedRatio OppositeSpeed(BalancerSpeedRatio ratio);
int SpeedOrder(BalancerSpeedRatio ratio);

std::vector<CounterweightPattern> BuildCounterweightPatterns(int counterweightCount);
std::vector<SynthesisSchemeDescriptor> BuildSchemeLibrary(
    BalancingSynthesisGoalKind goal,
    const BalancingSynthesisConstraints& constraints);

} // namespace engine::balancing