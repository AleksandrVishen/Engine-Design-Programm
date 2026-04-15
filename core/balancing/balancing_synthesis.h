#pragma once

#include <string>
#include <vector>

#include "core/balancing/balancing_pipeline.h"
#include "core/model/engine_model.h"

namespace engine::balancing
{

enum class BalancingSynthesisGoalKind
{
    CentrifugalForce = 0,
    CentrifugalMoment,
    InertiaForceFirstOrder,
    InertiaForceSecondOrder,
    InertiaMomentFirstOrder,
    InertiaMomentSecondOrder,
    InertiaForceTotal,
    InertiaMomentTotal,
    Combined
};

struct BalancingSynthesisConstraints
{
    int maxBalancerShaftCount = 2;
    int maxCounterweightsPerBalancerShaft = 4;
    int maxVariantsToReturn = 8;
};

struct BalancingSynthesisCandidateMetrics
{
    double rmsFc = 0.0;
    double rmsMc = 0.0;

    double rmsF1 = 0.0;
    double rmsF2 = 0.0;
    double rmsF = 0.0;

    double rmsM1 = 0.0;
    double rmsM2 = 0.0;
    double rmsM = 0.0;

    double totalCounterweightMassKg = 0.0;
    int balancerShaftCount = 0;
};

struct BalancingSynthesisCandidateExplanation
{
    std::string goalSummary;
    std::string strengths;
    std::string weaknesses;
    std::string decisionReason;
    std::string complexitySummary;
};

struct BalancingSynthesisCandidate
{
    EngineModel model;
    BalancingPipelineResult pipelineResult;
    BalancingSynthesisCandidateMetrics metrics;
    BalancingSynthesisCandidateExplanation explanation;

    double score = 0.0;

    std::string title;
    std::string description;
};

struct BalancingSynthesisMessage
{
    std::string message;
};

struct BalancingSynthesisResult
{
    bool ok = false;

    std::vector<BalancingSynthesisCandidate> candidates;
    std::vector<BalancingSynthesisMessage> errors;
    std::vector<BalancingSynthesisMessage> warnings;
};

} // namespace engine::balancing