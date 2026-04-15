#include <wx/wx.h>
#include "gui/frame/main_frame.h"

class EngineDesignApp final : public wxApp
{
public:
    bool OnInit() override
    {
        auto* frame = new MainFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(EngineDesignApp);