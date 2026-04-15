#include "gui/dialogs/window_size_dialog.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "gui/common/text_utf8.h"

WindowSizeDialog::WindowSizeDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, WXU8("Размер окна программы"),
               wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* label = new wxStaticText(this, wxID_ANY, WXU8("Выберите размер окна:"));
    label->SetForegroundColour(wxColour(235, 235, 235));
    root->Add(label, 0, wxALL, 12);

    wxArrayString items;
    items.Add("1280 x 720");
    items.Add("1366 x 768");
    items.Add("1600 x 900");
    items.Add("1920 x 1080");
    items.Add("2560 x 1440");

    m_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxSize(220, -1), items);
    m_choice->SetSelection(2);
    m_choice->SetForegroundColour(*wxBLACK);
    m_choice->SetBackgroundColour(*wxWHITE);
    root->Add(m_choice, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* buttons = CreateSeparatedButtonSizer(wxOK | wxCANCEL);
    if (buttons)
        root->Add(buttons, 0, wxEXPAND | wxALL, 12);

    SetSizerAndFit(root);
    Centre();
}

std::optional<std::pair<int, int>> WindowSizeDialog::GetSelectedSize() const
{
    if (!m_choice || m_choice->GetSelection() == wxNOT_FOUND)
        return std::nullopt;

    const wxString value = m_choice->GetStringSelection();

    long w = 0;
    long h = 0;

    const int xPos = value.Find('x');
    if (xPos == wxNOT_FOUND)
        return std::nullopt;

    wxString left = value.Left(xPos).Trim(true).Trim(false);
    wxString right = value.Mid(xPos + 1).Trim(true).Trim(false);

    if (!left.ToLong(&w) || !right.ToLong(&h))
        return std::nullopt;

    return std::make_pair(static_cast<int>(w), static_cast<int>(h));
}