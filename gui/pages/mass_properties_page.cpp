#include "gui/pages/mass_properties_page.h"

#include <wx/button.h>
#include <wx/grid.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "gui/common/text_utf8.h"
#include "gui/widgets/engine_scheme_panel.h"

MassPropertiesPage::MassPropertiesPage(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxHSCROLL)
{
    SetScrollRate(10, 10);

    BuildUi();
    BindEvents();
    UpdateSchemeReferencePoint();
    FitInside();
}

void MassPropertiesPage::SetModel(const EngineModel& model)
{
    if (m_schemePanel)
    {
        m_schemePanel->SetModel(model);
        m_schemePanel->SetShowReferencePoint(true);
        UpdateSchemeReferencePoint();
    }

    Layout();
    FitInside();
}

void MassPropertiesPage::SetOnCalculateRequested(std::function<void(const MassPropertiesInput&)> handler)
{
    m_onCalculateRequested = std::move(handler);
}

MassPropertiesInput MassPropertiesPage::GetInput() const
{
    MassPropertiesInput input;
    input.cylinderDiameterMm = ReadTextCtrlDouble(m_cylinderDiameterCtrl, 30.0);
    input.reciprocatingMassKg = ReadTextCtrlDouble(m_reciprocatingMassCtrl, 0.5);
    input.rotatingMassKg = ReadTextCtrlDouble(m_rotatingMassCtrl, 1.5);

    input.referenceXmm = ReadGridDouble(0, 0, 0.0);
    input.referenceYmm = ReadGridDouble(0, 1, 0.0);
    input.referenceZmm = ReadGridDouble(0, 2, 0.0);

    return input;
}

void MassPropertiesPage::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, WXU8("Массовые характеристики"));
    auto titleFont = title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 6);
    titleFont.SetWeight(wxFONTWEIGHT_NORMAL);
    title->SetFont(titleFont);
    title->SetForegroundColour(wxColour(245, 245, 245));

    root->Add(title, 0, wxLEFT | wxTOP | wxBOTTOM, 12);
    root->Add(new wxStaticLine(this), 0, wxEXPAND | wxBOTTOM, 10);

    auto* contentSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* leftHostPanel = new wxPanel(this);
    leftHostPanel->SetBackgroundColour(GetBackgroundColour());

    auto* leftHostSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* leftContentPanel = new wxPanel(leftHostPanel);
    leftContentPanel->SetBackgroundColour(GetBackgroundColour());
    leftContentPanel->SetMinSize(wxSize(520, -1));
    leftContentPanel->SetMaxSize(wxSize(520, -1));

    auto* leftSizer = new wxBoxSizer(wxVERTICAL);

    auto* formGrid = new wxFlexGridSizer(3, 2, 14, 12);
    formGrid->AddGrowableCol(1, 1);

    auto* diameterLabel =
        new wxStaticText(leftContentPanel, wxID_ANY, WXU8("Диаметр цилиндра, мм.:"));
    auto* reciprocatingLabel =
        new wxStaticText(leftContentPanel, wxID_ANY, WXU8("Масса поступат. частей, кг.:"));
    auto* rotatingLabel =
        new wxStaticText(leftContentPanel, wxID_ANY, WXU8("Масса вращат. частей, кг.:"));

    diameterLabel->SetForegroundColour(wxColour(230, 230, 230));
    reciprocatingLabel->SetForegroundColour(wxColour(230, 230, 230));
    rotatingLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_cylinderDiameterCtrl = new wxTextCtrl(leftContentPanel, wxID_ANY, "30");
    m_reciprocatingMassCtrl = new wxTextCtrl(leftContentPanel, wxID_ANY, "0.5");
    m_rotatingMassCtrl = new wxTextCtrl(leftContentPanel, wxID_ANY, "1.5");

    formGrid->Add(diameterLabel, 0, wxALIGN_CENTER_VERTICAL);
    formGrid->Add(m_cylinderDiameterCtrl, 1, wxEXPAND);

    formGrid->Add(reciprocatingLabel, 0, wxALIGN_CENTER_VERTICAL);
    formGrid->Add(m_reciprocatingMassCtrl, 1, wxEXPAND);

    formGrid->Add(rotatingLabel, 0, wxALIGN_CENTER_VERTICAL);
    formGrid->Add(m_rotatingMassCtrl, 1, wxEXPAND);

    leftSizer->Add(formGrid, 0, wxEXPAND | wxTOP | wxBOTTOM, 24);

    auto* refLabel = new wxStaticText(
        leftContentPanel,
        wxID_ANY,
        WXU8("Координаты точки, относительно которой\nпроизводится расчёт моментов от сил инерции:"));
    refLabel->SetForegroundColour(wxColour(230, 230, 230));
    leftSizer->Add(refLabel, 0, wxBOTTOM, 10);

    m_referenceGrid = new wxGrid(leftContentPanel, wxID_ANY);
    m_referenceGrid->CreateGrid(1, 3);
    m_referenceGrid->EnableEditing(true);
    m_referenceGrid->EnableDragGridSize(false);
    m_referenceGrid->EnableDragRowSize(false);
    m_referenceGrid->EnableDragColSize(false);

    m_referenceGrid->SetColLabelValue(0, WXU8("X, мм"));
    m_referenceGrid->SetColLabelValue(1, WXU8("Y, мм"));
    m_referenceGrid->SetColLabelValue(2, WXU8("Z, мм"));

    m_referenceGrid->SetRowLabelValue(0, "");
    m_referenceGrid->SetRowLabelSize(40);

    m_referenceGrid->SetCellValue(0, 0, "0");
    m_referenceGrid->SetCellValue(0, 1, "0");
    m_referenceGrid->SetCellValue(0, 2, "0");

    m_referenceGrid->SetColSize(0, 92);
    m_referenceGrid->SetColSize(1, 92);
    m_referenceGrid->SetColSize(2, 92);
    m_referenceGrid->SetMinSize(wxSize(340, 90));

    leftSizer->Add(m_referenceGrid, 0, wxBOTTOM, 24);
    leftSizer->AddStretchSpacer(1);

    leftContentPanel->SetSizer(leftSizer);

    leftHostSizer->Add(leftContentPanel, 0, wxEXPAND);
    leftHostSizer->AddStretchSpacer(1);
    leftHostPanel->SetSizer(leftHostSizer);

    auto* rightSizer = new wxBoxSizer(wxVERTICAL);

    auto* schemeTitle = new wxStaticText(this, wxID_ANY, WXU8("Схема коленчатого вала"));
    auto schemeTitleFont = schemeTitle->GetFont();
    schemeTitleFont.SetPointSize(schemeTitleFont.GetPointSize() + 3);
    schemeTitle->SetFont(schemeTitleFont);
    schemeTitle->SetForegroundColour(wxColour(245, 245, 245));
    rightSizer->Add(schemeTitle, 0, wxBOTTOM, 8);

    m_schemePanel = new EngineSchemePanel(this);
    m_schemePanel->SetShowReferencePoint(true);
    m_schemePanel->SetMinSize(wxSize(-1, 560));
    m_schemePanel->SetMaxSize(wxSize(-1, 560));

    rightSizer->Add(m_schemePanel, 0, wxEXPAND);
    rightSizer->AddStretchSpacer(1);

    contentSizer->Add(leftHostPanel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    contentSizer->Add(rightSizer, 1, wxEXPAND | wxRIGHT | wxBOTTOM, 12);

    root->Add(contentSizer, 1, wxEXPAND);

    auto* bottomSizer = new wxBoxSizer(wxHORIZONTAL);
    bottomSizer->AddStretchSpacer(1);

    m_calculateButton = new wxButton(this, wxID_ANY, WXU8("Расчёт"));
    m_calculateButton->SetMinSize(wxSize(160, 36));
    bottomSizer->Add(m_calculateButton, 0, wxRIGHT | wxBOTTOM, 12);

    root->Add(bottomSizer, 0, wxEXPAND);

    SetSizer(root);
    Layout();
    FitInside();
}

