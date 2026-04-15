#pragma once

#include <functional>
#include <wx/panel.h>

class wxButton;
class wxStaticText;

class NavigationPanel : public wxPanel
{
public:
    enum class PageId
    {
        Geometry = 0,
        KinematicResults = 1,
        MassProperties = 2,
        DynamicResults = 3,
        CounterweightSetup = 4,
        BalancingResults = 5
    };

    explicit NavigationPanel(wxWindow* parent);

    void SetOnPageSelected(std::function<void(PageId)> handler);
    void SetSelectedPage(PageId pageId);

private:
    void BuildUi();
    void BindEvents();
    void UpdateButtonStyles();

private:
    wxStaticText* m_title = nullptr;

    wxButton* m_geometryButton = nullptr;
    wxButton* m_resultsButton = nullptr;
    wxButton* m_massButton = nullptr;
    wxButton* m_dynamicResultsButton = nullptr;
    wxButton* m_counterweightSetupButton = nullptr;
    wxButton* m_balancingResultsButton = nullptr;

    PageId m_selectedPage = PageId::Geometry;

    std::function<void(PageId)> m_onPageSelected;
};