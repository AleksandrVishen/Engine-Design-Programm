#include "gui/pages/counterweight_setup_page.h"

#include <sstream>
#include <vector>

#include <wx/button.h>
#include <wx/busyinfo.h>
#include <wx/choicdlg.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

#include "gui/common/text_utf8.h"
#include "gui/widgets/counterweight_input_panel.h"
#include "gui/widgets/engine_scheme_panel.h"
#include "core/balancing/balancing_synthesizer.h"

namespace
{

wxString GoalToLabel(engine::balancing::BalancingSynthesisGoalKind goal)
{
    using engine::balancing::BalancingSynthesisGoalKind;

    switch (goal)
    {
    case BalancingSynthesisGoalKind::CentrifugalForce: return WXU8("Уравновесить Fc");
    case BalancingSynthesisGoalKind::CentrifugalMoment: return WXU8("Уравновесить Mc");
    case BalancingSynthesisGoalKind::InertiaForceFirstOrder: return WXU8("Уравновесить F1");
    case BalancingSynthesisGoalKind::InertiaForceSecondOrder: return WXU8("Уравновесить F2");
    case BalancingSynthesisGoalKind::InertiaMomentFirstOrder: return WXU8("Уравновесить M1");
    case BalancingSynthesisGoalKind::InertiaMomentSecondOrder: return WXU8("Уравновесить M2");
    case BalancingSynthesisGoalKind::InertiaForceTotal: return WXU8("Уравновесить F = F1 + F2");
    case BalancingSynthesisGoalKind::InertiaMomentTotal: return WXU8("Уравновесить M = M1 + M2");
    case BalancingSynthesisGoalKind::Combined:
    default: return WXU8("Комплексное уравновешивание");
    }
}

class BalancingVariantDialog final : public wxDialog
{
public:
    BalancingVariantDialog(wxWindow* parent,
                           const wxArrayString& titles,
                           const wxArrayString& descriptions)
        : wxDialog(parent,
                   wxID_ANY,
                   WXU8("Варианты уравновешивания"),
                   wxDefaultPosition,
                   wxSize(1400, 700),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_titles(titles),
          m_descriptions(descriptions)
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        auto* topLabel = new wxStaticText(
            this,
            wxID_ANY,
            WXU8("Выберите вариант. Слева — краткий список, справа — остаточные силы, моменты и состав схемы."));
        root->Add(topLabel, 0, wxALL, 10);

        auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* leftSizer = new wxBoxSizer(wxVERTICAL);
        auto* leftLabel = new wxStaticText(this, wxID_ANY, WXU8("Варианты"));
        leftSizer->Add(leftLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

        m_listBox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(520, 480), titles);
        leftSizer->Add(m_listBox, 1, wxEXPAND);

        contentSizer->Add(leftSizer, 0, wxEXPAND | wxALL, 10);

        auto* rightSizer = new wxBoxSizer(wxVERTICAL);
        auto* rightLabel = new wxStaticText(this, wxID_ANY, WXU8("Параметры выбранного варианта"));
        rightSizer->Add(rightLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

        m_detailsCtrl = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(760, 480),
            wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2 | wxHSCROLL);

        rightSizer->Add(m_detailsCtrl, 1, wxEXPAND);

        contentSizer->Add(rightSizer, 1, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 10);

        root->Add(contentSizer, 1, wxEXPAND);

        auto* buttonSizer = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
        if (buttonSizer)
            root->Add(buttonSizer, 0, wxALL | wxEXPAND, 10);

        SetSizer(root);
        Layout();
        CentreOnParent();

        if (!m_titles.empty())
        {
            m_listBox->SetSelection(0);
            m_selection = 0;
            UpdateDetails(0);
        }

        m_listBox->Bind(wxEVT_LISTBOX, &BalancingVariantDialog::OnSelectionChanged, this);
        m_listBox->Bind(wxEVT_LISTBOX_DCLICK, &BalancingVariantDialog::OnItemActivated, this);
    }

    int GetSelection() const
    {
        return m_selection;
    }

private:
    void UpdateDetails(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_titles.size()))
        {
            m_detailsCtrl->Clear();
            return;
        }

        m_detailsCtrl->SetValue(m_descriptions[static_cast<std::size_t>(index)]);
        m_detailsCtrl->ShowPosition(0);
    }

    void OnSelectionChanged(wxCommandEvent& event)
    {
        m_selection = event.GetSelection();
        UpdateDetails(m_selection);
    }

    void OnItemActivated(wxCommandEvent& event)
    {
        m_selection = event.GetSelection();
        UpdateDetails(m_selection);
        EndModal(wxID_OK);
    }

private:
    wxListBox* m_listBox = nullptr;
    wxTextCtrl* m_detailsCtrl = nullptr;

    wxArrayString m_titles;
    wxArrayString m_descriptions;

    int m_selection = 0;
};

} // namespace

CounterweightSetupPage::CounterweightSetupPage(wxWindow* parent)
    : wxPanel(parent)
{
    BuildUi();
    BindEvents();
}