void MassPropertiesPage::BindEvents()
{
    m_cylinderDiameterCtrl->Bind(wxEVT_TEXT, &MassPropertiesPage::OnAnyInputChanged, this);
    m_reciprocatingMassCtrl->Bind(wxEVT_TEXT, &MassPropertiesPage::OnAnyInputChanged, this);
    m_rotatingMassCtrl->Bind(wxEVT_TEXT, &MassPropertiesPage::OnAnyInputChanged, this);

    m_referenceGrid->Bind(wxEVT_GRID_CELL_CHANGED, &MassPropertiesPage::OnGridCellChanged, this);

    m_calculateButton->Bind(wxEVT_BUTTON, &MassPropertiesPage::OnCalculate, this);
}

void MassPropertiesPage::UpdateSchemeReferencePoint()
{
    if (!m_schemePanel)
        return;

    const double xMm = ReadGridDouble(0, 0, 0.0);
    const double yMm = ReadGridDouble(0, 1, 0.0);
    const double zMm = ReadGridDouble(0, 2, 0.0);

    m_schemePanel->SetReferencePointMm(xMm, yMm, zMm);
    m_schemePanel->SetShowReferencePoint(true);

    Layout();
    FitInside();
}

void MassPropertiesPage::OnAnyInputChanged(wxCommandEvent& event)
{
    wxUnusedVar(event);
    UpdateSchemeReferencePoint();
}

void MassPropertiesPage::OnGridCellChanged(wxGridEvent& event)
{
    UpdateSchemeReferencePoint();
    event.Skip();
}

void MassPropertiesPage::OnCalculate(wxCommandEvent& event)
{
    wxUnusedVar(event);

    if (m_onCalculateRequested)
    {
        m_onCalculateRequested(GetInput());
        return;
    }

    wxMessageBox(WXU8("Обработчик расчёта динамики не подключён."),
                 WXU8("Ошибка"),
                 wxOK | wxICON_WARNING,
                 this);
}

double MassPropertiesPage::ReadTextCtrlDouble(wxTextCtrl* ctrl, double fallback) const
{
    if (!ctrl)
        return fallback;

    double value = fallback;
    if (!ctrl->GetValue().ToDouble(&value))
        return fallback;

    return value;
}

double MassPropertiesPage::ReadGridDouble(int row, int col, double fallback) const
{
    if (!m_referenceGrid)
        return fallback;

    if (row < 0 || col < 0)
        return fallback;

    if (row >= m_referenceGrid->GetNumberRows() || col >= m_referenceGrid->GetNumberCols())
        return fallback;

    double value = fallback;
    if (!m_referenceGrid->GetCellValue(row, col).ToDouble(&value))
        return fallback;

    return value;
}