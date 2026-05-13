#include "gui/frame/main_frame.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

#include <wx/button.h>
#include <wx/display.h>
#include <wx/event.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/grid.h>
#include <wx/icon.h>
#include <wx/msgdlg.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/window.h>

#include "app/resource.h"

#include "core/balancing/balancing_pipeline.h"
#include "core/dynamic/dynamic_calculator.h"
#include "core/dynamic/dynamic_input_factory.h"
#include "core/kinematic/kinematic_model_builder.h"
#include "core/kinematic/kinematic_solver.h"

#include "gui/common/text_utf8.h"
#include "gui/dialogs/help_dialog.h"
#include "gui/dialogs/report_dialog.h"
#include "gui/dialogs/settings_dialog.h"
#include "gui/frame/main_menu_builder.h"
#include "gui/pages/balancing_result_page.h"
#include "gui/pages/counterweight_setup_page.h"
#include "gui/pages/dynamic_result_page.h"
#include "gui/pages/input_page.h"
#include "gui/pages/kinematic_result_page.h"
#include "gui/pages/mass_properties_page.h"
#include "gui/widgets/navigation_panel.h"
#include "core/balancing/balancing_synthesizer.h"

namespace
{
constexpr const char* kLegacyProjectMagic = "EDP_PROJECT_V1";
constexpr const char* kStateMagic = "EDS_STATE_V1";
constexpr const char* kProjectLinkMagic = "EDP_LINK_V1";

struct SavedProjectState
{
    EngineModel model;
    bool hasMassInput = false;
    MassPropertiesInput massInput;
    bool restoreDynamic = false;
    bool restoreBalancing = false;
};

bool SaveProjectStateToStream(const SavedProjectState& state, std::ostream& out)
{
    out << kStateMagic << "\n";

    const auto& m = state.model;
    out << static_cast<int>(m.kinematic.cycleType) << ' '
        << m.kinematic.shaftCount << ' '
        << m.kinematic.crankCountPerShaft << ' '
        << m.kinematic.cylindersPerCrank << ' '
        << static_cast<int>(m.kinematic.rodJointType) << ' '
        << static_cast<int>(m.kinematic.supportType) << ' '
        << std::setprecision(17)
        << m.kinematic.rpm << ' '
        << m.kinematic.deaxialMm << ' '
        << m.kinematic.crankRadiusM << ' '
        << m.kinematic.lambda << ' '
        << m.kinematic.alphaStepDeg << ' '
        << m.kinematic.mainJournalLengthM << ' '
        << m.kinematic.rodJournalLengthM << ' '
        << m.kinematic.webThicknessM << ' '
        << m.kinematic.articulatedRodRadiusM << ' '
        << m.kinematic.articulatedRodLengthM << "\n";

    out << m.shafts.size() << "\n";
    for (const auto& shaft : m.shafts)
    {
        out << shaft.shaftNumber << ' '
            << shaft.originXMm << ' '
            << shaft.originYMm << ' '
            << shaft.originZMm << "\n";

        out << shaft.cranks.size() << "\n";
        for (const auto& crank : shaft.cranks)
            out << crank.crankNumber << ' ' << crank.phaseDeg << "\n";

        out << shaft.cylinders.size() << "\n";
        for (const auto& cylinder : shaft.cylinders)
        {
            out << cylinder.cylinderNumber << ' '
                << cylinder.crankNumber << ' '
                << cylinder.axisTiltDeg << ' '
                << cylinder.axisPositionZ << ' '
                << cylinder.firingAngleDeg << "\n";
        }
    }

    const auto& balancing = m.balancing;
    out << balancing.crankCounterweights.enabled << ' '
        << balancing.crankCounterweights.massKg << ' '
        << balancing.crankCounterweights.radiusMm << ' '
        << static_cast<int>(balancing.crankCounterweights.countMode) << "\n";
    out << balancing.crankCounterweights.entries.size() << "\n";
    for (const auto& entry : balancing.crankCounterweights.entries)
        out << entry.shaftNumber << ' ' << entry.crankNumber << ' ' << entry.phaseDeg << "\n";

    out << balancing.balancerShafts.size() << "\n";
    for (const auto& shaft : balancing.balancerShafts)
    {
        out << shaft.enabled << ' '
            << shaft.originXMm << ' '
            << shaft.originYMm << ' '
            << shaft.originZMm << ' '
            << static_cast<int>(shaft.axis) << ' '
            << shaft.lengthMm << ' '
            << static_cast<int>(shaft.speedRatio) << ' '
            << shaft.shaftPhaseDeg << ' '
            << shaft.counterweightMassKg << ' '
            << shaft.counterweightRadiusMm << "\n";

        out << shaft.counterweights.size() << "\n";
        for (const auto& cw : shaft.counterweights)
            out << cw.positionAlongShaftMm << ' ' << cw.phaseDeg << "\n";
    }

    out << state.hasMassInput << "\n";
    if (state.hasMassInput)
    {
        out << state.massInput.cylinderDiameterMm << ' '
            << state.massInput.reciprocatingMassKg << ' '
            << state.massInput.rotatingMassKg << ' '
            << state.massInput.referenceXmm << ' '
            << state.massInput.referenceYmm << ' '
            << state.massInput.referenceZmm << "\n";
    }

    out << state.restoreDynamic << ' ' << state.restoreBalancing << "\n";
    return static_cast<bool>(out);
}

template <typename T>
bool ReadValue(std::istream& in, T& value)
{
    in >> value;
    return static_cast<bool>(in);
}

bool LoadProjectStateFromStream(std::istream& in, SavedProjectState& state)
{
    std::string magic;
    if (!ReadValue(in, magic))
        return false;
    if (magic != kStateMagic && magic != kLegacyProjectMagic)
        return false;

    auto& m = state.model;
    int cycle = 0;
    int rodJoint = 0;
    int supportType = 0;
    if (!ReadValue(in, cycle) ||
        !ReadValue(in, m.kinematic.shaftCount) ||
        !ReadValue(in, m.kinematic.crankCountPerShaft) ||
        !ReadValue(in, m.kinematic.cylindersPerCrank) ||
        !ReadValue(in, rodJoint) ||
        !ReadValue(in, supportType) ||
        !ReadValue(in, m.kinematic.rpm) ||
        !ReadValue(in, m.kinematic.deaxialMm) ||
        !ReadValue(in, m.kinematic.crankRadiusM) ||
        !ReadValue(in, m.kinematic.lambda) ||
        !ReadValue(in, m.kinematic.alphaStepDeg) ||
        !ReadValue(in, m.kinematic.mainJournalLengthM) ||
        !ReadValue(in, m.kinematic.rodJournalLengthM) ||
        !ReadValue(in, m.kinematic.webThicknessM) ||
        !ReadValue(in, m.kinematic.articulatedRodRadiusM) ||
        !ReadValue(in, m.kinematic.articulatedRodLengthM))
    {
        return false;
    }

    m.kinematic.cycleType = (cycle == 2) ? CycleType::TwoStroke : CycleType::FourStroke;
    m.kinematic.rodJointType =
        (rodJoint == static_cast<int>(RodJointType::Articulated))
            ? RodJointType::Articulated
            : RodJointType::SideBySide;
    m.kinematic.supportType =
        (supportType == static_cast<int>(SupportType::SemiSupported))
            ? SupportType::SemiSupported
            : SupportType::FullySupported;

    std::size_t shaftCount = 0;
    if (!ReadValue(in, shaftCount))
        return false;
    m.shafts.clear();
    m.shafts.resize(shaftCount);

    for (std::size_t i = 0; i < shaftCount; ++i)
    {
        auto& shaft = m.shafts[i];
        if (!ReadValue(in, shaft.shaftNumber) ||
            !ReadValue(in, shaft.originXMm) ||
            !ReadValue(in, shaft.originYMm) ||
            !ReadValue(in, shaft.originZMm))
        {
            return false;
        }

        std::size_t crankCount = 0;
        if (!ReadValue(in, crankCount))
            return false;
        shaft.cranks.resize(crankCount);
        for (std::size_t c = 0; c < crankCount; ++c)
        {
            if (!ReadValue(in, shaft.cranks[c].crankNumber) ||
                !ReadValue(in, shaft.cranks[c].phaseDeg))
            {
                return false;
            }
        }

        std::size_t cylinderCount = 0;
        if (!ReadValue(in, cylinderCount))
            return false;
        shaft.cylinders.resize(cylinderCount);
        for (std::size_t c = 0; c < cylinderCount; ++c)
        {
            if (!ReadValue(in, shaft.cylinders[c].cylinderNumber) ||
                !ReadValue(in, shaft.cylinders[c].crankNumber) ||
                !ReadValue(in, shaft.cylinders[c].axisTiltDeg) ||
                !ReadValue(in, shaft.cylinders[c].axisPositionZ) ||
                !ReadValue(in, shaft.cylinders[c].firingAngleDeg))
            {
                return false;
            }
        }
    }

    auto& balancing = m.balancing;
    int countMode = 0;
    if (!ReadValue(in, balancing.crankCounterweights.enabled) ||
        !ReadValue(in, balancing.crankCounterweights.massKg) ||
        !ReadValue(in, balancing.crankCounterweights.radiusMm) ||
        !ReadValue(in, countMode))
    {
        return false;
    }
    balancing.crankCounterweights.countMode = static_cast<CounterweightCountMode>(countMode);

    std::size_t entryCount = 0;
    if (!ReadValue(in, entryCount))
        return false;
    balancing.crankCounterweights.entries.resize(entryCount);
    for (std::size_t i = 0; i < entryCount; ++i)
    {
        if (!ReadValue(in, balancing.crankCounterweights.entries[i].shaftNumber) ||
            !ReadValue(in, balancing.crankCounterweights.entries[i].crankNumber) ||
            !ReadValue(in, balancing.crankCounterweights.entries[i].phaseDeg))
        {
            return false;
        }
    }

    std::size_t balancerShaftCount = 0;
    if (!ReadValue(in, balancerShaftCount))
        return false;
    balancing.balancerShafts.resize(balancerShaftCount);
    for (std::size_t i = 0; i < balancerShaftCount; ++i)
    {
        auto& shaft = balancing.balancerShafts[i];
        int axis = 0;
        int speedRatio = 0;
        if (!ReadValue(in, shaft.enabled) ||
            !ReadValue(in, shaft.originXMm) ||
            !ReadValue(in, shaft.originYMm) ||
            !ReadValue(in, shaft.originZMm) ||
            !ReadValue(in, axis) ||
            !ReadValue(in, shaft.lengthMm) ||
            !ReadValue(in, speedRatio) ||
            !ReadValue(in, shaft.shaftPhaseDeg) ||
            !ReadValue(in, shaft.counterweightMassKg) ||
            !ReadValue(in, shaft.counterweightRadiusMm))
        {
            return false;
        }
        shaft.axis = static_cast<BalancerAxis>(axis);
        shaft.speedRatio = static_cast<BalancerSpeedRatio>(speedRatio);

        std::size_t counterweightCount = 0;
        if (!ReadValue(in, counterweightCount))
            return false;
        shaft.counterweights.resize(counterweightCount);
        for (std::size_t c = 0; c < counterweightCount; ++c)
        {
            if (!ReadValue(in, shaft.counterweights[c].positionAlongShaftMm) ||
                !ReadValue(in, shaft.counterweights[c].phaseDeg))
            {
                return false;
            }
        }
    }

    if (!ReadValue(in, state.hasMassInput))
        return false;
    if (state.hasMassInput)
    {
        if (!ReadValue(in, state.massInput.cylinderDiameterMm) ||
            !ReadValue(in, state.massInput.reciprocatingMassKg) ||
            !ReadValue(in, state.massInput.rotatingMassKg) ||
            !ReadValue(in, state.massInput.referenceXmm) ||
            !ReadValue(in, state.massInput.referenceYmm) ||
            !ReadValue(in, state.massInput.referenceZmm))
        {
            return false;
        }
    }

    if (!ReadValue(in, state.restoreDynamic) ||
        !ReadValue(in, state.restoreBalancing))
    {
        return false;
    }

    return true;
}

bool SaveProjectDescriptorToStream(const std::string& edsFileNameUtf8, std::ostream& out)
{
    out << kProjectLinkMagic << "\n";
    out << edsFileNameUtf8 << "\n";
    return static_cast<bool>(out);
}

bool LoadProjectDescriptorFromStream(std::istream& in, std::string& edsPathUtf8)
{
    std::string magic;
    if (!ReadValue(in, magic) || magic != kProjectLinkMagic)
        return false;
    if (!ReadValue(in, edsPathUtf8) || edsPathUtf8.empty())
        return false;
    return true;
}

} // namespace

