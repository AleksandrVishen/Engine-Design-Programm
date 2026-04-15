#pragma once

#include <wx/panel.h>

#include "core/kinematic/kinematic_result.h"
#include "gui/widgets/kinematic_metric.h"

class wxGrid;

class KinematicTablePanel : public wxPanel
{
public:
    explicit KinematicTablePanel(wxWindow* parent);

    void SetResult(const engine::kinematic::KinematicResult& result);
    void SetMetric(KinematicMetric metric);
    void SetCurrentAlphaIndex(std::size_t index);

private:
    void RebuildGrid();
    const std::vector<double>* GetSeriesValues(
        const engine::kinematic::CylinderKinematicSeries& cylinder) const;

    wxString GetColumnLabel(int column) const;
    wxString FormatValue(double value) const;

private:
    engine::kinematic::KinematicResult m_result;
    KinematicMetric m_metric = KinematicMetric::Displacement;
    std::size_t m_currentAlphaIndex = 0;

    wxGrid* m_grid = nullptr;
};