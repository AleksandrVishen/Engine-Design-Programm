#include "gui/report/report_builder.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

#include "core/balancing/balancing_pipeline.h"
#include "core/dynamic/dynamic_result.h"

#include <wx/base64.h>
#include <wx/dcmemory.h>
#include <wx/datetime.h>
#include <wx/filesys.h>
#include <wx/fs_mem.h>
#include <wx/html/htmprint.h>
#include <wx/mstream.h>
#include <wx/print.h>
#include <wx/timer.h>
#include <wx/window.h>

namespace
{
struct SeriesStats
{
    double minValue = 0.0;
    double minAlphaDeg = 0.0;
    double maxValue = 0.0;
    double maxAlphaDeg = 0.0;
    bool valid = false;
};

wxString HtmlEscape(wxString text)
{
    text.Replace("&", "&amp;");
    text.Replace("<", "&lt;");
    text.Replace(">", "&gt;");
    text.Replace("\"", "&quot;");
    return text;
}

wxString FormatDouble(double value, int precision = 4)
{
    if (!std::isfinite(value))
        return wxString("—");
    return wxString::Format("%.*f", precision, value);
}

SeriesStats ComputeSeriesStats(const std::vector<double>& alphaDeg, const std::vector<double>& values)
{
    SeriesStats stats;
    const std::size_t count = std::min(alphaDeg.size(), values.size());
    if (count == 0)
        return stats;

    double minValue = std::numeric_limits<double>::infinity();
    double maxValue = -std::numeric_limits<double>::infinity();
    std::size_t minIndex = 0;
    std::size_t maxIndex = 0;
    bool found = false;

    for (std::size_t i = 0; i < count; ++i)
    {
        const double value = values[i];
        if (!std::isfinite(value))
            continue;

        if (!found || value < minValue)
        {
            minValue = value;
            minIndex = i;
        }
        if (!found || value > maxValue)
        {
            maxValue = value;
            maxIndex = i;
        }
        found = true;
    }

    if (!found)
        return stats;

    stats.minValue = minValue;
    stats.minAlphaDeg = alphaDeg[minIndex];
    stats.maxValue = maxValue;
    stats.maxAlphaDeg = alphaDeg[maxIndex];
    stats.valid = true;
    return stats;
}

bool HasSeriesEnabled(const ReportOptions& options, const std::string& id)
{
    for (const auto& option : options.seriesOptions)
    {
        if (option.seriesId == id && option.includeSeries)
            return true;
    }
    return false;
}

const ReportSeriesOptions* FindSeriesOptions(const ReportOptions& options, const std::string& id)
{
    for (const auto& option : options.seriesOptions)
    {
        if (option.seriesId == id)
            return &option;
    }
    return nullptr;
}

static std::vector<wxString> g_reportMemoryPaths;
static bool g_memoryFsHandlerAdded = false;

void EnsureReportMemoryFilesystem()
{
    if (!g_memoryFsHandlerAdded)
    {
        wxFileSystem::AddHandler(new wxMemoryFSHandler);
        g_memoryFsHandlerAdded = true;
    }
}

void ReleaseReportMemoryImages()
{
    for (const wxString& path : g_reportMemoryPaths)
        wxMemoryFSHandler::RemoveFile(path);
    g_reportMemoryPaths.clear();
}

bool RenderKinematicChartPng(
    const std::vector<double>& alphaDeg,
    const std::vector<double>& values,
    const wxString& title,
    const wxString& yUnit,
    wxMemoryOutputStream& out)
{
    (void)title;

    const std::size_t count = std::min(alphaDeg.size(), values.size());
    if (count < 2)
        return false;

    // Компактный холст для A4 / PDF (wxHtml масштабирует img по ширине).
    const int width = 600;
    const int height = 220;
    const int leftPad = 4;
    const int rightPad = 8;
    const int topPad = 6;
    const int bottomPad = 44;
    const int yTickColW = 58;
    const int plotLeft = leftPad + yTickColW;
    const int plotRight = width - rightPad;
    const int plotTop = topPad;
    const int plotBottom = height - bottomPad;
    const wxRect plotRect(plotLeft, plotTop, std::max(1, plotRight - plotLeft), std::max(1, plotBottom - plotTop));

    wxBitmap bitmap(width, height, 32);
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(*wxWHITE));
    dc.Clear();

    dc.SetTextForeground(*wxBLACK);
    wxFont fontLabel = dc.GetFont();
    fontLabel.SetPointSize(8);
    dc.SetFont(fontLabel);

    double alphaMin = alphaDeg.front();
    double alphaMax = alphaDeg.front();
    double valueMin = std::numeric_limits<double>::infinity();
    double valueMax = -std::numeric_limits<double>::infinity();

    for (std::size_t i = 0; i < count; ++i)
    {
        alphaMin = std::min(alphaMin, alphaDeg[i]);
        alphaMax = std::max(alphaMax, alphaDeg[i]);
        if (std::isfinite(values[i]))
        {
            valueMin = std::min(valueMin, values[i]);
            valueMax = std::max(valueMax, values[i]);
        }
    }

    if (!std::isfinite(valueMin) || !std::isfinite(valueMax))
        return false;

    const double alphaSpan = std::max(1e-9, alphaMax - alphaMin);
    const double valueSpanRaw = valueMax - valueMin;
    const double valueMargin = (valueSpanRaw < 1e-9) ? 1e-3 : valueSpanRaw * 0.08;
    valueMin -= valueMargin;
    valueMax += valueMargin;
    const double valueSpan = std::max(1e-9, valueMax - valueMin);

    dc.SetPen(wxPen(wxColour(120, 120, 120), 1));
    dc.DrawRectangle(plotRect);

