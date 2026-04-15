#include "gui/frame/main_frame.h"

#include <algorithm>
#include <exception>
#include <sstream>

#include <wx/display.h>
#include <wx/icon.h>
#include <wx/msgdlg.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>

#include "app/resource.h"

#include "core/balancing/balancing_pipeline.h"
#include "core/dynamic/dynamic_calculator.h"
#include "core/dynamic/dynamic_input_factory.h"

#include "gui/common/text_utf8.h"
#include "gui/dialogs/help_dialog.h"
#include "gui/dialogs/settings_dialog.h"
#include "gui/dialogs/window_size_dialog.h"
#include "gui/frame/main_menu_builder.h"
#include "gui/pages/balancing_result_page.h"
#include "gui/pages/counterweight_setup_page.h"
#include "gui/pages/dynamic_result_page.h"
#include "gui/pages/input_page.h"
#include "gui/pages/kinematic_result_page.h"
#include "gui/pages/mass_properties_page.h"
#include "gui/widgets/navigation_panel.h"
#include "core/balancing/balancing_synthesizer.h"

MainFrame::MainFrame()
    : wxFrame(nullptr,
              wxID_ANY,
              WXU8("Engine Design"),
              wxDefaultPosition,
              wxDefaultSize,
              wxDEFAULT_FRAME_STYLE)
{
#ifdef _WIN32
    SetIcon(wxICON(IDI_APP_ICON));
#endif

    BuildUi();
    BindEvents();
    SetMinSize(wxSize(1200, 760));
    ApplySafeWindowBounds(wxSize(1720, 920));
}

void MainFrame::ApplySafeWindowBounds(const wxSize& desiredSize)
{
    wxRect clientArea;

    const int displayIndex = wxDisplay::GetFromWindow(this);
    if (displayIndex != wxNOT_FOUND)
    {
        wxDisplay display(static_cast<unsigned int>(displayIndex));
        clientArea = display.GetClientArea();
    }
    else if (wxDisplay::GetCount() > 0)
    {
        wxDisplay display(static_cast<unsigned int>(0));
        clientArea = display.GetClientArea();
    }
    else
    {
        clientArea = wxGetClientDisplayRect();
    }

    clientArea.Deflate(8, 8);

    const int width = std::min(desiredSize.GetWidth(), clientArea.GetWidth());
    const int height = std::min(desiredSize.GetHeight(), clientArea.GetHeight());

    const int x = clientArea.GetX() + (clientArea.GetWidth() - width) / 2;
    const int y = clientArea.GetY() + (clientArea.GetHeight() - height) / 2;

    SetSize(x, y, width, height);
    Layout();
}

void MainFrame::BuildUi()
{
    SetMenuBar(MainMenuBuilder::Build());
    SetBackgroundColour(wxColour(12, 18, 28));

    auto* root = new wxBoxSizer(wxHORIZONTAL);

    m_navigationPanel = new NavigationPanel(this);
    m_navigationPanel->SetMinSize(wxSize(245, -1));

    m_book = new wxSimplebook(this, wxID_ANY);
    m_book->SetBackgroundColour(wxColour(12, 18, 28));

    m_inputPage = new InputPage(m_book);
    m_inputPage->SetAlphaStep(m_alphaStepDeg);

    m_resultPage = new KinematicResultPage(m_book);
    m_massPage = new MassPropertiesPage(m_book);
    m_dynamicResultPage = new DynamicResultPage(m_book);
    m_counterweightSetupPage = new CounterweightSetupPage(m_book);
    m_balancingResultPage = new BalancingResultPage(m_book);

    m_book->AddPage(m_inputPage, "Geometry", true);
    m_book->AddPage(m_resultPage, "Results", false);
    m_book->AddPage(m_massPage, "Mass", false);
    m_book->AddPage(m_dynamicResultPage, "DynamicResults", false);
    m_book->AddPage(m_counterweightSetupPage, "CounterweightSetup", false);
    m_book->AddPage(m_balancingResultPage, "BalancingResults", false);

    root->Add(m_navigationPanel, 0, wxEXPAND);
    root->Add(m_book, 1, wxEXPAND);

    SetSizer(root);
    Layout();

    m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::Geometry);
}

