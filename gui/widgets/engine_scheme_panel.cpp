#include "gui/widgets/engine_scheme_panel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/msgdlg.h>
#include <wx/pen.h>

#include "gui/common/text_utf8.h"
#include "core/model/engine_types.h"

namespace
{
    constexpr double kPi = 3.14159265358979323846;

    struct Vec3
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    struct CrankVisual
    {
        int shaftNumber = 0;
        int crankNumber = 0;

        Vec3 shaftAxisLeft;
        Vec3 shaftAxisRight;

        Vec3 mainBefore;
        Vec3 cheekUp;

        Vec3 pinStart;
        Vec3 pinEnd;

        Vec3 cheekRunEnd;
        Vec3 cheekDown;
        Vec3 mainAfter;

        Vec3 pinCenter;
        double pinCenterZ = 0.0;
    };

    double DegToRad(double deg)
    {
        return deg * kPi / 180.0;
    }

    Vec3 operator+(const Vec3& a, const Vec3& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    Vec3 operator-(const Vec3& a, const Vec3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    Vec3 operator*(const Vec3& v, double s)
    {
        return { v.x * s, v.y * s, v.z * s };
    }

    double Dot(const Vec3& a, const Vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    double Length(const Vec3& v)
    {
        return std::sqrt(Dot(v, v));
    }

    Vec3 Normalize(const Vec3& v)
    {
        const double len = Length(v);
        if (len <= std::numeric_limits<double>::epsilon())
            return { 0.0, 1.0, 0.0 };

        return { v.x / len, v.y / len, v.z / len };
    }

    bool HasIntermediateMainJournal(SupportType supportType, int leftCrankIndexZeroBased)
    {
        if (supportType == SupportType::FullySupported)
            return true;

        return (leftCrankIndexZeroBased % 2) == 1;
    }

    std::vector<int> BuildCylindersForCrank(const CrankshaftSpec& shaft, int crankNumber)
    {
        std::vector<int> result;
        for (size_t i = 0; i < shaft.cylinders.size(); ++i)
        {
            if (shaft.cylinders[i].crankNumber == crankNumber)
                result.push_back(static_cast<int>(i));
        }
        return result;
    }

    double SpeedRatioValue(BalancerSpeedRatio ratio)
    {
        switch (ratio)
        {
        case BalancerSpeedRatio::Plus1W:  return 1.0;
        case BalancerSpeedRatio::Plus2W:  return 2.0;
        case BalancerSpeedRatio::Minus1W: return -1.0;
        case BalancerSpeedRatio::Minus2W: return -2.0;
        default:                         return 1.0;
        }
    }

    Vec3 MakeBalancerRadiusVectorPreview(
        BalancerAxis axis,
        double radiusPreview,
        double alphaDeg,
        double shaftPhaseDeg,
        double counterweightPhaseDeg,
        BalancerSpeedRatio speedRatio)
    {
        const double theta =
            DegToRad(SpeedRatioValue(speedRatio) * alphaDeg + shaftPhaseDeg + counterweightPhaseDeg);

        switch (axis)
        {
        case BalancerAxis::X:
            return { 0.0, radiusPreview * std::cos(theta), radiusPreview * std::sin(theta) };

        case BalancerAxis::Y:
            return { radiusPreview * std::sin(theta), 0.0, radiusPreview * std::cos(theta) };

        case BalancerAxis::Z:
        default:
            return { radiusPreview * std::sin(theta), radiusPreview * std::cos(theta), 0.0 };
        }
    }

    Vec3 BalancerAxisDirection(BalancerAxis axis)
    {
        switch (axis)
        {
        case BalancerAxis::X: return { 1.0, 0.0, 0.0 };
        case BalancerAxis::Y: return { 0.0, 1.0, 0.0 };
        case BalancerAxis::Z:
        default:             return { 0.0, 0.0, 1.0 };
        }
    }

    int ResolveActualCounterweightCount(const EngineModel& model)
    {
        int totalCylinderCount = 0;
        for (const auto& shaft : model.shafts)
            totalCylinderCount += static_cast<int>(shaft.cylinders.size());

        const bool isSemiSupported =
            (model.kinematic.supportType == SupportType::SemiSupported);

        if (isSemiSupported && totalCylinderCount > 1)
            return 1;

        if ((totalCylinderCount % 2) != 0)
            return 2;

        switch (model.balancing.crankCounterweights.countMode)
        {
        case CounterweightCountMode::OnePerCrank:
            return 1;
        case CounterweightCountMode::TwoPerCrank:
            return 2;
        case CounterweightCountMode::Auto:
        default:
            return 2;
        }
    }

    bool IsLeftSingleCounterweight(size_t crankIndexWithinShaft)
    {
        return (crankIndexWithinShaft % 2) == 0;
    }

    double ResolveCounterweightPhaseDeg(
        const EngineModel& model,
        const CrankshaftSpec& shaft,
        size_t crankIndexWithinShaft)
    {
        const auto& crank = shaft.cranks[crankIndexWithinShaft];

        double phaseDeg = crank.phaseDeg;

        for (const auto& entry : model.balancing.crankCounterweights.entries)
        {
            if (entry.shaftNumber == shaft.shaftNumber &&
                entry.crankNumber == crank.crankNumber)
            {
                phaseDeg = entry.phaseDeg;
                break;
            }
        }

        const bool isSemiSupported =
            (model.kinematic.supportType == SupportType::SemiSupported);

        if (isSemiSupported && (crankIndexWithinShaft % 2) == 1)
        {
            const size_t firstIndex = crankIndexWithinShaft - 1;
            const auto& firstCrank = shaft.cranks[firstIndex];

            double firstPhaseDeg = firstCrank.phaseDeg;

            for (const auto& entry : model.balancing.crankCounterweights.entries)
            {
                if (entry.shaftNumber == shaft.shaftNumber &&
                    entry.crankNumber == firstCrank.crankNumber)
                {
                    firstPhaseDeg = entry.phaseDeg;
                    break;
                }
            }

            phaseDeg = firstPhaseDeg + 180.0;
        }

        return phaseDeg;
    }

    Vec3 MakeCrankCounterweightDirection(double phaseDeg, double alphaDeg)
    {
        const double theta = DegToRad(phaseDeg + alphaDeg + 180.0);
        return Normalize({ std::sin(theta), std::cos(theta), 0.0 });
    }
}

wxBEGIN_EVENT_TABLE(EngineSchemePanel, wxPanel)
    EVT_PAINT(EngineSchemePanel::OnPaint)
    EVT_SIZE(EngineSchemePanel::OnSize)
    EVT_LEFT_DOWN(EngineSchemePanel::OnLeftDown)
    EVT_LEFT_UP(EngineSchemePanel::OnLeftUp)
    EVT_RIGHT_DOWN(EngineSchemePanel::OnRightDown)
    EVT_RIGHT_UP(EngineSchemePanel::OnRightUp)
    EVT_MIDDLE_DOWN(EngineSchemePanel::OnMiddleDown)
    EVT_MOTION(EngineSchemePanel::OnMotion)
    EVT_MOUSEWHEEL(EngineSchemePanel::OnMouseWheel)
    EVT_LEAVE_WINDOW(EngineSchemePanel::OnLeaveWindow)
wxEND_EVENT_TABLE()

EngineSchemePanel::EngineSchemePanel(wxWindow* parent)
    : wxPanel(parent)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(wxSize(280, 280));
    ResetView();

    m_helpButton = new wxButton(this, wxID_ANY, "?", wxPoint(0, 0), wxSize(28, 28));
    m_helpButton->SetToolTip(WXU8("Подсказка по управлению схемой"));
    m_helpButton->Bind(wxEVT_BUTTON, &EngineSchemePanel::OnHelpClicked, this);

    UpdateHelpButtonPosition();
}

void EngineSchemePanel::ResetView()
{
    m_zoom = 1.0;
    m_panX = 0.0;
    m_panY = 0.0;
    m_yawDeg = 0.0;
    m_pitchDeg = 0.0;

    m_isPanning = false;
    m_isRotating = false;
}

void EngineSchemePanel::UpdateHelpButtonPosition()
{
    if (!m_helpButton)
        return;

    const wxSize client = GetClientSize();
    const wxSize btn = m_helpButton->GetSize();

    const int margin = 10;
    const int x = std::max(margin, client.x - btn.x - margin);
    const int y = margin;

    m_helpButton->Move(x, y);
}

void EngineSchemePanel::OnHelpClicked(wxCommandEvent& event)
{
    wxUnusedVar(event);

    wxMessageBox(
        WXU8("Управление схемой:\n\n"
             "Колесо мыши — приблизить / отдалить\n"
             "Левая кнопка мыши + перетаскивание — перемещение схемы\n"
             "Правая кнопка мыши + перетаскивание — вращение схемы\n"
             "ЛКМ + ПКМ одновременно — вращение схемы\n"
             "Средняя кнопка мыши — сбросить вид"),
        WXU8("Управление схемой"),
        wxOK | wxICON_INFORMATION,
        this);
}

void EngineSchemePanel::SetModel(const EngineModel& model)
{
    m_model = model;
    Refresh();
}

void EngineSchemePanel::ClearModel()
{
    m_model.reset();
    m_animationAlphaDeg = 0.0;
    Refresh();
}

void EngineSchemePanel::SetAnimationAlphaDeg(double alphaDeg)
{
    if (m_animationAlphaDeg == alphaDeg)
        return;

    m_animationAlphaDeg = alphaDeg;
    Refresh();
}

void EngineSchemePanel::SetShowReferencePoint(bool show)
{
    m_showReferencePoint = show;
    Refresh();
}

void EngineSchemePanel::SetReferencePointMm(double xMm, double yMm, double zMm)
{
    m_referenceXmm = xMm;
    m_referenceYmm = yMm;
    m_referenceZmm = zMm;
    Refresh();
}

void EngineSchemePanel::OnSize(wxSizeEvent& event)
{
    UpdateHelpButtonPosition();
    Refresh();
    event.Skip();
}

void EngineSchemePanel::OnLeftDown(wxMouseEvent& event)
{
    m_isPanning = true;
    m_lastMousePos = event.GetPosition();

    if (!HasCapture())
        CaptureMouse();
}

void EngineSchemePanel::OnLeftUp(wxMouseEvent& event)
{
    wxUnusedVar(event);
    m_isPanning = false;

    if (!m_isRotating && HasCapture())
        ReleaseMouse();
}

void EngineSchemePanel::OnRightDown(wxMouseEvent& event)
{
    m_isRotating = true;
    m_lastMousePos = event.GetPosition();

    if (!HasCapture())
        CaptureMouse();
}

void EngineSchemePanel::OnRightUp(wxMouseEvent& event)
{
    wxUnusedVar(event);
    m_isRotating = false;

    if (!m_isPanning && HasCapture())
        ReleaseMouse();
}

void EngineSchemePanel::OnMiddleDown(wxMouseEvent& event)
{
    wxUnusedVar(event);

    if (HasCapture())
        ReleaseMouse();

    ResetView();
    Refresh();
}

void EngineSchemePanel::OnMotion(wxMouseEvent& event)
{
    if (!event.Dragging())
        return;

    const wxPoint pos = event.GetPosition();
    const wxPoint delta = pos - m_lastMousePos;
    m_lastMousePos = pos;

    if (m_isPanning && m_isRotating)
    {
        m_yawDeg += delta.x * 0.4;
        m_pitchDeg += delta.y * 0.3;
        m_pitchDeg = std::clamp(m_pitchDeg, -80.0, 80.0);
        Refresh();
        return;
    }

    if (m_isPanning)
    {
        m_panX += delta.x;
        m_panY += delta.y;
        Refresh();
    }
    else if (m_isRotating)
    {
        m_yawDeg += delta.x * 0.4;
        m_pitchDeg += delta.y * 0.3;
        m_pitchDeg = std::clamp(m_pitchDeg, -80.0, 80.0);
        Refresh();
    }
}

void EngineSchemePanel::OnMouseWheel(wxMouseEvent& event)
{
    const int rotation = event.GetWheelRotation();
    if (rotation == 0)
        return;

    const double factor = (rotation > 0) ? 1.1 : (1.0 / 1.1);
    m_zoom *= factor;
    m_zoom = std::clamp(m_zoom, 0.2, 8.0);
    Refresh();
}

void EngineSchemePanel::OnLeaveWindow(wxMouseEvent& event)
{
    wxUnusedVar(event);

    const wxMouseState mouseState = wxGetMouseState();

    if (!mouseState.LeftIsDown())
        m_isPanning = false;
    if (!mouseState.RightIsDown())
        m_isRotating = false;

    if (!m_isPanning && !m_isRotating && HasCapture())
        ReleaseMouse();
}

wxPoint EngineSchemePanel::Project(double x, double y, double z, const wxRect& rect) const
{
    const double yaw = DegToRad(m_yawDeg);
    const double pitch = DegToRad(m_pitchDeg);

    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);

    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);

