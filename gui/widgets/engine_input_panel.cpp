#include "gui/widgets/engine_input_panel.h"

#include <cmath>
#include <utility>
#include <vector>

#include <wx/choice.h>
#include <wx/grid.h>
#include <wx/radiobut.h>
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

    double NormalizeDeg(double angleDeg)
    {
        double normalized = std::fmod(angleDeg, 360.0);
        if (normalized < 0.0)
            normalized += 360.0;
        return normalized;
    }

    wxString FormatPhase(double value)
    {
        return wxString::Format("%.10g", NormalizeDeg(value));
    }
}

EngineInputPanel::EngineInputPanel(wxWindow* parent)
    : wxPanel(parent)
{
    BuildUi();
    BindInternalEvents();
    RebuildTables();
    SetArticulatedMode(false);
}

void EngineInputPanel::SetOnDataChanged(std::function<void()> handler)
{
    m_onDataChanged = std::move(handler);
}

void EngineInputPanel::NotifyDataChanged()
{
    if (m_onDataChanged)
        m_onDataChanged();
}

void EngineInputPanel::SetAlphaStep(double alphaStepDeg)
{
    m_alphaStepDeg = alphaStepDeg;
    NotifyDataChanged();
}

double EngineInputPanel::GetAlphaStep() const
{
    return m_alphaStepDeg;
}

void EngineInputPanel::SetArticulatedMode(bool enabled)
{
    if (m_articulatedRadiusLabel) m_articulatedRadiusLabel->Show(enabled);
    if (m_articulatedRadiusCtrl) m_articulatedRadiusCtrl->Show(enabled);
    if (m_articulatedLengthLabel) m_articulatedLengthLabel->Show(enabled);
    if (m_articulatedLengthCtrl) m_articulatedLengthCtrl->Show(enabled);

    Layout();
    if (GetParent())
        GetParent()->Layout();
}

int EngineInputPanel::GetShaftCount() const
{
    return m_shaftCountSpin ? m_shaftCountSpin->GetValue() : 1;
}

int EngineInputPanel::GetCrankCount() const
{
    return m_crankCountSpin ? m_crankCountSpin->GetValue() : 1;
}

int EngineInputPanel::GetCylindersPerCrank() const
{
    if (!m_cylindersPerCrankChoice)
        return 1;

    long value = 1;
    m_cylindersPerCrankChoice->GetStringSelection().ToLong(&value);
    return static_cast<int>(value);
}

int EngineInputPanel::GetCylinderCountPerShaft() const
{
    return GetCrankCount() * GetCylindersPerCrank();
}

