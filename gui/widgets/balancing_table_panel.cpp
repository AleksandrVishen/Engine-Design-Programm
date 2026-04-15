#include "gui/widgets/balancing_table_panel.h"

#include <algorithm>
#include <cmath>

#include <wx/grid.h>
#include <wx/sizer.h>

#include "gui/common/text_utf8.h"

BalancingTablePanel::BalancingTablePanel(wxWindow* parent)
    : wxPanel(parent)
{
    SetBackgroundColour(wxColour(18, 24, 36));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    m_grid = new wxGrid(this, wxID_ANY);
    m_grid->CreateGrid(0, 0);

    m_grid->EnableEditing(false);
    m_grid->EnableDragGridSize(false);
    m_grid->EnableDragRowSize(false);
    m_grid->EnableDragColSize(true);

    m_grid->SetRowLabelSize(60);
    m_grid->SetColLabelAlignment(wxALIGN_CENTER, wxALIGN_CENTER);
    m_grid->SetDefaultCellAlignment(wxALIGN_RIGHT, wxALIGN_CENTER);

    m_grid->SetDefaultCellBackgroundColour(wxColour(24, 30, 44));
    m_grid->SetDefaultCellTextColour(wxColour(235, 235, 235));
    m_grid->SetLabelBackgroundColour(wxColour(32, 40, 58));
    m_grid->SetLabelTextColour(wxColour(245, 245, 245));
    m_grid->SetGridLineColour(wxColour(70, 80, 100));

    root->Add(m_grid, 1, wxEXPAND);
    SetSizer(root);

    RebuildGrid();
}

void BalancingTablePanel::SetResult(const engine::balancing::BalancingComposedResult& result)
{
    m_result = result;
    m_currentAlphaIndex = 0;
    RebuildGrid();
}

void BalancingTablePanel::SetMetric(BalancingMetric metric)
{
    if (m_metric == metric)
        return;

    m_metric = metric;
    RebuildGrid();
}

void BalancingTablePanel::SetViewMode(BalancingViewMode viewMode)
{
    if (m_viewMode == viewMode)
        return;

    m_viewMode = viewMode;
    RebuildGrid();
}

void BalancingTablePanel::SetComponent(BalancingComponent component)
{
    if (m_component == component)
        return;

    m_component = component;
    RebuildGrid();
}

void BalancingTablePanel::SetCurrentAlphaIndex(std::size_t index)
{
    if (m_result.alphaDeg.empty() || !m_grid)
        return;

    m_currentAlphaIndex = std::min(index, m_result.alphaDeg.size() - 1);

    m_grid->ClearSelection();
    if (m_currentAlphaIndex < static_cast<std::size_t>(m_grid->GetNumberRows()))
    {
        m_grid->SelectRow(static_cast<int>(m_currentAlphaIndex), false);
        m_grid->MakeCellVisible(static_cast<int>(m_currentAlphaIndex), 0);
    }
}

const std::vector<engine::kinematic::Vec3>* BalancingTablePanel::GetSelectedSeriesValues() const
{
    switch (m_viewMode)
    {
    case BalancingViewMode::Source:
        switch (m_metric)
        {
        case BalancingMetric::InertiaForce: return &m_result.sourceInertiaForce;
        case BalancingMetric::InertiaForceFirstOrder: return &m_result.sourceInertiaForce1;
        case BalancingMetric::InertiaForceSecondOrder: return &m_result.sourceInertiaForce2;
        case BalancingMetric::InertiaMomentFirstOrder: return &m_result.sourceInertiaMoment1;
        case BalancingMetric::InertiaMomentSecondOrder: return &m_result.sourceInertiaMoment2;
        case BalancingMetric::CentrifugalForce: return &m_result.sourceCentrifugalForce;
        case BalancingMetric::CentrifugalMoment: return &m_result.sourceCentrifugalMoment;
        }
        break;

    case BalancingViewMode::Counterweight:
        switch (m_metric)
        {
        case BalancingMetric::InertiaForce: return &m_result.counterweightInertiaForce;
        case BalancingMetric::InertiaForceFirstOrder: return &m_result.balancerInertiaForce1;
        case BalancingMetric::InertiaForceSecondOrder: return &m_result.balancerInertiaForce2;
        case BalancingMetric::InertiaMomentFirstOrder: return &m_result.balancerInertiaMoment1;
        case BalancingMetric::InertiaMomentSecondOrder: return &m_result.balancerInertiaMoment2;
        case BalancingMetric::CentrifugalForce: return &m_result.counterweightCentrifugalForce;
        case BalancingMetric::CentrifugalMoment: return &m_result.counterweightCentrifugalMoment;
        }
        break;

    case BalancingViewMode::Residual:
        switch (m_metric)
        {
        case BalancingMetric::InertiaForce: return &m_result.residualInertiaForce;
        case BalancingMetric::InertiaForceFirstOrder: return &m_result.residualInertiaForce1;
        case BalancingMetric::InertiaForceSecondOrder: return &m_result.residualInertiaForce2;
        case BalancingMetric::InertiaMomentFirstOrder: return &m_result.residualInertiaMoment1;
        case BalancingMetric::InertiaMomentSecondOrder: return &m_result.residualInertiaMoment2;
        case BalancingMetric::CentrifugalForce: return &m_result.residualCentrifugalForce;
        case BalancingMetric::CentrifugalMoment: return &m_result.residualCentrifugalMoment;
        }
        break;
    }

    return nullptr;
}