void MainFrame::BindEvents()
{
    Bind(wxEVT_MENU, &MainFrame::OnFileStub, this, ID_MENU_FILE_STUB);
    Bind(wxEVT_MENU, &MainFrame::OnOpenSettings, this, ID_MENU_SETTINGS);
    Bind(wxEVT_MENU, &MainFrame::OnOpenWindowSizeSettings, this, ID_MENU_SETTINGS_WINDOW_SIZE);
    Bind(wxEVT_MENU, &MainFrame::OnOpenHelp, this, ID_MENU_HELP);

    m_navigationPanel->SetOnPageSelected([this](NavigationPanel::PageId pageId)
    {
        switch (pageId)
        {
        case NavigationPanel::PageId::Geometry:
            ShowGeometryPage();
            break;

        case NavigationPanel::PageId::KinematicResults:
            ShowKinematicResultPage();
            break;

        case NavigationPanel::PageId::MassProperties:
            ShowMassPropertiesPage();
            break;

        case NavigationPanel::PageId::DynamicResults:
            ShowDynamicResultPage();
            break;

        case NavigationPanel::PageId::CounterweightSetup:
            ShowCounterweightSetupPage();
            break;

        case NavigationPanel::PageId::BalancingResults:
            ShowBalancingResultPage();
            break;
        }
    });

    m_inputPage->SetOnCalculationSucceeded(
        [this](const EngineModel& model, const engine::kinematic::KinematicResult& result)
        {
            m_lastEngineModel = model;
            m_lastKinematicResult = result;
            m_lastMassPropertiesInput.reset();
            m_lastDynamicResult.reset();
            m_lastBalancingPipelineResult.reset();

            if (m_resultPage)
                m_resultPage->SetResultData(model, result);

            if (m_massPage)
                m_massPage->SetModel(model);

            if (m_counterweightSetupPage)
                m_counterweightSetupPage->SetModel(model);

            ShowKinematicResultPage();
        });

    m_massPage->SetOnCalculateRequested(
        [this](const MassPropertiesInput& input)
        {
            if (!m_lastEngineModel.has_value() || !m_lastKinematicResult.has_value())
            {
                wxMessageBox(WXU8("Сначала выполните кинематический расчёт."),
                             WXU8("Ошибка"),
                             wxOK | wxICON_WARNING,
                             this);
                return;
            }

            m_lastMassPropertiesInput = input;
            m_lastDynamicResult.reset();
            m_lastBalancingPipelineResult.reset();

            try
            {
                const engine::dynamic::DynamicInput dynInput =
                    engine::dynamic::DynamicInputFactory::Create(input);

                engine::dynamic::DynamicCalculator calculator;
                m_lastDynamicResult = calculator.Calculate(*m_lastKinematicResult, dynInput);

                if (m_dynamicResultPage)
                    m_dynamicResultPage->SetResultData(*m_lastEngineModel, *m_lastDynamicResult);

                if (m_counterweightSetupPage && m_lastEngineModel.has_value())
                    m_counterweightSetupPage->SetModel(*m_lastEngineModel);

                ShowDynamicResultPage();
            }
            catch (const std::exception& ex)
            {
                wxMessageBox(wxString::Format(WXU8("Ошибка расчёта динамики:\n%s"), ex.what()),
                             WXU8("Ошибка"),
                             wxOK | wxICON_ERROR,
                             this);
            }
        });

    if (m_counterweightSetupPage)
    {
        m_counterweightSetupPage->SetOnInputChanged([this]()
        {
            m_lastBalancingPipelineResult.reset();
        });

        m_counterweightSetupPage->SetOnCalculateRequested([this](const EngineModel& updatedModel)
        {
            if (!m_lastDynamicResult.has_value() || !m_lastMassPropertiesInput.has_value())
            {
                wxMessageBox(WXU8("Сначала должен быть выполнен расчёт динамики."),
                             WXU8("Ошибка"),
                             wxOK | wxICON_WARNING,
                             this);
                ShowMassPropertiesPage();
                return;
            }

            m_lastEngineModel = updatedModel;
            m_lastBalancingPipelineResult.reset();

            try
            {
                engine::balancing::BalancingInput input;
                input.alphaDeg = m_lastDynamicResult->alphaDeg;
                input.rpm = m_lastEngineModel->kinematic.rpm;
                input.referenceX_M = m_lastMassPropertiesInput->referenceXmm / 1000.0;
                input.referenceY_M = m_lastMassPropertiesInput->referenceYmm / 1000.0;
                input.referenceZ_M = m_lastMassPropertiesInput->referenceZmm / 1000.0;

                engine::balancing::BalancingPipeline pipeline;
                engine::balancing::BalancingPipelineResult pipelineResult =
                    pipeline.Run(*m_lastEngineModel, *m_lastDynamicResult, input);

                if (!pipelineResult.ok)
                {
                    std::ostringstream oss;
                    oss << "Ошибка расчёта уравновешивания:\n";
                    for (const auto& error : pipelineResult.errors)
                        oss << " - " << error.message << "\n";

                    wxMessageBox(wxString::FromUTF8(oss.str().c_str()),
                                 WXU8("Ошибка"),
                                 wxOK | wxICON_ERROR,
                                 this);
                    return;
                }

                m_lastBalancingPipelineResult = pipelineResult;

                if (m_balancingResultPage)
                    m_balancingResultPage->SetResultData(*m_lastEngineModel, pipelineResult);

                if (m_book)
                    m_book->SetSelection(5);

                if (m_navigationPanel)
                    m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::BalancingResults);
            }
            catch (const std::exception& ex)
            {
                wxMessageBox(wxString::Format(WXU8("Ошибка расчёта уравновешивания:\n%s"), ex.what()),
                             WXU8("Ошибка"),
                             wxOK | wxICON_ERROR,
                             this);
            }
        });

        m_counterweightSetupPage->SetOnAutobalanceRequested(
            [this](const EngineModel& updatedModel,
                   engine::balancing::BalancingSynthesisGoalKind goal)
                -> engine::balancing::BalancingSynthesisResult
        {
            engine::balancing::BalancingSynthesisResult result;

            if (!m_lastDynamicResult.has_value() || !m_lastMassPropertiesInput.has_value())
            {
                result.ok = false;
                result.errors.push_back({ "Сначала должен быть выполнен расчёт динамики." });
                return result;
            }

            try
            {
                engine::balancing::BalancingSynthesizer synthesizer;
                engine::balancing::BalancingSynthesisConstraints constraints;
                constraints.maxBalancerShaftCount = 4;
                constraints.maxVariantsToReturn = 8;

                return synthesizer.Generate(
                    updatedModel,
                    *m_lastDynamicResult,
                    *m_lastMassPropertiesInput,
                    goal,
                    constraints);
            }
            catch (const std::exception& ex)
            {
                result.ok = false;
                result.errors.push_back({ ex.what() });
                return result;
            }
        });
    }
}