    constexpr int kXDivisions = 6;
    for (int i = 0; i <= kXDivisions; ++i)
    {
        const double t = static_cast<double>(i) / static_cast<double>(kXDivisions);
        const int x = plotRect.x + static_cast<int>(t * plotRect.width);
        dc.SetPen(wxPen(wxColour(220, 220, 220), 1));
        dc.DrawLine(x, plotRect.y, x, plotRect.y + plotRect.height);
        const double alpha = alphaMin + t * alphaSpan;
        const wxString alphaTxt = wxString::Format("%.0f", alpha);
        int tw = 0;
        int th = 0;
        dc.GetTextExtent(alphaTxt, &tw, &th);
        int tx = x - tw / 2;
        tx = std::max(plotRect.x, std::min(tx, plotRect.x + plotRect.width - tw));
        dc.DrawText(alphaTxt, tx, plotRect.y + plotRect.height + 4);
    }

    constexpr int kYDivisions = 5;
    for (int i = 0; i <= kYDivisions; ++i)
    {
        const double t = static_cast<double>(i) / static_cast<double>(kYDivisions);
        const int y = plotRect.y + plotRect.height - static_cast<int>(t * plotRect.height);
        dc.SetPen(wxPen(wxColour(220, 220, 220), 1));
        dc.DrawLine(plotRect.x, y, plotRect.x + plotRect.width, y);
        const double value = valueMin + t * valueSpan;
        const wxString valTxt = wxString::Format("%.4g", value);
        int tw = 0;
        int th = 0;
        dc.GetTextExtent(valTxt, &tw, &th);
        const int tx = std::max(leftPad, plotRect.x - tw - 4);
        const int tyMin = plotRect.y - 2;
        const int tyMax = plotRect.y + plotRect.height - th + 2;
        const int ty = std::max(tyMin, std::min(y - th / 2, tyMax));
        dc.DrawText(valTxt, tx, ty);
    }

    const wxString xCaption = wxString::FromUTF8(u8"Угол \u03b1, град");
    int xcw = 0;
    int xch = 0;
    dc.GetTextExtent(xCaption, &xcw, &xch);
    dc.DrawText(xCaption, plotRect.x + std::max(0, (plotRect.width - xcw) / 2), plotRect.y + plotRect.height + 20);

    if (!yUnit.IsEmpty())
    {
        int uw = 0;
        int uh = 0;
        dc.GetTextExtent(yUnit, &uw, &uh);
        const int ux = std::min(plotRect.x + plotRect.width - uw - 6, plotRect.x + plotRect.width - 8);
        dc.DrawText(yUnit, std::max(plotRect.x + 4, ux), plotRect.y + 4);
    }

    dc.SetPen(wxPen(wxColour(40, 90, 200), 2));
    bool first = true;
    wxPoint prev;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!std::isfinite(values[i]))
            continue;
        const double tx = (alphaDeg[i] - alphaMin) / alphaSpan;
        const double ty = (values[i] - valueMin) / valueSpan;
        const int x = plotRect.x + static_cast<int>(tx * plotRect.width);
        const int y = plotRect.y + plotRect.height - static_cast<int>(ty * plotRect.height);
        const wxPoint pt(x, y);
        if (!first)
            dc.DrawLine(prev, pt);
        first = false;
        prev = pt;
    }

    dc.SelectObject(wxNullBitmap);
    wxImage image = bitmap.ConvertToImage();
    if (!image.IsOk())
        return false;

    return image.SaveFile(out, wxBITMAP_TYPE_PNG);
}

wxString BuildChartImgSrcForHtml(
    const std::vector<double>& alphaDeg,
    const std::vector<double>& values,
    const wxString& title,
    const wxString& yUnit,
    ReportHtmlImageMode imageMode,
    const wxString& memPrefix,
    int& memSeq)
{
    wxMemoryOutputStream memoryStream;
    if (!RenderKinematicChartPng(alphaDeg, values, title, yUnit, memoryStream))
        return wxString();

    const std::size_t size = memoryStream.GetSize();
    if (size == 0)
        return wxString();

    if (imageMode == ReportHtmlImageMode::DataUriForBrowserFile)
    {
        std::string pngData;
        pngData.resize(size);
        memoryStream.CopyTo(pngData.data(), size);
        const wxString encoded = wxBase64Encode(pngData.data(), pngData.size());
        return "data:image/png;base64," + encoded;
    }

    EnsureReportMemoryFilesystem();
    const wxString memName = memPrefix + wxString::Format("g%04d.png", memSeq++);
    std::string pngData;
    pngData.resize(size);
    memoryStream.CopyTo(pngData.data(), size);
    wxMemoryFSHandler::AddFile(memName, pngData.data(), pngData.size());
    g_reportMemoryPaths.push_back(memName);
    return "memory:" + memName;
}

void AppendSeriesValueTable(
    std::ostringstream& html,
    const std::vector<double>& alphaDeg,
    const std::vector<double>& values,
    const wxString& unit,
    int stride)
{
    const std::size_t count = std::min(alphaDeg.size(), values.size());
    if (count == 0)
    {
        html << "<p>Нет данных.</p>";
        return;
    }

    const std::size_t step = static_cast<std::size_t>(std::max(1, stride));
    html << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\" width=\"100%\" class=\"data-table\">"
         << "<tr><th>Alpha, град</th><th>Значение, " << unit.ToUTF8() << "</th></tr>";
    for (std::size_t i = 0; i < count; i += step)
    {
        html << "<tr><td>" << alphaDeg[i] << "</td><td>";
        if (std::isfinite(values[i]))
            html << values[i];
        else
            html << "—";
        html << "</td></tr>";
    }
    html << "</table>";
}
} // namespace

