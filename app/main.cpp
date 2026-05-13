#include <wx/wx.h>
#include <wx/image.h>

#include "gui/frame/main_frame.h"

class EngineDesignApp final : public wxApp
{
public:
    bool OnInit() override
    {
        // PNG/JPEG и др.: без этого wxHtmlWindow не показывает встроенные в отчёт картинки (base64 PNG).
        wxInitAllImageHandlers();

        wxString startupProjectPath;
        if (argc > 1)
            startupProjectPath = argv[1];

        auto* frame = new MainFrame(startupProjectPath);
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(EngineDesignApp);