#include "gui/widgets/kinematic_chart_panel.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>

#include "gui/common/text_utf8.h"

wxBEGIN_EVENT_TABLE(KinematicChartPanel, wxPanel)
    EVT_PAINT(KinematicChartPanel::OnPaint)
    EVT_SIZE(KinematicChartPanel::OnSize)
wxEND_EVENT_TABLE()

namespace
{
wxColour GetSeriesColour(std::size_t index)
{
    static const wxColour palette[] =
    {
        wxColour(255, 99, 132),
        wxColour(54, 162, 235),
        wxColour(255, 206, 86),
        wxColour(75, 192, 192),
        wxColour(153, 102, 255),
        wxColour(255, 159, 64),
        wxColour(0, 200, 140),
        wxColour(220, 220, 220)
    };

    return palette[index % (sizeof(palette) / sizeof(palette[0]))];
}

double SafeNormalize(double value, double minValue, double maxValue)
{
    const double span = maxValue - minValue;
    if (std::abs(span) < 1e-12)
        return 0.5;

    return (value - minValue) / span;
}
}

KinematicChartPanel::KinematicChartPanel(wxWindow* parent)
    : wxPanel(parent)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(240, 260));
    SetBackgroundColour(wxColour(18, 24, 36));
    SetForegroundColour(wxColour(235, 235, 235));
}

void KinematicChartPanel::SetResult(const engine::kinematic::KinematicResult& result)
{
    m_result = result;
    m_currentAlphaIndex = 0;
    InvalidatePlotCache();
    Refresh();
}

void KinematicChartPanel::SetMetric(KinematicMetric metric)
{
    if (m_metric == metric)
        return;

    m_metric = metric;
    InvalidatePlotCache();
    Refresh();
}

void KinematicChartPanel::SetCurrentAlphaIndex(std::size_t index)
{
    if (m_result.alphaDeg.empty())
    {
        m_currentAlphaIndex = 0;
        Refresh();
        return;
    }

    const std::size_t clamped = std::min(index, m_result.alphaDeg.size() - 1);
    if (m_currentAlphaIndex == clamped)
        return;

    m_currentAlphaIndex = clamped;
    Refresh();
}

bool KinematicChartPanel::HasData() const
{
    return !m_result.alphaDeg.empty() && !m_result.cylinders.empty();
}

const std::vector<double>* KinematicChartPanel::GetSeriesValues(
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

wxString KinematicChartPanel::GetMetricLabel() const
{
    switch (m_metric)
    {
    case KinematicMetric::Displacement:
        return WXU8("s, м");

    case KinematicMetric::Velocity:
        return WXU8("v, м/с");

    case KinematicMetric::Acceleration:
        return WXU8("a, м/с²");

    case KinematicMetric::AccelerationFirstOrder:
        return WXU8("a1, м/с²");

    case KinematicMetric::AccelerationSecondOrder:
        return WXU8("a2, м/с²");

    case KinematicMetric::RodAngle:
        return WXU8("φ, рад");

    case KinematicMetric::RodAngularVelocity:
        return WXU8("ωш, рад/с");

    case KinematicMetric::RodAngularAcceleration:
        return WXU8("εш, рад/с²");
    }

    return wxString();
}

bool KinematicChartPanel::ComputeRanges(
    double& alphaMin, double& alphaMax, double& valueMin, double& valueMax) const
{
    if (!HasData())
        return false;

    alphaMin = m_result.alphaDeg.front();
    alphaMax = m_result.alphaDeg.back();

    valueMin = std::numeric_limits<double>::infinity();
    valueMax = -std::numeric_limits<double>::infinity();

    bool foundAny = false;
    double sumValues = 0.0;
    std::size_t valueCount = 0;
    double maxAbsValue = 0.0;

    for (const auto& cylinder : m_result.cylinders)
    {
        const auto* values = GetSeriesValues(cylinder);
        if (values == nullptr || values->empty())
            continue;

        for (double value : *values)
        {
            valueMin = std::min(valueMin, value);
            valueMax = std::max(valueMax, value);
            sumValues += value;
            ++valueCount;
            maxAbsValue = std::max(maxAbsValue, std::abs(value));
        }

        foundAny = true;
    }

    if (!foundAny || valueCount == 0)
        return false;

    if (!std::isfinite(valueMin) || !std::isfinite(valueMax))
        return false;

    const double span = valueMax - valueMin;
    const double scale = std::max(1.0, maxAbsValue);

    const bool nearlyConstant =
        span <= std::max(1e-9, scale * 1e-6);

    if (nearlyConstant)
    {
        const double center = sumValues / static_cast<double>(valueCount);
        const double halfRange = std::max(1e-3, scale * 1e-3);

        valueMin = center - halfRange;
        valueMax = center + halfRange;
    }
    else
    {
        const double margin = 0.08 * span;
        valueMin -= margin;
        valueMax += margin;
    }

    return true;
}

void KinematicChartPanel::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.Clear();

    const wxRect rect = GetClientRect();
    DrawBackground(dc, rect);

    if (!HasData())
    {
        DrawEmptyState(dc, rect);
        return;
    }

    double alphaMin = 0.0;
    double alphaMax = 0.0;
    double valueMin = 0.0;
    double valueMax = 0.0;

    if (!ComputeRanges(alphaMin, alphaMax, valueMin, valueMax))
    {
        DrawEmptyState(dc, rect);
        return;
    }

    const int leftMargin = 72;
    const int rightMargin = 24;
    const int topMargin = 18;
    const int bottomMargin = 56;

    wxRect plotRect(
        rect.x + leftMargin,
        rect.y + topMargin,
        std::max(10, rect.width - leftMargin - rightMargin),
        std::max(10, rect.height - topMargin - bottomMargin));

    EnsurePlotCache(plotRect, alphaMin, alphaMax, valueMin, valueMax);
    if (m_plotCacheValid && m_plotCache.IsOk())
        dc.DrawBitmap(m_plotCache, m_plotCacheArea.GetTopLeft(), false);
    else
    {
        DrawAxes(dc, plotRect, alphaMin, alphaMax, valueMin, valueMax);
        DrawSeries(dc, plotRect, alphaMin, alphaMax, valueMin, valueMax);
    }

    DrawCurrentAlphaMarker(dc, plotRect, alphaMin, alphaMax, valueMin, valueMax);
}

