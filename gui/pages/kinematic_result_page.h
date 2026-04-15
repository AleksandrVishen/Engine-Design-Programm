#pragma once

#include <wx/panel.h>
#include <wx/timer.h>

#include "core/kinematic/kinematic_result.h"
#include "core/model/engine_model.h"

class wxButton;
class wxChoice;
class wxSlider;
class wxStaticText;
class KinematicChartPanel;
class KinematicLegendPanel;
class EngineSchemePanel;
class wxNotebook;
class KinematicTablePanel;

class KinematicResultPage : public wxPanel
{
public:
    explicit KinematicResultPage(wxWindow* parent);

    void SetResultData(const EngineModel& model,
                       const engine::kinematic::KinematicResult& result);

private:
    void BuildUi();
    void OnMetricChanged(wxCommandEvent& event);
    void OnSliderChanged(wxCommandEvent& event);
    void OnPrev(wxCommandEvent& event);
    void OnNext(wxCommandEvent& event);
    void OnPlayPause(wxCommandEvent& event);
    void OnAnimationTimer(wxTimerEvent& event);

    void SetCurrentAlphaIndex(std::size_t index);
    void UpdateAnimationUi();
    void StopAnimation();

private:
    EngineModel m_model;
    bool m_hasModel = false;

    engine::kinematic::KinematicResult m_result;
    std::size_t m_currentAlphaIndex = 0;
    bool m_isPlaying = false;

    wxChoice* m_metricChoice = nullptr;
    KinematicChartPanel* m_chartPanel = nullptr;
    KinematicLegendPanel* m_legendPanel = nullptr;
    EngineSchemePanel* m_schemePanel = nullptr;
    wxStaticText* m_summaryText = nullptr;
    wxStaticText* m_currentAlphaText = nullptr;
    wxSlider* m_alphaSlider = nullptr;
    wxButton* m_prevButton = nullptr;
    wxButton* m_playPauseButton = nullptr;
    wxButton* m_nextButton = nullptr;

    wxTimer m_animationTimer;

    wxNotebook* m_leftNotebook = nullptr;
    KinematicTablePanel* m_tablePanel = nullptr;
};