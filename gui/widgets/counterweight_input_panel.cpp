#include "gui/widgets/counterweight_input_panel.h"

#include <algorithm>
#include <vector>

#include <wx/choice.h>
#include <wx/grid.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "gui/common/text_utf8.h"

namespace
{

wxStaticBoxSizer* MakeGroup(wxWindow* parent, const wxString& label)
{
    auto* sizer = new wxStaticBoxSizer(wxVERTICAL, parent, label);
    sizer->GetStaticBox()->SetForegroundColour(wxColour(235, 235, 235));
    sizer->GetStaticBox()->SetBackgroundColour(wxColour(12, 18, 28));
    return sizer;
}

void SetupGridColors(wxGrid* grid)
{
    grid->SetDefaultCellBackgroundColour(wxColour(235, 235, 235));
    grid->SetDefaultCellTextColour(*wxBLACK);
    grid->SetLabelBackgroundColour(wxColour(70, 150, 230));
    grid->SetLabelTextColour(*wxWHITE);
    grid->SetGridLineColour(wxColour(200, 200, 200));
}

void ResizeGrid(wxGrid* grid, int rows, int cols)
{
    if (!grid)
        return;

    const int safeRows = std::max(1, rows);
    const int safeCols = std::max(1, cols);

    const int currentRows = grid->GetNumberRows();
    const int currentCols = grid->GetNumberCols();

    if (currentRows < safeRows)
        grid->AppendRows(safeRows - currentRows);
    else if (currentRows > safeRows)
        grid->DeleteRows(0, currentRows - safeRows);

    if (currentCols < safeCols)
        grid->AppendCols(safeCols - currentCols);
    else if (currentCols > safeCols)
        grid->DeleteCols(0, currentCols - safeCols);
}

wxString FormatOneDecimal(double value)
{
    return wxString::Format("%.1f", value);
}

wxString FormatFiveDecimals(double value)
{
    return wxString::Format("%.5f", value);
}

std::vector<std::vector<wxString>> SnapshotGrid(wxGrid* grid)
{
    std::vector<std::vector<wxString>> snapshot;
    if (!grid)
        return snapshot;

    snapshot.resize(static_cast<std::size_t>(grid->GetNumberRows()));
    for (int row = 0; row < grid->GetNumberRows(); ++row)
    {
        snapshot[static_cast<std::size_t>(row)].resize(static_cast<std::size_t>(grid->GetNumberCols()));
        for (int col = 0; col < grid->GetNumberCols(); ++col)
        {
            snapshot[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] =
                grid->GetCellValue(row, col);
        }
    }

    return snapshot;
}

wxString SafeSnapshotValue(
    const std::vector<std::vector<wxString>>& snapshot,
    int row,
    int col)
{
    if (row < 0 || col < 0)
        return wxString();

    if (row >= static_cast<int>(snapshot.size()))
        return wxString();

    if (col >= static_cast<int>(snapshot[static_cast<std::size_t>(row)].size()))
        return wxString();

    return snapshot[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
}

wxString DefaultIfEmpty(const wxString& value, const wxString& fallback)
{
    return value.IsEmpty() ? fallback : value;
}

} // namespace

CounterweightInputPanel::CounterweightInputPanel(wxWindow* parent)
    : wxPanel(parent)
{
    BuildUi();
    BindEvents();
}

void CounterweightInputPanel::SetModel(const EngineModel& model)
{
    m_model = model;
    FillControlsFromModel();
}

bool CounterweightInputPanel::HasModel() const
{
    return m_model.has_value();
}

void CounterweightInputPanel::SetOnDataChanged(std::function<void(const EngineModel&)> handler)
{
    m_onDataChanged = std::move(handler);
}

void CounterweightInputPanel::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* crankGroup = MakeGroup(this, WXU8("Противовесы на продолжении щек коленчатого вала"));

    auto* topGrid = new wxFlexGridSizer(3, 2, 10, 12);
    topGrid->AddGrowableCol(1, 1);

