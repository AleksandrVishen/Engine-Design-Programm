#include "gui/widgets/kinematic_legend_panel.h"

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "gui/common/text_utf8.h"

KinematicLegendPanel::KinematicLegendPanel(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxBORDER_SIMPLE)
{
    SetScrollRate(0, 12);
    SetBackgroundColour(wxColour(22, 28, 42));
    SetForegroundColour(wxColour(235, 235, 235));

    SetMinSize(wxSize(-1, 130));
    SetMaxSize(wxSize(-1, 150));

    m_rootSizer = new wxBoxSizer(wxVERTICAL);
    SetSizer(m_rootSizer);

    auto* placeholder = new wxStaticText(this, wxID_ANY, WXU8("Легенда появится после расчета."));
    placeholder->SetForegroundColour(wxColour(210, 210, 210));
    m_rootSizer->Add(placeholder, 0, wxALL, 10);
}

wxColour KinematicLegendPanel::GetSeriesColour(std::size_t index) const
{
    static const wxColour palette[] =
    {
        wxColour(255, 99, 132),
        wxColour(54, 162, 235),
        wxColour(255, 206, 86),
        wxColour(75, 192, 192),
        wxColour(153, 102, 255),
        wxColour(255, 159, 64),
        wxColour(0, 200, 140),
        wxColour(220, 220, 220)
    };

    return palette[index % (sizeof(palette) / sizeof(palette[0]))];
}

void KinematicLegendPanel::SetResult(const engine::kinematic::KinematicResult& result)
{
    if (!m_rootSizer)
        return;

    m_rootSizer->Clear(true);

    auto* title = new wxStaticText(this, wxID_ANY, WXU8("Цилиндры"));
    title->SetForegroundColour(wxColour(240, 240, 240));
    m_rootSizer->Add(title, 0, wxLEFT | wxTOP | wxRIGHT | wxBOTTOM, 10);

    constexpr int kColumnCount = 4;

    auto* gridSizer = new wxGridSizer(kColumnCount, 6, 16);

    for (std::size_t i = 0; i < result.cylinders.size(); ++i)
    {
        const auto& cylinder = result.cylinders[i];

        auto* itemPanel = new wxPanel(this);
        itemPanel->SetBackgroundColour(wxColour(22, 28, 42));

        auto* itemSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* swatch = new wxPanel(itemPanel, wxID_ANY, wxDefaultPosition, wxSize(22, 8));
        swatch->SetMinSize(wxSize(22, 8));
        swatch->SetMaxSize(wxSize(22, 8));
        swatch->SetBackgroundColour(GetSeriesColour(i));

        const wxString typeText =
            (cylinder.linkType == engine::kinematic::CylinderLinkType::Main)
                ? WXU8("Main")
                : WXU8("Articulated");

        auto* label = new wxStaticText(
            itemPanel,
            wxID_ANY,
            wxString::Format("Cyl %d | %s", cylinder.cylinderNumber, typeText));
        label->SetForegroundColour(wxColour(225, 225, 225));

        itemSizer->Add(swatch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        itemSizer->Add(label, 0, wxALIGN_CENTER_VERTICAL);

        itemPanel->SetSizer(itemSizer);

        gridSizer->Add(itemPanel, 0, wxEXPAND);
    }

    m_rootSizer->Add(gridSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

    Layout();
    FitInside();
}