std::vector<ReportSeriesDescriptor> ReportBuilder::BuildSeriesDescriptors(
    const engine::kinematic::CylinderKinematicSeries& cylinder)
{
    return {
        { "displacementM", wxString::FromUTF8("Перемещение"), wxString::FromUTF8("м"), &cylinder.displacementM },
        { "velocityMps", wxString::FromUTF8("Скорость"), wxString::FromUTF8("м/с"), &cylinder.velocityMps },
        { "accelerationMps2", wxString::FromUTF8("Ускорение"), wxString::FromUTF8("м/с²"), &cylinder.accelerationMps2 },
        { "accelerationFirstOrderMps2", wxString::FromUTF8("Ускорение 1-го порядка"), wxString::FromUTF8("м/с²"), &cylinder.accelerationFirstOrderMps2 },
        { "accelerationSecondOrderMps2", wxString::FromUTF8("Ускорение 2-го порядка"), wxString::FromUTF8("м/с²"), &cylinder.accelerationSecondOrderMps2 },
        { "rodAngleRad", wxString::FromUTF8("Угол шатуна"), wxString::FromUTF8("рад"), &cylinder.rodAngleRad },
        { "rodAngularVelocityRadS", wxString::FromUTF8("Угловая скорость шатуна"), wxString::FromUTF8("рад/с"), &cylinder.rodAngularVelocityRadS },
        { "rodAngularAccelerationRadS2", wxString::FromUTF8("Угловое ускорение шатуна"), wxString::FromUTF8("рад/с²"), &cylinder.rodAngularAccelerationRadS2 },
    };
}

