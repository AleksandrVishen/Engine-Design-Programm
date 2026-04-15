#pragma once

#include <wx/panel.h>
#include <wx/timer.h>

#include "core/balancing/balancing_pipeline.h"
#include "core/model/engine_model.h"

class wxButton;
class wxChoice;
class wxSlider;
class wxStaticText;
class wxNotebook;

class EngineSchemePanel;
class BalancingChartPanel;
class BalancingTablePanel;

class BalancingResultPage : public wxPanel
{
public:
    explicit BalancingResultPage(wxWindow* parent);

    void SetResultData(const EngineModel& model,
                       const engine::balancing::BalancingPipelineResult& result);

private:
    void BuildUi();

    void OnMetricChanged(wxCommandEvent& event);
    void OnViewModeChanged(wxCommandEvent& event);
    void OnComponentChanged(wxCommandEvent& event);
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

    engine::balancing::BalancingPipelineResult m_pipelineResult;
    std::size_t m_currentAlphaIndex = 0;
    bool m_isPlaying = false;

    wxChoice* m_metricChoice = nullptr;
    wxChoice* m_viewModeChoice = nullptr;
    wxChoice* m_componentChoice = nullptr;

    wxNotebook* m_leftNotebook = nullptr;
    BalancingChartPanel* m_chartPanel = nullptr;
    BalancingTablePanel* m_tablePanel = nullptr;

    wxStaticText* m_summaryText = nullptr;
    wxStaticText* m_warningText = nullptr;

    EngineSchemePanel* m_schemePanel = nullptr;
    wxStaticText* m_currentAlphaText = nullptr;
    wxSlider* m_alphaSlider = nullptr;
    wxButton* m_prevButton = nullptr;
    wxButton* m_playPauseButton = nullptr;
    wxButton* m_nextButton = nullptr;

    wxTimer m_animationTimer;
};