void EngineInputPanel::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(wxColour(235, 235, 235));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* topGrid = new wxFlexGridSizer(2, 6, 10, 14);
    topGrid->AddGrowableCol(1, 0);
    topGrid->AddGrowableCol(3, 0);
    topGrid->AddGrowableCol(5, 0);

    m_shaftCountSpin = new wxSpinCtrl(this, wxID_ANY);
    m_shaftCountSpin->SetRange(1, 8);
    m_shaftCountSpin->SetValue(1);
    m_shaftCountSpin->SetForegroundColour(*wxBLACK);
    m_shaftCountSpin->SetBackgroundColour(*wxWHITE);

    m_crankCountSpin = new wxSpinCtrl(this, wxID_ANY);
    m_crankCountSpin->SetRange(1, 16);
    m_crankCountSpin->SetValue(2);
    m_crankCountSpin->SetForegroundColour(*wxBLACK);
    m_crankCountSpin->SetBackgroundColour(*wxWHITE);

    wxArrayString cycleItems;
    cycleItems.Add("2");
    cycleItems.Add("4");
    m_cycleChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, cycleItems);
    m_cycleChoice->SetSelection(1);

    wxArrayString cpcItems;
    for (int i = 1; i <= 12; ++i)
        cpcItems.Add(wxString::Format("%d", i));
    m_cylindersPerCrankChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, cpcItems);
    m_cylindersPerCrankChoice->SetSelection(0);

    topGrid->Add(new wxStaticText(this, wxID_ANY, WXU8("Число коленчатых валов:")), 0, wxALIGN_CENTER_VERTICAL);
    topGrid->Add(m_shaftCountSpin, 0, wxEXPAND);

    topGrid->Add(new wxStaticText(this, wxID_ANY, WXU8("Число кривошипов:")), 0, wxALIGN_CENTER_VERTICAL);
    topGrid->Add(m_crankCountSpin, 0, wxEXPAND);

    topGrid->Add(new wxStaticText(this, wxID_ANY, WXU8("Тактность:")), 0, wxALIGN_CENTER_VERTICAL);
    topGrid->Add(m_cycleChoice, 0, wxEXPAND);

    topGrid->Add(new wxStaticText(this, wxID_ANY, WXU8("Цилиндров на шатунную шейку:")), 0, wxALIGN_CENTER_VERTICAL);
    topGrid->Add(m_cylindersPerCrankChoice, 0, wxEXPAND);

    root->Add(topGrid, 0, wxBOTTOM, 12);

    auto* rodSizer = MakeGroup(this, WXU8("Сочленение шатунов"));
    m_sideBySideRadio = new wxRadioButton(this, wxID_ANY, WXU8("Рядом сидящие шатуны"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_articulatedRadio = new wxRadioButton(this, wxID_ANY, WXU8("Прицепной шатун"));

    m_sideBySideRadio->SetForegroundColour(wxColour(235, 235, 235));
    m_articulatedRadio->SetForegroundColour(wxColour(235, 235, 235));

    rodSizer->Add(m_sideBySideRadio, 0, wxALL, 4);
    rodSizer->Add(m_articulatedRadio, 0, wxALL, 4);

    m_articulatedSizer = new wxBoxSizer(wxHORIZONTAL);

    m_articulatedRadiusLabel = new wxStaticText(this, wxID_ANY, WXU8("Радиус прицепного шатуна, м:"));
    m_articulatedRadiusCtrl = new wxTextCtrl(this, wxID_ANY, "0.010");

    m_articulatedLengthLabel = new wxStaticText(this, wxID_ANY, WXU8("Длина прицепного шатуна, м:"));
    m_articulatedLengthCtrl = new wxTextCtrl(this, wxID_ANY, "0.080");

    m_articulatedSizer->Add(m_articulatedRadiusLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    m_articulatedSizer->Add(m_articulatedRadiusCtrl, 0, wxRIGHT, 20);
    m_articulatedSizer->Add(m_articulatedLengthLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    m_articulatedSizer->Add(m_articulatedLengthCtrl, 0);

    rodSizer->Add(m_articulatedSizer, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 4);

    root->Add(rodSizer, 0, wxEXPAND | wxBOTTOM, 12);

    auto* supportSizer = MakeGroup(this, WXU8("Опоры"));
    m_fullySupportedRadio = new wxRadioButton(this, wxID_ANY, WXU8("Полноопорный"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_semiSupportedRadio = new wxRadioButton(this, wxID_ANY, WXU8("Неполноопорный"));

    m_fullySupportedRadio->SetForegroundColour(wxColour(235, 235, 235));
    m_semiSupportedRadio->SetForegroundColour(wxColour(235, 235, 235));

    supportSizer->Add(m_fullySupportedRadio, 0, wxALL, 4);
    supportSizer->Add(m_semiSupportedRadio, 0, wxALL, 4);
    root->Add(supportSizer, 0, wxEXPAND | wxBOTTOM, 12);

    auto* originTitle = new wxStaticText(this, wxID_ANY, WXU8("Координаты начала коленчатых валов."));
    originTitle->SetForegroundColour(wxColour(235, 235, 235));
    root->Add(originTitle, 0, wxBOTTOM, 4);

    m_shaftOriginGrid = new wxGrid(this, wxID_ANY);
    m_shaftOriginGrid->CreateGrid(1, 3);
    m_shaftOriginGrid->EnableEditing(true);
    m_shaftOriginGrid->EnableDragRowSize(false);
    m_shaftOriginGrid->SetColLabelValue(0, WXU8("X, мм"));
    m_shaftOriginGrid->SetColLabelValue(1, WXU8("Y, мм"));
    m_shaftOriginGrid->SetColLabelValue(2, WXU8("Z, мм"));
    m_shaftOriginGrid->SetMinSize(wxSize(510, 120));
    SetupGridColors(m_shaftOriginGrid);
    root->Add(m_shaftOriginGrid, 0, wxEXPAND | wxBOTTOM, 14);

    auto* phaseTitle = new wxStaticText(this, wxID_ANY, WXU8("Геометрическая фаза кривошипов в град."));
    phaseTitle->SetForegroundColour(wxColour(235, 235, 235));
    root->Add(phaseTitle, 0, wxBOTTOM, 4);

    m_phaseGrid = new wxGrid(this, wxID_ANY);
    m_phaseGrid->CreateGrid(1, 1);
    m_phaseGrid->EnableEditing(true);
    m_phaseGrid->EnableDragRowSize(false);
    m_phaseGrid->SetMinSize(wxSize(510, 140));
    SetupGridColors(m_phaseGrid);
    root->Add(m_phaseGrid, 0, wxEXPAND | wxBOTTOM, 8);

    auto* tiltTitle = new wxStaticText(this, wxID_ANY, WXU8("Поворот осей цилиндров в град."));
    tiltTitle->SetForegroundColour(wxColour(235, 235, 235));
    root->Add(tiltTitle, 0, wxBOTTOM, 4);

    m_axisTiltGrid = new wxGrid(this, wxID_ANY);
    m_axisTiltGrid->CreateGrid(1, 1);
    m_axisTiltGrid->EnableEditing(true);
    m_axisTiltGrid->EnableDragRowSize(false);
    m_axisTiltGrid->SetMinSize(wxSize(510, 140));
    SetupGridColors(m_axisTiltGrid);
    root->Add(m_axisTiltGrid, 0, wxEXPAND);

    SetSizer(root);
    Layout();
}

void EngineInputPanel::BindInternalEvents()
{
    m_shaftCountSpin->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&)
    {
        RebuildTables();
        NotifyDataChanged();
    });

    m_crankCountSpin->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent&)
    {
        RebuildTables();
        NotifyDataChanged();
    });

    m_cycleChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        NotifyDataChanged();
    });

    m_cylindersPerCrankChoice->Bind(wxEVT_CHOICE, [this](wxCommandEvent&)
    {
        RebuildTables();
        NotifyDataChanged();
    });

    m_sideBySideRadio->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&)
    {
        SetArticulatedMode(false);
        NotifyDataChanged();
    });

    m_articulatedRadio->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&)
    {
        SetArticulatedMode(true);
        NotifyDataChanged();
    });

    m_fullySupportedRadio->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&)
    {
        RebuildTables();
        NotifyDataChanged();
    });

    m_semiSupportedRadio->Bind(wxEVT_RADIOBUTTON, [this](wxCommandEvent&)
    {
        RebuildTables();
        NotifyDataChanged();
    });

    m_articulatedRadiusCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });
    m_articulatedLengthCtrl->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { NotifyDataChanged(); });

    m_shaftOriginGrid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent&) { NotifyDataChanged(); });

    m_phaseGrid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent& event)
    {
        if (m_semiSupportedRadio && m_semiSupportedRadio->GetValue())
        {
            const int row = event.GetRow();
            const int col = event.GetCol();

            if ((col % 2) == 0)
            {
                double value = 0.0;
                const wxString text = m_phaseGrid->GetCellValue(row, col).Trim(true).Trim(false);
                if (text.ToDouble(&value) && (col + 1) < m_phaseGrid->GetNumberCols())
                {
                    m_phaseGrid->SetCellValue(row, col + 1, FormatPhase(value + 180.0));
                }
            }
            else
            {
                double firstValue = 0.0;
                if ((col - 1) >= 0)
                {
                    const wxString firstText =
                        m_phaseGrid->GetCellValue(row, col - 1).Trim(true).Trim(false);
                    if (firstText.ToDouble(&firstValue))
                    {
                        m_phaseGrid->SetCellValue(row, col, FormatPhase(firstValue + 180.0));
                    }
                }
            }
        }

        NotifyDataChanged();
    });

    m_axisTiltGrid->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent&) { NotifyDataChanged(); });
}

