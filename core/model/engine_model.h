#pragma once

#include <vector>

#include "engine_types.h"
#include "balancing_model.h"

struct CrankshaftThrow
{
    int crankNumber = 0;
    double phaseDeg = 0.0;
};

struct CylinderSpec
{
    int cylinderNumber = 0;
    int crankNumber = 0;

    // Поворот оси цилиндра вокруг оси z, градусы
    double axisTiltDeg = 0.0;

    // Будет вычисляться автоматически из геометрии вала
    double axisPositionZ = 0.0;

    double firingAngleDeg = 0.0;
};

struct CrankshaftSpec
{
    int shaftNumber = 0;

    // Начало коленчатого вала в глобальной системе координат
    double originXMm = 0.0;
    double originYMm = 0.0;
    double originZMm = 0.0;

    std::vector<CrankshaftThrow> cranks;
    std::vector<CylinderSpec> cylinders;
};

struct EngineKinematicParams
{
    CycleType cycleType = CycleType::FourStroke;

    // Общие для всех коленчатых валов
    int shaftCount = 1;
    int crankCountPerShaft = 1;
    int cylindersPerCrank = 1;

    RodJointType rodJointType = RodJointType::SideBySide;
    SupportType supportType = SupportType::FullySupported;

    double rpm = 4800.0;
    double deaxialMm = 0.0;
    double crankRadiusM = 0.02;
    double lambda = 0.3;
    double alphaStepDeg = 1.0;

    // Размеры коленчатого вала
    double mainJournalLengthM = 0.030;
    double rodJournalLengthM = 0.020;
    double webThicknessM = 0.010;

    // Параметры прицепного шатуна
    double articulatedRodRadiusM = 0.010;
    double articulatedRodLengthM = 0.080;
};

struct EngineDynamicParams
{
    double cylinderDiameterM = 0.0;
    double reciprocatingMassKg = 0.0;
    double rotatingMassKg = 0.0;
};

struct EngineModel
{
    EngineKinematicParams kinematic;
    EngineDynamicParams dynamic;

    std::vector<CrankshaftSpec> shafts;

    BalancingSystem balancing;
};