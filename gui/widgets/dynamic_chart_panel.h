#pragma once

#include <wx/bitmap.h>
#include <vector>
#include <wx/panel.h>

#include "core/dynamic/dynamic_result.h"
#include "gui/widgets/dynamic_metric.h"
#include "gui/widgets/dynamic_component.h"

class DynamicChartPanel : public wxPanel
{
public:
    explicit DynamicChartPanel(wxWindow* parent);

    void SetResult(const engine::dynamic::DynamicResult& result);
    void SetMetric(DynamicMetric metric);
    void SetComponent(DynamicComponent component);
    void SetCurrentAlphaIndex(std::size_t index);
    void SetSelectedCylinderIndices(const std::vector<int>& indices);
    void SetShowTotal(bool showTotal);

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
                                double alphaMax,
                                double valueMin,
                                double valueMax);
    void DrawEmptyState(wxDC& dc, const wxRect& rect);

    bool HasData() const;
    bool ComputeRanges(double& alphaMin, double& alphaMax, double& valueMin, double& valueMax) const;

    const std::vector<engine::kinematic::Vec3>* GetCylinderSeriesValues(
        const engine::dynamic::CylinderDynamicSeries& cylinder) const;

    const std::vector<engine::kinematic::Vec3>* GetTotalSeriesValues() const;

    double ExtractComponent(const engine::kinematic::Vec3& v) const;
    wxString GetMetricLabel() const;
    wxColour GetSeriesColour(std::size_t index) const;

    void InvalidatePlotCache();
    void EnsurePlotCache(const wxRect& plotRect,
                         double alphaMin,
                         double alphaMax,
                         double valueMin,
                         double valueMax);

    engine::dynamic::DynamicResult m_result;
    DynamicMetric m_metric = DynamicMetric::InertiaForce;
    DynamicComponent m_component = DynamicComponent::Magnitude;
    std::size_t m_currentAlphaIndex = 0;
    std::vector<int> m_selectedCylinderIndices;
    bool m_showTotal = false;

    wxBitmap m_plotCache;
    bool m_plotCacheValid = false;
    wxRect m_plotCacheArea{0, 0, 0, 0};

    wxDECLARE_EVENT_TABLE();
};