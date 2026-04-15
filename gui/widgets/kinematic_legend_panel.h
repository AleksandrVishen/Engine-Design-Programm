#pragma once

#include <wx/scrolwin.h>

#include "core/kinematic/kinematic_result.h"

class wxBoxSizer;

class KinematicLegendPanel : public wxScrolledWindow
{
public:
    explicit KinematicLegendPanel(wxWindow* parent);

    void SetResult(const engine::kinematic::KinematicResult& result);

private:
    wxColour GetSeriesColour(std::size_t index) const;

private:
    wxBoxSizer* m_rootSizer = nullptr;
};