void MainFrame::ShowGeometryPage()
{
    if (m_book)
        m_book->SetSelection(0);

    if (m_navigationPanel)
        m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::Geometry);
}

void MainFrame::ShowKinematicResultPage()
{
    if (m_book)
        m_book->SetSelection(1);

    if (m_navigationPanel)
        m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::KinematicResults);
}

void MainFrame::ShowMassPropertiesPage()
{
    if (m_book)
        m_book->SetSelection(2);

    if (m_navigationPanel)
        m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::MassProperties);
}

void MainFrame::ShowDynamicResultPage()
{
    if (!m_lastDynamicResult.has_value())
    {
        wxMessageBox(WXU8("Сначала выполните расчёт динамики."),
                     WXU8("Нет актуального расчёта"),
                     wxOK | wxICON_WARNING,
                     this);
        ShowMassPropertiesPage();
        return;
    }

    if (m_book)
        m_book->SetSelection(3);

    if (m_navigationPanel)
        m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::DynamicResults);
}

void MainFrame::ShowCounterweightSetupPage()
{
    if (!m_lastDynamicResult.has_value())
    {
        wxMessageBox(WXU8("Сначала выполните расчёт динамики."),
                     WXU8("Нет актуального расчёта"),
                     wxOK | wxICON_WARNING,
                     this);
        ShowMassPropertiesPage();
        return;
    }

    if (m_counterweightSetupPage && m_lastEngineModel.has_value())
        m_counterweightSetupPage->SetModel(*m_lastEngineModel);

    if (m_book)
        m_book->SetSelection(4);

    if (m_navigationPanel)
        m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::CounterweightSetup);
}

void MainFrame::ShowBalancingResultPage()
{
    if (!m_lastBalancingPipelineResult.has_value())
    {
        wxMessageBox(WXU8("Сначала нажмите кнопку «Расчёт» на странице установки противовесов."),
                     WXU8("Нет актуального расчёта"),
                     wxOK | wxICON_WARNING,
                     this);
        ShowCounterweightSetupPage();
        return;
    }

    if (!m_lastEngineModel.has_value())
    {
        wxMessageBox(WXU8("Нет актуальной модели двигателя."),
                     WXU8("Ошибка"),
                     wxOK | wxICON_WARNING,
                     this);
        return;
    }

    if (m_balancingResultPage)
        m_balancingResultPage->SetResultData(*m_lastEngineModel, *m_lastBalancingPipelineResult);

    if (m_book)
        m_book->SetSelection(5);

    if (m_navigationPanel)
        m_navigationPanel->SetSelectedPage(NavigationPanel::PageId::BalancingResults);
}

void MainFrame::OnFileStub(wxCommandEvent&)
{
    wxMessageBox(WXU8("Пункт \"Файл\" пока является заглушкой."),
                 WXU8("Информация"),
                 wxOK | wxICON_INFORMATION,
                 this);
}

void MainFrame::OnOpenSettings(wxCommandEvent&)
{
    SettingsDialog dlg(this, m_alphaStepDeg);
    if (dlg.ShowModal() == wxID_OK)
    {
        m_alphaStepDeg = dlg.GetSelectedAlphaStep();
        if (m_inputPage)
            m_inputPage->SetAlphaStep(m_alphaStepDeg);
    }
}

void MainFrame::OnOpenWindowSizeSettings(wxCommandEvent&)
{
    WindowSizeDialog dlg(this);
    if (dlg.ShowModal() != wxID_OK)
        return;

    const auto selectedSize = dlg.GetSelectedSize();
    if (!selectedSize.has_value())
        return;

    ApplySafeWindowBounds(wxSize(selectedSize->first, selectedSize->second));
}

void MainFrame::OnOpenHelp(wxCommandEvent&)
{
    HelpDialog dlg(this);
    dlg.ShowModal();
}