#pragma once

#include <vector>

#include <wx/dialog.h>

#include "core/kinematic/kinematic_result.h"
#include "core/model/engine_model.h"
#include "gui/report/report_builder.h"

namespace engine::dynamic
{
struct DynamicResult;
}
namespace engine::balancing
{
struct BalancingPipelineResult;
}

class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxHtmlWindow;
class wxScrolledWindow;
class wxSpinCtrl;
class wxStaticText;

class ReportDialog : public wxDialog
{
public:
    ReportDialog(
        wxWindow* parent,
        const EngineModel& model,
        const engine::kinematic::KinematicResult& result,
        const engine::dynamic::DynamicResult* dynamicResult = nullptr,
        const engine::balancing::BalancingPipelineResult* balancingPipelineResult = nullptr);
    ~ReportDialog() override;

private:
    struct SeriesRow
    {
        std::string seriesId;
        wxCheckBox* includeSeries = nullptr;
        wxCheckBox* includeChart = nullptr;
        wxCheckBox* includeStats = nullptr;
        wxCheckBox* includeTable = nullptr;
    };

    void BuildUi();
    void RebuildSeriesRows();
    void UpdatePreview();
    ReportOptions CollectOptions() const;
    void OnCylinderChanged(wxCommandEvent& event);
    void OnSaveReport(wxCommandEvent& event);
    void OnCloseDialog(wxCommandEvent& event);
    void OnRefreshPreview(wxCommandEvent& event);

private:
    EngineModel m_model;
    engine::kinematic::KinematicResult m_result;
    const engine::dynamic::DynamicResult* m_dynamicResult = nullptr;
    const engine::balancing::BalancingPipelineResult* m_balancingPipelineResult = nullptr;
    std::vector<ReportSeriesDescriptor> m_descriptors;
    std::vector<SeriesRow> m_seriesRows;

    wxCheckListBox* m_cylinderList = nullptr;
    wxScrolledWindow* m_seriesPanel = nullptr;
    wxCheckBox* m_includeCombinedTableCheck = nullptr;
    wxCheckListBox* m_combinedColumnsCheckList = nullptr;
    wxSpinCtrl* m_strideSpin = nullptr;
    wxCheckBox* m_includeDynamicForcesCheck = nullptr;
    wxCheckBox* m_includeBalancingCheck = nullptr;
    wxHtmlWindow* m_preview = nullptr;
};
