#pragma once

#include <optional>
#include <utility>
#include <wx/dialog.h>

class wxChoice;

class WindowSizeDialog : public wxDialog
{
public:
    explicit WindowSizeDialog(wxWindow* parent);

    std::optional<std::pair<int, int>> GetSelectedSize() const;

private:
    wxChoice* m_choice = nullptr;
};