    auto* massLabel = new wxStaticText(this, wxID_ANY, WXU8("Масса одного противовеса, кг:"));
    auto* radiusLabel = new wxStaticText(this, wxID_ANY, WXU8("Радиус до центра масс, мм:"));
    auto* countModeLabel = new wxStaticText(this, wxID_ANY, WXU8("Число противовесов на кривошип:"));

    massLabel->SetForegroundColour(wxColour(230, 230, 230));
    radiusLabel->SetForegroundColour(wxColour(230, 230, 230));
    countModeLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_crankMassCtrl = new wxTextCtrl(this, wxID_ANY, "0.00000");
    m_crankRadiusCtrl = new wxTextCtrl(this, wxID_ANY, "0.00000");

    m_crankCountModeChoice = new wxChoice(this, wxID_ANY);
    m_crankCountModeChoice->Append(WXU8("Авто"));
    m_crankCountModeChoice->Append("1");
    m_crankCountModeChoice->Append("2");
    m_crankCountModeChoice->SetSelection(0);

    topGrid->Add(massLabel, 0, wxALIGN_CENTER_VERTICAL);
    topGrid->Add(m_crankMassCtrl, 1, wxEXPAND);
    topGrid->Add(radiusLabel, 0, wxALIGN_CENTER_VERTICAL);
    topGrid->Add(m_crankRadiusCtrl, 1, wxEXPAND);
    topGrid->Add(countModeLabel, 0, wxALIGN_CENTER_VERTICAL);
    topGrid->Add(m_crankCountModeChoice, 1, wxEXPAND);

    crankGroup->Add(topGrid, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 6);

    root->Add(crankGroup, 0, wxEXPAND | wxBOTTOM, 10);

    auto* balancerGroup = MakeGroup(this, WXU8("Дополнительные валы"));

    auto* balancerTopSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* balancerCountLabel =
        new wxStaticText(this, wxID_ANY, WXU8("Количество дополнительных валов:"));
    balancerCountLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_balancerShaftCountSpin = new wxSpinCtrl(this, wxID_ANY);
    m_balancerShaftCountSpin->SetRange(0, 32);
    m_balancerShaftCountSpin->SetValue(0);
    m_balancerShaftCountSpin->SetForegroundColour(*wxBLACK);
    m_balancerShaftCountSpin->SetBackgroundColour(*wxWHITE);

    auto* balancerCounterweightCountLabel =
        new wxStaticText(this, wxID_ANY, WXU8("Противовесов на вал:"));
    balancerCounterweightCountLabel->SetForegroundColour(wxColour(230, 230, 230));

    m_balancerCounterweightCountSpin = new wxSpinCtrl(this, wxID_ANY);
    m_balancerCounterweightCountSpin->SetRange(0, 32);
    m_balancerCounterweightCountSpin->SetValue(0);
    m_balancerCounterweightCountSpin->SetForegroundColour(*wxBLACK);
    m_balancerCounterweightCountSpin->SetBackgroundColour(*wxWHITE);

