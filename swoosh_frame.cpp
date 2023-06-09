#include "targetver.h"
#include "swoosh_frame.h"

#include <vector>
#include <thread>
#include <iostream>
#include <functional>
#include <wx/splitter.h>
#include <wx/listbook.h>

#include "swoosh_app.h"
#include "util.h"

#include "data/folder.xpm"
#include "data/write.xpm"

#define UDP_SERVER_PORT 5559
#define TCP_SERVER_PORT 5559
#define USE_IPV6        0

enum {
  ID_SendTextMessage = wxID_HIGHEST + 1,
};

SwooshFrame::SwooshFrame()
  : wxFrame(nullptr, wxID_ANY, "Swoosh", wxDefaultPosition, wxSize(800, 600)),
    net(*this, UDP_SERVER_PORT, TCP_SERVER_PORT, USE_IPV6)
{
  messageTextFont.Create(12, wxFontFamily::wxFONTFAMILY_TELETYPE, wxFontStyle::wxFONTSTYLE_NORMAL, wxFontWeight::wxFONTWEIGHT_NORMAL);

  imageList = new wxImageList(32, 32);
  imageList->Add(wxBitmap(wxBitmap(write_xpm).ConvertToImage().Rescale(32, 32)));
  imageList->Add(wxBitmap(wxBitmap(folder_xpm).ConvertToImage().Rescale(32, 32)));

  SetupMenu();
  SetupStatusBar();
  SetupContent();
  SetMinSize(wxSize(640, 480));

  Bind(wxEVT_MENU, &SwooshFrame::OnAbout, this, wxID_ABOUT);
  Bind(wxEVT_MENU, &SwooshFrame::OnExit, this, wxID_EXIT);
  Bind(wxEVT_SIZE, &SwooshFrame::OnSize, this);
  Bind(wxEVT_CLOSE_WINDOW, &SwooshFrame::OnClose, this);

  Connect(ID_SendTextMessage, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SwooshFrame::OnSendTextClicked));
  wxAcceleratorEntry entries[] = {
    wxAcceleratorEntry(wxACCEL_CTRL, WXK_RETURN, ID_SendTextMessage),
  };
  this->SetAcceleratorTable(wxAcceleratorTable(sizeof(entries)/sizeof(entries[0]), entries));
}

void SwooshFrame::SendTextMessage()
{
  auto text = sendText->GetValue();
  if (text.length() != 0) {
    auto data = new SwooshLocalTextData(net.GenerateMessageId(), SwooshNode::GetTime(5000), text.ToStdString());
    net.AddLocalData(data);     // data must be added before the beacon is sent
    net.SendDataBeacon(data->GetMessageId());
    net.ReleaseLocalData(data); // so the message can expire
    sendText->SetValue("");
  }
}

void SwooshFrame::AddLocalFile(std::string file_name)
{
  // add data to serve and broadcast beacon
  auto data = new SwooshLocalFileData(net.GenerateMessageId(), file_name);
  net.AddLocalData(data);       // data must be added before the beacon is sent
  net.SendDataBeacon(data->GetMessageId());
  net.ReleaseLocalData(data);   // so the message can expire (even though files never expire)

  // add to local file list table
  auto name = GetPathFilename(file_name);
  wxVector<wxVariant> row;
  row.push_back(wxVariant("file"));
  row.push_back(wxVariant(name));
  row.push_back(wxVariant(file_name));
  localDataList->AppendItem(row, (wxUIntPtr) data);
  localDataList->Refresh();
}

void SwooshFrame::AddLocalDir(std::string dir_name)
{
  // add data to serve and broadcast beacon
  auto data = new SwooshLocalDirData(net.GenerateMessageId(), dir_name);
  net.AddLocalData(data);       // data must be added before the beacon is sent
  net.SendDataBeacon(data->GetMessageId());
  net.ReleaseLocalData(data);   // so the message can expire (even though files never expire)

  // add to local file list table
  auto name = GetPathFilename(dir_name);
  wxVector<wxVariant> row;
  row.push_back(wxVariant("dir"));
  row.push_back(wxVariant(name));
  row.push_back(wxVariant(dir_name));
  localDataList->AppendItem(row, (wxUIntPtr) data);
  localDataList->Refresh();
}

