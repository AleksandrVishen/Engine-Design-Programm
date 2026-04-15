#pragma once

#include <vector>
#include <wx/panel.h>

#include "core/balancing/balancing_composed_result.h"
#include "gui/widgets/balancing_component.h"
#include "gui/widgets/balancing_metric.h"

class BalancingChartPanel : public wxPanel
{
public:
    explicit BalancingChartPanel(wxWindow* parent);

    void SetResult(const engine::balancing::BalancingComposedResult& result);
    void SetMetric(BalancingMetric metric);
    void SetViewMode(BalancingViewMode viewMode);
    void SetComponent(BalancingComponent component);
    void SetCurrentAlphaIndex(std::size_t index);

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    void DrawBackground(wxDC& dc, const wxRect& rect);
    void DrawAxes(wxDC& dc,
                  const wxRect& plotRect,
                  double alphaMin,
                  double alphaMax,
                  double valueMin,
                  double valueMax);
    void DrawSeries(wxDC& dc,
                    const wxRect& plotRect,
                    double alphaMin,
                    double alphaMax,
                    double valueMin,
                    double valueMax);
    void DrawCurrentAlphaMarker(wxDC& dc,
                                const wxRect& plotRect,
                                double alphaMin,
                                double alphaMax);
    void DrawEmptyState(wxDC& dc, const wxRect& rect);

    bool HasData() const;
    bool ComputeRanges(double& alphaMin, double& alphaMax, double& valueMin, double& valueMax) const;

    const std::vector<engine::kinematic::Vec3>* GetSelectedSeriesValues() const;
    double ExtractComponent(const engine::kinematic::Vec3& v) const;
    wxString GetMetricLabel() const;

private:
    engine::balancing::BalancingComposedResult m_result;
    BalancingMetric m_metric = BalancingMetric::InertiaForce;
    BalancingViewMode m_viewMode = BalancingViewMode::Residual;
    BalancingComponent m_component = BalancingComponent::Magnitude;
    std::size_t m_currentAlphaIndex = 0;

    wxDECLARE_EVENT_TABLE();
};