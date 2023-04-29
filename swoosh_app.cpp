#include "targetver.h"
#include "swoosh_app.h"

#include "swoosh_frame.h"
#include "util.h"

wxIMPLEMENT_APP(SwooshApp);

bool SwooshApp::OnInit()
{
  wxInitAllImageHandlers();
  SwooshFrame *frame = new SwooshFrame();
  frame->Show(true);
  return true;
}

int ReadFileSize(std::string file_name, uint32_t *file_size)
{
  wxStructStat stat;
  if (wxStat(file_name, &stat) == 0) {
    *file_size = (uint32_t) stat.st_size;
    return 0;
  }
  return -1;
}

std::string GetPathFilename(std::string path)
{
  for (size_t i = 0; i < path.size(); i++) if (path[i] == '\\') path[i] = '/';

  auto last_slash = path.find_last_of('/');
  if (last_slash == wxString::npos) {
    last_slash = 0;
  } else {
    last_slash++;
  }
  return path.substr(last_slash, path.length());
}