    balancerTopSizer->Add(balancerCountLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    balancerTopSizer->Add(m_balancerShaftCountSpin, 0, wxRIGHT, 24);
    balancerTopSizer->Add(balancerCounterweightCountLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);
    balancerTopSizer->Add(m_balancerCounterweightCountSpin, 0);

    balancerGroup->Add(balancerTopSizer, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 6);

    auto* numbersLabel =
        new wxStaticText(this, wxID_ANY, WXU8("Числовые параметры дополнительных валов"));
    numbersLabel->SetForegroundColour(wxColour(230, 230, 230));
    balancerGroup->Add(numbersLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);

    m_balancerShaftGrid = new wxGrid(this, wxID_ANY);
    m_balancerShaftGrid->CreateGrid(1, 6);
    m_balancerShaftGrid->EnableEditing(true);
    m_balancerShaftGrid->EnableDragGridSize(false);
    m_balancerShaftGrid->EnableDragRowSize(false);
    m_balancerShaftGrid->EnableDragColSize(false);
    m_balancerShaftGrid->SetMinSize(wxSize(620, 110));
    SetupGridColors(m_balancerShaftGrid);
    balancerGroup->Add(m_balancerShaftGrid, 0, wxEXPAND | wxALL, 6);

    auto* smallTablesSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* axisBox = new wxBoxSizer(wxVERTICAL);
    auto* axisLabel = new wxStaticText(this, wxID_ANY, WXU8("Ось"));
    axisLabel->SetForegroundColour(wxColour(230, 230, 230));
    axisBox->Add(axisLabel, 0, wxBOTTOM, 4);

    m_balancerAxisGrid = new wxGrid(this, wxID_ANY);
    m_balancerAxisGrid->CreateGrid(1, 1);
    m_balancerAxisGrid->EnableEditing(true);
    m_balancerAxisGrid->EnableDragGridSize(false);
    m_balancerAxisGrid->EnableDragRowSize(false);
    m_balancerAxisGrid->EnableDragColSize(false);
    m_balancerAxisGrid->SetMinSize(wxSize(150, 110));
    SetupGridColors(m_balancerAxisGrid);
    axisBox->Add(m_balancerAxisGrid, 0, wxEXPAND);

    auto* speedBox = new wxBoxSizer(wxVERTICAL);
    auto* speedLabel = new wxStaticText(this, wxID_ANY, WXU8("Скорость"));
    speedLabel->SetForegroundColour(wxColour(230, 230, 230));
    speedBox->Add(speedLabel, 0, wxBOTTOM, 4);

    m_balancerSpeedGrid = new wxGrid(this, wxID_ANY);
    m_balancerSpeedGrid->CreateGrid(1, 1);
    m_balancerSpeedGrid->EnableEditing(true);
    m_balancerSpeedGrid->EnableDragGridSize(false);
    m_balancerSpeedGrid->EnableDragRowSize(false);
    m_balancerSpeedGrid->EnableDragColSize(false);
    m_balancerSpeedGrid->SetMinSize(wxSize(160, 110));
    SetupGridColors(m_balancerSpeedGrid);
    speedBox->Add(m_balancerSpeedGrid, 0, wxEXPAND);

    smallTablesSizer->Add(axisBox, 0, wxRIGHT, 14);
    smallTablesSizer->Add(speedBox, 0);

    balancerGroup->Add(smallTablesSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);

    auto* phaseGridLabel =
        new wxStaticText(this, wxID_ANY, WXU8("Геометрические фазы противовесов на дополнительных валах, град."));
    phaseGridLabel->SetForegroundColour(wxColour(230, 230, 230));
    balancerGroup->Add(phaseGridLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 6);

    m_balancerPhaseGrid = new wxGrid(this, wxID_ANY);
    m_balancerPhaseGrid->CreateGrid(1, 1);
    m_balancerPhaseGrid->EnableEditing(true);
    m_balancerPhaseGrid->EnableDragGridSize(false);
    m_balancerPhaseGrid->EnableDragRowSize(false);
    m_balancerPhaseGrid->EnableDragColSize(false);
    m_balancerPhaseGrid->SetMinSize(wxSize(620, 120));
    SetupGridColors(m_balancerPhaseGrid);
    balancerGroup->Add(m_balancerPhaseGrid, 0, wxEXPAND | wxALL, 6);

    root->Add(balancerGroup, 0, wxEXPAND);

    SetSizer(root);
    Layout();

    RebuildBalancerShaftGrid();
    RebuildBalancerAxisGrid();
    RebuildBalancerSpeedGrid();
    RebuildBalancerPhaseGrid();
}

void CounterweightInputPanel::BindEvents()
{
    m_crankMassCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&)
    {
        NotifyDataChanged();
    });

    m_crankRadiusCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&)
    {
        NotifyDataChanged();
    });

    m_crankCountModeChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        ApplyCrankCounterweightModeRules();
        NotifyDataChanged();
    });

    m_balancerShaftCountSpin->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&)
    {
        if (m_isUpdating)
            return;

        RebuildBalancerShaftGrid();
        RebuildBalancerAxisGrid();
        RebuildBalancerSpeedGrid();
        RebuildBalancerPhaseGrid();
        NormalizeBalancerPositionCells();
        NotifyDataChanged();
    });

    m_balancerCounterweightCountSpin->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&)
    {
        if (m_isUpdating)
            return;

        RebuildBalancerShaftGrid();
        RebuildBalancerPhaseGrid();
        NormalizeBalancerPositionCells();
        NotifyDataChanged();
    });

    m_balancerShaftGrid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent& event)
    {
        if (m_isUpdating)
            return;

        const int row = event.GetRow();
        if (row >= 0)
            NormalizeBalancerPositionCellsForRow(row);

        NotifyDataChanged();
    });

    m_balancerAxisGrid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent&)
    {
        NotifyDataChanged();
    });

    m_balancerSpeedGrid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent&)
    {
        NotifyDataChanged();
    });

    m_balancerPhaseGrid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent&)
    {
        NotifyDataChanged();
    });
}

