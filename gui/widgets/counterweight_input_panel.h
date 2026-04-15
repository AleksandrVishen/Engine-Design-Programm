#pragma once

#include <functional>
#include <optional>
#include <wx/panel.h>

#include "core/model/engine_model.h"

class wxChoice;
class wxGrid;
class wxSpinCtrl;
class wxTextCtrl;

class CounterweightInputPanel : public wxPanel
{
public:
    explicit CounterweightInputPanel(wxWindow* parent);

    void SetModel(const EngineModel& model);
    bool HasModel() const;

    EngineModel BuildUpdatedModel() const;

    void SetOnDataChanged(std::function<void(const EngineModel&)> handler);

private:
    void BuildUi();
    void BindEvents();

    void RebuildBalancerShaftGrid();
    void RebuildBalancerAxisGrid();
    void RebuildBalancerSpeedGrid();
    void RebuildBalancerPhaseGrid();

    void FillControlsFromModel();
    void NotifyDataChanged();

    void NormalizeBalancerPositionCells();
    void NormalizeBalancerPositionCellsForRow(int row);
    double ClampPositionToLength(double positionMm, double lengthMm) const;
    wxString FormatGridNumber(double value) const;

    void ApplyCrankCounterweightModeRules();
    int GetTotalCylinderCount() const;
    std::optional<CounterweightCountMode> GetForcedCrankCounterweightMode() const;
    int CountModeToChoiceIndex(CounterweightCountMode mode) const;

    double ReadTextCtrlDouble(wxTextCtrl* ctrl, double fallback) const;
    double ReadGridDouble(wxGrid* grid, int row, int col, double fallback) const;

    wxString AxisToString(BalancerAxis axis) const;
    BalancerAxis StringToAxis(const wxString& text) const;

    wxString SpeedRatioToString(BalancerSpeedRatio ratio) const;
    BalancerSpeedRatio StringToSpeedRatio(const wxString& text) const;

    CounterweightCountMode ChoiceToCountMode() const;

private:
    std::optional<EngineModel> m_model;
    bool m_isUpdating = false;

    wxTextCtrl* m_crankMassCtrl = nullptr;
    wxTextCtrl* m_crankRadiusCtrl = nullptr;
    wxChoice* m_crankCountModeChoice = nullptr;

    wxSpinCtrl* m_balancerShaftCountSpin = nullptr;
    wxSpinCtrl* m_balancerCounterweightCountSpin = nullptr;

    wxGrid* m_balancerShaftGrid = nullptr;
    wxGrid* m_balancerAxisGrid = nullptr;
    wxGrid* m_balancerSpeedGrid = nullptr;
    wxGrid* m_balancerPhaseGrid = nullptr;

    std::function<void(const EngineModel&)> m_onDataChanged;
};