MainFrame::MainFrame(const wxString& startupProjectPath)
    : wxFrame(nullptr,
              wxID_ANY,
              WXU8("Engine Design"),
              wxDefaultPosition,
              wxDefaultSize,
              wxDEFAULT_FRAME_STYLE),
      m_startupProjectPath(startupProjectPath)
{
#ifdef _WIN32
    SetIcon(wxICON(IDI_APP_ICON));
#endif

    BuildUi();
    BindEvents();
    UpdateWindowTitle();
    SetMinSize(wxSize(1200, 760));
    ApplySafeWindowBounds(wxSize(1720, 920));
    TryLoadLastProjectAtStartup();
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
    CreateStatusBar(1);
    SetStatusText(WXU8("Готово"));
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
    Bind(wxEVT_MENU, &MainFrame::OnOpenProject, this, ID_MENU_FILE_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnSaveProject, this, ID_MENU_FILE_SAVE);
    Bind(wxEVT_MENU, &MainFrame::OnSaveProjectAs, this, ID_MENU_FILE_SAVE_AS);
    Bind(wxEVT_MENU, &MainFrame::OnOpenSettings, this, ID_MENU_SETTINGS);
    Bind(wxEVT_MENU, &MainFrame::OnOpenReport, this, ID_MENU_REPORT_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnOpenHelp, this, ID_MENU_HELP);
    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnCloseWindow, this);

    Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);

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

            MarkProjectDirty();
            ShowKinematicResultPage();
        });

    m_inputPage->SetOnInputChanged([this]()
    {
        if (!m_isLoadingProject)
            MarkProjectDirty();
    });

    m_massPage->SetOnInputChanged([this]()
    {
        if (!m_isLoadingProject)
            MarkProjectDirty();
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
            MarkProjectDirty();

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
            if (!m_isLoadingProject)
                MarkProjectDirty();
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
            MarkProjectDirty();

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

void MainFrame::OnOpenProject(wxCommandEvent&)
{
    if (!ConfirmDiscardUnsavedChanges())
        return;

    wxFileDialog dialog(this,
                        WXU8("Открыть проект"),
                        wxEmptyString,
                        wxEmptyString,
                        WXU8("Engine Design Project (*.edp)|*.edp|All files (*.*)|*.*"),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK)
        return;

    LoadProjectFromPath(dialog.GetPath());
}

void MainFrame::OnSaveProject(wxCommandEvent&)
{
    if (!m_currentProjectPathUtf8.has_value() || m_currentProjectPathUtf8->empty())
    {
        SaveProjectAs();
        return;
    }

    SaveProjectToPath(wxString::FromUTF8(m_currentProjectPathUtf8->c_str()));
}

void MainFrame::OnSaveProjectAs(wxCommandEvent&)
{
    SaveProjectAs();
}

void MainFrame::OnOpenSettings(wxCommandEvent&)
{
    SettingsDialog dlg(this, m_alphaStepDeg);
    if (dlg.ShowModal() == wxID_OK)
    {
        m_alphaStepDeg = dlg.GetSelectedAlphaStep();
        if (m_inputPage)
            m_inputPage->SetAlphaStep(m_alphaStepDeg);
        MarkProjectDirty();
    }
}

void MainFrame::OnOpenHelp(wxCommandEvent&)
{
    HelpDialog dlg(this);
    dlg.ShowModal();
}

void MainFrame::OnOpenReport(wxCommandEvent&)
{
    if (!m_lastEngineModel.has_value() || !m_lastKinematicResult.has_value())
    {
        wxMessageBox(WXU8("Нет данных для отчета. Сначала выполните расчет."),
                     WXU8("Нет данных"),
                     wxOK | wxICON_WARNING,
                     this);
        return;
    }

    const engine::dynamic::DynamicResult* dynPtr =
        m_lastDynamicResult.has_value() ? &(*m_lastDynamicResult) : nullptr;
    const engine::balancing::BalancingPipelineResult* balPtr =
        m_lastBalancingPipelineResult.has_value() ? &(*m_lastBalancingPipelineResult) : nullptr;

    ReportDialog dlg(this, *m_lastEngineModel, *m_lastKinematicResult, dynPtr, balPtr);
    dlg.ShowModal();
}

void MainFrame::OnCloseWindow(wxCloseEvent& event)
{
    if (!ConfirmDiscardUnsavedChanges())
    {
        event.Veto();
        return;
    }

    event.Skip();
}

bool MainFrame::BuildCurrentEngineModelForSave(EngineModel& model)
{
    if (!m_inputPage)
        return false;

    EngineModel inputModel;
    wxString errorText;
    if (!m_inputPage->BuildCurrentModel(inputModel, errorText))
    {
        wxMessageBox(WXU8("Не удалось сохранить проект: заполните корректно входные данные."),
                     WXU8("Ошибка сохранения"),
                     wxOK | wxICON_WARNING,
                     this);
        return false;
    }

    if (m_lastEngineModel.has_value())
    {
        model = *m_lastEngineModel;
        model.kinematic = inputModel.kinematic;
        model.shafts = inputModel.shafts;
    }
    else
    {
        model = inputModel;
    }

    if (m_counterweightSetupPage)
    {
        const auto updatedModel = m_counterweightSetupPage->GetUpdatedModel();
        if (updatedModel.has_value())
            model.balancing = updatedModel->balancing;
    }

    return true;
}

bool MainFrame::SaveProjectToPath(const wxString& path)
{
    EngineModel model;
    if (!BuildCurrentEngineModelForSave(model))
        return false;

    SavedProjectState state;
    state.model = model;
    state.hasMassInput = (m_massPage != nullptr);
    if (state.hasMassInput)
        state.massInput = m_massPage->GetInput();
    state.restoreDynamic = m_lastDynamicResult.has_value();
    state.restoreBalancing = m_lastBalancingPipelineResult.has_value();

    const std::filesystem::path projectPath(path.wc_str());
    std::filesystem::path statePath = projectPath;
    statePath.replace_extension(L".eds");

    std::ofstream stateOut(statePath, std::ios::trunc);
    if (!stateOut)
    {
        wxMessageBox(WXU8("Не удалось открыть файл данных проекта для сохранения."),
                     WXU8("Ошибка сохранения"),
                     wxOK | wxICON_ERROR,
                     this);
        return false;
    }

    if (!SaveProjectStateToStream(state, stateOut))
    {
        wxMessageBox(WXU8("Не удалось записать данные проекта."),
                     WXU8("Ошибка сохранения"),
                     wxOK | wxICON_ERROR,
                     this);
        return false;
    }

    std::ofstream projectOut(projectPath, std::ios::trunc);
    if (!projectOut)
    {
        wxMessageBox(WXU8("Не удалось открыть файл проекта .edp для сохранения."),
                     WXU8("Ошибка сохранения"),
                     wxOK | wxICON_ERROR,
                     this);
        return false;
    }

    const std::string edsFileNameUtf8 = statePath.filename().u8string();
    if (!SaveProjectDescriptorToStream(edsFileNameUtf8, projectOut))
    {
        wxMessageBox(WXU8("Не удалось записать файл проекта .edp."),
                     WXU8("Ошибка сохранения"),
                     wxOK | wxICON_ERROR,
                     this);
        return false;
    }

    m_currentProjectPathUtf8 = std::string(path.ToUTF8().data());
    PersistLastProjectPath(path);
    MarkProjectClean();
    SetStatusText(WXU8("Проект сохранён: ") + path + WXU8(" (и ") +
                  wxString(statePath.wstring()) + WXU8(")"));
    return true;
}

bool MainFrame::SaveProjectAs()
{
    wxFileDialog dialog(this,
                        WXU8("Сохранить проект"),
                        wxEmptyString,
                        "project.edp",
                        WXU8("Engine Design Project (*.edp)|*.edp|All files (*.*)|*.*"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK)
        return false;

    return SaveProjectToPath(dialog.GetPath());
}

bool MainFrame::LoadProjectFromPath(const wxString& path)
{
    m_isLoadingProject = true;

    const std::filesystem::path inputPath(path.wc_str());
    std::filesystem::path statePath = inputPath;

    if (inputPath.extension() == L".edp")
    {
        std::ifstream projectIn(inputPath);
        if (!projectIn)
        {
            m_isLoadingProject = false;
            wxMessageBox(WXU8("Не удалось открыть файл проекта."),
                         WXU8("Ошибка открытия"),
                         wxOK | wxICON_ERROR,
                         this);
            return false;
        }

        std::string edsPathUtf8;
        if (LoadProjectDescriptorFromStream(projectIn, edsPathUtf8))
        {
            statePath = std::filesystem::path(edsPathUtf8);
            if (statePath.is_relative())
                statePath = inputPath.parent_path() / statePath;
        }
    }
    else
    {
        // Direct .eds opening is supported for convenience.
        statePath = inputPath;
    }

    std::ifstream in(statePath);
    if (!in)
    {
        m_isLoadingProject = false;
        wxMessageBox(WXU8("Не удалось открыть файл данных проекта (.eds)."),
                     WXU8("Ошибка открытия"),
                     wxOK | wxICON_ERROR,
                     this);
        return false;
    }

    SavedProjectState state;
    if (!LoadProjectStateFromStream(in, state))
    {
        m_isLoadingProject = false;
        wxMessageBox(WXU8("Файл данных проекта повреждён или имеет неподдерживаемый формат."),
                     WXU8("Ошибка открытия"),
                     wxOK | wxICON_ERROR,
                     this);
        return false;
    }

    if (m_inputPage)
    {
        m_inputPage->SetFromModel(state.model);
        m_alphaStepDeg = state.model.kinematic.alphaStepDeg;
    }

    const auto buildResult = engine::kinematic::KinematicModelBuilder::Build(state.model);
    if (!buildResult.ok)
    {
        m_isLoadingProject = false;
        wxMessageBox(WXU8("Не удалось восстановить кинематическую модель из проекта."),
                     WXU8("Ошибка открытия"),
                     wxOK | wxICON_ERROR,
                     this);
        return false;
    }

    m_lastEngineModel = state.model;
    m_lastKinematicResult = engine::kinematic::KinematicSolver::Solve(buildResult.model);
    m_lastMassPropertiesInput.reset();
    m_lastDynamicResult.reset();
    m_lastBalancingPipelineResult.reset();

    if (m_resultPage)
        m_resultPage->SetResultData(*m_lastEngineModel, *m_lastKinematicResult);
    if (m_massPage)
        m_massPage->SetModel(*m_lastEngineModel);
    if (m_counterweightSetupPage)
        m_counterweightSetupPage->SetModel(*m_lastEngineModel);

    if (state.hasMassInput && m_massPage)
    {
        m_massPage->SetInput(state.massInput);
        m_lastMassPropertiesInput = state.massInput;
    }

    if (state.restoreDynamic && state.hasMassInput)
    {
        const engine::dynamic::DynamicInput dynInput =
            engine::dynamic::DynamicInputFactory::Create(*m_lastMassPropertiesInput);
        engine::dynamic::DynamicCalculator calculator;
        m_lastDynamicResult = calculator.Calculate(*m_lastKinematicResult, dynInput);
        if (m_dynamicResultPage)
            m_dynamicResultPage->SetResultData(*m_lastEngineModel, *m_lastDynamicResult);
    }

    if (state.restoreBalancing &&
        m_lastDynamicResult.has_value() &&
        m_lastMassPropertiesInput.has_value())
    {
        engine::balancing::BalancingInput input;
        input.alphaDeg = m_lastDynamicResult->alphaDeg;
        input.rpm = m_lastEngineModel->kinematic.rpm;
        input.referenceX_M = m_lastMassPropertiesInput->referenceXmm / 1000.0;
        input.referenceY_M = m_lastMassPropertiesInput->referenceYmm / 1000.0;
        input.referenceZ_M = m_lastMassPropertiesInput->referenceZmm / 1000.0;

        engine::balancing::BalancingPipeline pipeline;
        const auto pipelineResult = pipeline.Run(*m_lastEngineModel, *m_lastDynamicResult, input);
        if (pipelineResult.ok)
        {
            m_lastBalancingPipelineResult = pipelineResult;
            if (m_balancingResultPage)
                m_balancingResultPage->SetResultData(*m_lastEngineModel, *m_lastBalancingPipelineResult);
        }
    }

    wxString projectPathForSession = path;
    if (inputPath.extension() == L".eds")
    {
        std::filesystem::path inferredProjectPath = inputPath;
        inferredProjectPath.replace_extension(L".edp");
        projectPathForSession = wxString(inferredProjectPath.wstring());
    }

    m_currentProjectPathUtf8 = std::string(projectPathForSession.ToUTF8().data());
    PersistLastProjectPath(projectPathForSession);
    m_isLoadingProject = false;
    MarkProjectClean();
    SetStatusText(WXU8("Проект открыт: ") + projectPathForSession);
    ShowKinematicResultPage();
    return true;
}

void MainFrame::MarkProjectDirty()
{
    m_isProjectDirty = true;
    UpdateWindowTitle();
}

void MainFrame::MarkProjectClean()
{
    m_isProjectDirty = false;
    UpdateWindowTitle();
}

void MainFrame::UpdateWindowTitle()
{
    wxString title = WXU8("Engine Design");
    if (m_currentProjectPathUtf8.has_value() && !m_currentProjectPathUtf8->empty())
    {
        const wxFileName fileName(wxString::FromUTF8(m_currentProjectPathUtf8->c_str()));
        title += " - " + fileName.GetFullName();
    }

    if (m_isProjectDirty)
        title += " *";

    SetTitle(title);
}

bool MainFrame::ConfirmDiscardUnsavedChanges()
{
    if (!m_isProjectDirty)
        return true;

    const int answer = wxMessageBox(
        WXU8("Есть несохранённые изменения.\nСохранить проект перед продолжением?"),
        WXU8("Несохранённые изменения"),
        wxYES_NO | wxCANCEL | wxICON_QUESTION,
        this);

    if (answer == wxCANCEL)
        return false;

    if (answer == wxYES)
    {
        if (!m_currentProjectPathUtf8.has_value() || m_currentProjectPathUtf8->empty())
            return SaveProjectAs();
        return SaveProjectToPath(wxString::FromUTF8(m_currentProjectPathUtf8->c_str()));
    }

    return true;
}

void MainFrame::PersistLastProjectPath(const wxString& path)
{
    std::ofstream out(std::filesystem::path(GetLastProjectPointerFilePath().wc_str()), std::ios::trunc);
    if (!out)
        return;
    out << path.ToUTF8().data() << "\n";
}

wxString MainFrame::GetLastProjectPointerFilePath() const
{
    const wxString dir = wxStandardPaths::Get().GetUserLocalDataDir();
    wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return wxFileName(dir, "last_project_path.txt").GetFullPath();
}

void MainFrame::TryLoadLastProjectAtStartup()
{
    if (!m_startupProjectPath.IsEmpty() && wxFileName::FileExists(m_startupProjectPath))
    {
        LoadProjectFromPath(m_startupProjectPath);
        return;
    }

    std::ifstream in(std::filesystem::path(GetLastProjectPointerFilePath().wc_str()));
    if (!in)
        return;

    std::string utf8Path;
    std::getline(in, utf8Path);
    if (utf8Path.empty())
        return;

    const wxString path = wxString::FromUTF8(utf8Path.c_str());
    if (path.IsEmpty() || !wxFileName::FileExists(path))
        return;

    LoadProjectFromPath(path);
}

bool MainFrame::IsMultilineTextInput(wxWindow* focus)
{
    if (!focus)
        return false;

    wxTextCtrl* tc = wxDynamicCast(focus, wxTextCtrl);
    return tc != nullptr && (tc->GetWindowStyleFlag() & wxTE_MULTILINE) != 0;
}

void MainFrame::NavigateToPreviousNavPage()
{
    if (!m_book)
        return;

    const int sel = m_book->GetSelection();
    switch (sel)
    {
    case 1:
        ShowGeometryPage();
        break;
    case 2:
        ShowKinematicResultPage();
        break;
    case 3:
        ShowMassPropertiesPage();
        break;
    case 4:
        ShowDynamicResultPage();
        break;
    case 5:
        ShowCounterweightSetupPage();
        break;
    default:
        break;
    }
}

void MainFrame::OnCharHook(wxKeyEvent& event)
{
    const int key = event.GetKeyCode();

    wxWindow* focus = wxWindow::FindFocus();
    wxWindow* top = focus;
    while (top && !top->IsTopLevel())
        top = top->GetParent();

    if (top != this)
    {
        event.Skip();
        return;
    }

    if (key == WXK_ESCAPE)
    {
        if (IsMultilineTextInput(focus))
        {
            event.Skip();
            return;
        }

        NavigateToPreviousNavPage();
        return;
    }

    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER)
    {
        if (IsMultilineTextInput(focus))
        {
            event.Skip();
            return;
        }

        if (wxDynamicCast(focus, wxGrid))
        {
            event.Skip();
            return;
        }

        const int sel = m_book ? m_book->GetSelection() : -1;

        if (event.ShiftDown() && sel == 4 && m_counterweightSetupPage)
        {
            m_counterweightSetupPage->RunAutobalance();
            return;
        }

        if (!event.ShiftDown() && wxDynamicCast(focus, wxButton))
        {
            event.Skip();
            return;
        }

        if (!event.ShiftDown())
        {
            if (sel == 0 && m_inputPage)
            {
                m_inputPage->RunKinematicCalculation();
                return;
            }

            if (sel == 2 && m_massPage)
            {
                m_massPage->RunDynamicCalculation();
                return;
            }

            if (sel == 4 && m_counterweightSetupPage)
            {
                m_counterweightSetupPage->RunPipelineCalculate();
                return;
            }
        }
    }

    event.Skip();
}