void CounterweightSetupPage::SetModel(const EngineModel& model)
{
    m_model = model;

    if (m_inputPanel)
        m_inputPanel->SetModel(model);

    ApplyPreviewModel(model);
    Layout();
    if (m_inputScroll)
        m_inputScroll->FitInside();
}

std::optional<EngineModel> CounterweightSetupPage::GetUpdatedModel() const
{
    if (!m_inputPanel || !m_inputPanel->HasModel())
        return std::nullopt;

    return m_inputPanel->BuildUpdatedModel();
}

void CounterweightSetupPage::SetOnInputChanged(std::function<void()> handler)
{
    m_onInputChanged = std::move(handler);
}

void CounterweightSetupPage::SetOnCalculateRequested(std::function<void(const EngineModel&)> handler)
{
    m_onCalculateRequested = std::move(handler);
}

void CounterweightSetupPage::SetOnAutobalanceRequested(
    std::function<engine::balancing::BalancingSynthesisResult(
        const EngineModel&,
        engine::balancing::BalancingSynthesisGoalKind)> handler)
{
    m_onAutobalanceRequested = std::move(handler);
}

void CounterweightSetupPage::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, WXU8("Установка противовесов"));
    auto titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 6);
    titleFont.SetWeight(wxFONTWEIGHT_NORMAL);
    title->SetFont(titleFont);
    title->SetForegroundColour(wxColour(245, 245, 245));

    root->Add(title, 0, wxLEFT | wxTOP | wxBOTTOM, 12);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 12);

    auto* contentSplitter = new wxSplitterWindow(
        this,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxSP_LIVE_UPDATE | wxSP_3D);
    contentSplitter->SetBackgroundColour(GetBackgroundColour());
    contentSplitter->SetMinimumPaneSize(360);
    contentSplitter->SetSashGravity(0.55);

    m_inputScroll = new wxScrolledWindow(
        contentSplitter,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxVSCROLL);
    m_inputScroll->SetScrollRate(10, 10);
    m_inputScroll->SetBackgroundColour(GetBackgroundColour());

    m_inputPanel = new CounterweightInputPanel(m_inputScroll);

    auto* scrollSizer = new wxBoxSizer(wxVERTICAL);
    scrollSizer->Add(m_inputPanel, 1, wxEXPAND);
    m_inputScroll->SetSizer(scrollSizer);

    auto* rightPane = new wxPanel(contentSplitter);
    rightPane->SetBackgroundColour(GetBackgroundColour());

    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    auto* schemeTitle = new wxStaticText(rightPane, wxID_ANY, WXU8("Схема двигателя и противовесов"));
    auto schemeTitleFont = schemeTitle->GetFont();
    schemeTitleFont.SetPointSize(schemeTitleFont.GetPointSize() + 5);
    schemeTitle->SetFont(schemeTitleFont);
    schemeTitle->SetForegroundColour(wxColour(245, 245, 245));

    rightSizer->Add(schemeTitle, 0, wxLEFT | wxBOTTOM, 6);

    m_schemePanel = new EngineSchemePanel(rightPane);
    rightSizer->Add(m_schemePanel, 1, wxEXPAND);

    rightPane->SetSizer(rightSizer);

    contentSplitter->SplitVertically(m_inputScroll, rightPane);
    contentSplitter->SetSashPosition(620);

    root->Add(contentSplitter, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    bottomSizer->AddStretchSpacer(1);

    m_autobalanceButton = new wxButton(this, wxID_ANY, WXU8("Уравновесить"));
    m_autobalanceButton->SetMinSize(wxSize(180, 36));
    bottomSizer->Add(m_autobalanceButton, 0, wxRIGHT | wxBOTTOM, 10);

    m_calculateButton = new wxButton(this, wxID_ANY, WXU8("Расчёт"));
    m_calculateButton->SetMinSize(wxSize(160, 36));
    bottomSizer->Add(m_calculateButton, 0, wxRIGHT | wxBOTTOM, 12);

    root->Add(bottomSizer, 0, wxEXPAND);

    SetSizer(root);
    Layout();
    m_inputScroll->FitInside();
}

void CounterweightSetupPage::BindEvents()
{
    if (!m_inputPanel)
        return;

    m_inputPanel->SetOnDataChanged([this](const EngineModel& updatedModel)
    {
        m_model = updatedModel;
        ApplyPreviewModel(updatedModel);

        if (m_onInputChanged)
            m_onInputChanged();

        Layout();
        if (m_inputScroll)
            m_inputScroll->FitInside();
    });

    if (m_calculateButton)
        m_calculateButton->Bind(wxEVT_BUTTON, &CounterweightSetupPage::OnCalculate, this);

    if (m_autobalanceButton)
        m_autobalanceButton->Bind(wxEVT_BUTTON, &CounterweightSetupPage::OnAutobalance, this);
}

void CounterweightSetupPage::ApplyPreviewModel(const EngineModel& model)
{
    if (m_schemePanel)
        m_schemePanel->SetModel(model);
}

