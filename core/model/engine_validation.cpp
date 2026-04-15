#include "core/model/engine_validation.h"

#include <cmath>

namespace
{
    void AddError(ValidationResult& result, const std::string& text)
    {
        result.ok = false;
        result.errors.push_back({ text });
    }

    double NormalizeDeg(double angleDeg)
    {
        double normalized = std::fmod(angleDeg, 360.0);
        if (normalized < 0.0)
            normalized += 360.0;
        return normalized;
    }

    double NormalizeSignedDeg(double angleDeg)
    {
        double normalized = NormalizeDeg(angleDeg);
        if (normalized > 180.0)
            normalized -= 360.0;
        return normalized;
    }

    bool IsAngleDifference180(double firstDeg, double secondDeg)
    {
        const double diff = NormalizeSignedDeg(secondDeg - firstDeg);
        return std::abs(std::abs(diff) - 180.0) <= 1e-9;
    }
}

ValidationResult EngineValidation::ValidateForCalculation(const EngineModel& model)
{
    ValidationResult result;
    result.ok = true;

    if (model.kinematic.shaftCount <= 0)
        AddError(result, "Количество коленчатых валов должно быть больше нуля.");

    if (model.kinematic.crankCountPerShaft <= 0)
        AddError(result, "Количество кривошипов должно быть больше нуля.");

    if (model.kinematic.cylindersPerCrank <= 0)
        AddError(result, "Количество цилиндров на шатунную шейку должно быть больше нуля.");

    if (model.kinematic.rpm <= 0.0)
        AddError(result, "Частота вращения должна быть больше нуля.");

    if (model.kinematic.crankRadiusM <= 0.0)
        AddError(result, "Радиус кривошипа должен быть больше нуля.");

    if (model.kinematic.lambda <= 0.0)
        AddError(result, "Лямбда должна быть больше нуля.");

    if (model.kinematic.mainJournalLengthM <= 0.0)
        AddError(result, "Длина коренной шейки должна быть больше нуля.");

    if (model.kinematic.rodJournalLengthM <= 0.0)
        AddError(result, "Длина шатунной шейки должна быть больше нуля.");

    if (model.kinematic.webThicknessM <= 0.0)
        AddError(result, "Толщина щеки должна быть больше нуля.");

    if (model.kinematic.rodJointType == RodJointType::Articulated)
    {
        if (model.kinematic.articulatedRodRadiusM <= 0.0)
            AddError(result, "Радиус прицепного шатуна должен быть больше нуля.");

        if (model.kinematic.articulatedRodLengthM <= 0.0)
            AddError(result, "Длина прицепного шатуна должна быть больше нуля.");

        if (model.kinematic.cylindersPerCrank < 2)
            AddError(result, "Прицепной шатун имеет смысл только при количестве цилиндров на шатунную шейку больше 1.");
    }

    if (static_cast<int>(model.shafts.size()) != model.kinematic.shaftCount)
        AddError(result, "Количество описаний коленчатых валов не совпадает с заданным количеством коленчатых валов.");

    const int expectedCrankCount = model.kinematic.crankCountPerShaft;
    const int expectedCylinderCountPerShaft =
        model.kinematic.crankCountPerShaft * model.kinematic.cylindersPerCrank;

    for (size_t shaftIndex = 0; shaftIndex < model.shafts.size(); ++shaftIndex)
    {
        const auto& shaft = model.shafts[shaftIndex];

        if (shaft.shaftNumber <= 0)
            AddError(result, "Номер коленчатого вала должен быть больше нуля.");

        if (static_cast<int>(shaft.cranks.size()) != expectedCrankCount)
        {
            AddError(
                result,
                "Количество кривошипов на одном из коленчатых валов не совпадает с общим заданным количеством.");
        }

        if (static_cast<int>(shaft.cylinders.size()) != expectedCylinderCountPerShaft)
        {
            AddError(
                result,
                "Количество цилиндров на одном из коленчатых валов не совпадает с ожидаемым количеством.");
        }

        for (size_t i = 0; i < shaft.cranks.size(); ++i)
        {
            const auto& crank = shaft.cranks[i];

            if (crank.crankNumber != static_cast<int>(i + 1))
            {
                AddError(
                    result,
                    "Нумерация кривошипов должна быть последовательной и начинаться с 1 на каждом коленчатом валу.");
                break;
            }

            if (!std::isfinite(crank.phaseDeg))
            {
                AddError(result, "Фаза кривошипа должна быть конечным числом.");
                break;
            }
        }

        if (model.kinematic.supportType == SupportType::SemiSupported)
        {
            for (size_t i = 1; i < shaft.cranks.size(); i += 2)
            {
                const double firstPhase = shaft.cranks[i - 1].phaseDeg;
                const double secondPhase = shaft.cranks[i].phaseDeg;

                if (!IsAngleDifference180(firstPhase, secondPhase))
                {
                    AddError(
                        result,
                        "Для неполноопорного вала второй кривошип каждой бескоренной пары "
                        "должен иметь геометрическую фазу, отличающуюся на 180° от первого.");
                    break;
                }
            }
        }

        for (size_t i = 0; i < shaft.cylinders.size(); ++i)
        {
            const auto& cylinder = shaft.cylinders[i];

            if (cylinder.cylinderNumber != static_cast<int>(i + 1))
            {
                AddError(
                    result,
                    "Нумерация цилиндров должна быть последовательной и начинаться с 1 на каждом коленчатом валу.");
                break;
            }

            if (cylinder.crankNumber <= 0 || cylinder.crankNumber > expectedCrankCount)
            {
                AddError(result, "Номер кривошипа у цилиндра выходит за допустимые пределы.");
                break;
            }

            if (!std::isfinite(cylinder.axisTiltDeg))
            {
                AddError(result, "Смещение оси цилиндра должно быть конечным числом.");
                break;
            }
        }
    }

    return result;
}