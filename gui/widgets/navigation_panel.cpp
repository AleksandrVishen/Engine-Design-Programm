#include "gui/widgets/navigation_panel.h"

#include <utility>

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "gui/common/text_utf8.h"

NavigationPanel::NavigationPanel(wxWindow* parent)
    : wxPanel(parent)
{
    BuildUi();
    BindEvents();
    UpdateButtonStyles();
}

void NavigationPanel::SetOnPageSelected(std::function<void(PageId)> handler)
{
    m_onPageSelected = std::move(handler);
}

void NavigationPanel::SetSelectedPage(PageId pageId)
{
    m_selectedPage = pageId;
    UpdateButtonStyles();
}

void NavigationPanel::BuildUi()
{
    SetBackgroundColour(wxColour(12, 18, 28));
    SetForegroundColour(*wxWHITE);

    auto* root = new wxBoxSizer(wxVERTICAL);

    m_title = new wxStaticText(this, wxID_ANY, WXU8("РАСЧЁТ ДВИГАТЕЛЯ"));
    auto titleFont = m_title->GetFont();
    titleFont.SetPointSize(titleFont.GetPointSize() + 3);
    titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    m_title->SetFont(titleFont);
    m_title->SetForegroundColour(wxColour(240, 240, 240));

    root->Add(m_title, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 14);

    const long buttonStyle = wxBU_LEFT | wxBORDER_NONE;

    m_geometryButton = new wxButton(this, wxID_ANY, WXU8("Геометрия коленчатого вала"),
                                    wxDefaultPosition, wxDefaultSize, buttonStyle);

    m_resultsButton = new wxButton(this, wxID_ANY, WXU8("Результаты кинематики"),
                                   wxDefaultPosition, wxDefaultSize, buttonStyle);

    m_massButton = new wxButton(this, wxID_ANY, WXU8("Массовые характеристики"),
                                wxDefaultPosition, wxDefaultSize, buttonStyle);

    m_dynamicResultsButton = new wxButton(this, wxID_ANY, WXU8("Результаты динамики"),
                                          wxDefaultPosition, wxDefaultSize, buttonStyle);

    m_counterweightSetupButton = new wxButton(this, wxID_ANY, WXU8("Установка противовесов"),
                                              wxDefaultPosition, wxDefaultSize, buttonStyle);

    m_balancingResultsButton = new wxButton(this, wxID_ANY, WXU8("Результаты уравновешивания"),
                                            wxDefaultPosition, wxDefaultSize, buttonStyle);

    auto buttonFont = m_geometryButton->GetFont();
    buttonFont.SetPointSize(buttonFont.GetPointSize() + 1);

    m_geometryButton->SetFont(buttonFont);
    m_resultsButton->SetFont(buttonFont);
    m_massButton->SetFont(buttonFont);
    m_dynamicResultsButton->SetFont(buttonFont);
    m_counterweightSetupButton->SetFont(buttonFont);
    m_balancingResultsButton->SetFont(buttonFont);

    m_geometryButton->SetMinSize(wxSize(220, 34));
    m_resultsButton->SetMinSize(wxSize(220, 34));
    m_massButton->SetMinSize(wxSize(220, 34));
    m_dynamicResultsButton->SetMinSize(wxSize(220, 34));
    m_counterweightSetupButton->SetMinSize(wxSize(220, 34));
    m_balancingResultsButton->SetMinSize(wxSize(220, 34));

    root->Add(m_geometryButton, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 10);
    root->Add(m_resultsButton, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
    root->Add(m_massButton, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
    root->Add(m_dynamicResultsButton, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
    root->Add(m_counterweightSetupButton, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
    root->Add(m_balancingResultsButton, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
    root->AddStretchSpacer(1);

    SetSizer(root);
}

void NavigationPanel::BindEvents()
{
    m_geometryButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_selectedPage = PageId::Geometry;
        UpdateButtonStyles();
        if (m_onPageSelected)
            m_onPageSelected(PageId::Geometry);
    });

    m_resultsButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_selectedPage = PageId::KinematicResults;
        UpdateButtonStyles();
        if (m_onPageSelected)
            m_onPageSelected(PageId::KinematicResults);
    });

    m_massButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_selectedPage = PageId::MassProperties;
        UpdateButtonStyles();
        if (m_onPageSelected)
            m_onPageSelected(PageId::MassProperties);
    });

    m_dynamicResultsButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_selectedPage = PageId::DynamicResults;
        UpdateButtonStyles();
        if (m_onPageSelected)
            m_onPageSelected(PageId::DynamicResults);
    });

    m_counterweightSetupButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_selectedPage = PageId::CounterweightSetup;
        UpdateButtonStyles();
        if (m_onPageSelected)
            m_onPageSelected(PageId::CounterweightSetup);
    });

    m_balancingResultsButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        m_selectedPage = PageId::BalancingResults;
        UpdateButtonStyles();
        if (m_onPageSelected)
            m_onPageSelected(PageId::BalancingResults);
    });
}

void NavigationPanel::UpdateButtonStyles()
{
    auto applyStyle = [&](wxButton* button, PageId pageId)
    {
        if (!button)
            return;

        const bool isSelected = (m_selectedPage == pageId);

        if (isSelected)
        {
            button->SetBackgroundColour(wxColour(45, 120, 210));
            button->SetForegroundColour(wxColour(255, 255, 255));
        }
        else
        {
            button->SetBackgroundColour(wxColour(20, 28, 42));
            button->SetForegroundColour(wxColour(235, 235, 235));
        }

        button->Refresh();
    };

    applyStyle(m_geometryButton, PageId::Geometry);
    applyStyle(m_resultsButton, PageId::KinematicResults);
    applyStyle(m_massButton, PageId::MassProperties);
    applyStyle(m_dynamicResultsButton, PageId::DynamicResults);
    applyStyle(m_counterweightSetupButton, PageId::CounterweightSetup);
    applyStyle(m_balancingResultsButton, PageId::BalancingResults);

    Refresh();
}