#pragma once

#include <optional>
#include <wx/panel.h>

#include "core/model/engine_model.h"

class wxButton;

class EngineSchemePanel : public wxPanel
{
public:
    explicit EngineSchemePanel(wxWindow* parent);

    void SetModel(const EngineModel& model);
    void ClearModel();

    void SetAnimationAlphaDeg(double alphaDeg);
    double GetAnimationAlphaDeg() const { return m_animationAlphaDeg; }

    void SetShowReferencePoint(bool show);
    void SetReferencePointMm(double xMm, double yMm, double zMm);

private:
    void OnPaint(wxPaintEvent& event);
    void OnSize(wxSizeEvent& event);

    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnRightUp(wxMouseEvent& event);
    void OnMiddleDown(wxMouseEvent& event);
    void OnMotion(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnLeaveWindow(wxMouseEvent& event);

    void OnHelpClicked(wxCommandEvent& event);

    void ResetView();
    void UpdateHelpButtonPosition();

    wxPoint Project(double x, double y, double z, const wxRect& rect) const;

    void DrawAxes(wxDC& dc, const wxRect& rect) const;
    void DrawReferenceAxes(wxDC& dc, const wxRect& rect, double x, double y, double z) const;
    void DrawEmptyState(wxDC& dc, const wxRect& rect) const;
    void DrawScheme(wxDC& dc, const wxRect& rect) const;

private:
    std::optional<EngineModel> m_model;

    double m_animationAlphaDeg = 0.0;

    double m_zoom = 1.0;
    double m_panX = 0.0;
    double m_panY = 0.0;

    double m_yawDeg = 0.0;
    double m_pitchDeg = 0.0;

    bool m_isPanning = false;
    bool m_isRotating = false;
    wxPoint m_lastMousePos;

    wxButton* m_helpButton = nullptr;

    bool m_showReferencePoint = false;
    double m_referenceXmm = 0.0;
    double m_referenceYmm = 0.0;
    double m_referenceZmm = 0.0;

    wxDECLARE_EVENT_TABLE();
};