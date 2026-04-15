#pragma once

#include <wx/dialog.h>

class wxChoice;

class SettingsDialog : public wxDialog
{
public:
    SettingsDialog(wxWindow* parent, double currentAlphaStep);

    double GetSelectedAlphaStep() const;

private:
    void BuildUi(double currentAlphaStep);

private:
    wxChoice* m_alphaStepChoice = nullptr;
};