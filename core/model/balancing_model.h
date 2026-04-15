#pragma once

#include <vector>

struct CrankCounterweightEntry
{
    int shaftNumber = 0;
    int crankNumber = 0;

    // Геометрическая фаза противовеса данного кривошипа.
    // На первом этапе хранится явно, чтобы GUI и нормализатор
    // могли ею управлять независимо.
    double phaseDeg = 0.0;
};

enum class CounterweightCountMode
{
    Auto = 0,
    OnePerCrank,
    TwoPerCrank
};

enum class BalancerAxis
{
    X = 0,
    Y,
    Z
};

enum class BalancerSpeedRatio
{
    Plus1W = 0,
    Plus2W,
    Minus1W,
    Minus2W
};

struct CrankCounterweightSystem
{
    bool enabled = false;

    // Масса одного противовеса на продолжении щеки
    double massKg = 0.0;

    // Радиус от оси коленчатого вала до центра масс противовеса
    double radiusMm = 0.0;

    // Пользовательский режим. Реальный режим определяется после нормализации.
    CounterweightCountMode countMode = CounterweightCountMode::Auto;

    // Геометрические фазы задаются по конкретным кривошипам конкретных валов.
    std::vector<CrankCounterweightEntry> entries;
};

struct BalancerCounterweightSpec
{
    // Координата центра масс вдоль оси дополнительного вала
    // относительно начала вала.
    double positionAlongShaftMm = 0.0;

    // Геометрическая фаза данного противовеса.
    double phaseDeg = 0.0;
};

struct BalancerShaftSpec
{
    bool enabled = true;

    // Начало дополнительного вала в глобальной системе координат:
    // левый конец вала.
    double originXMm = 0.0;
    double originYMm = 0.0;
    double originZMm = 0.0;

    BalancerAxis axis = BalancerAxis::Z;
    double lengthMm = 0.0;

    // Скорость относительно главного коленчатого вала.
    BalancerSpeedRatio speedRatio = BalancerSpeedRatio::Plus1W;

    // Пока не выводится в GUI, но закладывается в модель.
    double shaftPhaseDeg = 0.0;

    // Общие для всех противовесов данного вала.
    double counterweightMassKg = 0.0;
    double counterweightRadiusMm = 0.0;

    std::vector<BalancerCounterweightSpec> counterweights;
};

struct BalancingSystem
{
    CrankCounterweightSystem crankCounterweights;
    std::vector<BalancerShaftSpec> balancerShafts;
};