void SwooshFrame::AddRemoteData(SwooshRemotePermanentData *data)
{
  // check the beacon to see if we already have this file
  for (int row = 0; row < remoteDataList->GetItemCount(); row++) {
    auto item = remoteDataList->RowToItem(row);
    SwooshRemoteData *item_data = (SwooshRemoteData *) remoteDataList->GetItemData(item);
    if (net.BeaconsAreEqual(data->GetBeacon(), item_data->GetBeacon())) {
      delete data;
      return;
    }
  }

  // add to remote file list table
  wxVector<wxVariant> row;
  switch (data->GetType()) {
  case SWOOSH_DATA_FILE: row.push_back(wxVariant("file")); break;
  case SWOOSH_DATA_DIR:  row.push_back(wxVariant("dir"));  break;
  default:               row.push_back(wxVariant("?"));    break;
  }
  row.push_back(wxVariant(data->GetName()));
  row.push_back(wxVariant(""));
  row.push_back(wxVariant(0));
  remoteDataList->AppendItem(row, (wxUIntPtr) data);
  remoteDataList->Refresh();

  // store link between file and list table item
  for (int row = 0; row < remoteDataList->GetItemCount(); row++) {
    auto item = remoteDataList->RowToItem(row);
    if (remoteDataList->GetItemData(item) == (wxUIntPtr) data) {
      remoteDataItems[data] = item;
      return;
    }
  }

  DebugLog("WARNIG: remote file not found in list\n");
}

void SwooshFrame::AddTextMessage(const std::string &title, const std::string &content)
{
  long flags = wxTE_MULTILINE|wxTE_DONTWRAP|wxTE_RICH2|wxTE_READONLY|wxTE_AUTO_URL;

  wxTextCtrl *text = new wxTextCtrl(notebook, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, flags);
  text->SetFont(messageTextFont);
  text->AppendText(content);
  text->SetInsertionPoint(0);
  text->Bind(wxEVT_TEXT_URL, &SwooshFrame::OnUrlClicked, this);

  auto oldFocusWindow = FindFocus();
  bool moveFocus = (oldFocusWindow == sendText || oldFocusWindow == sendButton);
  notebook->AddPage(text, title, moveFocus, -1);
  if (moveFocus) {
    sendText->SetFocus();
  }
}

void SwooshFrame::SetupContent()
{
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
  long notebook_style = wxNB_LEFT;
  wxListbook *notebook = new wxListbook(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, notebook_style);
  notebook->SetImageList(imageList);
  mainSizer->Add(notebook, wxSizerFlags(5).Expand().Border(wxALL));

  wxPanel *textMessagesPanel = new wxPanel(notebook, wxID_ANY);
  SetupTextMessagesPanel(textMessagesPanel);
  notebook->AddPage(textMessagesPanel, "Messages", true, 0);

  wxPanel *fileMessagesPanel = new wxPanel(notebook, wxID_ANY);
  SetupFileMessagesPanel(fileMessagesPanel);
  notebook->AddPage(fileMessagesPanel, "Files", false, 1);

  this->SetSizer(mainSizer);
  mainSizer->SetSizeHints(this);
}

