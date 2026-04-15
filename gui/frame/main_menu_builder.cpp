#include "gui/frame/main_menu_builder.h"

#include "gui/common/text_utf8.h"

wxMenuBar* MainMenuBuilder::Build()
{
    auto* menuBar = new wxMenuBar();

    auto* fileMenu = new wxMenu();
    fileMenu->Append(ID_MENU_FILE_STUB, WXU8("Файл"));

    auto* settingsMenu = new wxMenu();
    settingsMenu->Append(ID_MENU_SETTINGS, WXU8("Шаг угла альфа..."));
    settingsMenu->AppendSeparator();
    settingsMenu->Append(ID_MENU_SETTINGS_WINDOW_SIZE, WXU8("Размер окна программы..."));

    auto* helpMenu = new wxMenu();
    helpMenu->Append(ID_MENU_HELP, WXU8("О программе "));

    menuBar->Append(fileMenu, WXU8("Файл"));
    menuBar->Append(settingsMenu, WXU8("Настройки"));
    menuBar->Append(helpMenu, WXU8("Справка"));

    return menuBar;
}