namespace
{
void AppendCylinderSectionHtml(
    std::ostringstream& html,
    const engine::kinematic::KinematicResult& result,
    int cylinderIndex,
    const ReportOptions& options,
    ReportHtmlImageMode imageMode,
    const wxString& memPrefix,
    int& memChartSeq)
{
    if (cylinderIndex < 0 || static_cast<std::size_t>(cylinderIndex) >= result.cylinders.size())
        return;

    const auto& cylinder = result.cylinders[static_cast<std::size_t>(cylinderIndex)];
    const auto series = ReportBuilder::BuildSeriesDescriptors(cylinder);

    html << "<h2>Цилиндр " << cylinder.cylinderNumber << " (вал " << cylinder.shaftNumber
         << ", кривошип " << cylinder.crankNumber << ")</h2>";

    for (const auto& descriptor : series)
    {
        const ReportSeriesOptions* seriesOptions = FindSeriesOptions(options, descriptor.id);
        if (seriesOptions == nullptr || !seriesOptions->includeSeries || descriptor.values == nullptr)
            continue;

        const auto& values = *descriptor.values;
        html << "<div class=\"report-series\">";
        html << "<h3>" << HtmlEscape(descriptor.displayName).ToUTF8() << "</h3>";

        const std::size_t count = std::min(values.size(), result.alphaDeg.size());
        if (count == 0)
        {
            html << "<p>Нет данных.</p></div>";
            continue;
        }

        if (seriesOptions->includeChart)
        {
            wxString chartSrc = BuildChartImgSrcForHtml(
                result.alphaDeg,
                values,
                descriptor.displayName,
                descriptor.unit,
                imageMode,
                memPrefix,
                memChartSeq);
            if (!chartSrc.IsEmpty())
                html << "<div class=\"chart-wrap\"><img src=\"" << chartSrc.ToUTF8() << "\" alt=\"\"/></div>";
            else
                html << "<p>График недоступен (нет данных).</p>";
        }

        if (seriesOptions->includeStats)
        {
            const SeriesStats stats = ComputeSeriesStats(result.alphaDeg, values);
            if (stats.valid)
            {
                html << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\" width=\"100%\" class=\"stats\">"
                     << "<tr><th>Величина</th><th>Минимум</th><th>Alpha min, град</th>"
                     << "<th>Максимум</th><th>Alpha max, град</th></tr>";
                html << "<tr><td>" << HtmlEscape(descriptor.displayName).ToUTF8() << "</td>"
                     << "<td>" << FormatDouble(stats.minValue, 4).ToUTF8() << " " << descriptor.unit.ToUTF8() << "</td>"
                     << "<td>" << FormatDouble(stats.minAlphaDeg, 2).ToUTF8() << "</td>"
                     << "<td>" << FormatDouble(stats.maxValue, 4).ToUTF8() << " " << descriptor.unit.ToUTF8() << "</td>"
                     << "<td>" << FormatDouble(stats.maxAlphaDeg, 2).ToUTF8() << "</td></tr></table>";
            }
            else
                html << "<p>Нет данных для статистики min/max.</p>";
        }

        if (seriesOptions->includeTable)
            AppendSeriesValueTable(html, result.alphaDeg, values, descriptor.unit, options.tableStride);

        html << "</div>";
    }

    if (options.includeCombinedTable && !options.combinedTableSeriesIds.empty())
    {
        html << "<h3>Таблица расчетных результатов (цилиндр " << cylinder.cylinderNumber << ")</h3>";
        html << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\" width=\"100%\" class=\"combined\">"
             << "<tr><th>Alpha, град</th>";
        for (const auto& id : options.combinedTableSeriesIds)
        {
            for (const auto& descriptor : series)
            {
                if (descriptor.id == id && HasSeriesEnabled(options, id))
                    html << "<th>" << HtmlEscape(descriptor.displayName).ToUTF8() << ", " << descriptor.unit.ToUTF8() << "</th>";
            }
        }
        html << "</tr>";

        const std::size_t count = result.alphaDeg.size();
        const std::size_t step = static_cast<std::size_t>(std::max(1, options.tableStride));
        for (std::size_t i = 0; i < count; i += step)
        {
            html << "<tr><td>" << result.alphaDeg[i] << "</td>";
            for (const auto& id : options.combinedTableSeriesIds)
            {
                bool appended = false;
                for (const auto& descriptor : series)
                {
                    if (descriptor.id != id || descriptor.values == nullptr || !HasSeriesEnabled(options, id))
                        continue;

                    const auto& vals = *descriptor.values;
                    if (i < vals.size() && std::isfinite(vals[i]))
                        html << "<td>" << vals[i] << "</td>";
                    else
                        html << "<td>—</td>";
                    appended = true;
                    break;
                }
                if (!appended)
                    html << "<td>—</td>";
            }
            html << "</tr>";
        }
        html << "</table>";
    }
}

void AppendCylinderSectionPlain(
    std::ostringstream& txt,
    const engine::kinematic::KinematicResult& result,
    int cylinderIndex,
    const ReportOptions& options)
{
    if (cylinderIndex < 0 || static_cast<std::size_t>(cylinderIndex) >= result.cylinders.size())
        return;

    const auto& cylinder = result.cylinders[static_cast<std::size_t>(cylinderIndex)];
    const auto series = ReportBuilder::BuildSeriesDescriptors(cylinder);

    txt << "\n"
        << "================================================================================\n";
    txt << "Цилиндр " << cylinder.cylinderNumber << " (вал " << cylinder.shaftNumber << ", кривошип "
        << cylinder.crankNumber << ")\n";
    txt << "================================================================================\n";

    for (const auto& descriptor : series)
    {
        const ReportSeriesOptions* seriesOptions = FindSeriesOptions(options, descriptor.id);
        if (seriesOptions == nullptr || !seriesOptions->includeSeries || descriptor.values == nullptr)
            continue;

        const auto& values = *descriptor.values;
        txt << "\n--- " << descriptor.displayName.ToUTF8() << " (" << descriptor.unit.ToUTF8() << ") ---\n";

        const std::size_t count = std::min(values.size(), result.alphaDeg.size());
        if (count == 0)
        {
            txt << "Нет данных.\n";
            continue;
        }

        if (seriesOptions->includeChart)
            txt << "(График в текстовом формате не вставляется; используйте HTML или PDF.)\n";

        if (seriesOptions->includeStats)
        {
            const SeriesStats stats = ComputeSeriesStats(result.alphaDeg, values);
            if (stats.valid)
            {
                txt << "Минимум: " << FormatDouble(stats.minValue, 4).ToUTF8() << " " << descriptor.unit.ToUTF8()
                    << " при alpha = " << FormatDouble(stats.minAlphaDeg, 2).ToUTF8() << " град\n";
                txt << "Максимум: " << FormatDouble(stats.maxValue, 4).ToUTF8() << " " << descriptor.unit.ToUTF8()
                    << " при alpha = " << FormatDouble(stats.maxAlphaDeg, 2).ToUTF8() << " град\n";
            }
            else
                txt << "Нет данных для статистики min/max.\n";
        }

        if (seriesOptions->includeTable)
        {
            txt << "Таблица (шаг " << std::max(1, options.tableStride) << "):\n";
            txt << "alpha_deg\tvalue\n";
            const std::size_t step = static_cast<std::size_t>(std::max(1, options.tableStride));
            for (std::size_t i = 0; i < count; i += step)
            {
                txt << result.alphaDeg[i] << '\t';
                if (std::isfinite(values[i]))
                    txt << values[i];
                else
                    txt << "—";
                txt << '\n';
            }
        }
    }

    if (options.includeCombinedTable && !options.combinedTableSeriesIds.empty())
    {
        txt << "\n--- Таблица расчетных результатов (цилиндр " << cylinder.cylinderNumber << ", шаг "
            << std::max(1, options.tableStride) << ") ---\n";

        txt << "alpha_deg";
        for (const auto& id : options.combinedTableSeriesIds)
        {
            for (const auto& descriptor : series)
            {
                if (descriptor.id == id && HasSeriesEnabled(options, id))
                    txt << '\t' << descriptor.displayName.ToUTF8();
            }
        }
        txt << '\n';

        const std::size_t count = result.alphaDeg.size();
        const std::size_t step = static_cast<std::size_t>(std::max(1, options.tableStride));
        for (std::size_t i = 0; i < count; i += step)
        {
            txt << result.alphaDeg[i];
            for (const auto& id : options.combinedTableSeriesIds)
            {
                txt << '\t';
                bool wrote = false;
                for (const auto& descriptor : series)
                {
                    if (descriptor.id != id || descriptor.values == nullptr || !HasSeriesEnabled(options, id))
                        continue;
                    const auto& vals = *descriptor.values;
                    if (i < vals.size() && std::isfinite(vals[i]))
                        txt << vals[i];
                    else
                        txt << "—";
                    wrote = true;
                    break;
                }
                if (!wrote)
                    txt << "—";
            }
            txt << '\n';
        }
    }
}

using Vec3 = engine::kinematic::Vec3;

bool DynamicMatchesKinematic(const engine::dynamic::DynamicResult& d, const engine::kinematic::KinematicResult& k)
{
    if (d.alphaDeg.size() != k.alphaDeg.size())
        return false;
    if (d.cylinders.size() != k.cylinders.size())
        return false;
    return true;
}

bool Vec3SeriesHasData(const std::vector<Vec3>& series)
{
    for (const auto& v : series)
    {
        if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
            continue;
        if (std::abs(v.x) > 1e-18 || std::abs(v.y) > 1e-18 || std::abs(v.z) > 1e-18)
            return true;
    }
    return false;
}

void StreamVec3CellHtml(std::ostringstream& html, double c)
{
    if (!std::isfinite(c))
        html << u8"—";
    else
        html << wxString::Format("%.5g", c).ToUTF8();
}

void AppendVec3SeriesHtml(
    std::ostringstream& html,
    const std::vector<double>& alphaDeg,
    const std::vector<Vec3>& series,
    const char* title,
    bool isMoment,
    int stride)
{
    if (series.size() != alphaDeg.size() || series.empty())
        return;
    if (!Vec3SeriesHasData(series))
        return;

    const char* unit = isMoment ? u8"Н·м" : u8"Н";
    html << "<h4>" << title << "</h4>\n";
    html << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\" width=\"100%\" class=\"vec-table\"><tr>"
         << "<th>" << u8"α, град" << "</th><th>X, " << unit << "</th><th>Y, " << unit << "</th><th>Z, " << unit
         << "</th></tr>";

    const std::size_t step = static_cast<std::size_t>(std::max(1, stride));
    for (std::size_t i = 0; i < alphaDeg.size(); i += step)
    {
        const Vec3& v = series[i];
        html << "<tr><td>" << alphaDeg[i] << "</td><td>";
        StreamVec3CellHtml(html, v.x);
        html << "</td><td>";
        StreamVec3CellHtml(html, v.y);
        html << "</td><td>";
        StreamVec3CellHtml(html, v.z);
        html << "</td></tr>";
    }
    html << "</table>\n";
}

void AppendCylinderDynamicSectionHtml(
    std::ostringstream& html,
    const engine::dynamic::CylinderDynamicSeries& c,
    const std::vector<double>& alphaDeg,
    int stride)
{
    html << "<h3>Цилиндр " << c.cylinderNumber << " (вал " << c.shaftNumber << ", кривошип " << c.crankNumber
         << ") — динамика</h3>\n";

    AppendVec3SeriesHtml(html, alphaDeg, c.inertiaForce, u8"Сила инерции F", false, stride);
    AppendVec3SeriesHtml(html, alphaDeg, c.inertiaForce1, u8"Сила инерции 1-го порядка F1", false, stride);
    AppendVec3SeriesHtml(html, alphaDeg, c.inertiaForce2, u8"Сила инерции 2-го порядка F2", false, stride);
    AppendVec3SeriesHtml(html, alphaDeg, c.inertiaMoment1, u8"Момент от сил 1-го порядка M1", true, stride);
    AppendVec3SeriesHtml(html, alphaDeg, c.inertiaMoment2, u8"Момент от сил 2-го порядка M2", true, stride);
    AppendVec3SeriesHtml(html, alphaDeg, c.centrifugalForce, u8"Центробежная сила Fc", false, stride);
    AppendVec3SeriesHtml(html, alphaDeg, c.centrifugalMoment, u8"Момент от центробежной силы Mc", true, stride);
}

void AppendDynamicTotalsHtml(std::ostringstream& html, const engine::dynamic::DynamicResult& dyn, int stride)
{
    html << "<h3>Сумма по всем цилиндрам</h3>\n";
    const auto& a = dyn.alphaDeg;
    AppendVec3SeriesHtml(html, a, dyn.totalInertiaForce, u8"Σ Сила инерции F", false, stride);
    AppendVec3SeriesHtml(html, a, dyn.totalInertiaForce1, u8"Σ Сила инерции 1-го порядка F1", false, stride);
    AppendVec3SeriesHtml(html, a, dyn.totalInertiaForce2, u8"Σ Сила инерции 2-го порядка F2", false, stride);
    AppendVec3SeriesHtml(html, a, dyn.totalInertiaMoment1, u8"Σ Момент от сил 1-го порядка M1", true, stride);
    AppendVec3SeriesHtml(html, a, dyn.totalInertiaMoment2, u8"Σ Момент от сил 2-го порядка M2", true, stride);
    AppendVec3SeriesHtml(html, a, dyn.totalCentrifugalForce, u8"Σ Центробежная сила Fc", false, stride);
    AppendVec3SeriesHtml(html, a, dyn.totalCentrifugalMoment, u8"Σ Момент от центробежной силы Mc", true, stride);
}

void AppendFullDynamicHtml(
    std::ostringstream& html,
    const engine::dynamic::DynamicResult& dyn,
    const std::vector<int>& cylinderIndices,
    int stride)
{
    html << "<h2>Динамика: силы и моменты</h2>\n";
    html << "<p class=\"meta\">Единицы: силы — Н, моменты — Н·м. Шаг таблиц по углу: " << stride << ".</p>\n";

    for (int idx : cylinderIndices)
    {
        if (idx < 0 || static_cast<std::size_t>(idx) >= dyn.cylinders.size())
            continue;
        AppendCylinderDynamicSectionHtml(html, dyn.cylinders[static_cast<std::size_t>(idx)], dyn.alphaDeg, stride);
    }
    AppendDynamicTotalsHtml(html, dyn, stride);
}

void AppendFullBalancingHtml(
    std::ostringstream& html,
    const engine::balancing::BalancingPipelineResult& pipe,
    int stride)
{
    html << "<h2>Уравновешивание</h2>\n";
    if (!pipe.ok)
    {
        html << "<p>Расчет уравновешивания завершился с ошибками.</p>\n<ul>\n";
        for (const auto& e : pipe.errors)
        {
            const wxString line = wxString::FromUTF8(e.message);
            html << "<li>" << HtmlEscape(line).ToUTF8() << "</li>\n";
        }
        html << "</ul>\n";
        if (!pipe.warnings.empty())
        {
            html << "<p>Предупреждения:</p>\n<ul>\n";
            for (const auto& w : pipe.warnings)
            {
                const wxString line = wxString::FromUTF8(w.message);
                html << "<li>" << HtmlEscape(line).ToUTF8() << "</li>\n";
            }
            html << "</ul>\n";
        }
        return;
    }

    const engine::balancing::BalancingComposedResult& c = pipe.composedResult;
    if (c.alphaDeg.empty())
    {
        html << "<p>Нет данных уравновешивания.</p>\n";
        return;
    }

    html << "<p class=\"meta\">Единицы: силы — Н, моменты — Н·м. Шаг таблиц по углу: " << stride << ".</p>\n";

    html << "<h3>Исходные величины (до противовесов)</h3>\n";
    AppendVec3SeriesHtml(html, c.alphaDeg, c.sourceInertiaForce, u8"Полная сила инерции F (исходная)", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.sourceInertiaForce1, u8"Сила инерции 1-го порядка F1 (исходная)", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.sourceInertiaForce2, u8"Сила инерции 2-го порядка F2 (исходная)", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.sourceInertiaMoment1, u8"Момент от сил 1-го порядка M1 (исходный)", true, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.sourceInertiaMoment2, u8"Момент от сил 2-го порядка M2 (исходный)", true, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.sourceCentrifugalForce, u8"Центробежная сила Fc (исходная)", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.sourceCentrifugalMoment, u8"Момент от центробежной силы Mc (исходный)", true, stride);

    html << "<h3>Остаточные величины после противовесов</h3>\n";
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualInertiaForce1, u8"Остаточная сила инерции 1-го порядка F1", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualInertiaForce2, u8"Остаточная сила инерции 2-го порядка F2", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualInertiaForce, u8"Остаточная полная сила инерции F", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualInertiaMoment1, u8"Остаточный момент M1", true, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualInertiaMoment2, u8"Остаточный момент M2", true, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualInertiaMoment, u8"Остаточный полный момент M", true, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualCentrifugalForce, u8"Остаточная центробежная сила Fc", false, stride);
    AppendVec3SeriesHtml(html, c.alphaDeg, c.residualCentrifugalMoment, u8"Остаточный момент от центробежной силы Mc", true, stride);

