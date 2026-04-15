#include "core/kinematic/kinematic_model_builder.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace engine::kinematic
{

namespace
{
bool IsFinite(double value)
{
    return std::isfinite(value);
}
}

void KinematicModelBuilder::AddIssue(KinematicBuildResult& result, const std::string& message)
{
    result.issues.push_back({message});
}

KinematicBuildResult KinematicModelBuilder::Build(const EngineModel& source)
{
    KinematicBuildResult result;
    NormalizedKinematicModel normalized;

    normalized.alphaStartDeg = 0.0;
    normalized.alphaEndDeg =
        (source.kinematic.cycleType == CycleType::FourStroke) ? 720.0 : 360.0;
    normalized.alphaStepDeg = source.kinematic.alphaStepDeg;
    normalized.rpm = source.kinematic.rpm;
    normalized.mainCrankRadiusM = source.kinematic.crankRadiusM;
    normalized.deaxialMm = source.kinematic.deaxialMm;

    // Пространственные параметры для 3D-геометрии коленчатого вала
    normalized.supportType = source.kinematic.supportType;
    normalized.mainJournalLengthM = source.kinematic.mainJournalLengthM;
    normalized.rodJournalLengthM = source.kinematic.rodJournalLengthM;
    normalized.webThicknessM = source.kinematic.webThicknessM;

    if (normalized.alphaStepDeg <= 0.0)
    {
        AddIssue(result, "Шаг alpha должен быть больше нуля.");
    }

    if (normalized.rpm <= 0.0)
    {
        AddIssue(result, "Частота вращения должна быть больше нуля.");
    }

    if (normalized.mainCrankRadiusM <= 0.0)
    {
        AddIssue(result, "Радиус основного кривошипа должен быть больше нуля.");
    }

    if (normalized.mainJournalLengthM <= 0.0)
    {
        AddIssue(result, "Длина коренной шейки должна быть больше нуля.");
    }

    if (normalized.rodJournalLengthM <= 0.0)
    {
        AddIssue(result, "Длина шатунной шейки должна быть больше нуля.");
    }

    if (normalized.webThicknessM <= 0.0)
    {
        AddIssue(result, "Толщина щёки кривошипа должна быть больше нуля.");
    }

    if (source.shafts.empty())
    {
        AddIssue(result, "В модели отсутствуют коленчатые валы.");
    }

    for (const auto& srcShaft : source.shafts)
    {
        NormalizedShaft shaft;
        shaft.shaftNumber = srcShaft.shaftNumber;
        shaft.originXMm = srcShaft.originXMm;
        shaft.originYMm = srcShaft.originYMm;
        shaft.originZMm = srcShaft.originZMm;

        if (!IsFinite(shaft.originXMm) || !IsFinite(shaft.originYMm) || !IsFinite(shaft.originZMm))
        {
            AddIssue(result, "Координаты начала коленчатого вала должны быть конечными числами.");
        }

        if (srcShaft.cranks.empty())
        {
            AddIssue(result, "У одного из коленчатых валов отсутствуют кривошипы.");
            continue;
        }

        for (const auto& srcCrank : srcShaft.cranks)
        {
            NormalizedThrow throwItem;
            throwItem.crankNumber = srcCrank.crankNumber;
            throwItem.phaseDeg = srcCrank.phaseDeg;

            if (!IsFinite(throwItem.phaseDeg))
            {
                AddIssue(result, "Фаза кривошипа должна быть конечным числом.");
            }

            std::vector<const CylinderSpec*> group;
            for (const auto& cyl : srcShaft.cylinders)
            {
                if (cyl.crankNumber == srcCrank.crankNumber)
                {
                    group.push_back(&cyl);
                }
            }

            std::sort(group.begin(), group.end(),
                [](const CylinderSpec* a, const CylinderSpec* b)
                {
                    return a->cylinderNumber < b->cylinderNumber;
                });

            if (group.empty())
            {
                AddIssue(result, "У одного из кривошипов отсутствуют связанные цилиндры.");
            }

            for (std::size_t i = 0; i < group.size(); ++i)
            {
                const auto* srcCyl = group[i];
                if (srcCyl == nullptr)
                {
                    AddIssue(result, "Внутренняя ошибка построения группы цилиндров.");
                    continue;
                }

                NormalizedCylinderLink link;
                link.cylinderNumber = srcCyl->cylinderNumber;
                link.groupOrder = static_cast<int>(i);
                link.axisTiltDeg = srcCyl->axisTiltDeg;
                link.axisOffsetZMm = srcCyl->axisPositionZ;

                if (!IsFinite(link.axisTiltDeg) || !IsFinite(link.axisOffsetZMm))
                {
                    AddIssue(result, "Параметры оси цилиндра должны быть конечными числами.");
                }

                if (source.kinematic.rodJointType == RodJointType::SideBySide)
                {
                    link.linkType = CylinderLinkType::Main;
                }
                else
                {
                    link.linkType = (i == 0)
                        ? CylinderLinkType::Main
                        : CylinderLinkType::Articulated;
                }

                if (source.kinematic.lambda <= 0.0)
                {
                    AddIssue(result, "Параметр lambda должен быть больше нуля для построения расчетной модели.");
                }
                else
                {
                    link.mainRodLengthM = normalized.mainCrankRadiusM / source.kinematic.lambda;
                }

                if (link.linkType == CylinderLinkType::Articulated)
                {
                    link.articulatedCrankRadiusM = source.kinematic.articulatedRodRadiusM;
                    link.articulatedRodLengthM = source.kinematic.articulatedRodLengthM;

                    if (link.articulatedCrankRadiusM <= 0.0)
                    {
                        AddIssue(result, "Для прицепного шатуна радиус должен быть больше нуля.");
                    }

                    if (link.articulatedRodLengthM <= 0.0)
                    {
                        AddIssue(result, "Для прицепного шатуна длина должна быть больше нуля.");
                    }
                }
                else
                {
                    link.articulatedCrankRadiusM = 0.0;
                    link.articulatedRodLengthM = 0.0;
                }

                throwItem.links.push_back(link);
            }

            shaft.throws.push_back(throwItem);
        }

        normalized.shafts.push_back(shaft);
    }

    result.ok = result.issues.empty();
    result.model = std::move(normalized);
    return result;
}

} // namespace engine::kinematic