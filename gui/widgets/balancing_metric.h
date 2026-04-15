#pragma once

enum class BalancingMetric
{
    InertiaForce = 0,
    InertiaForceFirstOrder,
    InertiaForceSecondOrder,
    InertiaMomentFirstOrder,
    InertiaMomentSecondOrder,
    CentrifugalForce,
    CentrifugalMoment
};

enum class BalancingViewMode
{
    Source = 0,
    Counterweight,
    Residual
};