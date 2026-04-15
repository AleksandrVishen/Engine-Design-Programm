#pragma once

#include <functional>
#include <wx/panel.h>

#include "core/model/engine_model.h"

class wxSpinCtrl;
class wxChoice;
class wxRadioButton;
class wxGrid;
class wxStaticText;
class wxTextCtrl;
class wxBoxSizer;

class EngineInputPanel : public wxPanel
{
public:
    explicit EngineInputPanel(wxWindow* parent);

    bool BuildPreviewModel(EngineModel& model) const;
    bool BuildCalculationModel(EngineModel& model, wxString& errorText) const;

    void SetAlphaStep(double alphaStepDeg);
    double GetAlphaStep() const;

    void SetOnDataChanged(std::function<void()> handler);

private:
    void BuildUi();
    void BindInternalEvents();
    void RebuildTables();
    void NotifyDataChanged();

    void SetArticulatedMode(bool enabled);

    int GetShaftCount() const;
    int GetCrankCount() const;
    int GetCylindersPerCrank() const;
    int GetCylinderCountPerShaft() const;

    bool ReadDouble(wxTextCtrl* ctrl, double& value, bool strict, const wxString& fieldName, wxString& errorText) const;
    bool ReadGridRow(wxGrid* grid, int row, int expectedCols, std::vector<double>& values, bool strict, const wxString& gridName, wxString& errorText) const;

    bool ReadGeneralParams(EngineModel& model, bool strict, wxString& errorText) const;
    bool ReadShaftOrigins(EngineModel& model, bool strict, wxString& errorText) const;
    bool ReadCranks(EngineModel& model, bool strict, wxString& errorText) const;
    bool ReadCylinders(EngineModel& model, bool strict, wxString& errorText) const;

private:
    wxSpinCtrl* m_shaftCountSpin = nullptr;
    wxSpinCtrl* m_crankCountSpin = nullptr;
    wxChoice* m_cycleChoice = nullptr;
    wxChoice* m_cylindersPerCrankChoice = nullptr;

    wxRadioButton* m_sideBySideRadio = nullptr;
    wxRadioButton* m_articulatedRadio = nullptr;
    wxRadioButton* m_fullySupportedRadio = nullptr;
    wxRadioButton* m_semiSupportedRadio = nullptr;

    wxStaticText* m_articulatedRadiusLabel = nullptr;
    wxTextCtrl* m_articulatedRadiusCtrl = nullptr;
    wxStaticText* m_articulatedLengthLabel = nullptr;
    wxTextCtrl* m_articulatedLengthCtrl = nullptr;
    wxBoxSizer* m_articulatedSizer = nullptr;

    wxGrid* m_shaftOriginGrid = nullptr;
    wxGrid* m_phaseGrid = nullptr;
    wxGrid* m_axisTiltGrid = nullptr;

    double m_alphaStepDeg = 1.0;
    std::function<void()> m_onDataChanged;
};