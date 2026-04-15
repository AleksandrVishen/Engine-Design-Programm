#pragma once

#include <wx/panel.h>

#include "core/kinematic/kinematic_result.h"
#include "gui/widgets/kinematic_metric.h"

class KinematicChartPanel : public wxPanel
{
public:
    explicit KinematicChartPanel(wxWindow* parent);

    void SetResult(const engine::kinematic::KinematicResult& result);
    void SetMetric(KinematicMetric metric);
    void SetCurrentAlphaIndex(std::size_t index);

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    void DrawBackground(wxDC& dc, const wxRect& rect);
    void DrawAxes(wxDC& dc, const wxRect& plotRect,
                  double alphaMin, double alphaMax,
                  double valueMin, double valueMax);
    void DrawSeries(wxDC& dc, const wxRect& plotRect,
                    double alphaMin, double alphaMax,
                    double valueMin, double valueMax);
    void DrawCurrentAlphaMarker(wxDC& dc, const wxRect& plotRect,
                                double alphaMin, double alphaMax);
    void DrawEmptyState(wxDC& dc, const wxRect& rect);

    bool HasData() const;
    bool ComputeRanges(double& alphaMin, double& alphaMax, double& valueMin, double& valueMax) const;
    const std::vector<double>* GetSeriesValues(const engine::kinematic::CylinderKinematicSeries& cylinder) const;
    wxString GetMetricLabel() const;

private:
    engine::kinematic::KinematicResult m_result;
    KinematicMetric m_metric = KinematicMetric::Displacement;
    std::size_t m_currentAlphaIndex = 0;

    wxDECLARE_EVENT_TABLE();
};