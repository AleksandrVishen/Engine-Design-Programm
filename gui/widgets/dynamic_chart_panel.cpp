#include "gui/widgets/dynamic_chart_panel.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>

wxBEGIN_EVENT_TABLE(DynamicChartPanel, wxPanel)
    EVT_PAINT(DynamicChartPanel::OnPaint)
    EVT_SIZE(DynamicChartPanel::OnSize)
wxEND_EVENT_TABLE()

DynamicChartPanel::DynamicChartPanel(wxWindow* parent)
    : wxPanel(parent)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(18, 24, 36));
    SetForegroundColour(wxColour(235, 235, 235));
}

void DynamicChartPanel::SetResult(const engine::dynamic::DynamicResult& result)
{
    m_result = result;
    m_currentAlphaIndex = 0;

    m_selectedCylinderIndices.clear();
    for (std::size_t i = 0; i < m_result.cylinders.size(); ++i)
        m_selectedCylinderIndices.push_back(static_cast<int>(i));

    InvalidatePlotCache();
    Refresh();
}

void DynamicChartPanel::SetMetric(DynamicMetric metric)
{
    if (m_metric == metric)
        return;

    m_metric = metric;
    InvalidatePlotCache();
    Refresh();
}

void DynamicChartPanel::SetComponent(DynamicComponent component)
{
    if (m_component == component)
        return;

    m_component = component;
    InvalidatePlotCache();
    Refresh();
}

void DynamicChartPanel::SetCurrentAlphaIndex(std::size_t index)
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

void DynamicChartPanel::SetSelectedCylinderIndices(const std::vector<int>& indices)
{
    m_selectedCylinderIndices = indices;
    InvalidatePlotCache();
    Refresh();
}

void DynamicChartPanel::SetShowTotal(bool showTotal)
{
    m_showTotal = showTotal;
    InvalidatePlotCache();
    Refresh();
}

wxColour DynamicChartPanel::GetSeriesColour(std::size_t index) const
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

