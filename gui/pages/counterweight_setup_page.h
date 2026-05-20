#pragma once

#include <functional>
#include <optional>
#include <wx/panel.h>

#include "core/balancing/balancing_synthesis.h"
#include "core/model/engine_model.h"

class wxButton;
class wxScrolledWindow;
class CounterweightInputPanel;
class EngineSchemePanel;

class CounterweightSetupPage : public wxPanel
{
public:
    explicit CounterweightSetupPage(wxWindow* parent);

    void SetModel(const EngineModel& model);
    std::optional<EngineModel> GetUpdatedModel() const;

    void SetOnInputChanged(std::function<void()> handler);
    void SetOnCalculateRequested(std::function<void(const EngineModel&)> handler);
    void SetOnAutobalanceRequested(
        std::function<engine::balancing::BalancingSynthesisResult(
            const EngineModel&,
            engine::balancing::BalancingSynthesisGoalKind)> handler);

    void RunPipelineCalculate();
    void RunAutobalance();

private:
    void BuildUi();
    void BindEvents();

    void ApplyPreviewModel(const EngineModel& model);
    void OnCalculate(wxCommandEvent& event);
    void OnAutobalance(wxCommandEvent& event);

private:
    std::optional<EngineModel> m_model;

    wxScrolledWindow* m_inputScroll = nullptr;
    CounterweightInputPanel* m_inputPanel = nullptr;
    EngineSchemePanel* m_schemePanel = nullptr;
    wxButton* m_calculateButton = nullptr;
    wxButton* m_autobalanceButton = nullptr;

    std::function<void()> m_onInputChanged;
    std::function<void(const EngineModel&)> m_onCalculateRequested;
    std::function<engine::balancing::BalancingSynthesisResult(
        const EngineModel&,
        engine::balancing::BalancingSynthesisGoalKind)> m_onAutobalanceRequested;
};