void KinematicChartPanel::InvalidatePlotCache()
{
    m_plotCacheValid = false;
}

void KinematicChartPanel::EnsurePlotCache(
    const wxRect& plotRect,
    double alphaMin,
    double alphaMax,
    double valueMin,
    double valueMax)
{
    const wxRect cacheRect(
        plotRect.x - 72,
        plotRect.y - 22,
        plotRect.width + 72 + 28,
        plotRect.height + 22 + 56);

    if (m_plotCacheValid && m_plotCache.IsOk() && m_plotCacheArea == cacheRect)
        return;

    if (cacheRect.width < 2 || cacheRect.height < 2)
        return;

    wxBitmap bmp(cacheRect.width, cacheRect.height, 24);
    if (!bmp.IsOk())
        return;

    wxMemoryDC mdc;
    mdc.SelectObject(bmp);
    mdc.SetBackground(wxBrush(wxColour(18, 24, 36)));
    mdc.Clear();
    mdc.SetFont(GetFont());
    mdc.SetDeviceOrigin(-cacheRect.x, -cacheRect.y);

    DrawAxes(mdc, plotRect, alphaMin, alphaMax, valueMin, valueMax);
    DrawSeries(mdc, plotRect, alphaMin, alphaMax, valueMin, valueMax);

    mdc.SetDeviceOrigin(0, 0);
    mdc.SelectObject(wxNullBitmap);

    m_plotCache = bmp;
    m_plotCacheArea = cacheRect;
    m_plotCacheValid = true;
}

void KinematicChartPanel::OnSize(wxSizeEvent& event)
{
    InvalidatePlotCache();
    Refresh();
    event.Skip();
}

void KinematicChartPanel::DrawBackground(wxDC& dc, const wxRect& rect)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(18, 24, 36)));
    dc.DrawRectangle(rect);
}