    const double x1 = x * cy + z * sy;
    const double z1 = -x * sy + z * cy;

    const double y2 = y * cp - z1 * sp;
    const double z2 = y * sp + z1 * cp;

    const double baseScale = std::min(rect.GetWidth(), rect.GetHeight()) * 0.18 * m_zoom;

    const double originX = rect.GetLeft() + rect.GetWidth() * 0.28 + m_panX;
    const double originY = rect.GetTop() + rect.GetHeight() * 0.78 + m_panY;

    const double screenX = originX + baseScale * (0.92 * z2 + 0.68 * x1);
    const double screenY = originY - baseScale * (y2 - 0.30 * x1);

    return wxPoint(
        static_cast<int>(std::lround(screenX)),
        static_cast<int>(std::lround(screenY)));
}

void EngineSchemePanel::DrawAxes(wxDC& dc, const wxRect& rect) const
{
    const wxPoint o = Project(0.0, 0.0, 0.0, rect);
    const wxPoint x = Project(0.90, 0.0, 0.0, rect);
    const wxPoint y = Project(0.0, 1.30, 0.0, rect);
    const wxPoint z = Project(0.0, 0.0, 1.20, rect);

    dc.SetPen(wxPen(wxColour(0, 190, 70), 2));
    dc.DrawLine(o, x);
    dc.DrawText("x", x + wxPoint(4, -1));

    dc.SetPen(wxPen(wxColour(0, 170, 255), 2));
    dc.DrawLine(o, y);
    dc.DrawText("y", y + wxPoint(-2, -16));

    dc.SetPen(wxPen(wxColour(210, 20, 20), 2));
    dc.DrawLine(o, z);
    dc.DrawText("z", z + wxPoint(-8, 0));
}

