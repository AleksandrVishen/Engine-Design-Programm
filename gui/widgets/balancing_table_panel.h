#pragma once

#include <wx/panel.h>

#include "core/balancing/balancing_composed_result.h"
#include "gui/widgets/balancing_component.h"
#include "gui/widgets/balancing_metric.h"

class wxGrid;

class BalancingTablePanel : public wxPanel
{
public:
    explicit BalancingTablePanel(wxWindow* parent);

    void SetResult(const engine::balancing::BalancingComposedResult& result);
    void SetMetric(BalancingMetric metric);
    void SetViewMode(BalancingViewMode viewMode);
    void SetComponent(BalancingComponent component);
    void SetCurrentAlphaIndex(std::size_t index);

private:
    void RebuildGrid();

    const std::vector<engine::kinematic::Vec3>* GetSelectedSeriesValues() const;
    double ExtractComponent(const engine::kinematic::Vec3& v) const;
    wxString GetColumnLabel() const;
    wxString FormatValue(double value) const;

private:
    engine::balancing::BalancingComposedResult m_result;
    BalancingMetric m_metric = BalancingMetric::InertiaForce;
    BalancingViewMode m_viewMode = BalancingViewMode::Residual;
    BalancingComponent m_component = BalancingComponent::Magnitude;
    std::size_t m_currentAlphaIndex = 0;

    wxGrid* m_grid = nullptr;
};