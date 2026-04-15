#include "gui/widgets/balancing_chart_panel.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <wx/dcbuffer.h>
#include "gui/common/text_utf8.h"

wxBEGIN_EVENT_TABLE(BalancingChartPanel, wxPanel)
    EVT_PAINT(BalancingChartPanel::OnPaint)
    EVT_SIZE(BalancingChartPanel::OnSize)
wxEND_EVENT_TABLE()

BalancingChartPanel::BalancingChartPanel(wxWindow* parent)
    : wxPanel(parent)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(18, 24, 36));
    SetForegroundColour(wxColour(235, 235, 235));
}

void BalancingChartPanel::SetResult(const engine::balancing::BalancingComposedResult& result)
{
    m_result = result;
    m_currentAlphaIndex = 0;
    Refresh();
}

void BalancingChartPanel::SetMetric(BalancingMetric metric)
{
    if (m_metric == metric)
        return;

    m_metric = metric;
    Refresh();
}

void BalancingChartPanel::SetViewMode(BalancingViewMode viewMode)
{
    if (m_viewMode == viewMode)
        return;

    m_viewMode = viewMode;
    Refresh();
}

void BalancingChartPanel::SetComponent(BalancingComponent component)
{
    if (m_component == component)
        return;

    m_component = component;
    Refresh();
}

void BalancingChartPanel::SetCurrentAlphaIndex(std::size_t index)
{
    if (m_result.alphaDeg.empty())
    {
        m_currentAlphaIndex = 0;
        Refresh();
        return;
    }

    m_currentAlphaIndex = std::min(index, m_result.alphaDeg.size() - 1);
    Refresh();
}

const std::vector<engine::kinematic::Vec3>* BalancingChartPanel::GetSelectedSeriesValues() const
{
    switch (m_viewMode)
    {
    case BalancingViewMode::Source:
        switch (m_metric)
        {
        case BalancingMetric::InertiaForce:
            return &m_result.sourceInertiaForce;
        case BalancingMetric::InertiaForceFirstOrder:
            return &m_result.sourceInertiaForce1;
        case BalancingMetric::InertiaForceSecondOrder:
            return &m_result.sourceInertiaForce2;
        case BalancingMetric::InertiaMomentFirstOrder:
            return &m_result.sourceInertiaMoment1;
        case BalancingMetric::InertiaMomentSecondOrder:
            return &m_result.sourceInertiaMoment2;
        case BalancingMetric::CentrifugalForce:
            return &m_result.sourceCentrifugalForce;
        case BalancingMetric::CentrifugalMoment:
            return &m_result.sourceCentrifugalMoment;
        }
        break;

    case BalancingViewMode::Counterweight:
        switch (m_metric)
        {
        case BalancingMetric::InertiaForce:
            return &m_result.counterweightInertiaForce;
        case BalancingMetric::InertiaForceFirstOrder:
            return &m_result.balancerInertiaForce1;
        case BalancingMetric::InertiaForceSecondOrder:
            return &m_result.balancerInertiaForce2;
        case BalancingMetric::InertiaMomentFirstOrder:
            return &m_result.balancerInertiaMoment1;
        case BalancingMetric::InertiaMomentSecondOrder:
            return &m_result.balancerInertiaMoment2;
        case BalancingMetric::CentrifugalForce:
            return &m_result.counterweightCentrifugalForce;
        case BalancingMetric::CentrifugalMoment:
            return &m_result.counterweightCentrifugalMoment;
        }
        break;

    case BalancingViewMode::Residual:
        switch (m_metric)
        {
        case BalancingMetric::InertiaForce:
            return &m_result.residualInertiaForce;
        case BalancingMetric::InertiaForceFirstOrder:
            return &m_result.residualInertiaForce1;
        case BalancingMetric::InertiaForceSecondOrder:
            return &m_result.residualInertiaForce2;
        case BalancingMetric::InertiaMomentFirstOrder:
            return &m_result.residualInertiaMoment1;
        case BalancingMetric::InertiaMomentSecondOrder:
            return &m_result.residualInertiaMoment2;
        case BalancingMetric::CentrifugalForce:
            return &m_result.residualCentrifugalForce;
        case BalancingMetric::CentrifugalMoment:
            return &m_result.residualCentrifugalMoment;
        }
        break;
    }

    return nullptr;
}

double BalancingChartPanel::ExtractComponent(const engine::kinematic::Vec3& v) const
{
    switch (m_component)
    {
    case BalancingComponent::X:
        return v.x;
    case BalancingComponent::Y:
        return v.y;
    case BalancingComponent::Z:
        return v.z;
    case BalancingComponent::Magnitude:
        return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    }

    return 0.0;
}

