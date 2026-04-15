#include "gui/widgets/kinematic_params_panel.h"

#include <utility>

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "gui/common/text_utf8.h"

KinematicParamsPanel::KinematicParamsPanel(wxWindow* parent)
    : wxPanel(parent)
{
    BuildUi();
    BindEvents();
}

void KinematicParamsPanel::SetOnDataChanged(std::function<void()> handler)
{
    m_onDataChanged = std::move(handler);
}

void KinematicParamsPanel::SetOnCalculate(std::function<void()> handler)
{
    m_onCalculate = std::move(handler);
}

void KinematicParamsPanel::NotifyDataChanged()
{
    if (m_onDataChanged)
        m_onDataChanged();
}

bool KinematicParamsPanel::ReadDouble(wxTextCtrl* ctrl, double& value, bool strict, const wxString& fieldName, wxString& errorText) const
{
    if (!ctrl)
        return false;

    const wxString text = ctrl->GetValue().Trim(true).Trim(false);

    if (text.IsEmpty())
    {
        if (strict)
        {
            errorText = WXU8("Не заполнено поле: ") + fieldName;
            return false;
        }

        value = 0.0;
        return true;
    }

    if (!text.ToDouble(&value))
    {
        if (strict)
        {
            errorText = WXU8("Некорректное числовое значение в поле: ") + fieldName;
            return false;
        }

        value = 0.0;
    }

    return true;
}

bool KinematicParamsPanel::FillModel(EngineModel& model, bool strict, wxString& errorText) const
{
    if (!ReadDouble(m_rpmCtrl, model.kinematic.rpm, strict, WXU8("Частота вращения"), errorText)) return false;
    if (!ReadDouble(m_deaxialCtrl, model.kinematic.deaxialMm, strict, WXU8("Дезаксиал"), errorText)) return false;
    if (!ReadDouble(m_radiusCtrl, model.kinematic.crankRadiusM, strict, WXU8("Радиус кривошипа"), errorText)) return false;
    if (!ReadDouble(m_lambdaCtrl, model.kinematic.lambda, strict, WXU8("Лямбда"), errorText)) return false;

    if (!ReadDouble(m_mainJournalLengthCtrl, model.kinematic.mainJournalLengthM, strict, WXU8("Длина коренной шейки"), errorText)) return false;
    if (!ReadDouble(m_rodJournalLengthCtrl, model.kinematic.rodJournalLengthM, strict, WXU8("Длина шатунной шейки"), errorText)) return false;
    if (!ReadDouble(m_webThicknessCtrl, model.kinematic.webThicknessM, strict, WXU8("Толщина щеки"), errorText)) return false;

    return true;
}

void KinematicParamsPanel::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* grid = new wxFlexGridSizer(3, 4, 10, 14);
    grid->AddGrowableCol(1, 1);
    grid->AddGrowableCol(3, 1);

    m_rpmCtrl = new wxTextCtrl(this, wxID_ANY, "4800");
    m_deaxialCtrl = new wxTextCtrl(this, wxID_ANY, "0.0");
    m_radiusCtrl = new wxTextCtrl(this, wxID_ANY, "0.020");
    m_lambdaCtrl = new wxTextCtrl(this, wxID_ANY, "0.3");

    m_mainJournalLengthCtrl = new wxTextCtrl(this, wxID_ANY, "0.030");
    m_rodJournalLengthCtrl = new wxTextCtrl(this, wxID_ANY, "0.020");

    grid->Add(new wxStaticText(this, wxID_ANY, WXU8("Частота вращения, об/мин:")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(m_rpmCtrl, 1, wxEXPAND);
    grid->Add(new wxStaticText(this, wxID_ANY, WXU8("Дезаксиал e, мм:")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(m_deaxialCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, WXU8("Радиус кривошипа r, м:")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(m_radiusCtrl, 1, wxEXPAND);
    grid->Add(new wxStaticText(this, wxID_ANY, WXU8("λ:")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(m_lambdaCtrl, 1, wxEXPAND);

    grid->Add(new wxStaticText(this, wxID_ANY, WXU8("Длина коренной шейки, м:")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(m_mainJournalLengthCtrl, 1, wxEXPAND);
    grid->Add(new wxStaticText(this, wxID_ANY, WXU8("Длина шатунной шейки, м:")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(m_rodJournalLengthCtrl, 1, wxEXPAND);

    root->Add(grid, 0, wxEXPAND | wxBOTTOM, 12);

    auto* webSizer = new wxBoxSizer(wxHORIZONTAL);
    webSizer->Add(new wxStaticText(this, wxID_ANY, WXU8("Толщина щеки, м:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

    m_webThicknessCtrl = new wxTextCtrl(this, wxID_ANY, "0.010");
    webSizer->Add(m_webThicknessCtrl, 0, wxRIGHT, 20);
    webSizer->AddStretchSpacer(1);

    root->Add(webSizer, 0, wxEXPAND | wxBOTTOM, 18);

    m_calculateButton = new wxButton(this, wxID_ANY, WXU8("Рассчитать кинематику"));
    m_calculateButton->SetMinSize(wxSize(240, 44));
    root->Add(m_calculateButton, 0, wxALIGN_RIGHT);

    SetSizer(root);
}

void KinematicParamsPanel::BindEvents()
{
    m_rpmCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });
    m_deaxialCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });
    m_radiusCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });
    m_lambdaCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });

    m_mainJournalLengthCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });
    m_rodJournalLengthCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });
    m_webThicknessCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });

    m_calculateButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        if (m_onCalculate)
            m_onCalculate();
    });
}