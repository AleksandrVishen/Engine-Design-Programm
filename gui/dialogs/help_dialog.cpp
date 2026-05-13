#include "gui/dialogs/help_dialog.h"

#include <wx/button.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>

#include "gui/common/text_utf8.h"

HelpDialog::HelpDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, WXU8("Справка"), wxDefaultPosition, wxSize(900, 700))
{
    BuildUi();
    Centre();
}

void HelpDialog::BuildUi()
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* notebook = new wxNotebook(this, wxID_ANY);

    auto* aboutPanel = new wxPanel(notebook);
    auto* keysPanel = new wxPanel(notebook);

    auto* aboutSizer = new wxBoxSizer(wxVERTICAL);
    auto* aboutText = new wxTextCtrl(
        aboutPanel,
        wxID_ANY,
        BuildAboutText(),
        wxDefaultPosition,
        wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    aboutSizer->Add(aboutText, 1, wxALL | wxEXPAND, 8);
    aboutPanel->SetSizer(aboutSizer);

    auto* keysSizer = new wxBoxSizer(wxVERTICAL);
    auto* keysText = new wxTextCtrl(
        keysPanel,
        wxID_ANY,
        BuildHotkeysText(),
        wxDefaultPosition,
        wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    keysSizer->Add(keysText, 1, wxALL | wxEXPAND, 8);
    keysPanel->SetSizer(keysSizer);

    notebook->AddPage(aboutPanel, WXU8("О программе"), true);
    notebook->AddPage(keysPanel, WXU8("Горячие клавиши"), false);

    root->Add(notebook, 1, wxALL | wxEXPAND, 10);
    root->Add(CreateSeparatedButtonSizer(wxOK), 0, wxALL | wxEXPAND, 10);

    SetSizer(root);
}

wxString HelpDialog::BuildAboutText() const
{
    wxString text;
    text += WXU8("Программа предназначена для расчёта и визуализации кривошипно-шатунных механизмов двигателей.\n\n");

    text += WXU8("Текущая версия программы позволяет:\n");
    text += WXU8("- задавать число коленчатых валов;\n");
    text += WXU8("- задавать число кривошипов на каждый вал;\n");
    text += WXU8("- выбирать тактность двигателя;\n");
    text += WXU8("- задавать число цилиндров на шатунную шейку;\n");
    text += WXU8("- выбирать тип сочленения шатунов: рядом сидящие или прицепной шатун;\n");
    text += WXU8("- выбирать тип опор: полноопорный или неполноопорный;\n");
    text += WXU8("- задавать координаты начала коленчатых валов;\n");
    text += WXU8("- задавать геометрические фазы кривошипов;\n");
    text += WXU8("- задавать поворот осей цилиндров;\n");
    text += WXU8("- задавать кинематические параметры механизма;\n");
    text += WXU8("- выполнять расчёт кинематики;\n");
    text += WXU8("- просматривать результаты кинематики в виде графиков, таблиц и анимации;\n");
    text += WXU8("- задавать массовые характеристики;\n");
    text += WXU8("- выполнять расчёт динамики;\n");
    text += WXU8("- просматривать результаты динамики в виде графиков, таблиц и анимации.\n\n");

    text += WXU8("Используемые кинематические параметры:\n");
    text += WXU8("- частота вращения, об/мин;\n");
    text += WXU8("- дезаксиал e, мм;\n");
    text += WXU8("- радиус кривошипа r, м;\n");
    text += WXU8("- λ;\n");
    text += WXU8("- длина коренной шейки, м;\n");
    text += WXU8("- длина шатунной шейки, м;\n");
    text += WXU8("- толщина щеки, м.\n\n");

    text += WXU8("Результаты кинематики включают:\n");
    text += WXU8("- перемещение поршня;\n");
    text += WXU8("- скорость;\n");
    text += WXU8("- ускорение;\n");
    text += WXU8("- ускорение 1-го порядка;\n");
    text += WXU8("- ускорение 2-го порядка;\n");
    text += WXU8("- угол шатуна;\n");
    text += WXU8("- угловую скорость шатуна;\n");
    text += WXU8("- угловое ускорение шатуна.\n\n");

    text += WXU8("Результаты динамики включают:\n");
    text += WXU8("- силу инерции F;\n");
    text += WXU8("- силу инерции 1-го порядка F1;\n");
    text += WXU8("- силу инерции 2-го порядка F2;\n");
    text += WXU8("- момент от сил 1-го порядка M1;\n");
    text += WXU8("- момент от сил 2-го порядка M2.\n\n");

    text += WXU8("Программа предназначена для инженерного анализа геометрии, кинематики и базовой динамики КШМ с визуальным контролем схемы и результатов расчёта.");
    return text;
}

wxString HelpDialog::BuildHotkeysText() const
{
    wxString text;
    text += WXU8("Файл (меню «Файл»):\n");
    text += WXU8("  Ctrl+O — открыть проект\n");
    text += WXU8("  Ctrl+S — сохранить проект\n");
    text += WXU8("  Ctrl+Shift+S — сохранить проект как…\n\n");

    text += WXU8("Отчёт:\n");
    text += WXU8("  Ctrl+R — открыть окно формирования отчёта\n\n");

    text += WXU8("Навигация по разделам:\n");
    text += WXU8("  Esc — перейти к предыдущему разделу в списке слева (на первом разделе действия нет).\n");
    text += WXU8("  В многострочном поле ввода Esc оставляется полю.\n\n");

    text += WXU8("Запуск расчётов (Enter):\n");
    text += WXU8("  Enter — то же, что основная кнопка расчёта на текущей странице:\n");
    text += WXU8("    • «Геометрия коленчатого вала» — расчёт кинематики;\n");
    text += WXU8("    • «Массовые характеристики» — расчёт динамики;\n");
    text += WXU8("    • «Установка противовесов» — расчёт уравновешивания (кнопка «Расчёт»).\n");
    text += WXU8("  Если фокус на обычной кнопке, Enter обрабатывает эта кнопка.\n");
    text += WXU8("  В таблице (сетке) и в многострочном тексте Enter не перехватывается.\n\n");

    text += WXU8("Установка противовесов:\n");
    text += WXU8("  Shift+Enter — запуск мастера «Уравновесить» (выбор цели и варианта).\n\n");

    text += WXU8("Схема коленчатого вала (панель схемы):\n");
    text += WXU8("  колесо мыши — приблизить / отдалить;\n");
    text += WXU8("  ЛКМ + перетаскивание — сдвиг схемы;\n");
    text += WXU8("  ПКМ + перетаскивание — поворот схемы;\n");
    text += WXU8("  ЛКМ и ПКМ одновременно + перетаскивание — поворот;\n");
    text += WXU8("  средняя кнопка мыши — сброс вида.\n");

    return text;
}
