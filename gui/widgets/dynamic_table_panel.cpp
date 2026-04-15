#include "gui/widgets/dynamic_table_panel.h"

#include <algorithm>
#include <cmath>

#include <wx/grid.h>
#include <wx/sizer.h>

#include "gui/common/text_utf8.h"

DynamicTablePanel::DynamicTablePanel(wxWindow* parent)
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

void DynamicTablePanel::SetResult(const engine::dynamic::DynamicResult& result)
{
    m_result = result;
    m_currentAlphaIndex = 0;

    m_selectedCylinderIndices.clear();
    for (std::size_t i = 0; i < m_result.cylinders.size(); ++i)
        m_selectedCylinderIndices.push_back(static_cast<int>(i));

    RebuildGrid();
}

void DynamicTablePanel::SetMetric(DynamicMetric metric)
{
    if (m_metric == metric)
        return;

    m_metric = metric;
    RebuildGrid();
}

void DynamicTablePanel::SetComponent(DynamicComponent component)
{
    if (m_component == component)
        return;

    m_component = component;
    RebuildGrid();
}

void DynamicTablePanel::SetCurrentAlphaIndex(std::size_t index)
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

void DynamicTablePanel::SetSelectedCylinderIndices(const std::vector<int>& indices)
{
    m_selectedCylinderIndices = indices;
    RebuildGrid();
}

void DynamicTablePanel::SetShowTotal(bool showTotal)
{
    m_showTotal = showTotal;
    RebuildGrid();
}

const std::vector<engine::kinematic::Vec3>* DynamicTablePanel::GetCylinderSeriesValues(
    const engine::dynamic::CylinderDynamicSeries& cylinder) const
{
    switch (m_metric)
    {
    case DynamicMetric::InertiaForce:
        return &cylinder.inertiaForce;
    case DynamicMetric::InertiaForceFirstOrder:
        return &cylinder.inertiaForce1;
    case DynamicMetric::InertiaForceSecondOrder:
        return &cylinder.inertiaForce2;
    case DynamicMetric::InertiaMomentFirstOrder:
        return &cylinder.inertiaMoment1;
    case DynamicMetric::InertiaMomentSecondOrder:
        return &cylinder.inertiaMoment2;
    case DynamicMetric::CentrifugalForce:
        return &cylinder.centrifugalForce;
    case DynamicMetric::CentrifugalMoment:
        return &cylinder.centrifugalMoment;
    }

    return nullptr;
}

const std::vector<engine::kinematic::Vec3>* DynamicTablePanel::GetTotalSeriesValues() const
{
    switch (m_metric)
    {
    case DynamicMetric::InertiaForce:
        return &m_result.totalInertiaForce;
    case DynamicMetric::InertiaForceFirstOrder:
        return &m_result.totalInertiaForce1;
    case DynamicMetric::InertiaForceSecondOrder:
        return &m_result.totalInertiaForce2;
    case DynamicMetric::InertiaMomentFirstOrder:
        return &m_result.totalInertiaMoment1;
    case DynamicMetric::InertiaMomentSecondOrder:
        return &m_result.totalInertiaMoment2;
    case DynamicMetric::CentrifugalForce:
        return &m_result.totalCentrifugalForce;
    case DynamicMetric::CentrifugalMoment:
        return &m_result.totalCentrifugalMoment;
    }

    return nullptr;
}

