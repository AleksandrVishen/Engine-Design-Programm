#pragma once

#include <string>
#include <vector>

#include <wx/bitmap.h>
#include <wx/string.h>

class wxWindow;

#include "core/kinematic/kinematic_result.h"
#include "core/model/engine_model.h"

namespace engine::dynamic
{
struct DynamicResult;
}
namespace engine::balancing
{
struct BalancingPipelineResult;
}

struct ReportSeriesDescriptor
{
    std::string id;
    wxString displayName;
    wxString unit;
    const std::vector<double>* values = nullptr;
};

struct ReportSeriesOptions
{
    std::string seriesId;
    bool includeSeries = false;
    bool includeChart = false;
    bool includeStats = false;
    bool includeTable = false;
};

struct ReportOptions
{
    /// Indices into `result.cylinders` (0-based). Order defines section order in the report.
    std::vector<int> selectedCylinderIndices;
    std::vector<ReportSeriesOptions> seriesOptions;
    bool includeCombinedTable = false;
    int tableStride = 5;
    std::vector<std::string> combinedTableSeriesIds;
    /// Таблицы сил и моментов по динамике (если передан `DynamicResult`).
    bool includeDynamicForces = false;
    /// Сводные таблицы уравновешивания (если передан успешный `BalancingPipelineResult`).
    bool includeBalancing = false;
};

struct ReportBuildResult
{
    wxString html;
    wxString plainText;
};

/// Как вставлять графики в HTML: wxHtmlWindow и wxHtmlPrintout не загружают data:image/png;base64 в теге img.
enum class ReportHtmlImageMode
{
    DataUriForBrowserFile,
    WxMemoryFilesystemForHtmlWindow
};

class ReportBuilder
{
public:
    static std::vector<ReportSeriesDescriptor> BuildSeriesDescriptors(
        const engine::kinematic::CylinderKinematicSeries& cylinder);

    static ReportBuildResult BuildHtml(
        const EngineModel& model,
        const engine::kinematic::KinematicResult& result,
        const ReportOptions& options,
        ReportHtmlImageMode imageMode = ReportHtmlImageMode::WxMemoryFilesystemForHtmlWindow,
        const engine::dynamic::DynamicResult* dynamicResult = nullptr,
        const engine::balancing::BalancingPipelineResult* balancingPipelineResult = nullptr);

    /// Удаляет PNG из виртуальной ФС (memory:), зарегистрированные для последнего отчёта в режиме memory.
    static void ClearReportMemoryImages();

    static ReportBuildResult BuildPlainText(
        const EngineModel& model,
        const engine::kinematic::KinematicResult& result,
        const ReportOptions& options,
        const engine::dynamic::DynamicResult* dynamicResult = nullptr,
        const engine::balancing::BalancingPipelineResult* balancingPipelineResult = nullptr);

    /// PDF export: on Windows tries «Microsoft Print to PDF» to `pdfPath`. If that fails, opens the print dialog.
    /// When `savedToRequestedPathOut` is non-null, it is set to true only if output should be at `pdfPath`.
    static bool ExportHtmlToPdf(
        wxWindow* parent,
        const wxString& html,
        const wxString& pdfPath,
        bool* savedToRequestedPathOut = nullptr);
};
