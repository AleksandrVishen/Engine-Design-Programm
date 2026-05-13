#pragma once

#include <wx/menu.h>

enum
{
    ID_MENU_FILE_OPEN = wxID_HIGHEST + 1,
    ID_MENU_FILE_SAVE,
    ID_MENU_FILE_SAVE_AS,
    ID_MENU_SETTINGS,
    ID_MENU_HELP,
    ID_MENU_REPORT_OPEN
};

class MainMenuBuilder
{
public:
    static wxMenuBar* Build();
};