void KinematicChartPanel::DrawAxes(
    wxDC& dc,
    const wxRect& plotRect,
    double alphaMin, double alphaMax,
    double valueMin, double valueMax)
{
    dc.SetPen(wxPen(wxColour(110, 120, 140), 1));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(plotRect);

    dc.SetTextForeground(wxColour(220, 220, 220));
    dc.SetFont(GetFont());

    constexpr int xTicks = 6;
    for (int i = 0; i <= xTicks; ++i)
    {
        const double t = static_cast<double>(i) / xTicks;
        const int x = plotRect.x + static_cast<int>(t * plotRect.width);

        dc.SetPen(wxPen(wxColour(55, 65, 85), 1));
        dc.DrawLine(x, plotRect.y, x, plotRect.y + plotRect.height);

        const double alpha = alphaMin + t * (alphaMax - alphaMin);
        dc.SetTextForeground(wxColour(200, 200, 200));
        dc.DrawText(wxString::Format("%.0f", alpha), x - 14, plotRect.y + plotRect.height + 6);
    }

    constexpr int yTicks = 5;
    for (int i = 0; i <= yTicks; ++i)
    {
        const double t = static_cast<double>(i) / yTicks;
        const int y = plotRect.y + plotRect.height - static_cast<int>(t * plotRect.height);

        dc.SetPen(wxPen(wxColour(55, 65, 85), 1));
        dc.DrawLine(plotRect.x, y, plotRect.x + plotRect.width, y);

        const double value = valueMin + t * (valueMax - valueMin);
        dc.SetTextForeground(wxColour(200, 200, 200));
        dc.DrawText(wxString::Format("%.3f", value), plotRect.x - 66, y - 8);
    }

    dc.SetTextForeground(wxColour(235, 235, 235));
    dc.DrawText(WXU8("α, град"), plotRect.x + plotRect.width - 54, plotRect.y + plotRect.height + 24);
    dc.DrawText(GetMetricLabel(), plotRect.x - 64, plotRect.y - 18);

    if (valueMin < 0.0 && valueMax > 0.0)
    {
        const double tZero = SafeNormalize(0.0, valueMin, valueMax);
        const int yZero = plotRect.y + plotRect.height - static_cast<int>(tZero * plotRect.height);

        dc.SetPen(wxPen(wxColour(150, 150, 150), 1, wxPENSTYLE_SHORT_DASH));
        dc.DrawLine(plotRect.x, yZero, plotRect.x + plotRect.width, yZero);
    }
}

void KinematicChartPanel::DrawSeries(
    wxDC& dc,
    const wxRect& plotRect,
    double alphaMin, double alphaMax,
    double valueMin, double valueMax)
{
    for (std::size_t cylinderIndex = 0; cylinderIndex < m_result.cylinders.size(); ++cylinderIndex)
    {
        const auto& cylinder = m_result.cylinders[cylinderIndex];
        const auto* values = GetSeriesValues(cylinder);

        if (values == nullptr || values->size() != m_result.alphaDeg.size() || values->empty())
            continue;

        dc.SetPen(wxPen(GetSeriesColour(cylinderIndex), 2));

        bool firstPoint = true;
        wxPoint prevPoint;

        for (std::size_t i = 0; i < values->size(); ++i)
        {
            const double alpha = m_result.alphaDeg[i];
            const double value = (*values)[i];

            const double tx = SafeNormalize(alpha, alphaMin, alphaMax);
            const double ty = SafeNormalize(value, valueMin, valueMax);

            const int x = plotRect.x + static_cast<int>(tx * plotRect.width);
            const int y = plotRect.y + plotRect.height - static_cast<int>(ty * plotRect.height);

            wxPoint currentPoint(x, y);

            if (!firstPoint)
            {
                dc.DrawLine(prevPoint, currentPoint);
            }

            prevPoint = currentPoint;
            firstPoint = false;
        }
    }
}

void KinematicChartPanel::DrawCurrentAlphaMarker(
    wxDC& dc,
    const wxRect& plotRect,
    double alphaMin,
    double alphaMax,
    double valueMin,
    double valueMax)
{
    if (m_result.alphaDeg.empty())
        return;

    const std::size_t index = std::min(m_currentAlphaIndex, m_result.alphaDeg.size() - 1);
    const double alpha = m_result.alphaDeg[index];

    constexpr int dotR = 5;
    auto drawDot = [&](int px, int py, const wxColour& fill)
    {
        dc.SetPen(wxPen(wxColour(255, 255, 255), 2));
        dc.SetBrush(wxBrush(fill));
        dc.DrawEllipse(px - dotR, py - dotR, 2 * dotR, 2 * dotR);
    };

    for (std::size_t cylinderIndex = 0; cylinderIndex < m_result.cylinders.size(); ++cylinderIndex)
    {
        const auto& cylinder = m_result.cylinders[cylinderIndex];
        const auto* values = GetSeriesValues(cylinder);

        if (values == nullptr || values->size() != m_result.alphaDeg.size() || index >= values->size())
            continue;

        const double value = (*values)[index];
        const double tx = SafeNormalize(alpha, alphaMin, alphaMax);
        const double ty = SafeNormalize(value, valueMin, valueMax);

        const int x = plotRect.x + static_cast<int>(tx * plotRect.width);
        const int y = plotRect.y + plotRect.height - static_cast<int>(ty * plotRect.height);

        drawDot(x, y, GetSeriesColour(cylinderIndex));
    }

    dc.SetTextForeground(wxColour(255, 255, 255));
    dc.DrawText(wxString::Format(WXU8("α=%.1f"), alpha), plotRect.x + 6, plotRect.y + 6);
}

void KinematicChartPanel::DrawEmptyState(wxDC& dc, const wxRect& rect)
{
    dc.SetTextForeground(wxColour(190, 190, 190));
    dc.DrawText(WXU8("Нет данных для построения графика."), rect.x + 24, rect.y + 24);
}