void SwooshFrame::SetupTextMessagesPanel(wxWindow *parent)
{
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  long splitterStyle = wxSP_LIVE_UPDATE|wxSP_3DSASH|wxSP_3D;
  wxSplitterWindow *splitter = new wxSplitterWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, splitterStyle);
  splitter->SetSashGravity(0.5);
  splitter->SetMinimumPaneSize(80);
  mainSizer->Add(splitter, wxSizerFlags(1).Expand().Border(0));

  // top panel
  wxPanel *topPanel = new wxPanel(splitter, wxID_ANY);
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

  long notebook_style = (
    wxAUI_NB_TOP |
    wxAUI_NB_TAB_MOVE |
    wxAUI_NB_SCROLL_BUTTONS |
    wxAUI_NB_CLOSE_ON_ACTIVE_TAB |
    wxAUI_NB_MIDDLE_CLICK_CLOSE
  );
  notebook = new wxAuiNotebook(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, notebook_style);
  topSizer->Add(notebook, wxSizerFlags(1).Expand().Border(wxALL));

  topPanel->SetSizer(topSizer);

  // bottom panel
  wxPanel *bottomPanel = new wxPanel(splitter, wxID_ANY);
  wxBoxSizer *bottomSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText *sendLabel = new wxStaticText(bottomPanel, wxID_ANY, "Text to send:");
  bottomSizer->Add(sendLabel, wxSizerFlags(0).Expand().Border(wxLEFT|wxRIGHT|wxTOP|wxBOTTOM));

  long sendTextStyle = wxTE_MULTILINE|wxTE_DONTWRAP|wxTE_RICH|wxWANTS_CHARS;
  sendText = new wxTextCtrl(bottomPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, sendTextStyle);
  sendText->Bind(wxEVT_KEY_UP, &SwooshFrame::OnSendTextKeyPressed, this);
  sendText->SetFont(messageTextFont);
  bottomSizer->Add(sendText, wxSizerFlags(1).Expand().Border(wxLEFT|wxRIGHT|wxBOTTOM));

  sendButton = new wxButton(bottomPanel, wxID_ANY, "Send");
  sendButton->Bind(wxEVT_BUTTON, &SwooshFrame::OnSendTextClicked, this);
  bottomSizer->Add(sendButton, wxSizerFlags(0).Expand().Border(wxLEFT|wxRIGHT|wxBOTTOM));

  bottomPanel->SetSizer(bottomSizer);
  splitter->SplitHorizontally(topPanel, bottomPanel, -100);

  parent->SetSizer(mainSizer);
  mainSizer->SetSizeHints(parent);
}

void SwooshFrame::SetupFileMessagesPanel(wxWindow *parent)
{
  wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);

  long splitterStyle = wxSP_LIVE_UPDATE|wxSP_3DSASH|wxSP_3D;
  wxSplitterWindow *splitter = new wxSplitterWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, splitterStyle);
  splitter->SetSashGravity(0.5);
  splitter->SetMinimumPaneSize(80);
  mainSizer->Add(splitter, wxSizerFlags(1).Expand().Border(0));

  // top panel
  wxPanel *topPanel = new wxPanel(splitter, wxID_ANY);
  wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText *receiveLabel = new wxStaticText(topPanel, wxID_ANY, "Receiving:");
  topSizer->Add(receiveLabel, wxSizerFlags(0).Expand().Border(wxLEFT|wxRIGHT|wxTOP));

  remoteDataList = new wxDataViewListCtrl(topPanel, wxID_ANY);
  remoteDataList->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 40, wxALIGN_CENTER);
  remoteDataList->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 130);
  remoteDataList->AppendTextColumn("Download to", wxDATAVIEW_CELL_INERT, 200);
  remoteDataList->AppendProgressColumn("Download progress", wxDATAVIEW_CELL_INERT);
  remoteDataList->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &SwooshFrame::OnRemoteFileActivated, this);
  topSizer->Add(remoteDataList, wxSizerFlags(1).Expand().Border(wxALL));

  topPanel->SetSizer(topSizer);

  // bottom panel
  wxPanel *bottomPanel = new wxPanel(splitter, wxID_ANY);
  wxBoxSizer *bottomSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText *sendLabel = new wxStaticText(bottomPanel, wxID_ANY, "Sending:");
  bottomSizer->Add(sendLabel, wxSizerFlags(0).Expand().Border(wxLEFT|wxRIGHT|wxTOP));

  localDataList = new wxDataViewListCtrl(bottomPanel, wxID_ANY);
  localDataList->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 40, wxALIGN_CENTER);
  localDataList->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 130);
  localDataList->AppendTextColumn("Location", wxDATAVIEW_CELL_INERT);
  localDataList->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &SwooshFrame::OnLocalFileActivated, this);
  bottomSizer->Add(localDataList, wxSizerFlags(1).Expand().Border(wxALL));

  wxBoxSizer *buttonsSizer = new wxBoxSizer(wxHORIZONTAL);
  buttonsSizer->AddStretchSpacer();

  wxButton *addSendFileButton = new wxButton(bottomPanel, wxID_ANY, "Add File");
  addSendFileButton->Bind(wxEVT_BUTTON, &SwooshFrame::OnAddSendFileClicked, this);
  buttonsSizer->Add(addSendFileButton, wxSizerFlags(0).Expand().Border(wxRIGHT));

  wxButton *addSendDirButton = new wxButton(bottomPanel, wxID_ANY, "Add Directory");
  addSendDirButton->Bind(wxEVT_BUTTON, &SwooshFrame::OnAddSendDirClicked, this);
  buttonsSizer->Add(addSendDirButton, wxSizerFlags(0).Expand().Border(0));

  bottomSizer->Add(buttonsSizer, wxSizerFlags(0).Expand().Border(wxLEFT|wxRIGHT|wxBOTTOM));

  bottomPanel->SetSizer(bottomSizer);
  splitter->SplitHorizontally(topPanel, bottomPanel, -100);

  parent->SetSizer(mainSizer);
  mainSizer->SetSizeHints(parent);
}