void EngineInputPanel::RebuildTables()
{
    const int shaftCount = GetShaftCount();
    const int crankCount = GetCrankCount();
    const int cylinderCountPerShaft = GetCylinderCountPerShaft();

    const auto rebuildRowsCols = [](wxGrid* grid, int rows, int cols)
    {
        const int currentRows = grid->GetNumberRows();
        const int currentCols = grid->GetNumberCols();

        if (currentRows < rows)
            grid->AppendRows(rows - currentRows);
        else if (currentRows > rows)
            grid->DeleteRows(0, currentRows - rows);

        if (currentCols < cols)
            grid->AppendCols(cols - currentCols);
        else if (currentCols > cols)
            grid->DeleteCols(0, currentCols - cols);
    };

    rebuildRowsCols(m_shaftOriginGrid, shaftCount, 3);
    rebuildRowsCols(m_phaseGrid, shaftCount, crankCount);
    rebuildRowsCols(m_axisTiltGrid, shaftCount, cylinderCountPerShaft);

    for (int row = 0; row < shaftCount; ++row)
    {
        m_shaftOriginGrid->SetRowLabelValue(row, wxString::Format(WXU8("КВ %d"), row + 1));
        m_phaseGrid->SetRowLabelValue(row, wxString::Format(WXU8("КВ %d"), row + 1));
        m_axisTiltGrid->SetRowLabelValue(row, wxString::Format(WXU8("КВ %d"), row + 1));
    }

    m_shaftOriginGrid->SetColLabelValue(0, WXU8("X, мм"));
    m_shaftOriginGrid->SetColLabelValue(1, WXU8("Y, мм"));
    m_shaftOriginGrid->SetColLabelValue(2, WXU8("Z, мм"));

    for (int col = 0; col < crankCount; ++col)
    {
        m_phaseGrid->SetColLabelValue(col, wxString::Format(WXU8("Кривошип %d"), col + 1));
        m_phaseGrid->SetColSize(col, 105);
    }

    for (int col = 0; col < cylinderCountPerShaft; ++col)
    {
        m_axisTiltGrid->SetColLabelValue(col, wxString::Format(WXU8("Цилиндр %d"), col + 1));
        m_axisTiltGrid->SetColSize(col, 105);
    }

    const bool isSemiSupported = m_semiSupportedRadio && m_semiSupportedRadio->GetValue();

    for (int row = 0; row < shaftCount; ++row)
    {
        if (m_shaftOriginGrid->GetCellValue(row, 0).IsEmpty())
            m_shaftOriginGrid->SetCellValue(row, 0, "0");
        if (m_shaftOriginGrid->GetCellValue(row, 1).IsEmpty())
            m_shaftOriginGrid->SetCellValue(row, 1, "0");
        if (m_shaftOriginGrid->GetCellValue(row, 2).IsEmpty())
            m_shaftOriginGrid->SetCellValue(row, 2, "0");

        for (int col = 0; col < crankCount; ++col)
        {
            if (m_phaseGrid->GetCellValue(row, col).IsEmpty())
                m_phaseGrid->SetCellValue(row, col, "0");
        }

        if (isSemiSupported)
        {
            for (int col = 1; col < crankCount; col += 2)
            {
                double firstValue = 0.0;
                const wxString firstText =
                    m_phaseGrid->GetCellValue(row, col - 1).Trim(true).Trim(false);

                if (firstText.ToDouble(&firstValue))
                {
                    m_phaseGrid->SetCellValue(row, col, FormatPhase(firstValue + 180.0));
                }
                else
                {
                    m_phaseGrid->SetCellValue(row, col, "180");
                }
            }
        }

        for (int col = 0; col < cylinderCountPerShaft; ++col)
        {
            if (m_axisTiltGrid->GetCellValue(row, col).IsEmpty())
                m_axisTiltGrid->SetCellValue(row, col, "0");
        }
    }

    Layout();
}