    if (!pipe.warnings.empty())
    {
        html << "<h3>Предупреждения расчета</h3>\n<ul>\n";
        for (const auto& w : pipe.warnings)
        {
            const wxString line = wxString::FromUTF8(w.message);
            html << "<li>" << HtmlEscape(line).ToUTF8() << "</li>\n";
        }
        html << "</ul>\n";
    }
}

void StreamVec3CellPlain(std::ostringstream& txt, double c)
{
    if (!std::isfinite(c))
        txt << "-";
    else
        txt << wxString::Format("%.5g", c).ToUTF8();
}

void AppendVec3SeriesPlain(
    std::ostringstream& txt,
    const std::vector<double>& alphaDeg,
    const std::vector<Vec3>& series,
    const char* title,
    bool isMoment,
    int stride)
{
    if (series.size() != alphaDeg.size() || series.empty())
        return;
    if (!Vec3SeriesHasData(series))
        return;

    const char* unit = isMoment ? "N*m" : "N";
    txt << "\n--- " << title << " (" << unit << ") ---\n";
    txt << "alpha_deg\tX\tY\tZ\n";

    const std::size_t step = static_cast<std::size_t>(std::max(1, stride));
    for (std::size_t i = 0; i < alphaDeg.size(); i += step)
    {
        const Vec3& v = series[i];
        txt << alphaDeg[i] << '\t';
        StreamVec3CellPlain(txt, v.x);
        txt << '\t';
        StreamVec3CellPlain(txt, v.y);
        txt << '\t';
        StreamVec3CellPlain(txt, v.z);
        txt << '\n';
    }
}