const std::vector<engine::kinematic::Vec3>* DynamicChartPanel::GetCylinderSeriesValues(
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

const std::vector<engine::kinematic::Vec3>* DynamicChartPanel::GetTotalSeriesValues() const
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

double DynamicChartPanel::ExtractComponent(const engine::kinematic::Vec3& v) const
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

wxString DynamicChartPanel::GetMetricLabel() const
{
    switch (m_metric)
    {
    case DynamicMetric::InertiaForce:
        return (m_component == DynamicComponent::Magnitude) ? "F, N" : "F component, N";
    case DynamicMetric::InertiaForceFirstOrder:
        return (m_component == DynamicComponent::Magnitude) ? "F1, N" : "F1 component, N";
    case DynamicMetric::InertiaForceSecondOrder:
        return (m_component == DynamicComponent::Magnitude) ? "F2, N" : "F2 component, N";
    case DynamicMetric::InertiaMomentFirstOrder:
        return (m_component == DynamicComponent::Magnitude) ? "M1, N*m" : "M1 component, N*m";
    case DynamicMetric::InertiaMomentSecondOrder:
        return (m_component == DynamicComponent::Magnitude) ? "M2, N*m" : "M2 component, N*m";
    case DynamicMetric::CentrifugalForce:
        return (m_component == DynamicComponent::Magnitude) ? "Fc, N" : "Fc component, N";
    case DynamicMetric::CentrifugalMoment:
        return (m_component == DynamicComponent::Magnitude) ? "Mc, N*m" : "Mc component, N*m";
    }

    return "Value";
}

bool DynamicChartPanel::HasData() const
{
    const std::size_t sampleCount = m_result.alphaDeg.size();
    if (sampleCount == 0)
        return false;

    if (m_showTotal)
    {
        const auto* total = GetTotalSeriesValues();
        if (total != nullptr && total->size() == sampleCount)
            return true;
    }

    for (int idx : m_selectedCylinderIndices)
    {
        if (idx < 0 || idx >= static_cast<int>(m_result.cylinders.size()))
            continue;

        const auto* values = GetCylinderSeriesValues(m_result.cylinders[static_cast<std::size_t>(idx)]);
        if (values != nullptr && values->size() == sampleCount)
            return true;
    }

    return false;
}

bool DynamicChartPanel::ComputeRanges(double& alphaMin,
                                      double& alphaMax,
                                      double& valueMin,
                                      double& valueMax) const
{
    if (!HasData())
        return false;

    alphaMin = m_result.alphaDeg.front();
    alphaMax = m_result.alphaDeg.back();

    valueMin = std::numeric_limits<double>::infinity();
    valueMax = -std::numeric_limits<double>::infinity();

    double sumValues = 0.0;
    std::size_t valueCount = 0;
    double maxAbsValue = 0.0;

    auto accumulateSeries = [&](const std::vector<engine::kinematic::Vec3>* values)
    {
        if (values == nullptr)
            return;

        for (const auto& v : *values)
        {
            const double value = ExtractComponent(v);
            valueMin = std::min(valueMin, value);
            valueMax = std::max(valueMax, value);
            sumValues += value;
            ++valueCount;
            maxAbsValue = std::max(maxAbsValue, std::abs(value));
        }
    };

    if (m_showTotal)
    {
        const auto* total = GetTotalSeriesValues();
        if (total != nullptr)
            accumulateSeries(total);
    }

    for (int idx : m_selectedCylinderIndices)
    {
        if (idx < 0 || idx >= static_cast<int>(m_result.cylinders.size()))
            continue;

        const auto* values =
            GetCylinderSeriesValues(m_result.cylinders[static_cast<std::size_t>(idx)]);
        accumulateSeries(values);
    }

    if (!std::isfinite(valueMin) || !std::isfinite(valueMax) || valueCount == 0)
        return false;

    const double span = valueMax - valueMin;
    const double scale = std::max(1.0, maxAbsValue);

    // Если ряд практически постоянный относительно своего уровня,
    // рисуем его как прямую, а не растягиваем микрошум на всю высоту.
    const bool nearlyConstant =
        span <= std::max(1e-9, scale * 1e-6);

    if (nearlyConstant)
    {
        const double center = sumValues / static_cast<double>(valueCount);

        // Небольшой, но не микроскопический диапазон вокруг среднего,
        // чтобы линия выглядела прямой и график не "взрывался".
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

void DynamicChartPanel::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    const wxRect rect = GetClientRect();

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

    DrawBackground(dc, rect);

    const wxRect plotRect(
        rect.x + 70,
        rect.y + 20,
        std::max(10, rect.width - 95),
        std::max(10, rect.height - 70));

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

void DynamicChartPanel::InvalidatePlotCache()
{
    m_plotCacheValid = false;
}

void DynamicChartPanel::EnsurePlotCache(
    const wxRect& plotRect,
    double alphaMin,
    double alphaMax,
    double valueMin,
    double valueMax)
{
    const wxRect cacheRect(
        plotRect.x - 70,
        plotRect.y - 28,
        plotRect.width + 70 + 45,
        plotRect.height + 28 + 56);

    if (m_plotCacheValid && m_plotCache.IsOk() && m_plotCacheArea == cacheRect)
        return;

    if (cacheRect.width < 2 || cacheRect.height < 2)
        return;

    wxBitmap bmp(cacheRect.width, cacheRect.height, 24);
    if (!bmp.IsOk())
        return;

    wxMemoryDC mdc;
    mdc.SelectObject(bmp);
    mdc.SetBackground(wxBrush(wxColour(20, 26, 38)));
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

void DynamicChartPanel::OnSize(wxSizeEvent& event)
{
    InvalidatePlotCache();
    Refresh();
    event.Skip();
}

void DynamicChartPanel::DrawBackground(wxDC& dc, const wxRect& rect)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(20, 26, 38)));
    dc.DrawRectangle(rect);
}

void DynamicChartPanel::DrawAxes(wxDC& dc,
                                 const wxRect& plotRect,
                                 double alphaMin,
                                 double alphaMax,
                                 double valueMin,
                                 double valueMax)
{
    dc.SetPen(wxPen(wxColour(140, 150, 170), 1));
    dc.DrawLine(plotRect.GetLeft(), plotRect.GetBottom(), plotRect.GetRight(), plotRect.GetBottom());
    dc.DrawLine(plotRect.GetLeft(), plotRect.GetTop(), plotRect.GetLeft(), plotRect.GetBottom());

    dc.SetTextForeground(wxColour(220, 220, 220));

    const int xTicks = 6;
    for (int i = 0; i <= xTicks; ++i)
    {
        const double t = static_cast<double>(i) / xTicks;
        const int x = plotRect.GetLeft() + static_cast<int>(t * plotRect.GetWidth());
        const double alpha = alphaMin + t * (alphaMax - alphaMin);

        dc.DrawLine(x, plotRect.GetBottom(), x, plotRect.GetBottom() + 5);
        dc.DrawText(wxString::Format("%.0f", alpha), x - 12, plotRect.GetBottom() + 8);
    }

    const int yTicks = 5;
    for (int i = 0; i <= yTicks; ++i)
    {
        const double t = static_cast<double>(i) / yTicks;
        const int y = plotRect.GetBottom() - static_cast<int>(t * plotRect.GetHeight());
        const double value = valueMin + t * (valueMax - valueMin);

        dc.DrawLine(plotRect.GetLeft() - 5, y, plotRect.GetLeft(), y);
        dc.DrawText(wxString::Format("%.2f", value), plotRect.GetLeft() - 62, y - 8);

        dc.SetPen(wxPen(wxColour(45, 55, 75), 1, wxPENSTYLE_DOT));
        dc.DrawLine(plotRect.GetLeft(), y, plotRect.GetRight(), y);
        dc.SetPen(wxPen(wxColour(140, 150, 170), 1));
    }

    // Выделенная нулевая линия
    if (valueMin <= 0.0 && valueMax >= 0.0 && std::abs(valueMax - valueMin) > 1e-12)
    {
        const int zeroY = plotRect.GetBottom() -
            static_cast<int>((0.0 - valueMin) * plotRect.GetHeight() / (valueMax - valueMin));

        dc.SetPen(wxPen(wxColour(200, 200, 210), 2));
        dc.DrawLine(plotRect.GetLeft(), zeroY, plotRect.GetRight(), zeroY);
    }

    dc.DrawText("alpha, deg", plotRect.GetRight() - 70, plotRect.GetBottom() + 28);
    dc.DrawText(GetMetricLabel(), plotRect.GetLeft() + 8, plotRect.GetTop() - 2);
}

void DynamicChartPanel::DrawSeries(wxDC& dc,
                                   const wxRect& plotRect,
                                   double alphaMin,
                                   double alphaMax,
                                   double valueMin,
                                   double valueMax)
{
    auto mapX = [&](double alpha) -> int
    {
        if (std::abs(alphaMax - alphaMin) < 1e-12)
            return plotRect.GetLeft();

        return plotRect.GetLeft() +
               static_cast<int>((alpha - alphaMin) * plotRect.GetWidth() / (alphaMax - alphaMin));
    };

    auto mapY = [&](double value) -> int
    {
        if (std::abs(valueMax - valueMin) < 1e-12)
            return plotRect.GetBottom();

        return plotRect.GetBottom() -
               static_cast<int>((value - valueMin) * plotRect.GetHeight() / (valueMax - valueMin));
    };

    if (m_showTotal)
    {
        const auto* total = GetTotalSeriesValues();
        if (total != nullptr && total->size() == m_result.alphaDeg.size())
        {
            dc.SetPen(wxPen(wxColour(245, 245, 245), 2));

            wxPoint prevPoint;
            bool hasPrev = false;

            for (std::size_t i = 0; i < total->size(); ++i)
            {
                const wxPoint currentPoint(
                    mapX(m_result.alphaDeg[i]),
                    mapY(ExtractComponent((*total)[i])));

                if (hasPrev)
                    dc.DrawLine(prevPoint, currentPoint);

                prevPoint = currentPoint;
                hasPrev = true;
            }
        }
    }

    std::size_t colorIndex = 0;

    for (int idx : m_selectedCylinderIndices)
    {
        if (idx < 0 || idx >= static_cast<int>(m_result.cylinders.size()))
            continue;

        const auto& cylinder = m_result.cylinders[static_cast<std::size_t>(idx)];
        const auto* values = GetCylinderSeriesValues(cylinder);
        if (values == nullptr || values->size() != m_result.alphaDeg.size())
            continue;

        dc.SetPen(wxPen(GetSeriesColour(colorIndex++), 2));

        wxPoint prevPoint;
        bool hasPrev = false;

        for (std::size_t i = 0; i < values->size(); ++i)
        {
            const wxPoint currentPoint(
                mapX(m_result.alphaDeg[i]),
                mapY(ExtractComponent((*values)[i])));

            if (hasPrev)
                dc.DrawLine(prevPoint, currentPoint);

            prevPoint = currentPoint;
            hasPrev = true;
        }
    }
}

void DynamicChartPanel::DrawCurrentAlphaMarker(wxDC& dc,
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

    if (std::abs(alphaMax - alphaMin) < 1e-12)
        return;

    auto mapX = [&](double a) -> int
    {
        return plotRect.GetLeft() +
               static_cast<int>((a - alphaMin) * plotRect.GetWidth() / (alphaMax - alphaMin));
    };

    auto mapY = [&](double value) -> int
    {
        if (std::abs(valueMax - valueMin) < 1e-12)
            return plotRect.GetBottom();

        return plotRect.GetBottom() -
               static_cast<int>((value - valueMin) * plotRect.GetHeight() / (valueMax - valueMin));
    };

    constexpr int dotR = 5;
    auto drawDot = [&](int px, int py, const wxColour& fill)
    {
        dc.SetPen(wxPen(wxColour(255, 255, 255), 2));
        dc.SetBrush(wxBrush(fill));
        dc.DrawEllipse(px - dotR, py - dotR, 2 * dotR, 2 * dotR);
    };

    if (m_showTotal)
    {
        const auto* total = GetTotalSeriesValues();
        if (total != nullptr && index < total->size() && total->size() == m_result.alphaDeg.size())
        {
            const int px = mapX(alpha);
            const int py = mapY(ExtractComponent((*total)[index]));
            drawDot(px, py, wxColour(245, 245, 245));
        }
    }

    std::size_t colorIndex = 0;
    for (int idx : m_selectedCylinderIndices)
    {
        if (idx < 0 || idx >= static_cast<int>(m_result.cylinders.size()))
            continue;

        const auto& cylinder = m_result.cylinders[static_cast<std::size_t>(idx)];
        const auto* values = GetCylinderSeriesValues(cylinder);
        if (values == nullptr || values->size() != m_result.alphaDeg.size() || index >= values->size())
            continue;

        const int px = mapX(alpha);
        const int py = mapY(ExtractComponent((*values)[index]));
        drawDot(px, py, GetSeriesColour(colorIndex++));
    }
}

void DynamicChartPanel::DrawEmptyState(wxDC& dc, const wxRect& rect)
{
    DrawBackground(dc, rect);
    dc.SetTextForeground(wxColour(210, 210, 210));
    dc.DrawText("Dynamic results are not available yet.", rect.x + 20, rect.y + 20);
}