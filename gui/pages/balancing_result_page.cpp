#include "gui/pages/balancing_result_page.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/slider.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "gui/common/text_utf8.h"
#include "gui/widgets/balancing_chart_panel.h"
#include "gui/widgets/balancing_component.h"
#include "gui/widgets/balancing_metric.h"
#include "gui/widgets/balancing_table_panel.h"
#include "gui/widgets/engine_scheme_panel.h"

BalancingResultPage::BalancingResultPage(wxWindow* parent)
    : wxPanel(parent),
      m_animationTimer(this)
{
    BuildUi();
    Bind(wxEVT_TIMER, &BalancingResultPage::OnAnimationTimer, this);
}

void BalancingResultPage::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, WXU8("Результаты уравновешивания"));
    auto titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 6);
    titleFont.SetWeight(wxFONTWEIGHT_NORMAL);
    title->SetFont(titleFont);
    title->SetForegroundColour(wxColour(245, 245, 245));

    root->Add(title, 0, wxLEFT | wxTOP | wxBOTTOM, 12);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    auto* contentSplitter = new wxSplitterWindow(
        this,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxSP_LIVE_UPDATE | wxSP_3D);
    contentSplitter->SetBackgroundColour(GetBackgroundColour());
    contentSplitter->SetMinimumPaneSize(300);
    contentSplitter->SetSashGravity(0.55);

    auto* leftPane = new wxPanel(contentSplitter);
    leftPane->SetBackgroundColour(GetBackgroundColour());

    auto* leftSizer = new wxBoxSizer(wxVERTICAL);

    auto* controlsSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* metricLabel = new wxStaticText(leftPane, wxID_ANY, WXU8("Параметр:"));
    metricLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_metricChoice = new wxChoice(leftPane, wxID_ANY);
    m_metricChoice->Append(WXU8("Полная сила инерции F"));
    m_metricChoice->Append(WXU8("Сила инерции 1-го порядка F1"));
    m_metricChoice->Append(WXU8("Сила инерции 2-го порядка F2"));
    m_metricChoice->Append(WXU8("Момент от силы 1-го порядка M1"));
    m_metricChoice->Append(WXU8("Момент от силы 2-го порядка M2"));
    m_metricChoice->Append(WXU8("Центробежная сила Fc"));
    m_metricChoice->Append(WXU8("Момент от центробежной силы Mc"));
    m_metricChoice->SetSelection(0);

    auto* viewModeLabel = new wxStaticText(leftPane, wxID_ANY, WXU8("Режим:"));
    viewModeLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_viewModeChoice = new wxChoice(leftPane, wxID_ANY);
    m_viewModeChoice->Append(WXU8("Исходная"));
    m_viewModeChoice->Append(WXU8("Вклад противовесов"));
    m_viewModeChoice->Append(WXU8("Остаточная"));
    m_viewModeChoice->SetSelection(2);

    auto* componentLabel = new wxStaticText(leftPane, wxID_ANY, WXU8("Компонента:"));
    componentLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_componentChoice = new wxChoice(leftPane, wxID_ANY);
    m_componentChoice->Append("X");
    m_componentChoice->Append("Y");
    m_componentChoice->Append("Z");
    m_componentChoice->Append(WXU8("|.|"));
    m_componentChoice->SetSelection(3);

    controlsSizer->Add(metricLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    controlsSizer->Add(m_metricChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    controlsSizer->Add(viewModeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    controlsSizer->Add(m_viewModeChoice, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 16);
    controlsSizer->Add(componentLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    controlsSizer->Add(m_componentChoice, 0, wxALIGN_CENTER_VERTICAL);

    leftSizer->Add(controlsSizer, 0, wxBOTTOM, 10);

    m_leftNotebook = new wxNotebook(leftPane, wxID_ANY);

    auto* chartPage = new wxPanel(m_leftNotebook);
    chartPage->SetBackgroundColour(GetBackgroundColour());
    auto* chartPageSizer = new wxBoxSizer(wxVERTICAL);

    m_chartPanel = new BalancingChartPanel(chartPage);
    m_chartPanel->SetMinSize(wxSize(-1, 300));
    chartPageSizer->Add(m_chartPanel, 1, wxEXPAND);

    chartPage->SetSizer(chartPageSizer);

    auto* tablePage = new wxPanel(m_leftNotebook);
    tablePage->SetBackgroundColour(GetBackgroundColour());
    auto* tablePageSizer = new wxBoxSizer(wxVERTICAL);

    m_tablePanel = new BalancingTablePanel(tablePage);
    tablePageSizer->Add(m_tablePanel, 1, wxEXPAND);

    tablePage->SetSizer(tablePageSizer);

    m_leftNotebook->AddPage(chartPage, WXU8("График"));
    m_leftNotebook->AddPage(tablePage, WXU8("Таблица"));

    leftSizer->Add(m_leftNotebook, 1, wxEXPAND | wxBOTTOM, 10);

    m_summaryText = new wxStaticText(
        leftPane,
        wxID_ANY,
        WXU8("Результаты уравновешивания еще не рассчитаны."));
    m_summaryText->SetForegroundColour(wxColour(220, 220, 220));
    leftSizer->Add(m_summaryText, 0, wxEXPAND | wxBOTTOM, 8);

    m_warningText = new wxStaticText(leftPane, wxID_ANY, "");
    m_warningText->SetForegroundColour(wxColour(255, 210, 120));
    leftSizer->Add(m_warningText, 0, wxEXPAND);

    leftPane->SetSizer(leftSizer);

    auto* rightPane = new wxPanel(contentSplitter);
    rightPane->SetBackgroundColour(GetBackgroundColour());

    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    auto* schemeTitle = new wxStaticText(rightPane, wxID_ANY, WXU8("Анимация механизма"));
    auto schemeTitleFont = schemeTitle->GetFont();
    schemeTitleFont.SetPointSize(schemeTitleFont.GetPointSize() + 3);
    schemeTitle->SetFont(schemeTitleFont);
    schemeTitle->SetForegroundColour(wxColour(245, 245, 245));

    rightSizer->Add(schemeTitle, 0, wxBOTTOM, 8);

    m_schemePanel = new EngineSchemePanel(rightPane);
    rightSizer->Add(m_schemePanel, 1, wxEXPAND | wxBOTTOM, 10);

    m_currentAlphaText = new wxStaticText(rightPane, wxID_ANY, WXU8("Текущий α: 0.0°"));
    m_currentAlphaText->SetForegroundColour(wxColour(230, 230, 230));
    rightSizer->Add(m_currentAlphaText, 0, wxBOTTOM, 8);

    m_alphaSlider = new wxSlider(
        rightPane,
        wxID_ANY,
        0,
        0,
        1,
        wxDefaultPosition,
        wxDefaultSize,
        wxSL_HORIZONTAL | wxSL_LABELS);
    rightSizer->Add(m_alphaSlider, 0, wxEXPAND | wxBOTTOM, 10);

    auto* animationButtons = new wxBoxSizer(wxHORIZONTAL);

    m_prevButton = new wxButton(rightPane, wxID_ANY, WXU8("<"));
    m_playPauseButton = new wxButton(rightPane, wxID_ANY, WXU8("Play"));
    m_nextButton = new wxButton(rightPane, wxID_ANY, WXU8(">"));

    animationButtons->Add(m_prevButton, 0, wxRIGHT, 8);
    animationButtons->Add(m_playPauseButton, 0, wxRIGHT, 8);
    animationButtons->Add(m_nextButton, 0);

    rightSizer->Add(animationButtons, 0, wxBOTTOM, 6);

    rightPane->SetSizer(rightSizer);

    contentSplitter->SplitVertically(leftPane, rightPane);
    contentSplitter->SetSashPosition(700);

    root->Add(contentSplitter, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    SetSizer(root);

    m_metricChoice->Bind(wxEVT_CHOICE, &BalancingResultPage::OnMetricChanged, this);
    m_viewModeChoice->Bind(wxEVT_CHOICE, &BalancingResultPage::OnViewModeChanged, this);
    m_componentChoice->Bind(wxEVT_CHOICE, &BalancingResultPage::OnComponentChanged, this);
    m_alphaSlider->Bind(wxEVT_SLIDER, &BalancingResultPage::OnSliderChanged, this);
    m_prevButton->Bind(wxEVT_BUTTON, &BalancingResultPage::OnPrev, this);
    m_playPauseButton->Bind(wxEVT_BUTTON, &BalancingResultPage::OnPlayPause, this);
    m_nextButton->Bind(wxEVT_BUTTON, &BalancingResultPage::OnNext, this);

    UpdateAnimationUi();
}

void BalancingResultPage::ApplyMetricAndComponentSelection()
{
    BalancingMetric metric = BalancingMetric::InertiaForce;

    switch (m_metricChoice ? m_metricChoice->GetSelection() : 0)
    {
    case 0: metric = BalancingMetric::InertiaForce; break;
    case 1: metric = BalancingMetric::InertiaForceFirstOrder; break;
    case 2: metric = BalancingMetric::InertiaForceSecondOrder; break;
    case 3: metric = BalancingMetric::InertiaMomentFirstOrder; break;
    case 4: metric = BalancingMetric::InertiaMomentSecondOrder; break;
    case 5: metric = BalancingMetric::CentrifugalForce; break;
    case 6: metric = BalancingMetric::CentrifugalMoment; break;
    default: metric = BalancingMetric::InertiaForce; break;
    }

    BalancingViewMode viewMode = BalancingViewMode::Residual;
    switch (m_viewModeChoice ? m_viewModeChoice->GetSelection() : 2)
    {
    case 0: viewMode = BalancingViewMode::Source; break;
    case 1: viewMode = BalancingViewMode::Counterweight; break;
    case 2: viewMode = BalancingViewMode::Residual; break;
    default: viewMode = BalancingViewMode::Residual; break;
    }

    BalancingComponent component = BalancingComponent::Magnitude;
    switch (m_componentChoice ? m_componentChoice->GetSelection() : 3)
    {
    case 0: component = BalancingComponent::X; break;
    case 1: component = BalancingComponent::Y; break;
    case 2: component = BalancingComponent::Z; break;
    case 3: component = BalancingComponent::Magnitude; break;
    default: component = BalancingComponent::Magnitude; break;
    }

    if (m_chartPanel)
    {
        m_chartPanel->SetMetric(metric);
        m_chartPanel->SetViewMode(viewMode);
        m_chartPanel->SetComponent(component);
    }

    if (m_tablePanel)
    {
        m_tablePanel->SetMetric(metric);
        m_tablePanel->SetViewMode(viewMode);
        m_tablePanel->SetComponent(component);
    }
}

void BalancingResultPage::OnMetricChanged(wxCommandEvent&)
{
    ApplyMetricAndComponentSelection();
}

void BalancingResultPage::OnViewModeChanged(wxCommandEvent&)
{
    ApplyMetricAndComponentSelection();
}

void BalancingResultPage::OnComponentChanged(wxCommandEvent&)
{
    ApplyMetricAndComponentSelection();
}

void BalancingResultPage::SetResultData(
    const EngineModel& model,
    const engine::balancing::BalancingPipelineResult& result)
{
    StopAnimation();

    m_model = model;
    m_hasModel = true;
    m_pipelineResult = result;
    m_currentAlphaIndex = 0;

    if (m_chartPanel)
    {
        m_chartPanel->SetResult(result.composedResult);
        m_chartPanel->SetCurrentAlphaIndex(0);
    }

    if (m_tablePanel)
    {
        m_tablePanel->SetResult(result.composedResult);
        m_tablePanel->SetCurrentAlphaIndex(0);
    }

    if (m_schemePanel)
    {
        m_schemePanel->SetModel(model);
        if (!result.composedResult.alphaDeg.empty())
            m_schemePanel->SetAnimationAlphaDeg(result.composedResult.alphaDeg.front());
        else
            m_schemePanel->SetAnimationAlphaDeg(0.0);
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);
    oss << "Точек alpha: " << result.composedResult.alphaDeg.size()
        << " | Предупреждений: " << result.warnings.size();

    if (!result.composedResult.alphaDeg.empty())
    {
        oss << " | Диапазон alpha: "
            << result.composedResult.alphaDeg.front()
            << " .. "
            << result.composedResult.alphaDeg.back()
            << " град.";
    }

    if (m_summaryText)
        m_summaryText->SetLabel(wxString::FromUTF8(oss.str().c_str()));

    if (m_warningText)
    {
        if (result.warnings.empty())
        {
            m_warningText->SetLabel("");
        }
        else
        {
            std::ostringstream wss;
            wss << "Первое предупреждение: " << result.warnings.front().message;
            m_warningText->SetLabel(wxString::FromUTF8(wss.str().c_str()));
        }
    }

    if (m_alphaSlider)
    {
        const int maxValue = result.composedResult.alphaDeg.empty()
            ? 1
            : static_cast<int>(result.composedResult.alphaDeg.size() - 1);
        m_alphaSlider->SetRange(0, maxValue);
        m_alphaSlider->SetValue(0);
    }

    ApplyMetricAndComponentSelection();
    UpdateAnimationUi();
    Layout();
}

void BalancingResultPage::OnSliderChanged(wxCommandEvent&)
{
    if (m_pipelineResult.composedResult.alphaDeg.empty() || !m_alphaSlider)
        return;

    SetCurrentAlphaIndex(static_cast<std::size_t>(m_alphaSlider->GetValue()));
}

void BalancingResultPage::OnPrev(wxCommandEvent&)
{
    if (m_pipelineResult.composedResult.alphaDeg.empty())
        return;

    if (m_currentAlphaIndex == 0)
        SetCurrentAlphaIndex(m_pipelineResult.composedResult.alphaDeg.size() - 1);
    else
        SetCurrentAlphaIndex(m_currentAlphaIndex - 1);
}

void BalancingResultPage::OnNext(wxCommandEvent&)
{
    if (m_pipelineResult.composedResult.alphaDeg.empty())
        return;

    SetCurrentAlphaIndex((m_currentAlphaIndex + 1) % m_pipelineResult.composedResult.alphaDeg.size());
}

void BalancingResultPage::OnPlayPause(wxCommandEvent&)
{
    if (m_pipelineResult.composedResult.alphaDeg.empty())
        return;

    if (m_isPlaying)
    {
        StopAnimation();
    }
    else
    {
        m_isPlaying = true;
        m_animationTimer.Start(33);
        UpdateAnimationUi();
    }
}

void BalancingResultPage::OnAnimationTimer(wxTimerEvent&)
{
    if (m_pipelineResult.composedResult.alphaDeg.empty())
        return;

    SetCurrentAlphaIndex((m_currentAlphaIndex + 1) % m_pipelineResult.composedResult.alphaDeg.size());
}

void BalancingResultPage::SetCurrentAlphaIndex(std::size_t index)
{
    if (m_pipelineResult.composedResult.alphaDeg.empty())
    {
        m_currentAlphaIndex = 0;
        UpdateAnimationUi();
        return;
    }

    m_currentAlphaIndex = std::min(index, m_pipelineResult.composedResult.alphaDeg.size() - 1);

    if (m_isPlaying)
    {
        if (m_schemePanel)
            m_schemePanel->SetAnimationAlphaDeg(
                m_pipelineResult.composedResult.alphaDeg[m_currentAlphaIndex]);

        if (m_chartPanel)
            m_chartPanel->SetCurrentAlphaIndex(m_currentAlphaIndex);

        if (m_alphaSlider && static_cast<std::size_t>(m_alphaSlider->GetValue()) != m_currentAlphaIndex)
            m_alphaSlider->SetValue(static_cast<int>(m_currentAlphaIndex));

        const double alpha = m_pipelineResult.composedResult.alphaDeg[m_currentAlphaIndex];
        if (m_currentAlphaText)
            m_currentAlphaText->SetLabel(wxString::Format(WXU8("Текущий α: %.1f°"), alpha));
        return;
    }

    if (m_chartPanel)
        m_chartPanel->SetCurrentAlphaIndex(m_currentAlphaIndex);

    if (m_tablePanel)
        m_tablePanel->SetCurrentAlphaIndex(m_currentAlphaIndex);

    if (m_schemePanel)
        m_schemePanel->SetAnimationAlphaDeg(m_pipelineResult.composedResult.alphaDeg[m_currentAlphaIndex]);

    if (m_alphaSlider && static_cast<std::size_t>(m_alphaSlider->GetValue()) != m_currentAlphaIndex)
        m_alphaSlider->SetValue(static_cast<int>(m_currentAlphaIndex));

    UpdateAnimationUi();
}

void BalancingResultPage::UpdateAnimationUi()
{
    double alpha = 0.0;
    if (!m_pipelineResult.composedResult.alphaDeg.empty())
        alpha = m_pipelineResult.composedResult.alphaDeg[
            std::min(m_currentAlphaIndex, m_pipelineResult.composedResult.alphaDeg.size() - 1)];

    if (m_currentAlphaText)
        m_currentAlphaText->SetLabel(wxString::Format(WXU8("Текущий α: %.1f°"), alpha));

    if (m_playPauseButton)
        m_playPauseButton->SetLabel(m_isPlaying ? WXU8("Pause") : WXU8("Play"));

    const bool hasData = !m_pipelineResult.composedResult.alphaDeg.empty();

    if (m_prevButton) m_prevButton->Enable(hasData);
    if (m_playPauseButton) m_playPauseButton->Enable(hasData);
    if (m_nextButton) m_nextButton->Enable(hasData);
    if (m_alphaSlider) m_alphaSlider->Enable(hasData);
}

void BalancingResultPage::StopAnimation()
{
    if (m_animationTimer.IsRunning())
        m_animationTimer.Stop();

    m_isPlaying = false;

    if (!m_pipelineResult.composedResult.alphaDeg.empty())
    {
        if (m_chartPanel)
            m_chartPanel->SetCurrentAlphaIndex(m_currentAlphaIndex);
        if (m_tablePanel)
            m_tablePanel->SetCurrentAlphaIndex(m_currentAlphaIndex);
    }

    UpdateAnimationUi();
}