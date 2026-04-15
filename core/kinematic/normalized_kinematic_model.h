#pragma once

#include <vector>

#include "core/model/engine_types.h"

namespace engine::kinematic
{

enum class CylinderLinkType
{
    Main,
    Articulated
};

struct NormalizedCylinderLink
{
    int cylinderNumber = 0;

    // Тип связи цилиндра с шатунной шейкой:
    // главный шатун или прицепной
    CylinderLinkType linkType = CylinderLinkType::Main;

    // Порядок цилиндра внутри группы одной шатунной шейки:
    // 0 - главный, 1..N - остальные
    int groupOrder = 0;

    // Угол оси цилиндра
    double axisTiltDeg = 0.0;

    // Смещение оси цилиндра вдоль локальной оси Z данного КВ
    double axisOffsetZMm = 0.0;

    // Длина главного шатуна
    double mainRodLengthM = 0.0;

    // Параметры прицепного шатуна.
    // Для главного цилиндра остаются нулями.
    double articulatedCrankRadiusM = 0.0;
    double articulatedRodLengthM = 0.0;
};

struct NormalizedThrow
{
    int crankNumber = 0;
    double phaseDeg = 0.0;

    std::vector<NormalizedCylinderLink> links;
};

struct NormalizedShaft
{
    int shaftNumber = 0;

    double originXMm = 0.0;
    double originYMm = 0.0;
    double originZMm = 0.0;

    std::vector<NormalizedThrow> throws;
};

struct NormalizedKinematicModel
{
    double alphaStartDeg = 0.0;
    double alphaEndDeg = 720.0;
    double alphaStepDeg = 1.0;

    double rpm = 0.0;
    double mainCrankRadiusM = 0.0;
    double deaxialMm = 0.0;

    // Пространственные параметры коленчатого вала,
    // необходимые для вычисления реального положения шатунной шейки по оси Z
    SupportType supportType = SupportType::FullySupported;
    double mainJournalLengthM = 0.0;
    double rodJournalLengthM = 0.0;
    double webThicknessM = 0.0;

    std::vector<NormalizedShaft> shafts;
};

} // namespace engine::kinematic