void AppendCylinderDynamicSectionPlain(
    std::ostringstream& txt,
    const engine::dynamic::CylinderDynamicSeries& c,
    const std::vector<double>& alphaDeg,
    int stride)
{
    txt << "\n"
        << "================================================================================\n";
    txt << "Цилиндр " << c.cylinderNumber << " (вал " << c.shaftNumber << ", кривошип " << c.crankNumber
        << ") — динамика\n";
    txt << "================================================================================\n";

    AppendVec3SeriesPlain(txt, alphaDeg, c.inertiaForce, u8"Сила инерции F", false, stride);
    AppendVec3SeriesPlain(txt, alphaDeg, c.inertiaForce1, u8"Сила инерции 1-го порядка F1", false, stride);
    AppendVec3SeriesPlain(txt, alphaDeg, c.inertiaForce2, u8"Сила инерции 2-го порядка F2", false, stride);
    AppendVec3SeriesPlain(txt, alphaDeg, c.inertiaMoment1, u8"Момент от сил 1-го порядка M1", true, stride);
    AppendVec3SeriesPlain(txt, alphaDeg, c.inertiaMoment2, u8"Момент от сил 2-го порядка M2", true, stride);
    AppendVec3SeriesPlain(txt, alphaDeg, c.centrifugalForce, u8"Центробежная сила Fc", false, stride);
    AppendVec3SeriesPlain(txt, alphaDeg, c.centrifugalMoment, u8"Момент от центробежной силы Mc", true, stride);
}

void AppendDynamicTotalsPlain(std::ostringstream& txt, const engine::dynamic::DynamicResult& dyn, int stride)
{
    txt << "\n"
        << "================================================================================\n";
    txt << "Сумма по всем цилиндрам (динамика)\n";
    txt << "================================================================================\n";
    const auto& a = dyn.alphaDeg;
    AppendVec3SeriesPlain(txt, a, dyn.totalInertiaForce, u8"Σ Сила инерции F", false, stride);
    AppendVec3SeriesPlain(txt, a, dyn.totalInertiaForce1, u8"Σ Сила инерции 1-го порядка F1", false, stride);
    AppendVec3SeriesPlain(txt, a, dyn.totalInertiaForce2, u8"Σ Сила инерции 2-го порядка F2", false, stride);
    AppendVec3SeriesPlain(txt, a, dyn.totalInertiaMoment1, u8"Σ Момент от сил 1-го порядка M1", true, stride);
    AppendVec3SeriesPlain(txt, a, dyn.totalInertiaMoment2, u8"Σ Момент от сил 2-го порядка M2", true, stride);
    AppendVec3SeriesPlain(txt, a, dyn.totalCentrifugalForce, u8"Σ Центробежная сила Fc", false, stride);
    AppendVec3SeriesPlain(txt, a, dyn.totalCentrifugalMoment, u8"Σ Момент от центробежной силы Mc", true, stride);
}