void SwooshFrame::SetupMenu()
{
  wxMenu* menuFile = new wxMenu;
  //menuFile->Append(ID_Hello, "&Send message...\tCtrl-H", "Send message");
  menuFile->AppendSeparator();
  menuFile->Append(wxID_EXIT);

  wxMenu* menuHelp = new wxMenu;
  menuHelp->Append(wxID_ABOUT);

  wxMenuBar* menuBar = new wxMenuBar;
  menuBar->Append(menuFile, "&File");
  menuBar->Append(menuHelp, "&Help");

  SetMenuBar(menuBar);
}

void SwooshFrame::SetupStatusBar()
{
  CreateStatusBar();
  SetStatusText("");
}

void SwooshFrame::OnExit(wxCommandEvent &event)
{
  Close(true);
}

void SwooshFrame::OnAbout(wxCommandEvent &event)
{
  wxMessageDialog dlg(this, "By MoeFH\n\nhttps://github.com/moefh/swoosh", "About Swoosh", wxOK);
  dlg.ShowModal();
}

void SwooshFrame::OnSendTextClicked(wxCommandEvent &event)
{
  SendTextMessage();
}

void SwooshFrame::OnClose(wxCloseEvent &event)
{
#if 0
  if (event.CanVeto()) {
    Show(false);
    event.Veto();
    //wxGetApp().ExitMainLoop();
    return;
  }
  //notification->RemoveIcon();
#endif
  net.Stop();
  Destroy();
}

void SwooshFrame::OnQuit(wxCommandEvent &WXUNUSED(event)) {
  Close(true);
}

void SwooshFrame::OnNetNotify(const std::string &message)
{
  std::string copy = message;
  wxGetApp().CallAfter([this, copy] {
    SetStatusText(copy);
  });
}

void SwooshFrame::OnNetReceivedData(SwooshRemoteData *data)
{
  // text
  SwooshRemoteTextData *text = dynamic_cast<SwooshRemoteTextData *>(data);
  if (text) {
    std::string message = text->GetText();
    wxGetApp().CallAfter([this, message] {
      AddTextMessage("Message", message);
    });
    delete text;
    return;
  }

  // permanent data (file, dir)
  SwooshRemotePermanentData *perm_data = dynamic_cast<SwooshRemotePermanentData *>(data);
  if (perm_data) {
    wxGetApp().CallAfter([this, perm_data] {
      AddRemoteData(perm_data);
    });
    return;
  }

  DebugLog("WARNING: ignoring message with unknown type\n");
  delete data;
}

void SwooshFrame::OnNetDataDownloading(SwooshRemotePermanentData *data, double progress)
{
  wxGetApp().CallAfter([this, data, progress] {
    auto it = remoteDataItems.find(data);
    if (it == remoteDataItems.end()) {
      DebugLog("ERROR: can't find downloaded data item\n");
      return;
    }

    wxDataViewItem &item = it->second;
    remoteDataList->GetStore()->SetValue(wxVariant((int)(progress*100)), item, 3);
    remoteDataList->Refresh();
  });
}

