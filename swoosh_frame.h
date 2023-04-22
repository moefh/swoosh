#ifndef SWOOSH_FRAME_H_FILE
#define SWOOSH_FRAME_H_FILE

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/aui/auibook.h>

#include "swoosh_node.h"

class SwooshFrame : public wxFrame, public SwooshNodeClient
{
protected:
  SwooshNode net;
  void OnNetNotify(const std::string &text);
  void OnNetReceivedData(SwooshData *data, const std::string &host, int port);

  wxFont messageTextFont;
  wxAuiNotebook *notebook;
  wxTextCtrl *sendText;
  wxButton *sendButton;
  void SetupMenu();
  void SetupContent();
  void SetupStatusBar();

  void AddTextPage(const std::string &title, const std::string &content);
  void SendTextMessage();

  void OnUrlClicked(wxTextUrlEvent &event);
  void OnSendTextKeyPressed(wxKeyEvent &event);

public:
  SwooshFrame();
  void OnSendClicked(wxCommandEvent &event);
  void OnExit(wxCommandEvent &event);
  void OnAbout(wxCommandEvent &event);
  void OnClose(wxCloseEvent &event);
  void OnQuit(wxCommandEvent &event);
};

#endif /* SWOOSH_FRAME_H_FILE */