void AppendFullDynamicPlain(
    std::ostringstream& txt,
    const engine::dynamic::DynamicResult& dyn,
    const std::vector<int>& cylinderIndices,
    int stride)
{
    txt << "\n\n*** ДИНАМИКА: СИЛЫ И МОМЕНТЫ ***\n";
    txt << "Единицы: силы — Н, моменты — Н·м. Шаг: " << stride << "\n";

    for (int idx : cylinderIndices)
    {
        if (idx < 0 || static_cast<std::size_t>(idx) >= dyn.cylinders.size())
            continue;
        AppendCylinderDynamicSectionPlain(txt, dyn.cylinders[static_cast<std::size_t>(idx)], dyn.alphaDeg, stride);
    }
    AppendDynamicTotalsPlain(txt, dyn, stride);
}

void AppendFullBalancingPlain(std::ostringstream& txt, const engine::balancing::BalancingPipelineResult& pipe, int stride)
{
    txt << "\n\n*** УРАВНОВЕШИВАНИЕ ***\n";
    if (!pipe.ok)
    {
        txt << "Расчет завершился с ошибками:\n";
        for (const auto& e : pipe.errors)
            txt << "  - " << e.message << "\n";
        if (!pipe.warnings.empty())
        {
            txt << "Предупреждения:\n";
            for (const auto& w : pipe.warnings)
                txt << "  - " << w.message << "\n";
        }
        return;
    }

    const engine::balancing::BalancingComposedResult& c = pipe.composedResult;
    if (c.alphaDeg.empty())
    {
        txt << "Нет данных composedResult.\n";
        return;
    }

    txt << "Единицы: силы — Н, моменты — Н·м. Шаг: " << stride << "\n";

    txt << "\n--- Исходные величины (до противовесов) ---\n";
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.sourceInertiaForce, u8"Полная сила инерции F (исходная)", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.sourceInertiaForce1, u8"Сила инерции 1-го порядка F1 (исходная)", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.sourceInertiaForce2, u8"Сила инерции 2-го порядка F2 (исходная)", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.sourceInertiaMoment1, u8"Момент M1 (исходный)", true, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.sourceInertiaMoment2, u8"Момент M2 (исходный)", true, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.sourceCentrifugalForce, u8"Центробежная сила Fc (исходная)", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.sourceCentrifugalMoment, u8"Момент Mc (исходный)", true, stride);

    txt << "\n--- Остаточные величины после противовесов ---\n";
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualInertiaForce1, u8"Остаточная F1", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualInertiaForce2, u8"Остаточная F2", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualInertiaForce, u8"Остаточная полная F", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualInertiaMoment1, u8"Остаточный M1", true, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualInertiaMoment2, u8"Остаточный M2", true, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualInertiaMoment, u8"Остаточный полный M", true, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualCentrifugalForce, u8"Остаточная Fc", false, stride);
    AppendVec3SeriesPlain(txt, c.alphaDeg, c.residualCentrifugalMoment, u8"Остаточный Mc", true, stride);

    if (!pipe.warnings.empty())
    {
        txt << "\nПредупреждения:\n";
        for (const auto& w : pipe.warnings)
            txt << "  - " << w.message << "\n";
    }
}

} // namespace