bool EngineInputPanel::ReadDouble(wxTextCtrl* ctrl, double& value, bool strict, const wxString& fieldName, wxString& errorText) const
{
    if (!ctrl)
        return false;

    const wxString text = ctrl->GetValue().Trim(true).Trim(false);

    if (text.IsEmpty())
    {
        if (strict)
        {
            errorText = WXU8("Не заполнено поле: ") + fieldName;
            return false;
        }

        value = 0.0;
        return true;
    }

    if (!text.ToDouble(&value))
    {
        if (strict)
        {
            errorText = WXU8("Некорректное числовое значение в поле: ") + fieldName;
            return false;
        }

        value = 0.0;
    }

    return true;
}

bool EngineInputPanel::ReadGridRow(wxGrid* grid, int row, int expectedCols, std::vector<double>& values, bool strict, const wxString& gridName, wxString& errorText) const
{
    values.clear();

    if (!grid)
        return false;

    for (int col = 0; col < expectedCols; ++col)
    {
        const wxString text = grid->GetCellValue(row, col).Trim(true).Trim(false);

        if (text.IsEmpty())
        {
            if (strict)
            {
                errorText = wxString::Format(WXU8("%s: пустая ячейка в строке %d, столбце %d."), gridName, row + 1, col + 1);
                return false;
            }

            values.push_back(0.0);
            continue;
        }

        double value = 0.0;
        if (!text.ToDouble(&value))
        {
            if (strict)
            {
                errorText = wxString::Format(WXU8("%s: некорректное число в строке %d, столбце %d."), gridName, row + 1, col + 1);
                return false;
            }

            value = 0.0;
        }

        values.push_back(value);
    }

    return true;
}

