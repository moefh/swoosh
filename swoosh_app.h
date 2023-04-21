#ifndef SWOOSH_APP_H_FILE
#define SWOOSH_APP_H_FILE

#include <wx/wx.h>

class SwooshApp : public wxApp
{
public:
  bool OnInit() override;
};

wxDECLARE_APP(SwooshApp);

#endif /* SWOOSH_APP_H_FILE */
