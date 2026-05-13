#include "gui/dialogs/settings_dialog.h"

#include <cmath>

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
    items.Add("0.5");
    items.Add("1.0");
    items.Add("5.0");
    items.Add("10.0");
    items.Add("45.0");

    m_alphaStepChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, items);

    static constexpr double kSteps[] = {0.5, 1.0, 5.0, 10.0, 45.0};
    int selection = 1;
    double bestDiff = 1e100;
    for (int i = 0; i < static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0])); ++i)
    {
        const double d = std::fabs(currentAlphaStep - kSteps[static_cast<std::size_t>(i)]);
        if (d < bestDiff)
        {
            bestDiff = d;
            selection = i;
        }
    }

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