
#include "targetver.h"

#include <vector>
#include <thread>
#include <iostream>
#include <functional>

#include "swoosh_frame.h"
#include "swoosh_app.h"
#include "util.h"

#include <wx/splitter.h>

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
  SetupMenu();
  SetupStatusBar();
  SetupContent();
  SetMinSize(wxSize(640, 480));

  Bind(wxEVT_MENU, &SwooshFrame::OnAbout, this, wxID_ABOUT);
  Bind(wxEVT_MENU, &SwooshFrame::OnExit, this, wxID_EXIT);
  Bind(wxEVT_SIZE, &SwooshFrame::OnSize, this);
  Bind(wxEVT_CLOSE_WINDOW, &SwooshFrame::OnClose, this);

  Connect(ID_SendTextMessage, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(SwooshFrame::OnSendClicked));
  wxAcceleratorEntry entries[] = {
    wxAcceleratorEntry(wxACCEL_CTRL, WXK_RETURN, ID_SendTextMessage),
  };
  this->SetAcceleratorTable(wxAcceleratorTable(sizeof(entries)/sizeof(entries[0]), entries));
}

void SwooshFrame::SendTextMessage()
{
  auto text = sendText->GetValue();
  if (text.length() != 0) {
    net.SendText(text.ToStdString());
    sendText->SetValue("");
  }
}

void SwooshFrame::AddTextPage(const std::string &title, const std::string &content)
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
  wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
  long splitterStyle = wxSP_LIVE_UPDATE|wxSP_3DSASH|wxSP_3D;
  wxSplitterWindow *splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, splitterStyle);
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
  topSizer->Add(notebook, wxSizerFlags(5).Expand().Border(wxALL));
  topPanel->SetSizer(topSizer);

  // bottom panel
  wxPanel *bottomPanel = new wxPanel(splitter, wxID_ANY);
  wxBoxSizer *bottomSizer = new wxBoxSizer(wxVERTICAL);
  long sendTextStyle = wxTE_MULTILINE|wxTE_DONTWRAP|wxTE_RICH|wxWANTS_CHARS;
  sendText = new wxTextCtrl(bottomPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, sendTextStyle);
  sendText->Bind(wxEVT_KEY_UP, &SwooshFrame::OnSendTextKeyPressed, this);
  sendText->SetFont(messageTextFont);
  bottomSizer->Add(sendText, wxSizerFlags(1).Expand().Border(wxLEFT|wxRIGHT|wxBOTTOM));

  sendButton = new wxButton(bottomPanel, wxID_ANY, "Send");
  sendButton->Bind(wxEVT_BUTTON, &SwooshFrame::OnSendClicked, this);
  bottomSizer->Add(sendButton, wxSizerFlags(0).Expand().Border(wxLEFT|wxRIGHT|wxBOTTOM));
  bottomPanel->SetSizer(bottomSizer);

  splitter->SplitHorizontally(topPanel, bottomPanel, -100);
  this->SetSizer(mainSizer);
  mainSizer->SetSizeHints(this);
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

void SwooshFrame::OnSendClicked(wxCommandEvent &event)
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

void SwooshFrame::OnNetReceivedData(SwooshData *data, const std::string &host, int port)
{
  // text
  SwooshTextData *text = dynamic_cast<SwooshTextData *>(data);
  if (text) {
    std::string source = host;
    std::string message = text->GetText();
    wxGetApp().CallAfter([this, source, message] {
      AddTextPage(source, message);
    });
    return;
  }

  DebugLog("WARNING: ignoring message of unknown type");
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