wxString BalancingChartPanel::GetMetricLabel() const
{
    wxString base;
    switch (m_metric)
    {
    case BalancingMetric::InertiaForce: base = WXU8("F"); break;
    case BalancingMetric::InertiaForceFirstOrder: base = WXU8("F1"); break;
    case BalancingMetric::InertiaForceSecondOrder: base = WXU8("F2"); break;
    case BalancingMetric::InertiaMomentFirstOrder: base = WXU8("M1"); break;
    case BalancingMetric::InertiaMomentSecondOrder: base = WXU8("M2"); break;
    case BalancingMetric::CentrifugalForce: base = WXU8("Fc"); break;
    case BalancingMetric::CentrifugalMoment: base = WXU8("Mc"); break;
    }

    wxString mode;
    switch (m_viewMode)
    {
    case BalancingViewMode::Source: mode = WXU8("исх."); break;
    case BalancingViewMode::Counterweight: mode = WXU8("вклад пр."); break;
    case BalancingViewMode::Residual: mode = WXU8("остат."); break;
    }

    wxString comp =
        (m_component == BalancingComponent::Magnitude) ? WXU8("|.|") :
        (m_component == BalancingComponent::X) ? WXU8("X") :
        (m_component == BalancingComponent::Y) ? WXU8("Y") : WXU8("Z");

    const bool isMoment =
        (m_metric == BalancingMetric::InertiaMomentFirstOrder ||
         m_metric == BalancingMetric::InertiaMomentSecondOrder ||
         m_metric == BalancingMetric::CentrifugalMoment);

    return wxString::Format(WXU8("%s %s %s, %s"),
                            base,
                            mode,
                            comp,
                            isMoment ? WXU8("Н·м") : WXU8("Н"));
}

bool BalancingChartPanel::HasData() const
{
    const auto* values = GetSelectedSeriesValues();
    return values != nullptr &&
           !m_result.alphaDeg.empty() &&
           values->size() == m_result.alphaDeg.size();
}

bool BalancingChartPanel::ComputeRanges(double& alphaMin,
                                        double& alphaMax,
                                        double& valueMin,
                                        double& valueMax) const
{
    if (!HasData())
        return false;

    const auto* values = GetSelectedSeriesValues();
    if (!values)
        return false;

    alphaMin = m_result.alphaDeg.front();
    alphaMax = m_result.alphaDeg.back();

    valueMin = std::numeric_limits<double>::infinity();
    valueMax = -std::numeric_limits<double>::infinity();

    double sumValues = 0.0;
    std::size_t valueCount = 0;
    double maxAbsValue = 0.0;

    for (const auto& v : *values)
    {
        const double value = ExtractComponent(v);
        valueMin = std::min(valueMin, value);
        valueMax = std::max(valueMax, value);
        sumValues += value;
        ++valueCount;
        maxAbsValue = std::max(maxAbsValue, std::abs(value));
    }

    if (!std::isfinite(valueMin) || !std::isfinite(valueMax) || valueCount == 0)
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

void BalancingChartPanel::OnPaint(wxPaintEvent&)
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

    DrawAxes(dc, plotRect, alphaMin, alphaMax, valueMin, valueMax);
    DrawSeries(dc, plotRect, alphaMin, alphaMax, valueMin, valueMax);
    DrawCurrentAlphaMarker(dc, plotRect, alphaMin, alphaMax);
}

void BalancingChartPanel::OnSize(wxSizeEvent& event)
{
    Refresh();
    event.Skip();
}

void BalancingChartPanel::DrawBackground(wxDC& dc, const wxRect& rect)
{
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(20, 26, 38)));
    dc.DrawRectangle(rect);
}

void BalancingChartPanel::DrawAxes(wxDC& dc,
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

void BalancingChartPanel::DrawSeries(wxDC& dc,
                                     const wxRect& plotRect,
                                     double alphaMin,
                                     double alphaMax,
                                     double valueMin,
                                     double valueMax)
{
    const auto* values = GetSelectedSeriesValues();
    if (!values)
        return;

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

    dc.SetPen(wxPen(wxColour(245, 245, 245), 2));

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

void BalancingChartPanel::DrawCurrentAlphaMarker(wxDC& dc,
                                                 const wxRect& plotRect,
                                                 double alphaMin,
                                                 double alphaMax)
{
    if (m_result.alphaDeg.empty())
        return;

    const double alpha = m_result.alphaDeg[std::min(m_currentAlphaIndex, m_result.alphaDeg.size() - 1)];

    if (std::abs(alphaMax - alphaMin) < 1e-12)
        return;

    const int x = plotRect.GetLeft() +
                  static_cast<int>((alpha - alphaMin) * plotRect.GetWidth() / (alphaMax - alphaMin));

    dc.SetPen(wxPen(wxColour(255, 210, 90), 1));
    dc.DrawLine(x, plotRect.GetTop(), x, plotRect.GetBottom());
}

void BalancingChartPanel::DrawEmptyState(wxDC& dc, const wxRect& rect)
{
    DrawBackground(dc, rect);
    dc.SetTextForeground(wxColour(210, 210, 210));
    dc.DrawText(WXU8("Результаты уравновешивания пока недоступны."), rect.x + 20, rect.y + 20);
}