void EngineSchemePanel::DrawReferenceAxes(wxDC& dc, const wxRect& rect, double x, double y, double z) const
{
    const wxPoint o = Project(x, y, z, rect);
    const wxPoint x1 = Project(x + 0.90, y, z, rect);
    const wxPoint y1 = Project(x, y + 1.00, z, rect);
    const wxPoint z1 = Project(x, y, z + 0.95, rect);

    dc.SetPen(wxPen(wxColour(170, 255, 0), 2));
    dc.DrawLine(o, x1);
    dc.DrawText("x1", x1 + wxPoint(4, -1));

    dc.SetPen(wxPen(wxColour(50, 90, 255), 2));
    dc.DrawLine(o, y1);
    dc.DrawText("y1", y1 + wxPoint(-2, -16));

    dc.SetPen(wxPen(wxColour(255, 220, 0), 2));
    dc.DrawLine(o, z1);
    dc.DrawText("z1", z1 + wxPoint(-8, 0));
}

void EngineSchemePanel::DrawEmptyState(wxDC& dc, const wxRect& rect) const
{
    dc.SetTextForeground(wxColour(190, 190, 190));
    dc.DrawText(WXU8("Нет данных для отображения."), rect.GetTopLeft() + wxPoint(12, 40));
}

void EngineSchemePanel::DrawScheme(wxDC& dc, const wxRect& rect) const
{
    if (!m_model.has_value())
        return;

    const auto& model = *m_model;

    dc.SetTextForeground(wxColour(235, 235, 235));
    dc.DrawText(wxString::Format(WXU8("Текущий α = %.1f°"), m_animationAlphaDeg),
                rect.GetTopLeft() + wxPoint(12, 12));

    DrawAxes(dc, rect);

    if (m_showReferencePoint)
    {
        const double refX = (m_referenceXmm / 1000.0) * 18.0;
        const double refY = (m_referenceYmm / 1000.0) * 18.0;
        const double refZ = (m_referenceZmm / 1000.0) * 18.0;
        DrawReferenceAxes(dc, rect, refX, refY, refZ);
    }

    if (model.shafts.empty())
        return;

    const double mainL = std::max(1e-6, model.kinematic.mainJournalLengthM);
    const double rodL = std::max(1e-6, model.kinematic.rodJournalLengthM);
    const double webT = std::max(1e-6, model.kinematic.webThicknessM);
    const double crankR = std::max(1e-6, model.kinematic.crankRadiusM);

    const double zScale = 18.0;
    const double rScale = 18.0;

    const double mainLz = mainL * zScale;
    const double rodLz = rodL * zScale;
    const double webTz = webT * zScale;
    const double crankRy = crankR * rScale;

    const bool hasValidLambda = model.kinematic.lambda > 0.0;

    double rodPreviewLength = 0.0;
    if (hasValidLambda)
    {
        const double rodLengthM = crankR / model.kinematic.lambda;
        rodPreviewLength = rodLengthM * rScale;
    }

    const bool articulatedMode = (model.kinematic.rodJointType == RodJointType::Articulated);
    const double articulatedRadiusPreview = model.kinematic.articulatedRodRadiusM * rScale;
    const double articulatedRodLengthPreview = model.kinematic.articulatedRodLengthM * rScale;

    const double cylinderBottomT = -0.10;
    const double cylinderTopT = hasValidLambda
        ? (rodPreviewLength + crankRy + 0.55)
        : (crankRy + 1.20);

    const double pistonW = 0.34;
    const double pistonH = 0.22;

    std::vector<CrankVisual> allCranks;

    for (const auto& shaft : model.shafts)
    {
        if (shaft.cranks.empty())
            continue;

        const int crankCount = static_cast<int>(shaft.cranks.size());

        const double shaftOriginX = (shaft.originXMm / 1000.0) * rScale;
        const double shaftOriginY = (shaft.originYMm / 1000.0) * rScale;
        const double shaftOriginZ = (shaft.originZMm / 1000.0) * zScale;

        std::vector<double> crankPinCenterZs;
        crankPinCenterZs.reserve(crankCount);

        double cursorZ = 0.5 * mainLz;

        for (int crankIndex = 0; crankIndex < crankCount; ++crankIndex)
        {
            cursorZ += webTz;
            const double pinStartZ = cursorZ;
            const double pinEndZ = pinStartZ + rodLz;
            const double pinCenterZ = 0.5 * (pinStartZ + pinEndZ);
            crankPinCenterZs.push_back(pinCenterZ);

            cursorZ = pinEndZ;
            cursorZ += webTz;

            if (crankIndex < crankCount - 1 && HasIntermediateMainJournal(model.kinematic.supportType, crankIndex))
                cursorZ += mainLz;
        }

        std::vector<CrankVisual> cranks;
        cranks.reserve(crankCount);

        for (int i = 0; i < crankCount; ++i)
        {
            const auto& crank = shaft.cranks[static_cast<size_t>(i)];
            const double phase = DegToRad(crank.phaseDeg + m_animationAlphaDeg);

            const double ex = crankRy * std::sin(phase);
            const double ey = crankRy * std::cos(phase);

            const double pinCenterZ = crankPinCenterZs[static_cast<size_t>(i)];

            CrankVisual cv;
            cv.shaftNumber = shaft.shaftNumber;
            cv.crankNumber = crank.crankNumber;
            cv.pinCenterZ = pinCenterZ;

            cv.shaftAxisLeft  = { shaftOriginX + 0.0, shaftOriginY + 0.0, shaftOriginZ + pinCenterZ - rodLz * 0.5 - webTz };
            cv.shaftAxisRight = { shaftOriginX + 0.0, shaftOriginY + 0.0, shaftOriginZ + pinCenterZ + rodLz * 0.5 + webTz };

            cv.mainBefore  = cv.shaftAxisLeft;
            cv.cheekUp     = { shaftOriginX + ex,  shaftOriginY + ey,  shaftOriginZ + pinCenterZ - rodLz * 0.5 - webTz };

            cv.pinStart    = { shaftOriginX + ex,  shaftOriginY + ey,  shaftOriginZ + pinCenterZ - rodLz * 0.5 };
            cv.pinEnd      = { shaftOriginX + ex,  shaftOriginY + ey,  shaftOriginZ + pinCenterZ + rodLz * 0.5 };

            cv.cheekRunEnd = { shaftOriginX + ex,  shaftOriginY + ey,  shaftOriginZ + pinCenterZ + rodLz * 0.5 + webTz };
            cv.cheekDown   = cv.shaftAxisRight;
            cv.mainAfter   = cv.shaftAxisRight;

            cv.pinCenter   = { shaftOriginX + ex,  shaftOriginY + ey,  shaftOriginZ + pinCenterZ };

            cranks.push_back(cv);
            allCranks.push_back(cv);
        }

        dc.SetPen(wxPen(wxColour(235, 235, 235), 2));

        if (!cranks.empty())
        {
            dc.DrawLine(
                Project(shaftOriginX, shaftOriginY, shaftOriginZ + 0.0, rect),
                Project(shaftOriginX, shaftOriginY, cranks.front().mainBefore.z, rect));
        }

        for (size_t i = 0; i < cranks.size(); ++i)
        {
            const auto& cv = cranks[i];

            dc.DrawLine(Project(cv.mainBefore.x, cv.mainBefore.y, cv.mainBefore.z, rect),
                        Project(cv.cheekUp.x, cv.cheekUp.y, cv.cheekUp.z, rect));

            dc.DrawLine(Project(cv.cheekUp.x, cv.cheekUp.y, cv.cheekUp.z, rect),
                        Project(cv.pinStart.x, cv.pinStart.y, cv.pinStart.z, rect));

            dc.DrawLine(Project(cv.pinStart.x, cv.pinStart.y, cv.pinStart.z, rect),
                        Project(cv.pinEnd.x, cv.pinEnd.y, cv.pinEnd.z, rect));

            dc.DrawLine(Project(cv.pinEnd.x, cv.pinEnd.y, cv.pinEnd.z, rect),
                        Project(cv.cheekRunEnd.x, cv.cheekRunEnd.y, cv.cheekRunEnd.z, rect));

            dc.DrawLine(Project(cv.cheekRunEnd.x, cv.cheekRunEnd.y, cv.cheekRunEnd.z, rect),
                        Project(cv.cheekDown.x, cv.cheekDown.y, cv.cheekDown.z, rect));

            if (i + 1 < cranks.size())
            {
                dc.DrawLine(Project(cv.cheekDown.x, cv.cheekDown.y, cv.cheekDown.z, rect),
                            Project(shaftOriginX, shaftOriginY, cranks[i + 1].mainBefore.z, rect));
            }
            else
            {
                const double endZ = shaftOriginZ + cursorZ + 0.5 * mainLz;
                dc.DrawLine(Project(cv.cheekDown.x, cv.cheekDown.y, cv.cheekDown.z, rect),
                            Project(shaftOriginX, shaftOriginY, endZ, rect));
            }
        }

        dc.SetTextForeground(wxColour(235, 235, 235));
        {
            const wxPoint shaftLabel = Project(shaftOriginX, shaftOriginY + 0.20, shaftOriginZ, rect);
            dc.DrawText(wxString::Format(WXU8("кв %d"), shaft.shaftNumber), shaftLabel + wxPoint(4, -16));
        }

        for (const auto& cv : cranks)
        {
            const wxPoint p = Project(cv.pinCenter.x, cv.pinCenter.y, cv.pinCenter.z, rect);
            dc.DrawText(wxString::Format(WXU8("%d кр."), cv.crankNumber), p + wxPoint(6, 10));
        }

        for (int crankIndex = 0; crankIndex < crankCount; ++crankIndex)
        {
            const int crankNumber = crankIndex + 1;
            const auto cylinderIndices = BuildCylindersForCrank(shaft, crankNumber);
            if (cylinderIndices.empty())
                continue;

            const Vec3 pinCenter = cranks[static_cast<size_t>(crankIndex)].pinCenter;

            const int mainCylIndex = cylinderIndices.front();
            const auto& mainCylinder = shaft.cylinders[static_cast<size_t>(mainCylIndex)];

            const double mainAxisAngle = DegToRad(mainCylinder.axisTiltDeg);
            const Vec3 mainAxisDir = Normalize({ std::sin(mainAxisAngle), std::cos(mainAxisAngle), 0.0 });
            const Vec3 mainAxisBase{ shaftOriginX, shaftOriginY, pinCenter.z };
            const Vec3 mainAxisBottom = mainAxisBase + mainAxisDir * cylinderBottomT;
            const Vec3 mainAxisTop = mainAxisBase + mainAxisDir * cylinderTopT;

            Vec3 mainPistonPin = mainAxisBase + mainAxisDir * 0.4;

            if (!hasValidLambda)
            {
                dc.SetPen(wxPen(wxColour(255, 255, 0), 2, wxPENSTYLE_SHORT_DASH));
                dc.DrawLine(Project(mainAxisBottom.x, mainAxisBottom.y, mainAxisBottom.z, rect),
                            Project(mainAxisTop.x, mainAxisTop.y, mainAxisTop.z, rect));

                dc.SetTextForeground(wxColour(235, 235, 235));
                const Vec3 labelPos = mainAxisBase + mainAxisDir * (cylinderTopT * 0.9);
                const wxPoint labelPoint = Project(labelPos.x, labelPos.y, labelPos.z, rect);
                dc.DrawText(wxString::Format(WXU8("%d цил."), mainCylinder.cylinderNumber), labelPoint + wxPoint(-12, -14));
            }
            else
            {
                const Vec3 rel = pinCenter - mainAxisBase;
                const double proj = Dot(rel, mainAxisDir);

                const Vec3 perp = rel - mainAxisDir * proj;
                const double perpLenSq = Dot(perp, perp);

                double pistonT = proj;
                if (perpLenSq < rodPreviewLength * rodPreviewLength)
                {
                    const double along = std::sqrt(rodPreviewLength * rodPreviewLength - perpLenSq);
                    pistonT = proj + along;
                }
                else
                {
                    pistonT = std::max(proj, 0.15);
                }

                if (pistonT > cylinderTopT - 0.10)
                    pistonT = cylinderTopT - 0.10;
                if (pistonT < cylinderBottomT + 0.10)
                    pistonT = cylinderBottomT + 0.10;

                mainPistonPin = mainAxisBase + mainAxisDir * pistonT;

                dc.SetPen(wxPen(wxColour(255, 255, 0), 2, wxPENSTYLE_SHORT_DASH));
                dc.DrawLine(Project(mainAxisBottom.x, mainAxisBottom.y, mainAxisBottom.z, rect),
                            Project(mainAxisTop.x, mainAxisTop.y, mainAxisTop.z, rect));

                const Vec3 pistonCenter = mainPistonPin + mainAxisDir * (pistonH * 0.55);

                Vec3 sideDir{ mainAxisDir.y, -mainAxisDir.x, 0.0 };
                sideDir = Normalize(sideDir);

                const Vec3 pLT = pistonCenter + sideDir * (-pistonW * 0.5) + mainAxisDir * (pistonH * 0.5);
                const Vec3 pRT = pistonCenter + sideDir * ( pistonW * 0.5) + mainAxisDir * (pistonH * 0.5);
                const Vec3 pRB = pistonCenter + sideDir * ( pistonW * 0.5) - mainAxisDir * (pistonH * 0.5);
                const Vec3 pLB = pistonCenter + sideDir * (-pistonW * 0.5) - mainAxisDir * (pistonH * 0.5);

                wxPoint poly[4] =
                {
                    Project(pLT.x, pLT.y, pLT.z, rect),
                    Project(pRT.x, pRT.y, pRT.z, rect),
                    Project(pRB.x, pRB.y, pRB.z, rect),
                    Project(pLB.x, pLB.y, pLB.z, rect)
                };

                dc.SetPen(wxPen(wxColour(70, 130, 255), 1));
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawPolygon(4, poly);

                dc.SetPen(wxPen(wxColour(255, 190, 210), 1));
                dc.DrawLine(Project(pinCenter.x, pinCenter.y, pinCenter.z, rect),
                            Project(mainPistonPin.x, mainPistonPin.y, mainPistonPin.z, rect));

                dc.SetTextForeground(wxColour(235, 235, 235));
                const Vec3 labelPos = pistonCenter + mainAxisDir * (pistonH * 0.9);
                const wxPoint labelPoint = Project(labelPos.x, labelPos.y, labelPos.z, rect);
                dc.DrawText(wxString::Format(WXU8("%d цил."), mainCylinder.cylinderNumber), labelPoint + wxPoint(-12, -14));
            }

            for (size_t k = 1; k < cylinderIndices.size(); ++k)
            {
                const int cylIndex = cylinderIndices[k];
                const auto& cylinder = shaft.cylinders[static_cast<size_t>(cylIndex)];

                const double axisAngle = DegToRad(cylinder.axisTiltDeg);
                const Vec3 axisDir = Normalize({ std::sin(axisAngle), std::cos(axisAngle), 0.0 });
                const Vec3 axisBase{ shaftOriginX, shaftOriginY, pinCenter.z };
                const Vec3 axisBottom = axisBase + axisDir * cylinderBottomT;
                const Vec3 axisTop = axisBase + axisDir * cylinderTopT;

                if (!hasValidLambda)
                {
                    dc.SetPen(wxPen(wxColour(255, 255, 0), 2, wxPENSTYLE_SHORT_DASH));
                    dc.DrawLine(Project(axisBottom.x, axisBottom.y, axisBottom.z, rect),
                                Project(axisTop.x, axisTop.y, axisTop.z, rect));

                    dc.SetTextForeground(wxColour(235, 235, 235));
                    const Vec3 labelPos = axisBase + axisDir * (cylinderTopT * 0.9);
                    const wxPoint labelPoint = Project(labelPos.x, labelPos.y, labelPos.z, rect);
                    dc.DrawText(wxString::Format(WXU8("%d цил."), cylinder.cylinderNumber), labelPoint + wxPoint(-12, -14));
                    continue;
                }

                Vec3 attachPoint = pinCenter;
                double effectiveRodLength = rodPreviewLength;

                if (articulatedMode)
                {
                    const Vec3 mainRodDir = Normalize(mainPistonPin - pinCenter);
                    const Vec3 sideDir = Normalize({ mainRodDir.y, -mainRodDir.x, 0.0 });

                    const double gamma = DegToRad(cylinder.axisTiltDeg - mainCylinder.axisTiltDeg);
                    const Vec3 articulatedRadiusDir = Normalize(mainRodDir * std::cos(gamma) + sideDir * std::sin(gamma));

                    attachPoint = pinCenter + articulatedRadiusDir * articulatedRadiusPreview;
                    effectiveRodLength = articulatedRodLengthPreview;

                    dc.SetPen(wxPen(wxColour(255, 160, 120), 1));
                    dc.DrawLine(Project(pinCenter.x, pinCenter.y, pinCenter.z, rect),
                                Project(attachPoint.x, attachPoint.y, attachPoint.z, rect));
                }

                const Vec3 rel = attachPoint - axisBase;
                const double proj = Dot(rel, axisDir);

                const Vec3 perp = rel - axisDir * proj;
                const double perpLenSq = Dot(perp, perp);

                double pistonT = proj;
                if (perpLenSq < effectiveRodLength * effectiveRodLength)
                {
                    const double along = std::sqrt(effectiveRodLength * effectiveRodLength - perpLenSq);
                    pistonT = proj + along;
                }
                else
                {
                    pistonT = std::max(proj, 0.15);
                }

                if (pistonT > cylinderTopT - 0.10)
                    pistonT = cylinderTopT - 0.10;
                if (pistonT < cylinderBottomT + 0.10)
                    pistonT = cylinderBottomT + 0.10;

                const Vec3 pistonPin = axisBase + axisDir * pistonT;

                dc.SetPen(wxPen(wxColour(255, 255, 0), 2, wxPENSTYLE_SHORT_DASH));
                dc.DrawLine(Project(axisBottom.x, axisBottom.y, axisBottom.z, rect),
                            Project(axisTop.x, axisTop.y, axisTop.z, rect));

                const Vec3 pistonCenter = pistonPin + axisDir * (pistonH * 0.55);

                Vec3 sideDir{ axisDir.y, -axisDir.x, 0.0 };
                sideDir = Normalize(sideDir);

                const Vec3 pLT = pistonCenter + sideDir * (-pistonW * 0.5) + axisDir * (pistonH * 0.5);
                const Vec3 pRT = pistonCenter + sideDir * ( pistonW * 0.5) + axisDir * (pistonH * 0.5);
                const Vec3 pRB = pistonCenter + sideDir * ( pistonW * 0.5) - axisDir * (pistonH * 0.5);
                const Vec3 pLB = pistonCenter + sideDir * (-pistonW * 0.5) - axisDir * (pistonH * 0.5);

                wxPoint poly[4] =
                {
                    Project(pLT.x, pLT.y, pLT.z, rect),
                    Project(pRT.x, pRT.y, pRT.z, rect),
                    Project(pRB.x, pRB.y, pRB.z, rect),
                    Project(pLB.x, pLB.y, pLB.z, rect)
                };

                dc.SetPen(wxPen(wxColour(70, 130, 255), 1));
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                dc.DrawPolygon(4, poly);

                dc.SetPen(wxPen(wxColour(255, 190, 210), 1));
                dc.DrawLine(Project(attachPoint.x, attachPoint.y, attachPoint.z, rect),
                            Project(pistonPin.x, pistonPin.y, pistonPin.z, rect));

                dc.SetTextForeground(wxColour(235, 235, 235));
                const Vec3 labelPos = pistonCenter + axisDir * (pistonH * 0.9);
                const wxPoint labelPoint = Project(labelPos.x, labelPos.y, labelPos.z, rect);
                dc.DrawText(wxString::Format(WXU8("%d цил."), cylinder.cylinderNumber), labelPoint + wxPoint(-12, -14));
            }
        }
    }

    const auto& crankCw = model.balancing.crankCounterweights;
    if (crankCw.massKg > 0.0 && crankCw.radiusMm > 0.0)
    {
        const int actualCount = ResolveActualCounterweightCount(model);
        const double radiusPreview = (crankCw.radiusMm / 1000.0) * rScale;

        dc.SetPen(wxPen(wxColour(250, 180, 40), 2));
        dc.SetBrush(wxBrush(wxColour(250, 180, 40)));

        for (size_t shaftIndex = 0; shaftIndex < model.shafts.size(); ++shaftIndex)
        {
            const auto& shaft = model.shafts[shaftIndex];

            for (size_t crankIndex = 0; crankIndex < shaft.cranks.size(); ++crankIndex)
            {
                const auto it = std::find_if(
                    allCranks.begin(),
                    allCranks.end(),
                    [&](const CrankVisual& visual)
                    {
                        return visual.shaftNumber == shaft.shaftNumber &&
                               visual.crankNumber == shaft.cranks[crankIndex].crankNumber;
                    });

                if (it == allCranks.end())
                    continue;

                const double phaseDeg =
                    ResolveCounterweightPhaseDeg(model, shaft, crankIndex);

                const Vec3 dir =
                    MakeCrankCounterweightDirection(phaseDeg, m_animationAlphaDeg);

                if (actualCount == 1)
                {
                    const Vec3 base =
                        IsLeftSingleCounterweight(crankIndex)
                            ? it->shaftAxisLeft
                            : it->shaftAxisRight;

                    const Vec3 massPoint = base + dir * radiusPreview;

                    dc.DrawLine(Project(base.x, base.y, base.z, rect),
                                Project(massPoint.x, massPoint.y, massPoint.z, rect));
                    dc.DrawCircle(Project(massPoint.x, massPoint.y, massPoint.z, rect), 4);
                }
                else
                {
                    const Vec3 leftMassPoint = it->shaftAxisLeft + dir * radiusPreview;
                    const Vec3 rightMassPoint = it->shaftAxisRight + dir * radiusPreview;

                    dc.DrawLine(Project(it->shaftAxisLeft.x, it->shaftAxisLeft.y, it->shaftAxisLeft.z, rect),
                                Project(leftMassPoint.x, leftMassPoint.y, leftMassPoint.z, rect));
                    dc.DrawCircle(Project(leftMassPoint.x, leftMassPoint.y, leftMassPoint.z, rect), 4);

                    dc.DrawLine(Project(it->shaftAxisRight.x, it->shaftAxisRight.y, it->shaftAxisRight.z, rect),
                                Project(rightMassPoint.x, rightMassPoint.y, rightMassPoint.z, rect));
                    dc.DrawCircle(Project(rightMassPoint.x, rightMassPoint.y, rightMassPoint.z, rect), 4);
                }
            }
        }
    }

    dc.SetPen(wxPen(wxColour(120, 255, 120), 2));
    dc.SetBrush(wxBrush(wxColour(120, 255, 120)));

    for (const auto& shaft : model.balancing.balancerShafts)
    {
        if (!shaft.enabled)
            continue;

        const Vec3 origin{
            (shaft.originXMm / 1000.0) * rScale,
            (shaft.originYMm / 1000.0) * rScale,
            (shaft.originZMm / 1000.0) * zScale
        };

        const Vec3 axisDir = BalancerAxisDirection(shaft.axis);
        const double lengthPreview = std::max(0.0, (shaft.lengthMm / 1000.0) * zScale);
        const Vec3 end = origin + axisDir * lengthPreview;

        dc.DrawLine(Project(origin.x, origin.y, origin.z, rect),
                    Project(end.x, end.y, end.z, rect));

        const double radiusPreview = std::max(0.0, (shaft.counterweightRadiusMm / 1000.0) * rScale);

        for (const auto& cw : shaft.counterweights)
        {
            const Vec3 center =
                origin + axisDir * ((cw.positionAlongShaftMm / 1000.0) * zScale);

            const Vec3 radiusVec = MakeBalancerRadiusVectorPreview(
                shaft.axis,
                radiusPreview,
                m_animationAlphaDeg,
                shaft.shaftPhaseDeg,
                cw.phaseDeg,
                shaft.speedRatio);

            const Vec3 massPoint = center + radiusVec;

            dc.DrawLine(Project(center.x, center.y, center.z, rect),
                        Project(massPoint.x, massPoint.y, massPoint.z, rect));
            dc.DrawCircle(Project(massPoint.x, massPoint.y, massPoint.z, rect), 4);
        }
    }
}

void EngineSchemePanel::OnPaint(wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(wxColour(12, 18, 28)));
    dc.Clear();

    const wxRect rect = GetClientRect();
    wxRect borderRect = rect;
    borderRect.Deflate(1);

    dc.SetPen(wxPen(wxColour(220, 220, 220), 2));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(borderRect);

    if (!m_model.has_value())
    {
        DrawEmptyState(dc, rect);
        return;
    }

    DrawScheme(dc, rect);
}