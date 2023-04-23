#ifndef SWOOSH_APP_H_FILE
#define SWOOSH_APP_H_FILE

#include <wx/wx.h>

class SwooshApp : public wxApp
{
public:
  bool OnInit() override;
};

wxDECLARE_APP(SwooshApp);

wxString GetPathFilename(wxString path);
std::string GetPathFilename(std::string path);
int ReadFileSize(std::string file_name, uint32_t *file_size);

#endif /* SWOOSH_APP_H_FILE */
