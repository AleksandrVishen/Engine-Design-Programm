#include "gui/pages/input_page.h"

#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/statline.h>
#include <wx/stattext.h>

#include "core/model/engine_validation.h"
#include "gui/common/text_utf8.h"
#include "gui/widgets/engine_input_panel.h"
#include "gui/widgets/engine_scheme_panel.h"
#include "gui/widgets/kinematic_params_panel.h"
#include "core/kinematic/kinematic_model_builder.h"
#include "core/kinematic/kinematic_solver.h"

InputPage::InputPage(wxWindow* parent)
    : wxPanel(parent)
{
    BuildUi();
    BindEvents();
    UpdatePreview();
}

void InputPage::SetOnCalculationSucceeded(
    std::function<void(const EngineModel&, const engine::kinematic::KinematicResult&)> handler)
{
    m_onCalculationSucceeded = std::move(handler);
}

void InputPage::SetOnInputChanged(std::function<void()> handler)
{
    m_onInputChanged = std::move(handler);
}

void InputPage::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, WXU8("Геометрия коленчатого вала"));
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
    contentSplitter->SetMinimumPaneSize(320);
    contentSplitter->SetSashGravity(0.5);

    m_inputScroll = new wxScrolledWindow(
        contentSplitter,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxVSCROLL);
    m_inputScroll->SetScrollRate(10, 10);
    m_inputScroll->SetBackgroundColour(GetBackgroundColour());

    m_inputPanel = new EngineInputPanel(m_inputScroll);

    auto* scrollSizer = new wxBoxSizer(wxVERTICAL);
    scrollSizer->Add(m_inputPanel, 1, wxEXPAND);
    m_inputScroll->SetSizer(scrollSizer);

    auto* rightPane = new wxPanel(contentSplitter);
    rightPane->SetBackgroundColour(GetBackgroundColour());

    m_schemePanel = new EngineSchemePanel(rightPane);
    m_kinematicParamsPanel = new KinematicParamsPanel(rightPane);

    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    auto* schemeTitle = new wxStaticText(rightPane, wxID_ANY, WXU8("Схема коленчатого вала"));
    auto schemeTitleFont = schemeTitle->GetFont();
    schemeTitleFont.SetPointSize(schemeTitleFont.GetPointSize() + 5);
    schemeTitle->SetFont(schemeTitleFont);
    schemeTitle->SetForegroundColour(wxColour(245, 245, 245));

    rightSizer->Add(schemeTitle, 0, wxLEFT | wxBOTTOM, 6);
    rightSizer->Add(m_schemePanel, 1, wxEXPAND | wxBOTTOM, 14);
    rightSizer->Add(m_kinematicParamsPanel, 0, wxEXPAND);

    rightPane->SetSizer(rightSizer);

    contentSplitter->SplitVertically(m_inputScroll, rightPane);
    contentSplitter->SetSashPosition(560);

    root->Add(contentSplitter, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    SetSizer(root);
    Layout();
    m_inputScroll->FitInside();
}

void InputPage::BindEvents()
{
    m_inputPanel->SetOnDataChanged([this]()
    {
        UpdatePreview();
        if (m_onInputChanged)
            m_onInputChanged();
    });

    m_kinematicParamsPanel->SetOnDataChanged([this]()
    {
        UpdatePreview();
        if (m_onInputChanged)
            m_onInputChanged();
    });

    m_kinematicParamsPanel->SetOnCalculate([this]()
    {
        OnCalculateRequested();
    });
}

void InputPage::SetAlphaStep(double alphaStepDeg)
{
    if (m_inputPanel)
        m_inputPanel->SetAlphaStep(alphaStepDeg);

    UpdatePreview();
}

double InputPage::GetAlphaStep() const
{
    return m_inputPanel ? m_inputPanel->GetAlphaStep() : 1.0;
}

bool InputPage::BuildCurrentModel(EngineModel& model, wxString& errorText) const
{
    return BuildCalculationModel(model, errorText);
}

void InputPage::SetFromModel(const EngineModel& model)
{
    if (m_inputPanel)
    {
        m_inputPanel->SetAlphaStep(model.kinematic.alphaStepDeg);
        m_inputPanel->SetFromModel(model);
    }

    if (m_kinematicParamsPanel)
        m_kinematicParamsPanel->SetFromModel(model);

    UpdatePreview();
}

bool InputPage::BuildPreviewModel(EngineModel& model) const
{
    wxString errorText;

    if (!m_inputPanel->BuildPreviewModel(model))
        return false;

    if (!m_kinematicParamsPanel->FillModel(model, false, errorText))
        return false;

    return true;
}

bool InputPage::BuildCalculationModel(EngineModel& model, wxString& errorText) const
{
    if (!m_inputPanel->BuildCalculationModel(model, errorText))
        return false;

    if (!m_kinematicParamsPanel->FillModel(model, true, errorText))
        return false;

    return true;
}

void InputPage::UpdatePreview()
{
    EngineModel model;
    if (BuildPreviewModel(model))
        m_schemePanel->SetModel(model);
    else
        m_schemePanel->ClearModel();

    Layout();
    if (m_inputScroll)
        m_inputScroll->FitInside();
}

void InputPage::OnCalculateRequested()
{
    EngineModel model;
    wxString errorText;

    if (!BuildCalculationModel(model, errorText))
    {
        wxMessageBox(errorText, WXU8("Ошибка ввода"), wxOK | wxICON_ERROR, this);
        return;
    }

    const auto validation = EngineValidation::ValidateForCalculation(model);
    if (!validation.ok)
    {
        wxString text;
        for (const auto& error : validation.errors)
        {
            text += WXU8("- ");
            text += wxString::FromUTF8(error.text.c_str());
            text += "\n";
        }

        wxMessageBox(text, WXU8("Ошибка валидации"), wxOK | wxICON_ERROR, this);
        return;
    }

    const auto buildResult = engine::kinematic::KinematicModelBuilder::Build(model);
    if (!buildResult.ok)
    {
        wxString text;
        for (const auto& issue : buildResult.issues)
        {
            text += WXU8("- ");
            text += wxString::FromUTF8(issue.message.c_str());
            text += "\n";
        }

        wxMessageBox(text, WXU8("Ошибка построения расчетной модели"), wxOK | wxICON_ERROR, this);
        return;
    }

    const auto result = engine::kinematic::KinematicSolver::Solve(buildResult.model);

    if (m_onCalculationSucceeded)
        m_onCalculationSucceeded(model, result);
}

void InputPage::RunKinematicCalculation()
{
    OnCalculateRequested();
}
