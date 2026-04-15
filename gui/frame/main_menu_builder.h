#pragma once

#include <wx/menu.h>

enum
{
    ID_MENU_FILE_STUB = wxID_HIGHEST + 1,
    ID_MENU_SETTINGS,
    ID_MENU_SETTINGS_WINDOW_SIZE,
    ID_MENU_HELP
};

class MainMenuBuilder
{
public:
    static wxMenuBar* Build();
};