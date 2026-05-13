#pragma once

#include <wx/dialog.h>

class HelpDialog : public wxDialog
{
public:
    explicit HelpDialog(wxWindow* parent);

private:
    void BuildUi();
    wxString BuildAboutText() const;
    wxString BuildHotkeysText() const;
};
