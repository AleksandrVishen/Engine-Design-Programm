#pragma once

#include <optional>
#include <wx/frame.h>

#include "core/balancing/balancing_pipeline.h"
#include "core/dynamic/dynamic_result.h"
#include "core/model/engine_model.h"
#include "core/model/mass_properties_input.h"
#include "core/kinematic/kinematic_result.h"

class wxSimplebook;
class NavigationPanel;
class InputPage;
class KinematicResultPage;
class MassPropertiesPage;
class DynamicResultPage;
class CounterweightSetupPage;
class BalancingResultPage;

class MainFrame : public wxFrame
{
public:
    MainFrame();

private:
    void BuildUi();
    void BindEvents();

    void ShowGeometryPage();
    void ShowKinematicResultPage();
    void ShowMassPropertiesPage();
    void ShowDynamicResultPage();
    void ShowCounterweightSetupPage();
    void ShowBalancingResultPage();

    void OnFileStub(wxCommandEvent& event);
    void OnOpenSettings(wxCommandEvent& event);
    void OnOpenWindowSizeSettings(wxCommandEvent& event);
    void OnOpenHelp(wxCommandEvent& event);

    void ApplySafeWindowBounds(const wxSize& desiredSize);

private:
    NavigationPanel* m_navigationPanel = nullptr;
    wxSimplebook* m_book = nullptr;
    InputPage* m_inputPage = nullptr;
    KinematicResultPage* m_resultPage = nullptr;
    MassPropertiesPage* m_massPage = nullptr;
    DynamicResultPage* m_dynamicResultPage = nullptr;
    CounterweightSetupPage* m_counterweightSetupPage = nullptr;
    BalancingResultPage* m_balancingResultPage = nullptr;

    double m_alphaStepDeg = 1.0;

    std::optional<EngineModel> m_lastEngineModel;
    std::optional<engine::kinematic::KinematicResult> m_lastKinematicResult;
    std::optional<MassPropertiesInput> m_lastMassPropertiesInput;
    std::optional<engine::dynamic::DynamicResult> m_lastDynamicResult;
    std::optional<engine::balancing::BalancingPipelineResult> m_lastBalancingPipelineResult;
};