double BalancingTablePanel::ExtractComponent(const engine::kinematic::Vec3& v) const
{
    switch (m_component)
    {
    case BalancingComponent::X:
        return v.x;
    case BalancingComponent::Y:
        return v.y;
    case BalancingComponent::Z:
        return v.z;
    case BalancingComponent::Magnitude:
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    return 0.0;
}

wxString BalancingTablePanel::GetColumnLabel() const
{
    wxString metricText;
    switch (m_metric)
    {
    case BalancingMetric::InertiaForce: metricText = WXU8("F"); break;
    case BalancingMetric::InertiaForceFirstOrder: metricText = WXU8("F1"); break;
    case BalancingMetric::InertiaForceSecondOrder: metricText = WXU8("F2"); break;
    case BalancingMetric::InertiaMomentFirstOrder: metricText = WXU8("M1"); break;
    case BalancingMetric::InertiaMomentSecondOrder: metricText = WXU8("M2"); break;
    case BalancingMetric::CentrifugalForce: metricText = WXU8("Fc"); break;
    case BalancingMetric::CentrifugalMoment: metricText = WXU8("Mc"); break;
    }

    wxString modeText;
    switch (m_viewMode)
    {
    case BalancingViewMode::Source: modeText = WXU8("исх."); break;
    case BalancingViewMode::Counterweight: modeText = WXU8("вклад пр."); break;
    case BalancingViewMode::Residual: modeText = WXU8("остат."); break;
    }

    wxString compText;
    switch (m_component)
    {
    case BalancingComponent::X: compText = WXU8("X"); break;
    case BalancingComponent::Y: compText = WXU8("Y"); break;
    case BalancingComponent::Z: compText = WXU8("Z"); break;
    case BalancingComponent::Magnitude: compText = WXU8("|.|"); break;
    }

    const bool isMoment =
        (m_metric == BalancingMetric::InertiaMomentFirstOrder ||
         m_metric == BalancingMetric::InertiaMomentSecondOrder ||
         m_metric == BalancingMetric::CentrifugalMoment);

    return wxString::Format(WXU8("%s %s %s, %s"),
                            metricText,
                            modeText,
                            compText,
                            isMoment ? WXU8("Н·м") : WXU8("Н"));
}

wxString BalancingTablePanel::FormatValue(double value) const
{
    if (!std::isfinite(value))
        return WXU8("-");

    return wxString::Format("%.6f", value);
}

void BalancingTablePanel::RebuildGrid()
{
    if (!m_grid)
        return;

    const auto* values = GetSelectedSeriesValues();
    const int targetRows = static_cast<int>(m_result.alphaDeg.size());
    const int targetCols = 2;

    const int currentRows = m_grid->GetNumberRows();
    const int currentCols = m_grid->GetNumberCols();

    if (currentRows < targetRows)
        m_grid->AppendRows(targetRows - currentRows);
    else if (currentRows > targetRows)
        m_grid->DeleteRows(0, currentRows - targetRows);

    if (currentCols < targetCols)
        m_grid->AppendCols(targetCols - currentCols);
    else if (currentCols > targetCols)
        m_grid->DeleteCols(0, currentCols - targetCols);

    if (targetCols >= 1)
        m_grid->SetColLabelValue(0, WXU8("α, град"));
    if (targetCols >= 2)
        m_grid->SetColLabelValue(1, GetColumnLabel());

    for (int row = 0; row < targetRows; ++row)
    {
        m_grid->SetRowLabelValue(row, wxString::Format("%d", row));
        m_grid->SetCellValue(row, 0, wxString::Format("%.1f", m_result.alphaDeg[static_cast<std::size_t>(row)]));

        wxString text = WXU8("-");
        if (values != nullptr && static_cast<std::size_t>(row) < values->size())
            text = FormatValue(ExtractComponent((*values)[static_cast<std::size_t>(row)]));

        m_grid->SetCellValue(row, 1, text);
    }

    if (targetCols > 0)
        m_grid->SetColSize(0, 90);
    if (targetCols > 1)
        m_grid->SetColSize(1, 220);

    SetCurrentAlphaIndex(m_currentAlphaIndex);
}