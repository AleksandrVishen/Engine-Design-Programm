#include "gui/widgets/dynamic_legend_panel.h"

#include <algorithm>

#include <wx/dcbuffer.h>

#include "gui/common/text_utf8.h"

wxBEGIN_EVENT_TABLE(DynamicLegendPanel, wxScrolledWindow)
    EVT_PAINT(DynamicLegendPanel::OnPaint)
    EVT_SIZE(DynamicLegendPanel::OnSize)
wxEND_EVENT_TABLE()

DynamicLegendPanel::DynamicLegendPanel(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(20, 26, 38));
    SetForegroundColour(wxColour(235, 235, 235));

    SetScrollRate(0, 14);
    SetMinSize(wxSize(-1, 72));

    UpdateVirtualSize();
}

void DynamicLegendPanel::SetResult(const engine::dynamic::DynamicResult& result)
{
    m_result = result;
    UpdateVirtualSize();
    Refresh();
}

void DynamicLegendPanel::SetSelectedCylinderIndices(const std::vector<int>& indices)
{
    m_selectedCylinderIndices = indices;
    UpdateVirtualSize();
    Refresh();
}

void DynamicLegendPanel::SetShowTotal(bool showTotal)
{
    m_showTotal = showTotal;
    UpdateVirtualSize();
    Refresh();
}

wxColour DynamicLegendPanel::GetSeriesColour(std::size_t index) const
{
    static const wxColour palette[] =
    {
        wxColour(255, 99, 132),
        wxColour(80, 190, 255),
        wxColour(255, 205, 86),
        wxColour(75, 192, 192),
        wxColour(153, 102, 255),
        wxColour(255, 159, 64),
        wxColour(120, 220, 120),
        wxColour(220, 120, 220)
    };

    return palette[index % (sizeof(palette) / sizeof(palette[0]))];
}

int DynamicLegendPanel::ComputeRequiredHeight(const wxRect& rect) const
{
    if (m_result.cylinders.empty())
        return 60;

    const int titleY = 8;
    const int startY = 34;
    const int rowHeight = 22;
    const int itemSpacingX = 18;
    const int itemWidth = 150;

    int x = rect.x + 12;
    int y = rect.y + startY;

    int itemCount = 0;
    if (m_showTotal)
        ++itemCount;
    itemCount += static_cast<int>(m_selectedCylinderIndices.size());

    if (itemCount == 0)
        return 60;

    auto advanceItem = [&]()
    {
        if (x + itemWidth > rect.GetRight() - 10)
        {
            x = rect.x + 12;
            y += rowHeight;
        }

        x += itemWidth + itemSpacingX;
    };

    for (int i = 0; i < itemCount; ++i)
        advanceItem();

    return std::max(60, y + rowHeight + titleY);
}

void DynamicLegendPanel::UpdateVirtualSize()
{
    const wxRect clientRect = GetClientRect();
    const int width = std::max(200, clientRect.width);
    const int height = ComputeRequiredHeight(wxRect(0, 0, width, std::max(60, clientRect.height)));

    SetVirtualSize(width, height);
    SetScrollRate(0, 14);
}

void DynamicLegendPanel::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    PrepareDC(dc);

    const wxRect rect(0, 0, GetVirtualSize().GetWidth(), GetVirtualSize().GetHeight());

    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    if (m_result.cylinders.empty())
    {
        DrawEmptyState(dc, rect);
        return;
    }

    DrawLegend(dc, rect);
}

void DynamicLegendPanel::OnSize(wxSizeEvent& event)
{
    UpdateVirtualSize();
    Refresh();
    event.Skip();
}

void DynamicLegendPanel::DrawEmptyState(wxDC& dc, const wxRect& rect)
{
    dc.SetTextForeground(wxColour(210, 210, 210));
    dc.DrawText(WXU8("Легенда появится после расчёта динамики."), rect.x + 10, rect.y + 10);
}

void DynamicLegendPanel::DrawLegend(wxDC& dc, const wxRect& rect)
{
    dc.SetTextForeground(wxColour(240, 240, 240));
    dc.DrawText(WXU8("Легенда:"), rect.x + 10, rect.y + 8);

    int x = rect.x + 12;
    int y = rect.y + 34;
    const int rowHeight = 22;
    const int swatchW = 18;
    const int swatchH = 8;
    const int itemSpacingX = 18;
    const int itemWidth = 150;

    auto drawItem = [&](const wxColour& color, const wxString& label)
    {
        if (x + itemWidth > rect.GetRight() - 10)
        {
            x = rect.x + 12;
            y += rowHeight;
        }

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(color));
        dc.DrawRectangle(x, y + 5, swatchW, swatchH);

        dc.SetTextForeground(wxColour(225, 225, 225));
        dc.DrawText(label, x + swatchW + 8, y);

        x += itemWidth + itemSpacingX;
    };

    if (m_showTotal)
        drawItem(wxColour(245, 245, 245), WXU8("Σ Все цилиндры"));

    std::size_t colorIndex = 0;
    for (int idx : m_selectedCylinderIndices)
    {
        if (idx < 0 || idx >= static_cast<int>(m_result.cylinders.size()))
            continue;

        const auto& cylinder = m_result.cylinders[static_cast<std::size_t>(idx)];
        drawItem(GetSeriesColour(colorIndex++),
                 wxString::Format(WXU8("Цил %d"), cylinder.cylinderNumber));
    }
}