ReportBuildResult ReportBuilder::BuildHtml(
    const EngineModel& model,
    const engine::kinematic::KinematicResult& result,
    const ReportOptions& options,
    ReportHtmlImageMode imageMode,
    const engine::dynamic::DynamicResult* dynamicResult,
    const engine::balancing::BalancingPipelineResult* balancingPipelineResult)
{
    ReportBuildResult out;
    if (result.cylinders.empty())
    {
        out.html = "<html><body><p>Нет данных для отчета.</p></body></html>";
        return out;
    }

    std::vector<int> cylinderIndices = options.selectedCylinderIndices;
    std::sort(cylinderIndices.begin(), cylinderIndices.end());
    cylinderIndices.erase(std::unique(cylinderIndices.begin(), cylinderIndices.end()), cylinderIndices.end());
    cylinderIndices.erase(
        std::remove_if(
            cylinderIndices.begin(),
            cylinderIndices.end(),
            [&](int idx)
            { return idx < 0 || static_cast<std::size_t>(idx) >= result.cylinders.size(); }),
        cylinderIndices.end());

    if (cylinderIndices.empty())
    {
        out.html = "<html><body><p>Не выбран ни один цилиндр для отчета.</p></body></html>";
        return out;
    }

    std::ostringstream html;
    html << R"(<html><head><meta charset="utf-8"><style>
body{font-family:Segoe UI,Arial,sans-serif;margin:10px 12px;color:#1a1a1a;font-size:13px;}
h1{font-size:20px;margin:0 0 6px 0;}
h2{font-size:16px;margin:14px 0 6px 0;page-break-after:avoid;}
h3{font-size:14px;margin:10px 0 4px 0;page-break-after:avoid;}
h4{font-size:13px;margin:8px 0 3px 0;page-break-after:avoid;}
table{border-collapse:collapse;width:100%;max-width:100%;margin:6px 0 10px 0;table-layout:fixed;}
th,td{border:1px solid #666;padding:3px 5px;font-size:12px;word-wrap:break-word;vertical-align:top;}
th{background:#e8e8e8;font-weight:bold;}
table.vec-table th,table.vec-table td{font-size:11px;padding:2px 4px;}
.chart-wrap{text-align:center;margin:4px auto 8px auto;max-width:100%;}
.chart-wrap img{max-width:100%;width:auto;height:auto;border:1px solid #bbb;display:block;margin:0 auto;}
.report-series{margin-bottom:10px;page-break-inside:avoid;}
.meta{color:#444;margin-bottom:6px;font-size:12px;}
table.combined th,table.combined td{font-size:11px;padding:2px 4px;}
</style></head><body>)";

    html << "<h1>Отчет по результатам расчета</h1>";
    html << "<div class='meta'>Дата формирования: " << wxDateTime::Now().FormatISOCombined(' ').ToUTF8() << "</div>";
    html << "<div class='meta'>Частота вращения: " << model.kinematic.rpm << " об/мин; "
         << "Число цилиндров: " << result.cylinders.size() << "; "
         << "Радиус кривошипа: " << model.kinematic.crankRadiusM << " м; "
         << "lambda: " << model.kinematic.lambda << "</div>";

    wxString memPrefix;
    int memChartSeq = 0;
    if (imageMode == ReportHtmlImageMode::WxMemoryFilesystemForHtmlWindow)
    {
        ReleaseReportMemoryImages();
        EnsureReportMemoryFilesystem();
        memPrefix = wxString::Format("edrep/r%lld/", static_cast<long long>(wxGetUTCTimeMillis().GetValue()));
    }

    for (int cylinderIndex : cylinderIndices)
        AppendCylinderSectionHtml(html, result, cylinderIndex, options, imageMode, memPrefix, memChartSeq);

    if (options.includeDynamicForces && dynamicResult != nullptr)
    {
        if (DynamicMatchesKinematic(*dynamicResult, result))
            AppendFullDynamicHtml(html, *dynamicResult, cylinderIndices, options.tableStride);
        else
            html << "<p class=\"meta\">Динамика: размеры данных не совпадают с кинематикой, блок пропущен.</p>";
    }

    if (options.includeBalancing && balancingPipelineResult != nullptr)
        AppendFullBalancingHtml(html, *balancingPipelineResult, options.tableStride);

    html << "</body></html>";
    out.html = wxString::FromUTF8(html.str().c_str());
    return out;
}

ReportBuildResult ReportBuilder::BuildPlainText(
    const EngineModel& model,
    const engine::kinematic::KinematicResult& result,
    const ReportOptions& options,
    const engine::dynamic::DynamicResult* dynamicResult,
    const engine::balancing::BalancingPipelineResult* balancingPipelineResult)
{
    ReportBuildResult out;
    if (result.cylinders.empty())
    {
        out.plainText = wxString::FromUTF8("Нет данных для отчета.\n");
        return out;
    }

    std::vector<int> cylinderIndices = options.selectedCylinderIndices;
    std::sort(cylinderIndices.begin(), cylinderIndices.end());
    cylinderIndices.erase(std::unique(cylinderIndices.begin(), cylinderIndices.end()), cylinderIndices.end());
    cylinderIndices.erase(
        std::remove_if(
            cylinderIndices.begin(),
            cylinderIndices.end(),
            [&](int idx)
            { return idx < 0 || static_cast<std::size_t>(idx) >= result.cylinders.size(); }),
        cylinderIndices.end());

    if (cylinderIndices.empty())
    {
        out.plainText = wxString::FromUTF8("Не выбран ни один цилиндр для отчета.\n");
        return out;
    }

    std::ostringstream txt;
    txt << "Отчет по результатам расчета\n";
    txt << "Дата формирования: " << wxDateTime::Now().FormatISOCombined(' ').ToUTF8() << "\n";
    txt << "Частота вращения: " << model.kinematic.rpm << " об/мин\n";
    txt << "Число цилиндров: " << result.cylinders.size() << "\n";
    txt << "Радиус кривошипа: " << model.kinematic.crankRadiusM << " м\n";
    txt << "lambda: " << model.kinematic.lambda << "\n";

    for (int cylinderIndex : cylinderIndices)
        AppendCylinderSectionPlain(txt, result, cylinderIndex, options);

    if (options.includeDynamicForces && dynamicResult != nullptr)
    {
        if (DynamicMatchesKinematic(*dynamicResult, result))
            AppendFullDynamicPlain(txt, *dynamicResult, cylinderIndices, options.tableStride);
        else
            txt << "\n(Динамика: несовпадение размеров с кинематикой, блок пропущен.)\n";
    }

    if (options.includeBalancing && balancingPipelineResult != nullptr)
        AppendFullBalancingPlain(txt, *balancingPipelineResult, options.tableStride);

    out.plainText = wxString::FromUTF8(txt.str().c_str());
    return out;
}

namespace
{
/// wxHtmlPrintout(wxString) sets the print job *title*, not the HTML body. Body must be set via SetHtmlText.
/// We also force CheckFit to true so wide tables still paginate (default dialog uses Cancel as default and
/// skipping pagination yields blank output).
class ReportPdfHtmlPrintout final : public wxHtmlPrintout
{
public:
    explicit ReportPdfHtmlPrintout(const wxString& html)
        : wxHtmlPrintout(wxString::FromUTF8(u8"Отчет"))
    {
        SetHtmlText(html, wxEmptyString, true);
    }

protected:
    bool CheckFit(const wxSize& pageArea, const wxSize& docArea) const override
    {
        (void)pageArea;
        (void)docArea;
        return true;
    }
};
} // namespace

bool ReportBuilder::ExportHtmlToPdf(
    wxWindow* parent,
    const wxString& html,
    const wxString& pdfPath,
    bool* savedToRequestedPathOut)
{
    if (savedToRequestedPathOut != nullptr)
        *savedToRequestedPathOut = false;

    ReportPdfHtmlPrintout printout(html);
    printout.SetFooter(wxS("@PAGENUM@/@PAGESCNT@"));

#ifdef __WXMSW__
    {
        wxPrintData printData;
        printData.SetPrinterName(wxS("Microsoft Print to PDF"));
        printData.SetFilename(pdfPath);
        wxPrintDialogData dialogData(printData);
        wxPrinter printer(&dialogData);
        if (printer.Print(parent, &printout, false))
        {
            if (savedToRequestedPathOut != nullptr)
                *savedToRequestedPathOut = true;
            return true;
        }
    }
#endif

    wxHtmlEasyPrinting easyPrinting;
    easyPrinting.SetParentWindow(parent);
    return easyPrinting.PrintText(html);
}

void ReportBuilder::ClearReportMemoryImages()
{
    ReleaseReportMemoryImages();
}
