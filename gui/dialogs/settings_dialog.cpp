#include "gui/dialogs/settings_dialog.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "gui/common/text_utf8.h"

SettingsDialog::SettingsDialog(wxWindow* parent, double currentAlphaStep)
    : wxDialog(parent, wxID_ANY, WXU8("Настройки"), wxDefaultPosition, wxDefaultSize)
{
    BuildUi(currentAlphaStep);
    Centre();
}

void SettingsDialog::BuildUi(double currentAlphaStep)
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    root->Add(new wxStaticText(this, wxID_ANY, WXU8("Шаг угла alpha, град:")), 0, wxALL, 10);

    wxArrayString items;
    items.Add("1.0");
    items.Add("0.5");
    items.Add("0.25");
    items.Add("0.1");

    m_alphaStepChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, items);

    int selection = 0;
    if (currentAlphaStep == 0.5) selection = 1;
    else if (currentAlphaStep == 0.25) selection = 2;
    else if (currentAlphaStep == 0.1) selection = 3;

    m_alphaStepChoice->SetSelection(selection);

    root->Add(m_alphaStepChoice, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);
    root->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxALL | wxEXPAND, 10);

    SetSizerAndFit(root);
}

double SettingsDialog::GetSelectedAlphaStep() const
{
    if (!m_alphaStepChoice)
        return 1.0;

    double value = 1.0;
    m_alphaStepChoice->GetStringSelection().ToDouble(&value);
    return value;
}