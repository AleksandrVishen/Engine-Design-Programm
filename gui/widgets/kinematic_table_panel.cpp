#include "gui/widgets/kinematic_table_panel.h"

#include <algorithm>
#include <cmath>

#include <wx/grid.h>
#include <wx/sizer.h>

#include "gui/common/text_utf8.h"

KinematicTablePanel::KinematicTablePanel(wxWindow* parent)
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

    root->Add(m_grid, 1, wxEXPAND | wxALL, 0);
    SetSizer(root);

    RebuildGrid();
}

void KinematicTablePanel::SetResult(const engine::kinematic::KinematicResult& result)
{
    m_result = result;
    m_currentAlphaIndex = 0;
    RebuildGrid();
}

void KinematicTablePanel::SetMetric(KinematicMetric metric)
{
    if (m_metric == metric)
        return;

    m_metric = metric;
    RebuildGrid();
}

void KinematicTablePanel::SetCurrentAlphaIndex(std::size_t index)
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

const std::vector<double>* KinematicTablePanel::GetSeriesValues(
    const engine::kinematic::CylinderKinematicSeries& cylinder) const
{
    switch (m_metric)
    {
    case KinematicMetric::Displacement:
        return &cylinder.displacementM;

    case KinematicMetric::Velocity:
        return &cylinder.velocityMps;

    case KinematicMetric::Acceleration:
        return &cylinder.accelerationMps2;

    case KinematicMetric::AccelerationFirstOrder:
        return &cylinder.accelerationFirstOrderMps2;

    case KinematicMetric::AccelerationSecondOrder:
        return &cylinder.accelerationSecondOrderMps2;

    case KinematicMetric::RodAngle:
        return &cylinder.rodAngleRad;

    case KinematicMetric::RodAngularVelocity:
        return &cylinder.rodAngularVelocityRadS;

    case KinematicMetric::RodAngularAcceleration:
        return &cylinder.rodAngularAccelerationRadS2;
    }

    return nullptr;
}

wxString KinematicTablePanel::GetColumnLabel(int column) const
{
    if (column == 0)
        return WXU8("α, град");

    const int cylinderIndex = column - 1;
    if (cylinderIndex < 0 || cylinderIndex >= static_cast<int>(m_result.cylinders.size()))
        return wxString();

    const auto& cylinder = m_result.cylinders[static_cast<std::size_t>(cylinderIndex)];

    wxString metricShort;
    wxString unit;

    switch (m_metric)
    {
    case KinematicMetric::Displacement:
        metricShort = WXU8("s");
        unit = WXU8("м");
        break;

    case KinematicMetric::Velocity:
        metricShort = WXU8("v");
        unit = WXU8("м/с");
        break;

    case KinematicMetric::Acceleration:
        metricShort = WXU8("a");
        unit = WXU8("м/с²");
        break;

    case KinematicMetric::AccelerationFirstOrder:
        metricShort = WXU8("a1");
        unit = WXU8("м/с²");
        break;

    case KinematicMetric::AccelerationSecondOrder:
        metricShort = WXU8("a2");
        unit = WXU8("м/с²");
        break;

    case KinematicMetric::RodAngle:
        metricShort = WXU8("φ");
        unit = WXU8("рад");
        break;

    case KinematicMetric::RodAngularVelocity:
        metricShort = WXU8("ω");
        unit = WXU8("рад/с");
        break;

    case KinematicMetric::RodAngularAcceleration:
        metricShort = WXU8("ε");
        unit = WXU8("рад/с²");
        break;
    }

    return wxString::Format(WXU8("Ц%d %s, %s"),
                            cylinder.cylinderNumber,
                            metricShort,
                            unit);
}

wxString KinematicTablePanel::FormatValue(double value) const
{
    if (!std::isfinite(value))
        return WXU8("-");

    return wxString::Format("%.6f", value);
}

void KinematicTablePanel::RebuildGrid()
{
    if (!m_grid)
        return;

    const int targetRows = static_cast<int>(m_result.alphaDeg.size());
    const int targetCols = 1 + static_cast<int>(m_result.cylinders.size());

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

    for (std::size_t cylinderIndex = 0; cylinderIndex < m_result.cylinders.size(); ++cylinderIndex)
    {
        const auto& cylinder = m_result.cylinders[cylinderIndex];
        const auto* values = GetSeriesValues(cylinder);

        const int col = static_cast<int>(cylinderIndex) + 1;

        for (int row = 0; row < targetRows; ++row)
        {
            wxString text = WXU8("-");
            if (values != nullptr && static_cast<std::size_t>(row) < values->size())
                text = FormatValue((*values)[static_cast<std::size_t>(row)]);

            m_grid->SetCellValue(row, col, text);
        }
    }

    if (targetCols > 0)
        m_grid->SetColSize(0, 90);

    for (int col = 1; col < targetCols; ++col)
        m_grid->SetColSize(col, 140);

    SetCurrentAlphaIndex(m_currentAlphaIndex);
}