bool EngineInputPanel::ReadGeneralParams(EngineModel& model, bool strict, wxString& errorText) const
{
    model.kinematic.cycleType =
        (m_cycleChoice->GetStringSelection() == "2") ? CycleType::TwoStroke : CycleType::FourStroke;

    model.kinematic.shaftCount = GetShaftCount();
    model.kinematic.crankCountPerShaft = GetCrankCount();
    model.kinematic.cylindersPerCrank = GetCylindersPerCrank();
    model.kinematic.rodJointType = m_articulatedRadio->GetValue() ? RodJointType::Articulated : RodJointType::SideBySide;
    model.kinematic.supportType = m_semiSupportedRadio->GetValue() ? SupportType::SemiSupported : SupportType::FullySupported;
    model.kinematic.alphaStepDeg = m_alphaStepDeg;

    if (model.kinematic.rodJointType == RodJointType::Articulated)
    {
        if (!ReadDouble(m_articulatedRadiusCtrl, model.kinematic.articulatedRodRadiusM, strict, WXU8("Радиус прицепного шатуна"), errorText)) return false;
        if (!ReadDouble(m_articulatedLengthCtrl, model.kinematic.articulatedRodLengthM, strict, WXU8("Длина прицепного шатуна"), errorText)) return false;
    }

    return true;
}