void CounterweightInputPanel::RebuildBalancerShaftGrid()
{
    const int shaftCount = m_balancerShaftCountSpin ? m_balancerShaftCountSpin->GetValue() : 0;
    const int counterweightCount = m_balancerCounterweightCountSpin ? m_balancerCounterweightCountSpin->GetValue() : 0;

    const auto snapshot = SnapshotGrid(m_balancerShaftGrid);

    const int baseCols = 6;
    const int totalCols = baseCols + std::max(0, counterweightCount);

    ResizeGrid(m_balancerShaftGrid, std::max(1, shaftCount), totalCols);

    m_balancerShaftGrid->SetColLabelValue(0, WXU8("X0, мм"));
    m_balancerShaftGrid->SetColLabelValue(1, WXU8("Y0, мм"));
    m_balancerShaftGrid->SetColLabelValue(2, WXU8("Z0, мм"));
    m_balancerShaftGrid->SetColLabelValue(3, WXU8("L, мм"));
    m_balancerShaftGrid->SetColLabelValue(4, WXU8("m, кг"));
    m_balancerShaftGrid->SetColLabelValue(5, WXU8("r, мм"));

    for (int col = 0; col < baseCols; ++col)
        m_balancerShaftGrid->SetColSize(col, 108);

    for (int i = 0; i < counterweightCount; ++i)
    {
        const int col = baseCols + i;
        m_balancerShaftGrid->SetColLabelValue(col, wxString::Format(WXU8("Положение %d пр., мм"), i + 1));
        m_balancerShaftGrid->SetColSize(col, 170);
    }

    for (int row = 0; row < std::max(1, shaftCount); ++row)
    {
        m_balancerShaftGrid->SetRowLabelValue(row, wxString::Format(WXU8("Доп. вал %d"), row + 1));

        const wxString x0 = SafeSnapshotValue(snapshot, row, 0);
        const wxString y0 = SafeSnapshotValue(snapshot, row, 1);
        const wxString z0 = SafeSnapshotValue(snapshot, row, 2);
        const wxString length = SafeSnapshotValue(snapshot, row, 3);
        const wxString mass = SafeSnapshotValue(snapshot, row, 4);
        const wxString radius = SafeSnapshotValue(snapshot, row, 5);

        m_balancerShaftGrid->SetCellValue(row, 0, DefaultIfEmpty(x0, wxString("0")));
        m_balancerShaftGrid->SetCellValue(row, 1, DefaultIfEmpty(y0, wxString("0")));
        m_balancerShaftGrid->SetCellValue(row, 2, DefaultIfEmpty(z0, wxString("0")));
        m_balancerShaftGrid->SetCellValue(row, 3, DefaultIfEmpty(length, wxString("0")));
        m_balancerShaftGrid->SetCellValue(row, 4, DefaultIfEmpty(mass, wxString("0.00000")));
        m_balancerShaftGrid->SetCellValue(row, 5, DefaultIfEmpty(radius, wxString("0.00000")));

        for (int i = 0; i < counterweightCount; ++i)
        {
            const int col = baseCols + i;
            const wxString pos = SafeSnapshotValue(snapshot, row, col);
            m_balancerShaftGrid->SetCellValue(row, col, DefaultIfEmpty(pos, wxString("0")));
        }
    }
}