void CounterweightSetupPage::OnCalculate(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (!m_inputPanel || !m_inputPanel->HasModel())
    {
        wxMessageBox(WXU8("Нет данных для расчёта уравновешивания."),
                     WXU8("Ошибка"),
                     wxOK | wxICON_WARNING,
                     this);
        return;
    }

    const EngineModel updatedModel = m_inputPanel->BuildUpdatedModel();
    m_model = updatedModel;
    ApplyPreviewModel(updatedModel);

    if (m_onCalculateRequested)
        m_onCalculateRequested(updatedModel);
}

void CounterweightSetupPage::OnAutobalance(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (!m_inputPanel || !m_inputPanel->HasModel())
    {
        wxMessageBox(WXU8("Нет данных для автоподбора."),
                     WXU8("Ошибка"),
                     wxOK | wxICON_WARNING,
                     this);
        return;
    }

    if (!m_onAutobalanceRequested)
    {
        wxMessageBox(WXU8("Обработчик автоподбора не подключён."),
                     WXU8("Ошибка"),
                     wxOK | wxICON_WARNING,
                     this);
        return;
    }

    wxArrayString goalItems;
    std::vector<engine::balancing::BalancingSynthesisGoalKind> goalValues = {
        engine::balancing::BalancingSynthesisGoalKind::CentrifugalForce,
        engine::balancing::BalancingSynthesisGoalKind::CentrifugalMoment,
        engine::balancing::BalancingSynthesisGoalKind::InertiaForceFirstOrder,
        engine::balancing::BalancingSynthesisGoalKind::InertiaForceSecondOrder,
        engine::balancing::BalancingSynthesisGoalKind::InertiaMomentFirstOrder,
        engine::balancing::BalancingSynthesisGoalKind::InertiaMomentSecondOrder,
        engine::balancing::BalancingSynthesisGoalKind::InertiaForceTotal,
        engine::balancing::BalancingSynthesisGoalKind::InertiaMomentTotal,
        engine::balancing::BalancingSynthesisGoalKind::Combined
    };

    for (const auto goal : goalValues)
        goalItems.Add(GoalToLabel(goal));

    wxSingleChoiceDialog goalDlg(
        this,
        WXU8("Выберите цель автоподбора уравновешивания."),
        WXU8("Автоподбор"),
        goalItems);

    if (goalDlg.ShowModal() != wxID_OK)
        return;

    const int goalIndex = goalDlg.GetSelection();
    if (goalIndex < 0 || goalIndex >= static_cast<int>(goalValues.size()))
        return;

    const EngineModel currentModel = m_inputPanel->BuildUpdatedModel();

    engine::balancing::BalancingSynthesisResult synthesisResult;
    {
        wxBusyInfoFlags flags;
        flags.Parent(this);
        flags.Title(WXU8("Идёт автоподбор уравновешивания"));
        flags.Text(WXU8("Пожалуйста, подождите..."));

        wxBusyInfo busyInfo(flags);

        if (m_autobalanceButton)
            m_autobalanceButton->Disable();
        if (m_calculateButton)
            m_calculateButton->Disable();

        wxYieldIfNeeded();
        Update();

        synthesisResult =
            m_onAutobalanceRequested(currentModel, goalValues[static_cast<std::size_t>(goalIndex)]);

        if (m_autobalanceButton)
            m_autobalanceButton->Enable();
        if (m_calculateButton)
            m_calculateButton->Enable();
    }

    if (!synthesisResult.ok || synthesisResult.candidates.empty())
    {
        std::ostringstream oss;
        oss << "Не удалось подобрать варианты.\n";
        for (const auto& error : synthesisResult.errors)
            oss << " - " << error.message << "\n";

        wxMessageBox(wxString::FromUTF8(oss.str().c_str()),
                     WXU8("Автоподбор"),
                     wxOK | wxICON_WARNING,
                     this);
        return;
    }

    wxArrayString variantTitles;
    wxArrayString variantDescriptions;

    for (const auto& candidate : synthesisResult.candidates)
    {
        variantTitles.Add(wxString::FromUTF8(candidate.title.c_str()));

        wxString details;
        if (!candidate.description.empty())
        {
            details = wxString::FromUTF8(candidate.description.c_str());
        }
        else
        {
            details = WXU8("Описание отсутствует.");
        }

        variantDescriptions.Add(details);
    }

    BalancingVariantDialog variantDlg(this, variantTitles, variantDescriptions);

    if (variantDlg.ShowModal() != wxID_OK)
        return;

    const int variantIndex = variantDlg.GetSelection();
    if (variantIndex < 0 || variantIndex >= static_cast<int>(synthesisResult.candidates.size()))
        return;

    const EngineModel selectedModel =
        synthesisResult.candidates[static_cast<std::size_t>(variantIndex)].model;

    SetModel(selectedModel);

    if (m_onInputChanged)
        m_onInputChanged();
}

void CounterweightSetupPage::RunPipelineCalculate()
{
    wxCommandEvent e(wxEVT_BUTTON);
    OnCalculate(e);
}

void CounterweightSetupPage::RunAutobalance()
{
    wxCommandEvent e(wxEVT_BUTTON);
    OnAutobalance(e);
}