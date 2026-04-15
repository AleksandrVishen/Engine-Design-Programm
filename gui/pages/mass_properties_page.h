#pragma once

#include <functional>
#include <wx/grid.h>
#include <wx/scrolwin.h>

#include "core/model/engine_model.h"
#include "core/model/mass_properties_input.h"

class wxButton;
class wxTextCtrl;
class EngineSchemePanel;

class MassPropertiesPage : public wxScrolledWindow
{
public:
    explicit MassPropertiesPage(wxWindow* parent);

    void SetModel(const EngineModel& model);
    MassPropertiesInput GetInput() const;

    void SetOnCalculateRequested(std::function<void(const MassPropertiesInput&)> handler);

private:
    void BuildUi();
    void BindEvents();

    void UpdateSchemeReferencePoint();
    void OnAnyInputChanged(wxCommandEvent& event);
    void OnGridCellChanged(wxGridEvent& event);
    void OnCalculate(wxCommandEvent& event);

    double ReadTextCtrlDouble(wxTextCtrl* ctrl, double fallback) const;
    double ReadGridDouble(int row, int col, double fallback) const;

private:
    EngineSchemePanel* m_schemePanel = nullptr;

    wxTextCtrl* m_cylinderDiameterCtrl = nullptr;
    wxTextCtrl* m_reciprocatingMassCtrl = nullptr;
    wxTextCtrl* m_rotatingMassCtrl = nullptr;

    wxGrid* m_referenceGrid = nullptr;
    wxButton* m_calculateButton = nullptr;

    std::function<void(const MassPropertiesInput&)> m_onCalculateRequested;
};