void CounterweightInputPanel::RebuildBalancerAxisGrid()
{
    const int shaftCount = m_balancerShaftCountSpin ? m_balancerShaftCountSpin->GetValue() : 0;
    const auto snapshot = SnapshotGrid(m_balancerAxisGrid);

    ResizeGrid(m_balancerAxisGrid, std::max(1, shaftCount), 1);

    m_balancerAxisGrid->SetColLabelValue(0, WXU8("Ось"));
    m_balancerAxisGrid->SetColSize(0, 60);

    static const wxString axisChoices[] = { "X", "Y", "Z" };

    for (int row = 0; row < std::max(1, shaftCount); ++row)
    {
        m_balancerAxisGrid->SetRowLabelValue(row, wxString::Format(WXU8("Вал %d"), row + 1));
        m_balancerAxisGrid->SetCellEditor(row, 0, new wxGridCellChoiceEditor(3, axisChoices));

        const wxString axis = SafeSnapshotValue(snapshot, row, 0);
        m_balancerAxisGrid->SetCellValue(row, 0, DefaultIfEmpty(axis, wxString("Z")));
    }
}

void CounterweightInputPanel::RebuildBalancerSpeedGrid()
{
    const int shaftCount = m_balancerShaftCountSpin ? m_balancerShaftCountSpin->GetValue() : 0;
    const auto snapshot = SnapshotGrid(m_balancerSpeedGrid);

    ResizeGrid(m_balancerSpeedGrid, std::max(1, shaftCount), 1);

    m_balancerSpeedGrid->SetColLabelValue(0, WXU8("Скорость"));
    m_balancerSpeedGrid->SetColSize(0, 70);

    static const wxString speedChoices[] = { "+1w", "+2w", "-1w", "-2w" };

    for (int row = 0; row < std::max(1, shaftCount); ++row)
    {
        m_balancerSpeedGrid->SetRowLabelValue(row, wxString::Format(WXU8("Вал %d"), row + 1));
        m_balancerSpeedGrid->SetCellEditor(row, 0, new wxGridCellChoiceEditor(4, speedChoices));

        const wxString speed = SafeSnapshotValue(snapshot, row, 0);
        m_balancerSpeedGrid->SetCellValue(row, 0, DefaultIfEmpty(speed, wxString("+1w")));
    }
}

void CounterweightInputPanel::RebuildBalancerPhaseGrid()
{
    const int shaftCount = m_balancerShaftCountSpin ? m_balancerShaftCountSpin->GetValue() : 0;
    const int counterweightCount = m_balancerCounterweightCountSpin ? m_balancerCounterweightCountSpin->GetValue() : 0;

    const auto snapshot = SnapshotGrid(m_balancerPhaseGrid);

    ResizeGrid(m_balancerPhaseGrid, std::max(1, shaftCount), std::max(1, counterweightCount));

    for (int row = 0; row < std::max(1, shaftCount); ++row)
    {
        m_balancerPhaseGrid->SetRowLabelValue(row, wxString::Format(WXU8("Доп. вал %d"), row + 1));
    }

    for (int col = 0; col < std::max(1, counterweightCount); ++col)
    {
        m_balancerPhaseGrid->SetColLabelValue(col, wxString::Format(WXU8("Противовес %d"), col + 1));
        m_balancerPhaseGrid->SetColSize(col, 105);
    }

    for (int row = 0; row < std::max(1, shaftCount); ++row)
    {
        for (int col = 0; col < std::max(1, counterweightCount); ++col)
        {
            const wxString phase = SafeSnapshotValue(snapshot, row, col);
            m_balancerPhaseGrid->SetCellValue(row, col, DefaultIfEmpty(phase, wxString("0")));
        }
    }
}

