#pragma once

#include <functional>
#include <wx/panel.h>

#include "core/model/engine_model.h"

class wxTextCtrl;
class wxButton;

class KinematicParamsPanel : public wxPanel
{
public:
    explicit KinematicParamsPanel(wxWindow* parent);

    bool FillModel(EngineModel& model, bool strict, wxString& errorText) const;
    void SetFromModel(const EngineModel& model);

    void SetOnDataChanged(std::function<void()> handler);
    void SetOnCalculate(std::function<void()> handler);

private:
    void BuildUi();
    void BindEvents();
    void NotifyDataChanged();

    bool ReadDouble(wxTextCtrl* ctrl, double& value, bool strict, const wxString& fieldName, wxString& errorText) const;

    wxTextCtrl* m_rpmCtrl = nullptr;
    wxTextCtrl* m_deaxialCtrl = nullptr;
    wxTextCtrl* m_radiusCtrl = nullptr;
    wxTextCtrl* m_lambdaCtrl = nullptr;

    wxTextCtrl* m_mainJournalLengthCtrl = nullptr;
    wxTextCtrl* m_rodJournalLengthCtrl = nullptr;
    wxTextCtrl* m_webThicknessCtrl = nullptr;

    wxButton* m_calculateButton = nullptr;

    std::function<void()> m_onDataChanged;
    std::function<void()> m_onCalculate;
};