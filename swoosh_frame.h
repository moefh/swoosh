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

  wxImageList *imageList;
  wxFont messageTextFont;
  wxAuiNotebook *notebook;
  wxTextCtrl *sendText;
  wxButton *sendButton;

  std::map<SwooshRemotePermanentData *, wxDataViewItem> remoteDataItems;
  wxDataViewListCtrl *remoteDataList;
  wxDataViewListCtrl *localDataList;

  void SetupMenu();
  void SetupContent();
  void SetupTextMessagesPanel(wxWindow *parent);
  void SetupFileMessagesPanel(wxWindow *parent);
  void SetupStatusBar();

  void AddTextMessage(const std::string &title, const std::string &content);
  void AddRemoteData(SwooshRemotePermanentData *data);
  void AddLocalFile(std::string file_name);
  void AddLocalDir(std::string file_name);
  void SendTextMessage();

  void OnUrlClicked(wxTextUrlEvent &event);
  void OnSendTextKeyPressed(wxKeyEvent &event);
  void OnSendTextClicked(wxCommandEvent &event);
  void OnLocalFileActivated(wxDataViewEvent &event);
  void OnRemoteFileActivated(wxDataViewEvent &event);
  void OnAddSendFileClicked(wxCommandEvent &event);
  void OnAddSendDirClicked(wxCommandEvent &event);
  void OnExit(wxCommandEvent &event);
  void OnAbout(wxCommandEvent &event);
  void OnClose(wxCloseEvent &event);
  void OnQuit(wxCommandEvent &event);

  // from SwooshNodeClient -- these handlers will be called from other threads:
  virtual void OnNetNotify(const std::string &text);
  virtual void OnNetReceivedData(SwooshRemoteData *data);
  virtual void OnNetDataDownloading(SwooshRemotePermanentData *data, double progress);
  virtual void OnNetDataDownloaded(SwooshRemotePermanentData *data, bool success);

public:
  SwooshFrame();
};

#endif /* SWOOSH_FRAME_H_FILE */
