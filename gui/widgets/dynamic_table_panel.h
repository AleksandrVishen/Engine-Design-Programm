#pragma once

#include <vector>
#include <wx/panel.h>

#include "core/dynamic/dynamic_result.h"
#include "gui/widgets/dynamic_metric.h"
#include "gui/widgets/dynamic_component.h"

class wxGrid;

class DynamicTablePanel : public wxPanel
{
public:
    explicit DynamicTablePanel(wxWindow* parent);

    void SetResult(const engine::dynamic::DynamicResult& result);
    void SetMetric(DynamicMetric metric);
    void SetComponent(DynamicComponent component);
    void SetCurrentAlphaIndex(std::size_t index);
    void SetSelectedCylinderIndices(const std::vector<int>& indices);
    void SetShowTotal(bool showTotal);

private:
    void RebuildGrid();

    const std::vector<engine::kinematic::Vec3>* GetCylinderSeriesValues(
        const engine::dynamic::CylinderDynamicSeries& cylinder) const;
    const std::vector<engine::kinematic::Vec3>* GetTotalSeriesValues() const;

    double ExtractComponent(const engine::kinematic::Vec3& v) const;
    wxString GetColumnLabel(int column) const;
    wxString FormatValue(double value) const;

private:
    engine::dynamic::DynamicResult m_result;
    DynamicMetric m_metric = DynamicMetric::InertiaForce;
    DynamicComponent m_component = DynamicComponent::Magnitude;
    std::size_t m_currentAlphaIndex = 0;
    std::vector<int> m_selectedCylinderIndices;
    bool m_showTotal = false;

    wxGrid* m_grid = nullptr;
};