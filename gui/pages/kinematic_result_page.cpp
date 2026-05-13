#include "gui/pages/kinematic_result_page.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/slider.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "gui/common/text_utf8.h"
#include "gui/widgets/engine_scheme_panel.h"
#include "gui/widgets/kinematic_chart_panel.h"
#include "gui/widgets/kinematic_legend_panel.h"
#include "gui/widgets/kinematic_table_panel.h"

KinematicResultPage::KinematicResultPage(wxWindow* parent)
    : wxPanel(parent),
      m_animationTimer(this)
{
    BuildUi();

    Bind(wxEVT_TIMER, &KinematicResultPage::OnAnimationTimer, this);
}

void KinematicResultPage::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, WXU8("Результаты расчета кинематики"));
    auto titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 6);
    titleFont.SetWeight(wxFONTWEIGHT_NORMAL);
    title->SetFont(titleFont);
    title->SetForegroundColour(wxColour(245, 245, 245));

    root->Add(title, 0, wxLEFT | wxTOP | wxBOTTOM, 12);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    // =========================================================================
    // ЛЕВАЯ ЧАСТЬ: контейнер-хост, который занимает место,
    // но внутри содержит блок фиксированной ширины.
    // Именно это не даёт графику и легенде растягиваться на всю страницу.
    // =========================================================================
    auto* leftHostPanel = new wxPanel(this);
    leftHostPanel->SetBackgroundColour(GetBackgroundColour());

    auto* leftHostSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* leftContentPanel = new wxPanel(leftHostPanel);
    leftContentPanel->SetBackgroundColour(GetBackgroundColour());

    // Ключевая настройка:
    // фиксируем рабочую ширину блока результатов.
    // Можешь потом подвинуть 960 / 980 / 1000 под свой вкус.
    leftContentPanel->SetMinSize(wxSize(650, -1));
    leftContentPanel->SetMaxSize(wxSize(650, -1));

    auto* leftSizer = new wxBoxSizer(wxVERTICAL);

    auto* controlsSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* metricLabel = new wxStaticText(leftContentPanel, wxID_ANY, WXU8("Параметр:"));
    metricLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_metricChoice = new wxChoice(leftContentPanel, wxID_ANY);
    m_metricChoice->Append(WXU8("Перемещение s"));
    m_metricChoice->Append(WXU8("Скорость v"));
    m_metricChoice->Append(WXU8("Ускорение a"));
    m_metricChoice->Append(WXU8("Ускорение 1-го порядка a1"));
    m_metricChoice->Append(WXU8("Ускорение 2-го порядка a2"));
    m_metricChoice->Append(WXU8("Угол шатуна φ"));
    m_metricChoice->Append(WXU8("Угловая скорость шатуна ω"));
    m_metricChoice->Append(WXU8("Угловое ускорение шатуна ε"));
    m_metricChoice->SetSelection(0);

    controlsSizer->Add(metricLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    controlsSizer->Add(m_metricChoice, 0, wxALIGN_CENTER_VERTICAL);

    leftSizer->Add(controlsSizer, 0, wxBOTTOM, 10);

    m_leftNotebook = new wxNotebook(leftContentPanel, wxID_ANY);

    auto* chartPage = new wxPanel(m_leftNotebook);
    chartPage->SetBackgroundColour(GetBackgroundColour());
    auto* chartPageSizer = new wxBoxSizer(wxVERTICAL);

    m_chartPanel = new KinematicChartPanel(chartPage);
    m_chartPanel->SetMinSize(wxSize(-1, 320));
    chartPageSizer->Add(m_chartPanel, 1, wxEXPAND);

    chartPage->SetSizer(chartPageSizer);

    auto* tablePage = new wxPanel(m_leftNotebook);
    tablePage->SetBackgroundColour(GetBackgroundColour());
    auto* tablePageSizer = new wxBoxSizer(wxVERTICAL);

    m_tablePanel = new KinematicTablePanel(tablePage);
    tablePageSizer->Add(m_tablePanel, 1, wxEXPAND);

    tablePage->SetSizer(tablePageSizer);

    m_leftNotebook->AddPage(chartPage, WXU8("График"));
    m_leftNotebook->AddPage(tablePage, WXU8("Таблица"));

    leftSizer->Add(m_leftNotebook, 1, wxEXPAND | wxBOTTOM, 10);

    m_legendPanel = new KinematicLegendPanel(leftContentPanel);
    leftSizer->Add(m_legendPanel, 0, wxEXPAND | wxBOTTOM, 10);

    m_summaryText = new wxStaticText(
        leftContentPanel,
        wxID_ANY,
        WXU8("Результаты еще не рассчитаны."));
    m_summaryText->SetForegroundColour(wxColour(220, 220, 220));

    leftSizer->Add(m_summaryText, 0, wxEXPAND);

    leftContentPanel->SetSizer(leftSizer);

    // Держим фиксированный блок у левого края,
    // а остаток ширины оставляем пустым внутри host-контейнера.
    leftHostSizer->Add(leftContentPanel, 0, wxEXPAND);
    leftHostSizer->AddStretchSpacer(1);

    leftHostPanel->SetSizer(leftHostSizer);

    // =========================================================================
    // ПРАВАЯ ЧАСТЬ: анимация
    // =========================================================================
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    auto* schemeTitle = new wxStaticText(this, wxID_ANY, WXU8("Анимация механизма"));
    auto schemeTitleFont = schemeTitle->GetFont();
    schemeTitleFont.SetPointSize(schemeTitleFont.GetPointSize() + 3);
    schemeTitle->SetFont(schemeTitleFont);
    schemeTitle->SetForegroundColour(wxColour(245, 245, 245));

    rightSizer->Add(schemeTitle, 0, wxBOTTOM, 8);

    m_schemePanel = new EngineSchemePanel(this);
    rightSizer->Add(m_schemePanel, 1, wxEXPAND | wxBOTTOM, 10);

    m_currentAlphaText = new wxStaticText(this, wxID_ANY, WXU8("Текущий α: 0.0°"));
    m_currentAlphaText->SetForegroundColour(wxColour(230, 230, 230));
    rightSizer->Add(m_currentAlphaText, 0, wxBOTTOM, 8);

    m_alphaSlider = new wxSlider(
        this,
        wxID_ANY,
        0,
        0,
        1,
        wxDefaultPosition,
        wxDefaultSize,
        wxSL_HORIZONTAL | wxSL_LABELS);
    rightSizer->Add(m_alphaSlider, 0, wxEXPAND | wxBOTTOM, 10);

    auto* animationButtons = new wxBoxSizer(wxHORIZONTAL);

    m_prevButton = new wxButton(this, wxID_ANY, WXU8("<"));
    m_playPauseButton = new wxButton(this, wxID_ANY, WXU8("Play"));
    m_nextButton = new wxButton(this, wxID_ANY, WXU8(">"));

    animationButtons->Add(m_prevButton, 0, wxRIGHT, 8);
    animationButtons->Add(m_playPauseButton, 0, wxRIGHT, 8);
    animationButtons->Add(m_nextButton, 0);

    rightSizer->Add(animationButtons, 0, wxBOTTOM, 6);

    // Левая часть шире, правая уже.
    // Но главное — ширина рабочего блока слева ограничена внутренней панелью.
    contentSizer->Add(leftHostPanel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    contentSizer->Add(rightSizer, 1, wxEXPAND | wxRIGHT | wxBOTTOM, 12);

    root->Add(contentSizer, 1, wxEXPAND);

    SetSizer(root);

    m_metricChoice->Bind(wxEVT_CHOICE, &KinematicResultPage::OnMetricChanged, this);
    m_alphaSlider->Bind(wxEVT_SLIDER, &KinematicResultPage::OnSliderChanged, this);
    m_prevButton->Bind(wxEVT_BUTTON, &KinematicResultPage::OnPrev, this);
    m_playPauseButton->Bind(wxEVT_BUTTON, &KinematicResultPage::OnPlayPause, this);
    m_nextButton->Bind(wxEVT_BUTTON, &KinematicResultPage::OnNext, this);

    UpdateAnimationUi();
}

void KinematicResultPage::OnMetricChanged(wxCommandEvent&)
{
    if (!m_metricChoice)
        return;

    KinematicMetric metric = KinematicMetric::Displacement;

    switch (m_metricChoice->GetSelection())
    {
    case 0:
        metric = KinematicMetric::Displacement;
        break;
    case 1:
        metric = KinematicMetric::Velocity;
        break;
    case 2:
        metric = KinematicMetric::Acceleration;
        break;
    case 3:
        metric = KinematicMetric::AccelerationFirstOrder;
        break;
    case 4:
        metric = KinematicMetric::AccelerationSecondOrder;
        break;
    case 5:
        metric = KinematicMetric::RodAngle;
        break;
    case 6:
        metric = KinematicMetric::RodAngularVelocity;
        break;
    case 7:
        metric = KinematicMetric::RodAngularAcceleration;
        break;
    default:
        metric = KinematicMetric::Displacement;
        break;
    }

    if (m_chartPanel)
        m_chartPanel->SetMetric(metric);

    if (m_tablePanel)
        m_tablePanel->SetMetric(metric);
}

void KinematicResultPage::SetResultData(
    const EngineModel& model,
    const engine::kinematic::KinematicResult& result)
{
    StopAnimation();

    m_model = model;
    m_hasModel = true;
    m_result = result;
    m_currentAlphaIndex = 0;

    if (m_chartPanel)
    {
        m_chartPanel->SetResult(result);
        m_chartPanel->SetCurrentAlphaIndex(0);
    }

    if (m_legendPanel)
        m_legendPanel->SetResult(result);

    if (m_schemePanel)
    {
        m_schemePanel->SetModel(model);
        if (!result.alphaDeg.empty())
            m_schemePanel->SetAnimationAlphaDeg(result.alphaDeg.front());
        else
            m_schemePanel->SetAnimationAlphaDeg(0.0);
    }

    if (m_tablePanel)
    {
        m_tablePanel->SetResult(result);
        m_tablePanel->SetCurrentAlphaIndex(0);
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(3);

    oss << "Точек alpha: " << result.alphaDeg.size()
        << " | Цилиндров: " << result.cylinders.size();

    if (!result.alphaDeg.empty())
    {
        oss << " | Диапазон alpha: "
            << result.alphaDeg.front()
            << " .. "
            << result.alphaDeg.back()
            << " град.";
    }

    if (m_summaryText)
        m_summaryText->SetLabel(wxString::FromUTF8(oss.str().c_str()));

    if (m_alphaSlider)
    {
        const int maxValue = result.alphaDeg.empty() ? 1 : static_cast<int>(result.alphaDeg.size() - 1);
        m_alphaSlider->SetRange(0, maxValue);
        m_alphaSlider->SetValue(0);
    }

    UpdateAnimationUi();
    Layout();
}

void KinematicResultPage::OnSliderChanged(wxCommandEvent&)
{
    if (m_result.alphaDeg.empty() || !m_alphaSlider)
        return;

    SetCurrentAlphaIndex(static_cast<std::size_t>(m_alphaSlider->GetValue()));
}

void KinematicResultPage::OnPrev(wxCommandEvent&)
{
    if (m_result.alphaDeg.empty())
        return;

    if (m_currentAlphaIndex == 0)
        SetCurrentAlphaIndex(m_result.alphaDeg.size() - 1);
    else
        SetCurrentAlphaIndex(m_currentAlphaIndex - 1);
}

void KinematicResultPage::OnNext(wxCommandEvent&)
{
    if (m_result.alphaDeg.empty())
        return;

    SetCurrentAlphaIndex((m_currentAlphaIndex + 1) % m_result.alphaDeg.size());
}

void KinematicResultPage::OnPlayPause(wxCommandEvent&)
{
    if (m_result.alphaDeg.empty())
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

void KinematicResultPage::OnAnimationTimer(wxTimerEvent&)
{
    if (m_result.alphaDeg.empty())
        return;

    SetCurrentAlphaIndex((m_currentAlphaIndex + 1) % m_result.alphaDeg.size());
}

void KinematicResultPage::SetCurrentAlphaIndex(std::size_t index)
{
    if (m_result.alphaDeg.empty())
    {
        m_currentAlphaIndex = 0;
        UpdateAnimationUi();
        return;
    }

    m_currentAlphaIndex = std::min(index, m_result.alphaDeg.size() - 1);

    // During playback: skip wxGrid updates (expensive); refresh chart so the marker follows curves.
    if (m_isPlaying)
    {
        if (m_schemePanel)
            m_schemePanel->SetAnimationAlphaDeg(m_result.alphaDeg[m_currentAlphaIndex]);

        if (m_chartPanel)
            m_chartPanel->SetCurrentAlphaIndex(m_currentAlphaIndex);

        if (m_alphaSlider && static_cast<std::size_t>(m_alphaSlider->GetValue()) != m_currentAlphaIndex)
            m_alphaSlider->SetValue(static_cast<int>(m_currentAlphaIndex));

        const double alpha = m_result.alphaDeg[m_currentAlphaIndex];
        if (m_currentAlphaText)
            m_currentAlphaText->SetLabel(wxString::Format(WXU8("Текущий α: %.1f°"), alpha));
        return;
    }

    if (m_chartPanel)
        m_chartPanel->SetCurrentAlphaIndex(m_currentAlphaIndex);

    if (m_tablePanel)
        m_tablePanel->SetCurrentAlphaIndex(m_currentAlphaIndex);

    if (m_schemePanel)
        m_schemePanel->SetAnimationAlphaDeg(m_result.alphaDeg[m_currentAlphaIndex]);

    if (m_alphaSlider && static_cast<std::size_t>(m_alphaSlider->GetValue()) != m_currentAlphaIndex)
        m_alphaSlider->SetValue(static_cast<int>(m_currentAlphaIndex));

    UpdateAnimationUi();
}

void KinematicResultPage::UpdateAnimationUi()
{
    double alpha = 0.0;
    if (!m_result.alphaDeg.empty())
        alpha = m_result.alphaDeg[std::min(m_currentAlphaIndex, m_result.alphaDeg.size() - 1)];

    if (m_currentAlphaText)
        m_currentAlphaText->SetLabel(wxString::Format(WXU8("Текущий α: %.1f°"), alpha));

    if (m_playPauseButton)
        m_playPauseButton->SetLabel(m_isPlaying ? WXU8("Pause") : WXU8("Play"));

    const bool hasData = !m_result.alphaDeg.empty();

    if (m_prevButton) m_prevButton->Enable(hasData);
    if (m_playPauseButton) m_playPauseButton->Enable(hasData);
    if (m_nextButton) m_nextButton->Enable(hasData);
    if (m_alphaSlider) m_alphaSlider->Enable(hasData);
}

void KinematicResultPage::StopAnimation()
{
    if (m_animationTimer.IsRunning())
        m_animationTimer.Stop();

    m_isPlaying = false;

    if (!m_result.alphaDeg.empty())
    {
        if (m_chartPanel)
            m_chartPanel->SetCurrentAlphaIndex(m_currentAlphaIndex);
        if (m_tablePanel)
            m_tablePanel->SetCurrentAlphaIndex(m_currentAlphaIndex);
    }

    UpdateAnimationUi();
}