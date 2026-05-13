#include "gui/frame/main_menu_builder.h"

#include "gui/common/text_utf8.h"

wxMenuBar* MainMenuBuilder::Build()
{
    auto* menuBar = new wxMenuBar();

    auto* fileMenu = new wxMenu();
    fileMenu->Append(
        ID_MENU_FILE_OPEN,
        WXU8("Открыть проект...\tCtrl+O"),
        WXU8("Открыть сохранённый проект (Ctrl+O)"));
    fileMenu->AppendSeparator();
    fileMenu->Append(
        ID_MENU_FILE_SAVE,
        WXU8("Сохранить\tCtrl+S"),
        WXU8("Сохранить текущий проект (Ctrl+S)"));
    fileMenu->Append(
        ID_MENU_FILE_SAVE_AS,
        WXU8("Сохранить как...\tCtrl+Shift+S"),
        WXU8("Сохранить проект под новым именем (Ctrl+Shift+S)"));

    auto* settingsMenu = new wxMenu();
    settingsMenu->Append(ID_MENU_SETTINGS, WXU8("Шаг угла альфа..."));

    auto* helpMenu = new wxMenu();
    helpMenu->Append(ID_MENU_HELP, WXU8("Справка..."));

    auto* reportMenu = new wxMenu();
    reportMenu->Append(ID_MENU_REPORT_OPEN,
                       WXU8("Сформировать отчет...\tCtrl+R"),
                       WXU8("Открыть окно формирования отчета"));

    menuBar->Append(fileMenu, WXU8("Файл"));
    menuBar->Append(settingsMenu, WXU8("Настройки"));
    menuBar->Append(reportMenu, WXU8("Отчет"));
    menuBar->Append(helpMenu, WXU8("Справка"));

    return menuBar;
}