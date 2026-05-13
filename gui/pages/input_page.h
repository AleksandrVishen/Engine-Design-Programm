#pragma once

#include <functional>
#include <wx/scrolwin.h>
#include "core/kinematic/kinematic_result.h"
#include "core/model/engine_model.h"

class EngineInputPanel;
class EngineSchemePanel;
class KinematicParamsPanel;

class InputPage : public wxScrolledWindow
{
public:
    explicit InputPage(wxWindow* parent);

    void SetAlphaStep(double alphaStepDeg);
    double GetAlphaStep() const;
    bool BuildCurrentModel(EngineModel& model, wxString& errorText) const;
    void SetFromModel(const EngineModel& model);

    void SetOnCalculationSucceeded(
        std::function<void(const EngineModel&, const engine::kinematic::KinematicResult&)> handler);
    void SetOnInputChanged(std::function<void()> handler);

    void RunKinematicCalculation();

private:
    void BuildUi();
    void BindEvents();

    void UpdatePreview();
    bool BuildPreviewModel(EngineModel& model) const;
    bool BuildCalculationModel(EngineModel& model, wxString& errorText) const;
    void OnCalculateRequested();

private:
    EngineInputPanel* m_inputPanel = nullptr;
    EngineSchemePanel* m_schemePanel = nullptr;
    KinematicParamsPanel* m_kinematicParamsPanel = nullptr;

    std::function<void(const EngineModel&, const engine::kinematic::KinematicResult&)> m_onCalculationSucceeded;
    std::function<void()> m_onInputChanged;
};