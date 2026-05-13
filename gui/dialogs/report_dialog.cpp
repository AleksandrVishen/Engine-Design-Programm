#include "gui/dialogs/report_dialog.h"

#include <array>
#include <map>
#include <unordered_set>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/checklst.h>
#include <wx/datetime.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/html/htmlwin.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>

#include "gui/common/text_utf8.h"

#include "core/balancing/balancing_pipeline.h"
#include "core/dynamic/dynamic_result.h"

namespace
{
wxString MakeDefaultFileName()
{
    return "report_" + wxDateTime::Now().FormatISODate() + ".html";
}
} // namespace

ReportDialog::ReportDialog(
    wxWindow* parent,
    const EngineModel& model,
    const engine::kinematic::KinematicResult& result,
    const engine::dynamic::DynamicResult* dynamicResult,
    const engine::balancing::BalancingPipelineResult* balancingPipelineResult)
    : wxDialog(parent,
               wxID_ANY,
               WXU8("Формирование отчета"),
               wxDefaultPosition,
               wxSize(1300, 820),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      m_model(model),
      m_result(result),
      m_dynamicResult(dynamicResult),
      m_balancingPipelineResult(balancingPipelineResult)
{
    BuildUi();
    RebuildSeriesRows();
    UpdatePreview();
}

ReportDialog::~ReportDialog()
{
    ReportBuilder::ClearReportMemoryImages();
}

void ReportDialog::BuildUi()
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    auto* content = new wxBoxSizer(wxHORIZONTAL);

    auto* leftPanel = new wxPanel(this);
    auto* leftSizer = new wxBoxSizer(wxVERTICAL);

    leftSizer->Add(new wxStaticText(leftPanel, wxID_ANY, WXU8("Цилиндры (отдельный раздел на каждый отмеченный):")),
                   0,
                   wxBOTTOM,
                   4);
    m_cylinderList = new wxCheckListBox(leftPanel, wxID_ANY);
    for (std::size_t i = 0; i < m_result.cylinders.size(); ++i)
    {
        const auto& c = m_result.cylinders[i];
        m_cylinderList->Append(wxString::Format(
            WXU8("Цилиндр %d (вал %d, кривошип %d)"),
            c.cylinderNumber,
            c.shaftNumber,
            c.crankNumber));
    }
    for (unsigned int i = 0; i < m_cylinderList->GetCount(); ++i)
        m_cylinderList->Check(i, true);
    m_cylinderList->SetMinSize(wxSize(320, 120));
    leftSizer->Add(m_cylinderList, 0, wxEXPAND | wxBOTTOM, 10);

    leftSizer->Add(new wxStaticText(leftPanel, wxID_ANY, WXU8("Данные для отчета")), 0, wxBOTTOM, 6);
    m_seriesPanel = new wxScrolledWindow(leftPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_seriesPanel->SetScrollRate(0, 12);
    leftSizer->Add(m_seriesPanel, 1, wxEXPAND | wxBOTTOM, 10);

    m_includeCombinedTableCheck = new wxCheckBox(leftPanel, wxID_ANY, WXU8("Добавить таблицу результатов"));
    m_includeCombinedTableCheck->SetValue(false);
    leftSizer->Add(m_includeCombinedTableCheck, 0, wxBOTTOM, 6);

    leftSizer->Add(new wxStaticText(leftPanel, wxID_ANY, WXU8("Колонки таблицы:")), 0, wxBOTTOM, 4);
    m_combinedColumnsCheckList = new wxCheckListBox(leftPanel, wxID_ANY);
    leftSizer->Add(m_combinedColumnsCheckList, 0, wxEXPAND | wxBOTTOM, 8);

    leftSizer->Add(new wxStaticText(leftPanel, wxID_ANY, WXU8("Шаг строк таблицы:")), 0, wxBOTTOM, 4);
    m_strideSpin = new wxSpinCtrl(leftPanel, wxID_ANY);
    m_strideSpin->SetRange(1, 200);
    m_strideSpin->SetValue(5);
    leftSizer->Add(m_strideSpin, 0, wxEXPAND | wxBOTTOM, 8);

    leftSizer->Add(new wxStaticText(leftPanel, wxID_ANY, WXU8("Дополнительные разделы")), 0, wxBOTTOM, 4);
    m_includeDynamicForcesCheck = new wxCheckBox(
        leftPanel,
        wxID_ANY,
        WXU8("Силы и моменты по динамике (таблицы Fx, Fy, Fz)"));
    m_includeBalancingCheck = new wxCheckBox(
        leftPanel,
        wxID_ANY,
        WXU8("Уравновешивание (исходные, остаточные и итоговые величины)"));
    leftSizer->Add(m_includeDynamicForcesCheck, 0, wxEXPAND | wxBOTTOM, 4);
    leftSizer->Add(m_includeBalancingCheck, 0, wxEXPAND | wxBOTTOM, 4);

    if (m_dynamicResult != nullptr)
    {
        m_includeDynamicForcesCheck->SetValue(true);
        m_includeDynamicForcesCheck->Enable(true);
    }
    else
    {
        m_includeDynamicForcesCheck->SetValue(false);
        m_includeDynamicForcesCheck->Enable(false);
    }

    if (m_balancingPipelineResult != nullptr)
    {
        m_includeBalancingCheck->SetValue(true);
        m_includeBalancingCheck->Enable(true);
    }
    else
    {
        m_includeBalancingCheck->SetValue(false);
        m_includeBalancingCheck->Enable(false);
    }

    leftPanel->SetSizer(leftSizer);

    auto* rightPanel = new wxPanel(this);
    auto* rightSizer = new wxBoxSizer(wxVERTICAL);
    rightSizer->Add(new wxStaticText(rightPanel, wxID_ANY, WXU8("Предпросмотр отчета")), 0, wxBOTTOM, 6);
    m_preview = new wxHtmlWindow(rightPanel, wxID_ANY);
    rightSizer->Add(m_preview, 1, wxEXPAND);
    rightPanel->SetSizer(rightSizer);

    content->Add(leftPanel, 0, wxEXPAND | wxALL, 10);
    content->Add(rightPanel, 1, wxEXPAND | wxTOP | wxRIGHT | wxBOTTOM, 10);
    root->Add(content, 1, wxEXPAND);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* refreshButton = new wxButton(this, wxID_ANY, WXU8("Обновить предпросмотр"));
    auto* saveButton = new wxButton(this, wxID_ANY, WXU8("Сохранить"));
    auto* closeButton = new wxButton(this, wxID_CLOSE, WXU8("Закрыть"));
    buttons->Add(refreshButton, 0, wxRIGHT, 8);
    buttons->Add(saveButton, 0, wxRIGHT, 8);
    buttons->Add(closeButton, 0);
    root->Add(buttons, 0, wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM, 10);

    SetSizer(root);
    Layout();

    m_cylinderList->Bind(wxEVT_CHECKLISTBOX, &ReportDialog::OnCylinderChanged, this);
    m_includeCombinedTableCheck->Bind(wxEVT_CHECKBOX, &ReportDialog::OnRefreshPreview, this);
    m_combinedColumnsCheckList->Bind(wxEVT_CHECKLISTBOX, &ReportDialog::OnRefreshPreview, this);
    m_strideSpin->Bind(wxEVT_SPINCTRL, &ReportDialog::OnRefreshPreview, this);
    m_includeDynamicForcesCheck->Bind(wxEVT_CHECKBOX, &ReportDialog::OnRefreshPreview, this);
    m_includeBalancingCheck->Bind(wxEVT_CHECKBOX, &ReportDialog::OnRefreshPreview, this);
    refreshButton->Bind(wxEVT_BUTTON, &ReportDialog::OnRefreshPreview, this);
    saveButton->Bind(wxEVT_BUTTON, &ReportDialog::OnSaveReport, this);
    closeButton->Bind(wxEVT_BUTTON, &ReportDialog::OnCloseDialog, this);
}

void ReportDialog::RebuildSeriesRows()
{
    std::map<std::string, std::array<bool, 4>> previousRowChecks;
    for (const auto& row : m_seriesRows)
    {
        previousRowChecks[row.seriesId] = {
            row.includeSeries->IsChecked(),
            row.includeChart->IsChecked(),
            row.includeStats->IsChecked(),
            row.includeTable->IsChecked(),
        };
    }

    std::unordered_set<std::string> combinedColumnCheckedIds;
    for (unsigned int i = 0; i < m_combinedColumnsCheckList->GetCount(); ++i)
    {
        if (m_combinedColumnsCheckList->IsChecked(i) && i < m_descriptors.size())
            combinedColumnCheckedIds.insert(m_descriptors[i].id);
    }

    m_seriesRows.clear();
    m_combinedColumnsCheckList->Clear();

    if (m_result.cylinders.empty())
        return;

    int refCylinderIndex = 0;
    for (unsigned int i = 0; i < m_cylinderList->GetCount(); ++i)
    {
        if (m_cylinderList->IsChecked(i))
        {
            refCylinderIndex = static_cast<int>(i);
            break;
        }
    }

    m_descriptors = ReportBuilder::BuildSeriesDescriptors(
        m_result.cylinders[static_cast<std::size_t>(refCylinderIndex)]);

    auto* panelSizer = new wxBoxSizer(wxVERTICAL);
    for (const auto& descriptor : m_descriptors)
    {
        bool incSeries = true;
        bool incChart = true;
        bool incStats = true;
        bool incTable = false;
        const auto prevIt = previousRowChecks.find(descriptor.id);
        if (prevIt != previousRowChecks.end())
        {
            incSeries = (*prevIt).second[0];
            incChart = (*prevIt).second[1];
            incStats = (*prevIt).second[2];
            incTable = (*prevIt).second[3];
        }

        auto* rowPanel = new wxPanel(m_seriesPanel);
        auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* includeSeries = new wxCheckBox(rowPanel, wxID_ANY, descriptor.displayName);
        includeSeries->SetValue(incSeries);
        auto* chartCheck = new wxCheckBox(rowPanel, wxID_ANY, WXU8("График"));
        chartCheck->SetValue(incChart);
        auto* statsCheck = new wxCheckBox(rowPanel, wxID_ANY, WXU8("Max/Min"));
        statsCheck->SetValue(incStats);
        auto* tableCheck = new wxCheckBox(rowPanel, wxID_ANY, WXU8("Таблица"));
        tableCheck->SetValue(incTable);

        rowSizer->Add(includeSeries, 1, wxRIGHT, 6);
        rowSizer->Add(chartCheck, 0, wxRIGHT, 6);
        rowSizer->Add(statsCheck, 0, wxRIGHT, 6);
        rowSizer->Add(tableCheck, 0);
        rowPanel->SetSizer(rowSizer);

        panelSizer->Add(rowPanel, 0, wxEXPAND | wxBOTTOM, 4);
        m_seriesRows.push_back({ descriptor.id, includeSeries, chartCheck, statsCheck, tableCheck });

        m_combinedColumnsCheckList->Append(descriptor.displayName);
    }

    for (unsigned int i = 0; i < m_combinedColumnsCheckList->GetCount(); ++i)
    {
        if (i < m_descriptors.size() && combinedColumnCheckedIds.count(m_descriptors[i].id) != 0)
            m_combinedColumnsCheckList->Check(i, true);
    }

    m_seriesPanel->SetSizer(panelSizer);
    m_seriesPanel->FitInside();
    m_seriesPanel->Layout();

    for (auto& row : m_seriesRows)
    {
        row.includeSeries->Bind(wxEVT_CHECKBOX, &ReportDialog::OnRefreshPreview, this);
        row.includeChart->Bind(wxEVT_CHECKBOX, &ReportDialog::OnRefreshPreview, this);
        row.includeStats->Bind(wxEVT_CHECKBOX, &ReportDialog::OnRefreshPreview, this);
        row.includeTable->Bind(wxEVT_CHECKBOX, &ReportDialog::OnRefreshPreview, this);
    }
}

ReportOptions ReportDialog::CollectOptions() const
{
    ReportOptions options;
    for (unsigned int i = 0; i < m_cylinderList->GetCount(); ++i)
    {
        if (m_cylinderList->IsChecked(i))
            options.selectedCylinderIndices.push_back(static_cast<int>(i));
    }

    options.includeCombinedTable = m_includeCombinedTableCheck->IsChecked();
    options.tableStride = m_strideSpin->GetValue();

    for (const auto& row : m_seriesRows)
    {
        ReportSeriesOptions seriesOption;
        seriesOption.seriesId = row.seriesId;
        seriesOption.includeSeries = row.includeSeries->IsChecked();
        seriesOption.includeChart = row.includeChart->IsChecked();
        seriesOption.includeStats = row.includeStats->IsChecked();
        seriesOption.includeTable = row.includeTable->IsChecked();
        options.seriesOptions.push_back(seriesOption);
    }

    for (std::size_t i = 0; i < m_descriptors.size(); ++i)
    {
        if (m_combinedColumnsCheckList->IsChecked(static_cast<unsigned int>(i)))
            options.combinedTableSeriesIds.push_back(m_descriptors[i].id);
    }

    options.includeDynamicForces =
        m_includeDynamicForcesCheck->IsEnabled() && m_includeDynamicForcesCheck->IsChecked();
    options.includeBalancing = m_includeBalancingCheck->IsEnabled() && m_includeBalancingCheck->IsChecked();

    return options;
}

void ReportDialog::UpdatePreview()
{
    if (!m_preview)
        return;

    const ReportBuildResult result = ReportBuilder::BuildHtml(
        m_model,
        m_result,
        CollectOptions(),
        ReportHtmlImageMode::WxMemoryFilesystemForHtmlWindow,
        m_dynamicResult,
        m_balancingPipelineResult);
    m_preview->SetPage(result.html);
}

void ReportDialog::OnSaveReport(wxCommandEvent&)
{
    const ReportOptions options = CollectOptions();
    if (options.selectedCylinderIndices.empty())
    {
        wxMessageBox(WXU8("Отметьте хотя бы один цилиндр для сохранения отчета."),
                     WXU8("Нет выбора"),
                     wxOK | wxICON_WARNING,
                     this);
        return;
    }

    wxFileDialog saveDialog(
        this,
        WXU8("Сохранить отчет"),
        wxEmptyString,
        MakeDefaultFileName(),
        WXU8("HTML (*.html)|*.html|Текст (*.txt)|*.txt|PDF (*.pdf)|*.pdf"),
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    if (saveDialog.ShowModal() != wxID_OK)
        return;

    wxFileName fileName(saveDialog.GetPath());
    wxString ext = fileName.GetExt().Lower();
    if (ext.empty())
    {
        const int filterIndex = saveDialog.GetFilterIndex();
        if (filterIndex == 1)
            ext = "txt";
        else if (filterIndex == 2)
            ext = "pdf";
        else
            ext = "html";
        fileName.SetExt(ext);
    }

    const wxString path = fileName.GetFullPath();

    if (ext == "pdf")
    {
        const ReportBuildResult htmlResult = ReportBuilder::BuildHtml(
            m_model,
            m_result,
            options,
            ReportHtmlImageMode::WxMemoryFilesystemForHtmlWindow,
            m_dynamicResult,
            m_balancingPipelineResult);
        bool savedToPath = false;
        if (!ReportBuilder::ExportHtmlToPdf(this, htmlResult.html, path, &savedToPath))
        {
            UpdatePreview();
            wxMessageBox(WXU8("Не удалось выполнить экспорт в PDF."),
                         WXU8("Ошибка"),
                         wxOK | wxICON_ERROR,
                         this);
            return;
        }
        if (!savedToPath)
        {
            UpdatePreview();
            wxMessageBox(
                WXU8("Открыт диалог печати. Для сохранения в PDF выберите принтер «Microsoft Print to PDF» "
                     "(или аналог) и укажите файл."),
                WXU8("PDF"),
                wxOK | wxICON_INFORMATION,
                this);
            return;
        }
        UpdatePreview();
    }
    else if (ext == "txt")
    {
        const ReportBuildResult textResult = ReportBuilder::BuildPlainText(
            m_model,
            m_result,
            options,
            m_dynamicResult,
            m_balancingPipelineResult);
        wxFile file(path, wxFile::write);
        if (!file.IsOpened())
        {
            wxMessageBox(WXU8("Не удалось открыть файл для сохранения."),
                         WXU8("Ошибка"),
                         wxOK | wxICON_ERROR,
                         this);
            return;
        }
        const wxScopedCharBuffer utf8 = textResult.plainText.ToUTF8();
        if (!file.Write(utf8.data(), utf8.length()))
        {
            wxMessageBox(WXU8("Не удалось записать отчет в файл."),
                         WXU8("Ошибка"),
                         wxOK | wxICON_ERROR,
                         this);
            return;
        }
    }
    else
    {
        const ReportBuildResult htmlResult = ReportBuilder::BuildHtml(
            m_model,
            m_result,
            options,
            ReportHtmlImageMode::DataUriForBrowserFile,
            m_dynamicResult,
            m_balancingPipelineResult);
        wxFile file(path, wxFile::write);
        if (!file.IsOpened())
        {
            wxMessageBox(WXU8("Не удалось открыть файл для сохранения."),
                         WXU8("Ошибка"),
                         wxOK | wxICON_ERROR,
                         this);
            return;
        }

        if (!file.Write(htmlResult.html))
        {
            wxMessageBox(WXU8("Не удалось записать отчет в файл."),
                         WXU8("Ошибка"),
                         wxOK | wxICON_ERROR,
                         this);
            return;
        }
    }

    wxMessageBox(WXU8("Отчет сохранен."), WXU8("Готово"), wxOK | wxICON_INFORMATION, this);
}

void ReportDialog::OnCloseDialog(wxCommandEvent&)
{
    EndModal(wxID_CLOSE);
}

void ReportDialog::OnRefreshPreview(wxCommandEvent&)
{
    UpdatePreview();
}

void ReportDialog::OnCylinderChanged(wxCommandEvent&)
{
    bool any = false;
    for (unsigned int i = 0; i < m_cylinderList->GetCount(); ++i)
    {
        if (m_cylinderList->IsChecked(i))
        {
            any = true;
            break;
        }
    }
    if (!any && m_cylinderList->GetCount() > 0)
        m_cylinderList->Check(0);

    RebuildSeriesRows();
    UpdatePreview();
}
