
#include "swoosh_app.h"
#include "swoosh_frame.h"

wxIMPLEMENT_APP(SwooshApp);

bool SwooshApp::OnInit()
{
    SwooshFrame* frame = new SwooshFrame();
    frame->Show(true);
    return true;
}
