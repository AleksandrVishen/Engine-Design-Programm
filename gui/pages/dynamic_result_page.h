#pragma once

#include <vector>
#include <wx/panel.h>
#include <wx/timer.h>

#include "core/dynamic/dynamic_result.h"
#include "core/model/engine_model.h"

class wxButton;
class wxCheckBox;
class wxCheckListBox;
class wxChoice;
class wxSlider;
class wxStaticText;
class wxNotebook;

class EngineSchemePanel;
class DynamicChartPanel;
class DynamicTablePanel;
class DynamicLegendPanel;

class DynamicResultPage : public wxPanel
{
public:
    explicit DynamicResultPage(wxWindow* parent);

    void SetResultData(const EngineModel& model,
                       const engine::dynamic::DynamicResult& result);

private:
    void BuildUi();
    void PopulateCylinderCheckList();
    void ApplyCylinderSelection();
    std::vector<int> CollectSelectedCylinderIndices() const;

    void OnMetricChanged(wxCommandEvent& event);
    void OnComponentChanged(wxCommandEvent& event);
    void OnCylinderCheckChanged(wxCommandEvent& event);
    void OnShowTotalChanged(wxCommandEvent& event);
    void OnSelectAllCylinders(wxCommandEvent& event);
    void OnClearCylinderSelection(wxCommandEvent& event);
    void OnSliderChanged(wxCommandEvent& event);
    void OnPrev(wxCommandEvent& event);
    void OnNext(wxCommandEvent& event);
    void OnPlayPause(wxCommandEvent& event);
    void OnAnimationTimer(wxTimerEvent& event);

    void ApplyMetricAndComponentSelection();
    void SetCurrentAlphaIndex(std::size_t index);
    void UpdateAnimationUi();
    void StopAnimation();

private:
    EngineModel m_model;
    bool m_hasModel = false;

    engine::dynamic::DynamicResult m_result;
    std::size_t m_currentAlphaIndex = 0;
    bool m_isPlaying = false;

    wxChoice* m_metricChoice = nullptr;
    wxChoice* m_componentChoice = nullptr;

    wxNotebook* m_leftNotebook = nullptr;
    DynamicChartPanel* m_chartPanel = nullptr;
    DynamicTablePanel* m_tablePanel = nullptr;
    DynamicLegendPanel* m_legendPanel = nullptr;

    wxCheckBox* m_showTotalCheckBox = nullptr;
    wxCheckListBox* m_cylinderCheckList = nullptr;
    wxButton* m_selectAllButton = nullptr;
    wxButton* m_clearSelectionButton = nullptr;

    wxStaticText* m_summaryText = nullptr;
    EngineSchemePanel* m_schemePanel = nullptr;
    wxStaticText* m_currentAlphaText = nullptr;
    wxSlider* m_alphaSlider = nullptr;
    wxButton* m_prevButton = nullptr;
    wxButton* m_playPauseButton = nullptr;
    wxButton* m_nextButton = nullptr;

    wxTimer m_animationTimer;
};