#include "core/balancing/balancing_pipeline.h"

#include <cmath>
#include <cstddef>
#include <sstream>
#include <stdexcept>

#include "core/balancing/balancing_calculator.h"
#include "core/balancing/balancing_composer.h"

namespace engine::balancing
{

namespace
{

void AddError(BalancingPipelineResult& result, const std::string& message)
{
    result.errors.push_back({message});
}

void AddWarning(BalancingPipelineResult& result, const std::string& message)
{
    result.warnings.push_back({message});
}

bool IsFinite(double value)
{
    return std::isfinite(value);
}

void ValidateInputAgainstDynamicResult(const engine::dynamic::DynamicResult& dynamicResult,
                                       const BalancingInput& input,
                                       BalancingPipelineResult& result)
{
    if (input.alphaDeg.empty())
    {
        AddError(result, "Массив углов alphaDeg для расчета уравновешивания пуст.");
        return;
    }

    if (dynamicResult.alphaDeg.empty())
    {
        AddError(result, "Результат динамики не содержит угловых отсчетов.");
        return;
    }

    if (input.alphaDeg.size() != dynamicResult.alphaDeg.size())
    {
        std::ostringstream oss;
        oss << "Размер массива углов уравновешивания не совпадает с динамикой: "
            << "balancing=" << input.alphaDeg.size()
            << ", dynamic=" << dynamicResult.alphaDeg.size() << ".";
        AddError(result, oss.str());
        return;
    }

    for (std::size_t i = 0; i < input.alphaDeg.size(); ++i)
    {
        if (!IsFinite(input.alphaDeg[i]))
        {
            std::ostringstream oss;
            oss << "Массив углов alphaDeg содержит нечисловое значение на позиции " << i << ".";
            AddError(result, oss.str());
        }

        if (input.alphaDeg[i] != dynamicResult.alphaDeg[i])
        {
            std::ostringstream oss;
            oss << "Массив углов уравновешивания не совпадает с динамикой на позиции " << i
                << ": balancing=" << input.alphaDeg[i]
                << ", dynamic=" << dynamicResult.alphaDeg[i] << ".";
            AddError(result, oss.str());
            break;
        }
    }

    if (!IsFinite(input.rpm) || input.rpm <= 0.0)
    {
        AddError(result, "Частота вращения rpm для расчета уравновешивания должна быть больше нуля.");
    }

    if (!IsFinite(input.referenceX_M) ||
        !IsFinite(input.referenceY_M) ||
        !IsFinite(input.referenceZ_M))
    {
        AddError(result, "Координаты опорной точки моментов должны быть конечными числами.");
    }
}

void TransferBuilderMessages(const BalancingBuildResult& buildResult,
                             BalancingPipelineResult& pipelineResult)
{
    for (const auto& error : buildResult.errors)
    {
        AddError(pipelineResult, error.message);
    }

    for (const auto& warning : buildResult.warnings)
    {
        AddWarning(pipelineResult, warning.message);
    }
}

} // namespace

BalancingPipelineResult BalancingPipeline::Run(
    const EngineModel& sourceModel,
    const engine::dynamic::DynamicResult& dynamicResult,
    const BalancingInput& input) const
{
    BalancingPipelineResult result;

    ValidateInputAgainstDynamicResult(dynamicResult, input, result);
    if (!result.errors.empty())
    {
        result.ok = false;
        return result;
    }

    const BalancingBuildResult buildResult = BalancingModelBuilder::Build(sourceModel);
    TransferBuilderMessages(buildResult, result);

    if (!buildResult.ok)
    {
        result.ok = false;
        return result;
    }

    result.normalizedModel = buildResult.model;

    try
    {
        const BalancingCalculator calculator;
        result.balancingResult = calculator.Calculate(sourceModel, result.normalizedModel, input);

        const BalancingComposer composer;
        result.composedResult = composer.Compose(dynamicResult, result.balancingResult);
    }
    catch (const std::exception& ex)
    {
        AddError(result, ex.what());
        result.ok = false;
        return result;
    }
    catch (...)
    {
        AddError(result, "Неизвестная ошибка при расчете уравновешивания.");
        result.ok = false;
        return result;
    }

    result.ok = result.errors.empty();
    return result;
}

} // namespace engine::balancing