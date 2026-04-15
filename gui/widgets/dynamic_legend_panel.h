#pragma once

#include <vector>
#include <wx/scrolwin.h>

#include "core/dynamic/dynamic_result.h"

class DynamicLegendPanel : public wxScrolledWindow
{
public:
    explicit DynamicLegendPanel(wxWindow* parent);

    void SetResult(const engine::dynamic::DynamicResult& result);
    void SetSelectedCylinderIndices(const std::vector<int>& indices);
    void SetShowTotal(bool showTotal);

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    wxColour GetSeriesColour(std::size_t index) const;
    void DrawEmptyState(wxDC& dc, const wxRect& rect);
    void DrawLegend(wxDC& dc, const wxRect& rect);
    int ComputeRequiredHeight(const wxRect& rect) const;
    void UpdateVirtualSize();

private:
    engine::dynamic::DynamicResult m_result;
    std::vector<int> m_selectedCylinderIndices;
    bool m_showTotal = false;

    wxDECLARE_EVENT_TABLE();
};