void CounterweightInputPanel::FillControlsFromModel()
{
    if (!m_model.has_value())
        return;

    m_isUpdating = true;

    const auto& model = *m_model;
    const auto& crankSystem = model.balancing.crankCounterweights;

    m_crankMassCtrl->SetValue(FormatFiveDecimals(crankSystem.massKg));
    m_crankRadiusCtrl->SetValue(FormatFiveDecimals(crankSystem.radiusMm));
    m_crankCountModeChoice->SetSelection(CountModeToChoiceIndex(crankSystem.countMode));

    m_balancerShaftCountSpin->SetValue(static_cast<int>(model.balancing.balancerShafts.size()));

    int maxCounterweightCount = 0;
    for (const auto& shaft : model.balancing.balancerShafts)
        maxCounterweightCount = std::max(maxCounterweightCount, static_cast<int>(shaft.counterweights.size()));

    m_balancerCounterweightCountSpin->SetValue(maxCounterweightCount);

    RebuildBalancerShaftGrid();
    RebuildBalancerAxisGrid();
    RebuildBalancerSpeedGrid();
    RebuildBalancerPhaseGrid();

    for (int row = 0; row < static_cast<int>(model.balancing.balancerShafts.size()); ++row)
    {
        const auto& shaft = model.balancing.balancerShafts[static_cast<std::size_t>(row)];

        m_balancerShaftGrid->SetCellValue(row, 0, FormatOneDecimal(shaft.originXMm));
        m_balancerShaftGrid->SetCellValue(row, 1, FormatOneDecimal(shaft.originYMm));
        m_balancerShaftGrid->SetCellValue(row, 2, FormatOneDecimal(shaft.originZMm));
        m_balancerShaftGrid->SetCellValue(row, 3, FormatOneDecimal(shaft.lengthMm));
        m_balancerShaftGrid->SetCellValue(row, 4, FormatFiveDecimals(shaft.counterweightMassKg));
        m_balancerShaftGrid->SetCellValue(row, 5, FormatFiveDecimals(shaft.counterweightRadiusMm));

        m_balancerAxisGrid->SetCellValue(row, 0, AxisToString(shaft.axis));
        m_balancerSpeedGrid->SetCellValue(row, 0, SpeedRatioToString(shaft.speedRatio));

        for (int col = 0; col < static_cast<int>(shaft.counterweights.size()); ++col)
        {
            const int positionCol = 6 + col;
            if (positionCol < m_balancerShaftGrid->GetNumberCols())
            {
                m_balancerShaftGrid->SetCellValue(
                    row,
                    positionCol,
                    FormatOneDecimal(shaft.counterweights[static_cast<std::size_t>(col)].positionAlongShaftMm));
            }

            if (col < m_balancerPhaseGrid->GetNumberCols())
            {
                m_balancerPhaseGrid->SetCellValue(
                    row,
                    col,
                    FormatOneDecimal(shaft.counterweights[static_cast<std::size_t>(col)].phaseDeg));
            }
        }
    }

    NormalizeBalancerPositionCells();
    ApplyCrankCounterweightModeRules();

    m_isUpdating = false;
    Layout();
}

void CounterweightInputPanel::NormalizeBalancerPositionCells()
{
    if (!m_balancerShaftGrid || !m_balancerCounterweightCountSpin)
        return;

    for (int row = 0; row < m_balancerShaftGrid->GetNumberRows(); ++row)
        NormalizeBalancerPositionCellsForRow(row);
}

void CounterweightInputPanel::NormalizeBalancerPositionCellsForRow(int row)
{
    if (!m_balancerShaftGrid || !m_balancerCounterweightCountSpin)
        return;

    if (row < 0 || row >= m_balancerShaftGrid->GetNumberRows())
        return;

    const int counterweightCount = m_balancerCounterweightCountSpin->GetValue();
    const double lengthMm = std::max(0.0, ReadGridDouble(m_balancerShaftGrid, row, 3, 0.0));

    const bool previousUpdating = m_isUpdating;
    m_isUpdating = true;

    m_balancerShaftGrid->SetCellValue(row, 3, FormatGridNumber(lengthMm));

    for (int col = 0; col < counterweightCount; ++col)
    {
        const int positionCol = 6 + col;
        if (positionCol >= m_balancerShaftGrid->GetNumberCols())
            break;

        const double rawPositionMm =
            ReadGridDouble(m_balancerShaftGrid, row, positionCol, 0.0);

        const double clampedPositionMm =
            ClampPositionToLength(rawPositionMm, lengthMm);

        m_balancerShaftGrid->SetCellValue(
            row,
            positionCol,
            FormatGridNumber(clampedPositionMm));
    }

    m_isUpdating = previousUpdating;
}

double CounterweightInputPanel::ClampPositionToLength(double positionMm, double lengthMm) const
{
    const double safeLength = std::max(0.0, lengthMm);
    return std::clamp(positionMm, 0.0, safeLength);
}