double DynamicTablePanel::ExtractComponent(const engine::kinematic::Vec3& v) const
{
    switch (m_component)
    {
    case DynamicComponent::X:
        return v.x;
    case DynamicComponent::Y:
        return v.y;
    case DynamicComponent::Z:
        return v.z;
    case DynamicComponent::Magnitude:
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    return 0.0;
}

wxString DynamicTablePanel::GetColumnLabel(int column) const
{
    if (column == 0)
        return WXU8("α, град");

    int offset = 1;

    wxString metricText;
    switch (m_metric)
    {
    case DynamicMetric::InertiaForce: metricText = WXU8("F"); break;
    case DynamicMetric::InertiaForceFirstOrder: metricText = WXU8("F1"); break;
    case DynamicMetric::InertiaForceSecondOrder: metricText = WXU8("F2"); break;
    case DynamicMetric::InertiaMomentFirstOrder: metricText = WXU8("M1"); break;
    case DynamicMetric::InertiaMomentSecondOrder: metricText = WXU8("M2"); break;
    case DynamicMetric::CentrifugalForce: metricText = WXU8("Fc"); break;
    case DynamicMetric::CentrifugalMoment: metricText = WXU8("Mc"); break;
    }

    wxString compText;
    switch (m_component)
    {
    case DynamicComponent::X: compText = WXU8("X"); break;
    case DynamicComponent::Y: compText = WXU8("Y"); break;
    case DynamicComponent::Z: compText = WXU8("Z"); break;
    case DynamicComponent::Magnitude: compText = WXU8("|.|"); break;
    }

    const bool isMoment =
        (m_metric == DynamicMetric::InertiaMomentFirstOrder ||
         m_metric == DynamicMetric::InertiaMomentSecondOrder ||
         m_metric == DynamicMetric::CentrifugalMoment);

    wxString unit = isMoment ? WXU8("Н·м") : WXU8("Н");

    if (m_showTotal)
    {
        if (column == 1)
            return wxString::Format(WXU8("Σ %s %s, %s"), metricText, compText, unit);

        offset = 2;
    }

    const int selectedIndex = column - offset;
    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(m_selectedCylinderIndices.size()))
        return wxString();

    const int cylIdx = m_selectedCylinderIndices[static_cast<std::size_t>(selectedIndex)];
    if (cylIdx < 0 || cylIdx >= static_cast<int>(m_result.cylinders.size()))
        return wxString();

    const auto& cylinder = m_result.cylinders[static_cast<std::size_t>(cylIdx)];

    return wxString::Format(WXU8("Ц%d %s %s, %s"),
                            cylinder.cylinderNumber,
                            metricText,
                            compText,
                            unit);
}

wxString DynamicTablePanel::FormatValue(double value) const
{
    if (!std::isfinite(value))
        return WXU8("-");

    return wxString::Format("%.6f", value);
}

void DynamicTablePanel::RebuildGrid()
{
    if (!m_grid)
        return;

    const int targetRows = static_cast<int>(m_result.alphaDeg.size());
    const int targetCols =
        1 +
        (m_showTotal ? 1 : 0) +
        static_cast<int>(m_selectedCylinderIndices.size());

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

    for (int col = 0; col < targetCols; ++col)
        m_grid->SetColLabelValue(col, GetColumnLabel(col));

    for (int row = 0; row < targetRows; ++row)
    {
        m_grid->SetRowLabelValue(row, wxString::Format("%d", row));
        m_grid->SetCellValue(row, 0, wxString::Format("%.1f", m_result.alphaDeg[static_cast<std::size_t>(row)]));
    }

    int currentCol = 1;

    if (m_showTotal)
    {
        const auto* total = GetTotalSeriesValues();
        for (int row = 0; row < targetRows; ++row)
        {
            wxString text = WXU8("-");
            if (total != nullptr && static_cast<std::size_t>(row) < total->size())
                text = FormatValue(ExtractComponent((*total)[static_cast<std::size_t>(row)]));

            m_grid->SetCellValue(row, currentCol, text);
        }
        ++currentCol;
    }

    for (std::size_t selectedPos = 0; selectedPos < m_selectedCylinderIndices.size(); ++selectedPos)
    {
        const int cylIdx = m_selectedCylinderIndices[selectedPos];
        if (cylIdx < 0 || cylIdx >= static_cast<int>(m_result.cylinders.size()))
            continue;

        const auto& cylinder = m_result.cylinders[static_cast<std::size_t>(cylIdx)];
        const auto* values = GetCylinderSeriesValues(cylinder);

        for (int row = 0; row < targetRows; ++row)
        {
            wxString text = WXU8("-");
            if (values != nullptr && static_cast<std::size_t>(row) < values->size())
                text = FormatValue(ExtractComponent((*values)[static_cast<std::size_t>(row)]));

            m_grid->SetCellValue(row, currentCol, text);
        }

        ++currentCol;
    }

    if (targetCols > 0)
        m_grid->SetColSize(0, 90);

    for (int col = 1; col < targetCols; ++col)
        m_grid->SetColSize(col, 155);

    SetCurrentAlphaIndex(m_currentAlphaIndex);
}