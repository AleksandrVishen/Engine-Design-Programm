#pragma once

#include <optional>
#include <string>
#include <wx/frame.h>
#include <wx/event.h>

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
    explicit MainFrame(const wxString& startupProjectPath = wxString());

private:
    void BuildUi();
    void BindEvents();

    void ShowGeometryPage();
    void ShowKinematicResultPage();
    void ShowMassPropertiesPage();
    void ShowDynamicResultPage();
    void ShowCounterweightSetupPage();
    void ShowBalancingResultPage();

    void OnOpenProject(wxCommandEvent& event);
    void OnSaveProject(wxCommandEvent& event);
    void OnSaveProjectAs(wxCommandEvent& event);
    void OnOpenSettings(wxCommandEvent& event);
    void OnOpenReport(wxCommandEvent& event);
    void OnOpenHelp(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    void ApplySafeWindowBounds(const wxSize& desiredSize);
    bool SaveProjectToPath(const wxString& path);
    bool SaveProjectAs();
    bool LoadProjectFromPath(const wxString& path);
    bool BuildCurrentEngineModelForSave(EngineModel& model);
    void MarkProjectDirty();
    void MarkProjectClean();
    void UpdateWindowTitle();
    bool ConfirmDiscardUnsavedChanges();
    void PersistLastProjectPath(const wxString& path);
    void TryLoadLastProjectAtStartup();
    wxString GetLastProjectPointerFilePath() const;

    void OnCharHook(wxKeyEvent& event);
    void NavigateToPreviousNavPage();
    static bool IsMultilineTextInput(wxWindow* focus);

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
    std::optional<std::string> m_currentProjectPathUtf8;
    bool m_isProjectDirty = false;
    bool m_isLoadingProject = false;
    wxString m_startupProjectPath;
};