wxString CounterweightInputPanel::FormatGridNumber(double value) const
{
    return wxString::Format("%.10g", value);
}

int CounterweightInputPanel::GetTotalCylinderCount() const
{
    if (!m_model.has_value())
        return 0;

    int total = 0;
    for (const auto& shaft : m_model->shafts)
        total += static_cast<int>(shaft.cylinders.size());

    return total;
}

std::optional<CounterweightCountMode> CounterweightInputPanel::GetForcedCrankCounterweightMode() const
{
    if (!m_model.has_value())
        return std::nullopt;

    const int totalCylinderCount = GetTotalCylinderCount();
    const bool isSemiSupported =
        (m_model->kinematic.supportType == SupportType::SemiSupported);

    if (isSemiSupported && totalCylinderCount > 1)
        return CounterweightCountMode::OnePerCrank;

    if ((totalCylinderCount % 2) != 0)
        return CounterweightCountMode::TwoPerCrank;

    return std::nullopt;
}

int CounterweightInputPanel::CountModeToChoiceIndex(CounterweightCountMode mode) const
{
    switch (mode)
    {
    case CounterweightCountMode::Auto:
        return 0;
    case CounterweightCountMode::OnePerCrank:
        return 1;
    case CounterweightCountMode::TwoPerCrank:
    default:
        return 2;
    }
}

void CounterweightInputPanel::ApplyCrankCounterweightModeRules()
{
    if (!m_crankCountModeChoice)
        return;

    const bool previousUpdating = m_isUpdating;
    m_isUpdating = true;

    const auto forcedMode = GetForcedCrankCounterweightMode();
    if (forcedMode.has_value())
    {
        m_crankCountModeChoice->SetSelection(CountModeToChoiceIndex(*forcedMode));
        m_crankCountModeChoice->Enable(false);
    }
    else
    {
        if (!m_crankCountModeChoice->IsEnabled())
            m_crankCountModeChoice->Enable(true);
    }

    m_isUpdating = previousUpdating;
}

EngineModel CounterweightInputPanel::BuildUpdatedModel() const
{
    if (!m_model.has_value())
        return EngineModel{};

    EngineModel result = *m_model;

    auto& crankSystem = result.balancing.crankCounterweights;
    crankSystem.enabled = true;
    crankSystem.massKg = ReadTextCtrlDouble(m_crankMassCtrl, 0.0);
    crankSystem.radiusMm = ReadTextCtrlDouble(m_crankRadiusCtrl, 0.0);
    crankSystem.countMode = ChoiceToCountMode();

    const int totalCylinderCount = GetTotalCylinderCount();
    const bool isSemiSupported =
        (result.kinematic.supportType == SupportType::SemiSupported);

    if (isSemiSupported && totalCylinderCount > 1)
        crankSystem.countMode = CounterweightCountMode::OnePerCrank;
    else if ((totalCylinderCount % 2) != 0)
        crankSystem.countMode = CounterweightCountMode::TwoPerCrank;

    crankSystem.entries.clear();

    for (auto& shaft : result.shafts)
    {
        for (const auto& crank : shaft.cranks)
        {
            CrankCounterweightEntry entry;
            entry.shaftNumber = shaft.shaftNumber;
            entry.crankNumber = crank.crankNumber;
            entry.phaseDeg = crank.phaseDeg;
            crankSystem.entries.push_back(entry);
        }
    }

    const int balancerShaftCount = m_balancerShaftCountSpin ? m_balancerShaftCountSpin->GetValue() : 0;
    const int balancerCounterweightCount =
        m_balancerCounterweightCountSpin ? m_balancerCounterweightCountSpin->GetValue() : 0;

    result.balancing.balancerShafts.clear();
    result.balancing.balancerShafts.resize(static_cast<std::size_t>(balancerShaftCount));

    for (int row = 0; row < balancerShaftCount; ++row)
    {
        auto& shaft = result.balancing.balancerShafts[static_cast<std::size_t>(row)];
        shaft.enabled = true;
        shaft.originXMm = ReadGridDouble(m_balancerShaftGrid, row, 0, 0.0);
        shaft.originYMm = ReadGridDouble(m_balancerShaftGrid, row, 1, 0.0);
        shaft.originZMm = ReadGridDouble(m_balancerShaftGrid, row, 2, 0.0);
        shaft.lengthMm = std::max(0.0, ReadGridDouble(m_balancerShaftGrid, row, 3, 0.0));
        shaft.counterweightMassKg = ReadGridDouble(m_balancerShaftGrid, row, 4, 0.0);
        shaft.counterweightRadiusMm = ReadGridDouble(m_balancerShaftGrid, row, 5, 0.0);

        shaft.axis = StringToAxis(m_balancerAxisGrid->GetCellValue(row, 0));
        shaft.speedRatio = StringToSpeedRatio(m_balancerSpeedGrid->GetCellValue(row, 0));

        shaft.shaftPhaseDeg = 0.0;
        shaft.counterweights.clear();

        for (int col = 0; col < balancerCounterweightCount; ++col)
        {
            BalancerCounterweightSpec cw;
            const double rawPositionMm =
                ReadGridDouble(m_balancerShaftGrid, row, 6 + col, 0.0);
            cw.positionAlongShaftMm = ClampPositionToLength(rawPositionMm, shaft.lengthMm);
            cw.phaseDeg = ReadGridDouble(m_balancerPhaseGrid, row, col, 0.0);
            shaft.counterweights.push_back(cw);
        }
    }

    return result;
}

