#ifndef SWOOSH_FRAME_H_FILE
#define SWOOSH_FRAME_H_FILE

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/aui/auibook.h>
#include <wx/dataview.h>

#include "swoosh_node.h"

class SwooshFrame : public wxFrame, public SwooshNodeClient
{
protected:
  SwooshNode net;
  virtual void OnNetNotify(const std::string &text);
  virtual void OnNetReceivedData(SwooshData *data);
  virtual void OnNetDataDownloading(SwooshData *data, double progress);
  virtual void OnNetDataDownloaded(SwooshData *data, bool success);

  wxImageList *imageList;
  wxFont messageTextFont;
  wxAuiNotebook *notebook;
  wxTextCtrl *sendText;
  wxButton *sendButton;

  std::map<SwooshData *, wxDataViewItem> remoteFileDataItem;
  wxDataViewListCtrl *remoteFileList;
  wxDataViewListCtrl *localFileList;

  void SetupMenu();
  void SetupContent();
  void SetupTextMessagesPanel(wxWindow *parent);
  void SetupFileMessagesPanel(wxWindow *parent);
  void SetupStatusBar();

  void AddTextMessagePage(const std::string &title, const std::string &content);
  void AddRemoteFile(SwooshFileData *file);

  void SendTextMessage();
  void SendFile(std::string file_name);

  void OnUrlClicked(wxTextUrlEvent &event);
  void OnSendTextKeyPressed(wxKeyEvent &event);
  void OnSendTextClicked(wxCommandEvent &event);
  void OnRemoteFileActivated(wxDataViewEvent &event);
  void OnAddSendFileClicked(wxCommandEvent &event);
  void OnAddSendDirClicked(wxCommandEvent &event);
  void OnExit(wxCommandEvent &event);
  void OnAbout(wxCommandEvent &event);
  void OnClose(wxCloseEvent &event);
  void OnQuit(wxCommandEvent &event);

public:
  SwooshFrame();
};

#endif /* SWOOSH_FRAME_H_FILE */
