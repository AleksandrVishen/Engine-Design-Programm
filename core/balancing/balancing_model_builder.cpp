#include "core/balancing/balancing_model_builder.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

namespace engine::balancing
{

namespace
{

using CrankKey = std::pair<int, int>;

void AddError(BalancingBuildResult& result, const std::string& message)
{
    result.errors.push_back({message});
}

void AddWarning(BalancingBuildResult& result, const std::string& message)
{
    result.warnings.push_back({message});
}

bool IsFinite(double value)
{
    return std::isfinite(value);
}

double NormalizeDeg(double angleDeg)
{
    double normalized = std::fmod(angleDeg, 360.0);
    if (normalized < 0.0)
    {
        normalized += 360.0;
    }
    return normalized;
}

int CountTotalCylinders(const EngineModel& source)
{
    int total = 0;
    for (const auto& shaft : source.shafts)
    {
        total += static_cast<int>(shaft.cylinders.size());
    }
    return total;
}

std::set<CrankKey> CollectExistingCrankKeys(const EngineModel& source)
{
    std::set<CrankKey> keys;

    for (const auto& shaft : source.shafts)
    {
        for (const auto& crank : shaft.cranks)
        {
            keys.insert({shaft.shaftNumber, crank.crankNumber});
        }
    }

    return keys;
}

void ValidateCrankCounterweightEntries(
    const EngineModel& source,
    BalancingBuildResult& result)
{
    const auto& system = source.balancing.crankCounterweights;
    const auto existingKeys = CollectExistingCrankKeys(source);

    std::set<CrankKey> seen;

    for (const auto& entry : system.entries)
    {
        const CrankKey key{entry.shaftNumber, entry.crankNumber};

        if (existingKeys.find(key) == existingKeys.end())
        {
            std::ostringstream oss;
            oss << "В системе противовесов коленчатого вала указана запись для "
                << "несуществующего кривошипа: shaftNumber=" << entry.shaftNumber
                << ", crankNumber=" << entry.crankNumber << ".";
            AddError(result, oss.str());
            continue;
        }

        if (!IsFinite(entry.phaseDeg))
        {
            std::ostringstream oss;
            oss << "Фаза противовеса должна быть конечным числом для "
                << "shaftNumber=" << entry.shaftNumber
                << ", crankNumber=" << entry.crankNumber << ".";
            AddError(result, oss.str());
        }

        const auto [_, inserted] = seen.insert(key);
        if (!inserted)
        {
            std::ostringstream oss;
            oss << "Обнаружена дублирующая запись противовеса для "
                << "shaftNumber=" << entry.shaftNumber
                << ", crankNumber=" << entry.crankNumber << ".";
            AddError(result, oss.str());
        }
    }
}

std::optional<double> FindUserCrankPhaseDeg(
    const CrankCounterweightSystem& system,
    int shaftNumber,
    int crankNumber)
{
    for (const auto& entry : system.entries)
    {
        if (entry.shaftNumber == shaftNumber && entry.crankNumber == crankNumber)
        {
            return entry.phaseDeg;
        }
    }

    return std::nullopt;
}

int ResolveActualCounterweightCount(
    const EngineModel& source,
    const CrankCounterweightSystem& system)
{
    const int totalCylinderCount = CountTotalCylinders(source);
    const bool isSemiSupported =
        (source.kinematic.supportType == SupportType::SemiSupported);

    if (isSemiSupported && totalCylinderCount > 1)
    {
        return 1;
    }

    if ((totalCylinderCount % 2) != 0)
    {
        return 2;
    }

    switch (system.countMode)
    {
    case CounterweightCountMode::OnePerCrank:
        return 1;

    case CounterweightCountMode::TwoPerCrank:
        return 2;

    case CounterweightCountMode::Auto:
    default:
        return 2;
    }
}

CrankCounterweightSide ResolveSingleCounterweightSide(int crankIndexWithinShaft)
{
    return ((crankIndexWithinShaft % 2) == 0)
        ? CrankCounterweightSide::Left
        : CrankCounterweightSide::Right;
}

bool IsSecondCrankInUnsupportedPair(
    const EngineModel& source,
    int crankIndexWithinShaft)
{
    const bool isSemiSupported =
        (source.kinematic.supportType == SupportType::SemiSupported);

    if (!isSemiSupported)
    {
        return false;
    }

    return ((crankIndexWithinShaft % 2) == 1);
}

double ResolveNormalizedCrankPhaseDeg(
    const EngineModel& source,
    const CrankCounterweightSystem& system,
    int shaftIndex,
    int crankIndexWithinShaft,
    int actualCount)
{
    const auto& shaft = source.shafts[shaftIndex];
    const auto& crank = shaft.cranks[crankIndexWithinShaft];

    double phaseDeg = crank.phaseDeg;

    if (const auto userPhase = FindUserCrankPhaseDeg(system, shaft.shaftNumber, crank.crankNumber))
    {
        phaseDeg = *userPhase;
    }

    if (actualCount == 1 && IsSecondCrankInUnsupportedPair(source, crankIndexWithinShaft))
    {
        const int firstInPairIndex = crankIndexWithinShaft - 1;
        const auto& firstCrank = shaft.cranks[firstInPairIndex];

        double firstPhaseDeg = firstCrank.phaseDeg;

        if (const auto firstUserPhase =
                FindUserCrankPhaseDeg(system, shaft.shaftNumber, firstCrank.crankNumber))
        {
            firstPhaseDeg = *firstUserPhase;
        }

        phaseDeg = firstPhaseDeg + 180.0;
    }

    return NormalizeDeg(phaseDeg);
}

void ValidateIgnoredUserPhasesForUnsupportedPairs(
    const EngineModel& source,
    BalancingBuildResult& result,
    int actualCount)
{
    if (actualCount != 1)
    {
        return;
    }

    const bool isSemiSupported =
        (source.kinematic.supportType == SupportType::SemiSupported);

    if (!isSemiSupported)
    {
        return;
    }

    const auto& system = source.balancing.crankCounterweights;

    for (std::size_t shaftIndex = 0; shaftIndex < source.shafts.size(); ++shaftIndex)
    {
        const auto& shaft = source.shafts[shaftIndex];

        for (std::size_t crankIndex = 0; crankIndex < shaft.cranks.size(); ++crankIndex)
        {
            if (!IsSecondCrankInUnsupportedPair(source, static_cast<int>(crankIndex)))
            {
                continue;
            }

            const auto& crank = shaft.cranks[crankIndex];
            if (FindUserCrankPhaseDeg(system, shaft.shaftNumber, crank.crankNumber).has_value())
            {
                std::ostringstream oss;
                oss << "Фаза для второго кривошипа бескоренной пары игнорируется: "
                    << "shaftNumber=" << shaft.shaftNumber
                    << ", crankNumber=" << crank.crankNumber
                    << ". Она автоматически вычисляется как фаза первого кривошипа пары + 180°.";
                AddWarning(result, oss.str());
            }
        }
    }
}

void BuildCrankCounterweights(
    const EngineModel& source,
    BalancingBuildResult& result,
    NormalizedBalancingModel& normalized)
{
    const auto& system = source.balancing.crankCounterweights;

    if (!system.enabled)
    {
        return;
    }

    ValidateCrankCounterweightEntries(source, result);

    if (!IsFinite(system.massKg))
{
    AddError(
        result,
        "Масса противовеса на коленчатом валу должна быть конечным числом.");
}

if (!IsFinite(system.radiusMm))
{
    AddError(
        result,
        "Радиус противовеса на коленчатом валу должен быть конечным числом.");
}

if (!result.errors.empty())
{
    return;
}

if (system.massKg < 0.0)
{
    AddError(
        result,
        "Масса противовеса на коленчатом валу не может быть отрицательной.");
}

if (system.radiusMm < 0.0)
{
    AddError(
        result,
        "Радиус противовеса на коленчатом валу не может быть отрицательным.");
}

if (!result.errors.empty())
{
    return;
}

const bool massIsZero = (system.massKg == 0.0);
const bool radiusIsZero = (system.radiusMm == 0.0);

if (massIsZero && radiusIsZero)
{
    // Пользователь не задавал противовесы на продолжении щек.
    // Это допустимый случай: просто не строим нормализованные противовесы.
    return;
}

if (massIsZero != radiusIsZero)
{
    AddError(
        result,
        "Для противовесов на коленчатом валу масса и радиус должны быть "
        "либо оба равны нулю, либо оба быть больше нуля.");
    return;
}

    const int actualCount = ResolveActualCounterweightCount(source, system);

    ValidateIgnoredUserPhasesForUnsupportedPairs(source, result, actualCount);

    for (std::size_t shaftIndex = 0; shaftIndex < source.shafts.size(); ++shaftIndex)
    {
        const auto& shaft = source.shafts[shaftIndex];

        for (std::size_t crankIndex = 0; crankIndex < shaft.cranks.size(); ++crankIndex)
        {
            const auto& crank = shaft.cranks[crankIndex];

            NormalizedCrankCounterweight item;
            item.enabled = true;
            item.shaftNumber = shaft.shaftNumber;
            item.crankNumber = crank.crankNumber;
            item.phaseDeg = ResolveNormalizedCrankPhaseDeg(
                source,
                system,
                static_cast<int>(shaftIndex),
                static_cast<int>(crankIndex),
                actualCount);
            item.massKg = system.massKg;
            item.radiusMm = system.radiusMm;
            item.count = actualCount;
            item.side = (actualCount == 1)
                ? ResolveSingleCounterweightSide(static_cast<int>(crankIndex))
                : CrankCounterweightSide::Both;

            normalized.crankCounterweights.push_back(item);
        }
    }
}

void ValidateBalancerShaftCounterweightPositions(
    const BalancerShaftSpec& shaft,
    BalancingBuildResult& result,
    int shaftIndex)
{
    for (std::size_t counterweightIndex = 0;
         counterweightIndex < shaft.counterweights.size();
         ++counterweightIndex)
    {
        const auto& cw = shaft.counterweights[counterweightIndex];

        if (!IsFinite(cw.positionAlongShaftMm))
        {
            std::ostringstream oss;
            oss << "Положение противовеса на дополнительном валу должно быть конечным числом: "
                << "shaftIndex=" << shaftIndex
                << ", counterweightIndex=" << static_cast<int>(counterweightIndex) << ".";
            AddError(result, oss.str());
            continue;
        }

        if (cw.positionAlongShaftMm < 0.0 || cw.positionAlongShaftMm > shaft.lengthMm)
        {
            std::ostringstream oss;
            oss << "Положение противовеса выходит за пределы длины дополнительного вала: "
                << "shaftIndex=" << shaftIndex
                << ", counterweightIndex=" << static_cast<int>(counterweightIndex)
                << ", positionAlongShaftMm=" << cw.positionAlongShaftMm
                << ", lengthMm=" << shaft.lengthMm << ".";
            AddError(result, oss.str());
        }

        if (!IsFinite(cw.phaseDeg))
        {
            std::ostringstream oss;
            oss << "Фаза противовеса на дополнительном валу должна быть конечным числом: "
                << "shaftIndex=" << shaftIndex
                << ", counterweightIndex=" << static_cast<int>(counterweightIndex) << ".";
            AddError(result, oss.str());
        }
    }
}

void BuildBalancerShaftCounterweights(
    const EngineModel& source,
    BalancingBuildResult& result,
    NormalizedBalancingModel& normalized)
{
    for (std::size_t shaftIndex = 0; shaftIndex < source.balancing.balancerShafts.size(); ++shaftIndex)
    {
        const auto& shaft = source.balancing.balancerShafts[shaftIndex];

        if (!shaft.enabled)
        {
            continue;
        }

        if (!IsFinite(shaft.originXMm) || !IsFinite(shaft.originYMm) || !IsFinite(shaft.originZMm))
        {
            std::ostringstream oss;
            oss << "Координаты начала дополнительного вала должны быть конечными числами: "
                << "shaftIndex=" << static_cast<int>(shaftIndex) << ".";
            AddError(result, oss.str());
        }

        if (!IsFinite(shaft.lengthMm) || shaft.lengthMm <= 0.0)
        {
            std::ostringstream oss;
            oss << "Длина дополнительного вала должна быть больше нуля: "
                << "shaftIndex=" << static_cast<int>(shaftIndex) << ".";
            AddError(result, oss.str());
        }

        if (!IsFinite(shaft.shaftPhaseDeg))
        {
            std::ostringstream oss;
            oss << "Фаза дополнительного вала должна быть конечным числом: "
                << "shaftIndex=" << static_cast<int>(shaftIndex) << ".";
            AddError(result, oss.str());
        }

        if (!IsFinite(shaft.counterweightMassKg) || shaft.counterweightMassKg <= 0.0)
        {
            std::ostringstream oss;
            oss << "Масса противовеса дополнительного вала должна быть больше нуля: "
                << "shaftIndex=" << static_cast<int>(shaftIndex) << ".";
            AddError(result, oss.str());
        }

        if (!IsFinite(shaft.counterweightRadiusMm) || shaft.counterweightRadiusMm <= 0.0)
        {
            std::ostringstream oss;
            oss << "Радиус противовеса дополнительного вала должен быть больше нуля: "
                << "shaftIndex=" << static_cast<int>(shaftIndex) << ".";
            AddError(result, oss.str());
        }

        ValidateBalancerShaftCounterweightPositions(
            shaft,
            result,
            static_cast<int>(shaftIndex));

        for (std::size_t counterweightIndex = 0;
             counterweightIndex < shaft.counterweights.size();
             ++counterweightIndex)
        {
            const auto& cw = shaft.counterweights[counterweightIndex];

            NormalizedBalancerCounterweight item;
            item.shaftIndex = static_cast<int>(shaftIndex);
            item.counterweightIndex = static_cast<int>(counterweightIndex);

            item.originXMm = shaft.originXMm;
            item.originYMm = shaft.originYMm;
            item.originZMm = shaft.originZMm;

            item.axis = shaft.axis;
            item.lengthMm = shaft.lengthMm;

            item.speedRatio = shaft.speedRatio;
            item.shaftPhaseDeg = NormalizeDeg(shaft.shaftPhaseDeg);

            item.massKg = shaft.counterweightMassKg;
            item.radiusMm = shaft.counterweightRadiusMm;

            item.positionAlongShaftMm = cw.positionAlongShaftMm;
            item.phaseDeg = NormalizeDeg(cw.phaseDeg);

            item.centerXmm = shaft.originXMm;
            item.centerYmm = shaft.originYMm;
            item.centerZmm = shaft.originZMm;

            switch (shaft.axis)
            {
            case BalancerAxis::X:
                item.centerXmm += cw.positionAlongShaftMm;
                break;

            case BalancerAxis::Y:
                item.centerYmm += cw.positionAlongShaftMm;
                break;

            case BalancerAxis::Z:
                item.centerZmm += cw.positionAlongShaftMm;
                break;
            }

            normalized.balancerCounterweights.push_back(item);
        }
    }
}

} // namespace

BalancingBuildResult BalancingModelBuilder::Build(const EngineModel& source)
{
    BalancingBuildResult result;
    NormalizedBalancingModel normalized;

    if (source.shafts.empty())
    {
        AddError(result, "В модели отсутствуют коленчатые валы.");
    }

    BuildCrankCounterweights(source, result, normalized);
    BuildBalancerShaftCounterweights(source, result, normalized);

    result.ok = result.errors.empty();
    result.model = std::move(normalized);
    return result;
}

} // namespace engine::balancing