void CounterweightInputPanel::NotifyDataChanged()
{
    if (m_isUpdating || !m_onDataChanged || !m_model.has_value())
        return;

    m_onDataChanged(BuildUpdatedModel());
}

double CounterweightInputPanel::ReadTextCtrlDouble(wxTextCtrl* ctrl, double fallback) const
{
    if (!ctrl)
        return fallback;

    double value = fallback;
    if (!ctrl->GetValue().ToDouble(&value))
        return fallback;

    return value;
}

double CounterweightInputPanel::ReadGridDouble(wxGrid* grid, int row, int col, double fallback) const
{
    if (!grid)
        return fallback;

    if (row < 0 || col < 0 || row >= grid->GetNumberRows() || col >= grid->GetNumberCols())
        return fallback;

    double value = fallback;
    if (!grid->GetCellValue(row, col).ToDouble(&value))
        return fallback;

    return value;
}

wxString CounterweightInputPanel::AxisToString(BalancerAxis axis) const
{
    switch (axis)
    {
    case BalancerAxis::X: return "X";
    case BalancerAxis::Y: return "Y";
    case BalancerAxis::Z:
    default: return "Z";
    }
}

BalancerAxis CounterweightInputPanel::StringToAxis(const wxString& text) const
{
    if (text == "X")
        return BalancerAxis::X;
    if (text == "Y")
        return BalancerAxis::Y;
    return BalancerAxis::Z;
}

wxString CounterweightInputPanel::SpeedRatioToString(BalancerSpeedRatio ratio) const
{
    switch (ratio)
    {
    case BalancerSpeedRatio::Plus1W: return "+1w";
    case BalancerSpeedRatio::Plus2W: return "+2w";
    case BalancerSpeedRatio::Minus1W: return "-1w";
    case BalancerSpeedRatio::Minus2W:
    default: return "-2w";
    }
}

BalancerSpeedRatio CounterweightInputPanel::StringToSpeedRatio(const wxString& text) const
{
    if (text == "+1w")
        return BalancerSpeedRatio::Plus1W;
    if (text == "+2w")
        return BalancerSpeedRatio::Plus2W;
    if (text == "-1w")
        return BalancerSpeedRatio::Minus1W;
    return BalancerSpeedRatio::Minus2W;
}

CounterweightCountMode CounterweightInputPanel::ChoiceToCountMode() const
{
    switch (m_crankCountModeChoice ? m_crankCountModeChoice->GetSelection() : 0)
    {
    case 1:
        return CounterweightCountMode::OnePerCrank;
    case 2:
        return CounterweightCountMode::TwoPerCrank;
    case 0:
    default:
        return CounterweightCountMode::Auto;
    }
}