bool EngineInputPanel::ReadShaftOrigins(EngineModel& model, bool strict, wxString& errorText) const
{
    const int shaftCount = GetShaftCount();

    model.shafts.clear();
    model.shafts.resize(static_cast<size_t>(shaftCount));

    for (int row = 0; row < shaftCount; ++row)
    {
        std::vector<double> coords;
        if (!ReadGridRow(m_shaftOriginGrid, row, 3, coords, strict, WXU8("Таблица начал коленчатых валов"), errorText))
            return false;

        auto& shaft = model.shafts[static_cast<size_t>(row)];
        shaft.shaftNumber = row + 1;
        shaft.originXMm = coords[0];
        shaft.originYMm = coords[1];
        shaft.originZMm = coords[2];
    }

    return true;
}

bool EngineInputPanel::ReadCranks(EngineModel& model, bool strict, wxString& errorText) const
{
    const int shaftCount = GetShaftCount();
    const int crankCount = GetCrankCount();
    const bool isSemiSupported = m_semiSupportedRadio && m_semiSupportedRadio->GetValue();

    for (int row = 0; row < shaftCount; ++row)
    {
        std::vector<double> phases;
        if (!ReadGridRow(m_phaseGrid, row, crankCount, phases, strict, WXU8("Таблица фаз кривошипов"), errorText))
            return false;

        auto& shaft = model.shafts[static_cast<size_t>(row)];
        shaft.cranks.clear();

        for (int i = 0; i < crankCount; ++i)
        {
            CrankshaftThrow crank;
            crank.crankNumber = i + 1;
            crank.phaseDeg = phases[static_cast<size_t>(i)];
            shaft.cranks.push_back(crank);
        }

        if (isSemiSupported)
        {
            for (int i = 1; i < crankCount; i += 2)
            {
                shaft.cranks[static_cast<size_t>(i)].phaseDeg =
                    NormalizeDeg(shaft.cranks[static_cast<size_t>(i - 1)].phaseDeg + 180.0);
            }
        }
    }

    return true;
}

bool EngineInputPanel::ReadCylinders(EngineModel& model, bool strict, wxString& errorText) const
{
    const int shaftCount = GetShaftCount();
    const int cylinderCountPerShaft = GetCylinderCountPerShaft();

    for (int row = 0; row < shaftCount; ++row)
    {
        std::vector<double> tilts;
        if (!ReadGridRow(m_axisTiltGrid, row, cylinderCountPerShaft, tilts, strict, WXU8("Таблица смещений осей цилиндров"), errorText))
            return false;

        auto& shaft = model.shafts[static_cast<size_t>(row)];
        shaft.cylinders.clear();

        for (int i = 0; i < cylinderCountPerShaft; ++i)
        {
            CylinderSpec cylinder;
            cylinder.cylinderNumber = i + 1;
            cylinder.crankNumber = (i / GetCylindersPerCrank()) + 1;
            cylinder.axisTiltDeg = tilts[static_cast<size_t>(i)];
            cylinder.axisPositionZ = 0.0;
            cylinder.firingAngleDeg = 0.0;
            shaft.cylinders.push_back(cylinder);
        }
    }

    return true;
}

bool EngineInputPanel::BuildPreviewModel(EngineModel& model) const
{
    wxString errorText;
    const bool strict = false;

    if (!ReadGeneralParams(model, strict, errorText))
        return false;
    if (!ReadShaftOrigins(model, strict, errorText))
        return false;
    if (!ReadCranks(model, strict, errorText))
        return false;
    if (!ReadCylinders(model, strict, errorText))
        return false;

    return true;
}

bool EngineInputPanel::BuildCalculationModel(EngineModel& model, wxString& errorText) const
{
    const bool strict = true;

    if (!ReadGeneralParams(model, strict, errorText))
        return false;
    if (!ReadShaftOrigins(model, strict, errorText))
        return false;
    if (!ReadCranks(model, strict, errorText))
        return false;
    if (!ReadCylinders(model, strict, errorText))
        return false;

    return true;
}