void SwooshFrame::OnNetDataDownloaded(SwooshRemotePermanentData *data, bool success)
{
  wxGetApp().CallAfter([this, data, success] {
    auto it = remoteDataItems.find(data);
    if (it == remoteDataItems.end()) {
      DebugLog("ERROR: can't find downloaded data item\n");
      return;
    }

    wxDataViewItem &item = it->second;
    if (success) {
      remoteDataList->GetStore()->SetValue(wxVariant(100), item, 3);
      remoteDataList->Refresh();
    }
  });
}

void SwooshFrame::OnUrlClicked(wxTextUrlEvent &event)
{
  auto &mouse = event.GetMouseEvent();
  if (mouse.Button(wxMOUSE_BTN_LEFT) && mouse.LeftUp()) {
    auto textCtrl = dynamic_cast<wxTextCtrl*>(event.GetEventObject());
    if (!textCtrl) {
      event.Skip();
      return;
    }
    auto text = textCtrl->GetValue();
    auto start = event.GetURLStart();
    auto end = event.GetURLEnd();
    if (start < 0 || end > (long)text.Length() || end <= start) {
      event.Skip();
      return;
    }
    wxLaunchDefaultBrowser(text.SubString(start, end-1));
  } else {
    event.Skip();
  }
}

void SwooshFrame::OnSendTextKeyPressed(wxKeyEvent &event)
{
  if (event.ControlDown() && event.GetKeyCode() == WXK_RETURN) {
    SendTextMessage();
    return;
  }
  event.Skip();
}

void SwooshFrame::OnAddSendFileClicked(wxCommandEvent &event)
{
  wxFileDialog openFileDialog(this, "Select file to send", "", "", "All files|*.*", wxFD_OPEN|wxFD_FILE_MUST_EXIST);
  if (openFileDialog.ShowModal() == wxID_CANCEL)
    return;

  AddLocalFile(openFileDialog.GetPath().ToStdString());
}

void SwooshFrame::OnAddSendDirClicked(wxCommandEvent &event)
{
  wxDirDialog openDirDialog(this, "Select directory to send", "", wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
  if (openDirDialog.ShowModal() == wxID_CANCEL)
    return;

  AddLocalDir(openDirDialog.GetPath().ToStdString());
}

void SwooshFrame::OnRemoteFileActivated(wxDataViewEvent &event)
{
  auto item = event.GetItem();
  auto data = (SwooshRemotePermanentData *) remoteDataList->GetItemData(item);

  wxVariant val;
  remoteDataList->GetStore()->GetValue(val, item, 2);
  auto local_path = val.GetString();

  if (local_path.IsEmpty()) {
    switch (data->GetType()) {
    case SWOOSH_DATA_FILE:
      {
        wxFileDialog saveFileDialog(this, "Download to", "", data->GetName(), "All files|*.*", wxFD_SAVE);
        if (saveFileDialog.ShowModal() == wxID_CANCEL)
          return;
        local_path = saveFileDialog.GetPath();
        remoteDataList->GetStore()->SetValue(wxVariant(local_path), item, 2);
        remoteDataList->Refresh();
        break;
      }

    case SWOOSH_DATA_DIR:
      {
        wxDirDialog saveDirDialog(this, "Download to", "", wxDD_DEFAULT_STYLE);
        if (saveDirDialog.ShowModal() == wxID_CANCEL)
          return;
        local_path = saveDirDialog.GetPath();
        remoteDataList->GetStore()->SetValue(wxVariant(local_path), item, 2);
        remoteDataList->Refresh();
        break;
      }

    default:
      DebugLog("WARNING: ignoring activation on unknown data type\n");
      return;
    }
  }
  
  net.ReceiveDataContent(data, local_path.ToStdString());
}

void SwooshFrame::OnLocalFileActivated(wxDataViewEvent &event)
{
  auto item = event.GetItem();
  auto file = (SwooshLocalFileData *) localDataList->GetItemData(item);

  if (!file) {
    DebugLog("WARNING: can't find item data\n");
    return;
  }

  net.SendDataBeacon(file->GetMessageId());
}
