/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2009-2016 Vaclav Slavik
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a
 *  copy of this software and associated documentation files (the "Software"),
 *  to deal in the Software without restriction, including without limitation
 *  the rights to use, copy, modify, merge, publish, distribute, sublicense,
 *  and/or sell copies of the Software, and to permit persons to whom the
 *  Software is furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 */

#include "update_notifier/thirdparty/winsparkle/src/ui.h"

#include "update_notifier/thirdparty/winsparkle/src/appcast.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/thirdparty/winsparkle/src/settings.h"
#include "update_notifier/thirdparty/winsparkle/src/threads.h"
#include "update_notifier/thirdparty/winsparkle/src/updatedownloader.h"

#include "app/vivaldi_resources.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"

#define wxNO_NET_LIB
#define wxNO_XML_LIB
#define wxNO_XRC_LIB
#define wxNO_ADV_LIB
#define wxNO_HTML_LIB

#if !defined(__WXMSW__)
#define __WXMSW__
#endif
#include <wx/setup.h>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/dcclient.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/evtloop.h>
#include <wx/filename.h>
#include <wx/gauge.h>
#include <wx/intl.h>
#include <wx/msgdlg.h>
#include <wx/msw/ole/activex.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/timer.h>
#include <wx/webview.h>

#include <commctrl.h>
#include <string>

#define _UNICODE 1
#define UNICODE 1

#include <stdio.h>
#include <stdlib.h>

#include <Windows.h>

#if !wxCHECK_VERSION(2, 9, 0)
#error "wxWidgets >= 2.9 is required to compile this code"
#endif

namespace winsparkle {

/*--------------------------------------------------------------------------*
                                  helpers
 *--------------------------------------------------------------------------*/

namespace {

UIDelegate* g_delegate = nullptr;

// shows/hides layout element
void DoShowElement(wxWindow* w, bool show) {
  w->GetContainingSizer()->Show(w, show, true /*recursive*/);
}

void DoShowElement(wxSizer* s, bool show) {
  s->ShowItems(show);
}

#define SHOW(c) DoShowElement(c, true)
#define HIDE(c) DoShowElement(c, false)

wxIcon LoadNamedIcon(HMODULE module, const wchar_t* iconName, int size) {
  HICON hIcon = NULL;

  if (FAILED(LoadIconWithScaleDown(module, iconName, size, size, &hIcon)))
    hIcon = NULL;

  if (!hIcon) {
    hIcon = static_cast<HICON>(
        LoadImage(module, iconName, IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
  }

  if (!hIcon)
    return wxNullIcon;

  wxIcon icon;
  icon.SetHICON(static_cast<WXHICON>(hIcon));
  icon.SetWidth(size);
  icon.SetHeight(size);

  return icon;
}

BOOL CALLBACK GetFirstIconProc(HMODULE hModule,
                               LPCTSTR lpszType,
                               LPTSTR lpszName,
                               LONG_PTR lParam) {
  if (IS_INTRESOURCE(lpszName))
    *((LPTSTR*)lParam) = lpszName;
  else
    *((LPTSTR*)lParam) = _tcsdup(lpszName);
  return FALSE;  // stop on the first icon found
}

wxIcon GetApplicationIcon(int size) {
  HMODULE hParentExe = GetModuleHandle(NULL);
  if (!hParentExe)
    return wxNullIcon;

  LPTSTR iconName = 0;
  EnumResourceNames(hParentExe, RT_GROUP_ICON, GetFirstIconProc,
                    (LONG_PTR)&iconName);

  if (GetLastError() != ERROR_SUCCESS &&
      GetLastError() != ERROR_RESOURCE_ENUM_USER_STOP)
    return wxNullIcon;

  wxIcon icon = LoadNamedIcon(hParentExe, iconName, size);

  if (!IS_INTRESOURCE(iconName))
    free(iconName);

  return icon;
}

struct EnumProcessWindowsData {
  DWORD process_id = 0;
  wxRect biggest;
};

BOOL CALLBACK EnumProcessWindowsCallback(HWND handle, LPARAM lParam) {
  EnumProcessWindowsData& data =
      *reinterpret_cast<EnumProcessWindowsData*>(lParam);

  if (!IsWindowVisible(handle) || IsIconic(handle))
    return TRUE;

  DWORD process_id = 0;
  GetWindowThreadProcessId(handle, &process_id);
  if (data.process_id != process_id)
    return TRUE;  // another process' window

  if (GetWindow(handle, GW_OWNER) != 0)
    return TRUE;  // child, not main, window

  RECT rwin;
  GetWindowRect(handle, &rwin);
  wxRect r(rwin.left, rwin.top, rwin.right - rwin.left, rwin.bottom - rwin.top);
  if (r.width * r.height > data.biggest.width * data.biggest.height)
    data.biggest = r;

  return TRUE;
}

wxPoint GetWindowOriginSoThatItFits(int display, const wxRect& windowRect) {
  wxPoint pos = windowRect.GetTopLeft();
  wxRect desktop = wxDisplay(display).GetClientArea();
  if (!desktop.Contains(windowRect)) {
    if (pos.x < desktop.x)
      pos.x = desktop.x;
    if (pos.y < desktop.y)
      pos.y = desktop.y;
    wxPoint bottomRightDiff =
        windowRect.GetBottomRight() - desktop.GetBottomRight();
    if (bottomRightDiff.x > 0)
      pos.x -= bottomRightDiff.x;
    if (bottomRightDiff.y > 0)
      pos.y -= bottomRightDiff.y;
  }
  return pos;
}

void CenterWindowOnHostApplication(wxTopLevelWindow* win) {
  // find application's biggest window:
  EnumProcessWindowsData data;
  data.process_id = GetCurrentProcessId();
  EnumWindows(EnumProcessWindowsCallback, (LPARAM)&data);

  if (data.biggest.IsEmpty()) {
    // no parent window to center on, so center on the screen
    win->Center();
    return;
  }

  const wxRect& host(data.biggest);

  // and center WinSparkle on it:
  wxSize winsz = win->GetClientSize();
  wxPoint pos(host.x + (host.width - winsz.x) / 2,
              host.y + (host.height - winsz.y) / 2);

  // make sure the window is fully visible:
  int display = wxDisplay::GetFromPoint(
      wxPoint(host.x + host.width / 2, host.y + host.height / 2));
  if (display != wxNOT_FOUND) {
    pos = GetWindowOriginSoThatItFits(display, wxRect(pos, winsz));
  }

  win->Move(pos);
}

void EnsureWindowIsFullyVisible(wxTopLevelWindow* win) {
  int display = wxDisplay::GetFromWindow(win);
  if (display == wxNOT_FOUND)
    return;

  wxPoint pos = GetWindowOriginSoThatItFits(display, win->GetRect());
  win->Move(pos);
}

// Locks window updates to reduce flicker. Redoes layout in dtor.
struct LayoutChangesGuard {
  LayoutChangesGuard(wxTopLevelWindow* win) : m_win(win) { m_win->Freeze(); }

  ~LayoutChangesGuard() {
    m_win->Layout();
    m_win->GetSizer()->SetSizeHints(m_win);
    m_win->Refresh();
    EnsureWindowIsFullyVisible(m_win);
    m_win->Thaw();
    m_win->Update();
  }

  wxTopLevelWindow* m_win;
};

class WinsparkleTranslationsLoader : public wxResourceTranslationsLoader {
 protected:
  WXHINSTANCE GetModule() const override { return GetModuleHandle(nullptr); }
};

/*--------------------------------------------------------------------------*
                        Base class for WinSparkle dialogs
 *--------------------------------------------------------------------------*/

class WinSparkleDialog : public wxDialog {
 protected:
  WinSparkleDialog();

  void UpdateLayout();
  static void SetBoldFont(wxWindow* win);
  static void SetHeadingFont(wxWindow* win);

  // enable/disable resizing of the dialog
  void MakeResizable(bool resizable = true);

 protected:
  // sizer for the main area of the dialog (to the right of the icon)
  wxSizer* m_mainAreaSizer;

  // High DPI support:
  double m_scaleFactor;
#define PX(x) (int((x)*m_scaleFactor))

  static const int MESSAGE_AREA_WIDTH = 300;
};

WinSparkleDialog::WinSparkleDialog()
    : wxDialog(NULL,
               wxID_ANY,
               _("Software Update"),
               wxDefaultPosition,
               wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxDIALOG_NO_PARENT) {
  wxSize dpi = wxClientDC(this).GetPPI();
  m_scaleFactor = dpi.y / 96.0;

  wxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);

  // Load the dialog box icon: the first 48x48 application icon will be loaded,
  // if available, otherwise, the standard WinSparkle icon will be used.
  wxIcon bigIcon = GetApplicationIcon(PX(48));
  if (bigIcon.IsOk()) {
    topSizer->Add(new wxStaticBitmap(this, wxID_ANY, bigIcon),
                  wxSizerFlags(0).Border(wxALL, PX(10)));
  }

  m_mainAreaSizer = new wxBoxSizer(wxVERTICAL);
  topSizer->Add(m_mainAreaSizer,
                wxSizerFlags(1).Expand().Border(wxALL, PX(10)));

  SetSizer(topSizer);

  MakeResizable(false);
}

void WinSparkleDialog::MakeResizable(bool resizable) {
  bool is_resizable = (GetWindowStyleFlag() & wxRESIZE_BORDER) != 0;
  if (is_resizable == resizable)
    return;

  ToggleWindowStyle(wxRESIZE_BORDER);
  Refresh();  // to paint the gripper
}

void WinSparkleDialog::UpdateLayout() {
  Layout();
  GetSizer()->SetSizeHints(this);
}

void WinSparkleDialog::SetBoldFont(wxWindow* win) {
  wxFont f = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
  f.SetWeight(wxFONTWEIGHT_BOLD);

  win->SetFont(f);
}

void WinSparkleDialog::SetHeadingFont(wxWindow* win) {
  wxFont f = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

  int winver;
  wxGetOsVersion(&winver);
  if (winver >= 6)  // Windows Vista, 7 or newer
  {
    // 9pt is base font, 12pt is for "Main instructions". See
    // http://msdn.microsoft.com/en-us/library/aa511282%28v=MSDN.10%29.aspx
    f.SetPointSize(f.GetPointSize() + 3);
    win->SetForegroundColour(wxColour(0x00, 0x33, 0x99));
  } else  // Windows XP/2000
  {
    f.SetWeight(wxFONTWEIGHT_BOLD);
    f.SetPointSize(f.GetPointSize() + 1);
  }

  win->SetFont(f);
}

/*--------------------------------------------------------------------------*
                      Window for communicating with the user
 *--------------------------------------------------------------------------*/

const int ID_SKIP_VERSION = wxNewId();
const int ID_REMIND_LATER = wxNewId();
const int ID_INSTALL = wxNewId();
const int ID_RUN_INSTALLER = wxNewId();

class App;

class UpdateDialog : public WinSparkleDialog {
 public:
  UpdateDialog();

  void ShowAsMainWindow();

  // changes state into "checking for updates"
  void StateCheckingUpdates();
  // change state into "no updates found"
  void StateNoUpdateFound();
  // change state into "update error"
  void StateUpdateError(Error error);
  // change state into "a new version is available"
  void StateUpdateAvailable(Appcast info);
  // change state into "downloading update"
  void StateDownloading();
  // update download progress
  void DownloadProgress(DownloadReport report);
  // change state into "update downloaded"
  void StateDownloaded();

 private:
  friend class App;
  void EnablePulsing(bool enable);
  void OnTimer(wxTimerEvent& event);
  void OnCloseButton(wxCommandEvent& event);
  void OnClose(wxCloseEvent& event);

  void OnSkipVersion(wxCommandEvent&);
  void OnRemindLater(wxCommandEvent&);
  void OnInstall(wxCommandEvent&);

  void OnRunInstaller(wxCommandEvent&);

  // Return false if the label already show the message.
  bool SetMessage(const wxString& text, int width = MESSAGE_AREA_WIDTH);

  void SetBusyCursor(bool on) {
    if (on != m_showing_busy_cursor) {
      if (on) {
        wxBeginBusyCursor();
      } else {
        wxEndBusyCursor();
      }
      m_showing_busy_cursor = on;
    }
  }

 private:
  wxTimer m_timer;
  wxSizer* m_buttonSizer;
  wxStaticText* m_heading;
  wxStaticText* m_message;
  wxGauge* m_progress;
  wxStaticText* m_progressLabel;
  wxButton* m_closeButton;
  wxSizer* m_closeButtonSizer;
  wxButton* m_runInstallerButton;
  wxSizer* m_runInstallerButtonSizer;
  wxButton* m_installButton;
  wxSizer* m_updateButtonsSizer;
  wxSizer* m_releaseNotesSizer;

  wxWebView* m_webBrowser;

  // current appcast data (only valid after StateUpdateAvailable())
  Appcast m_appcast;

  bool m_showing_busy_cursor = false;

  static const int RELNOTES_WIDTH = 460;
  static const int RELNOTES_HEIGHT = 200;
};

UpdateDialog::UpdateDialog() : m_timer(this) {
  m_webBrowser =
      wxWebView::New(this, wxID_ANY, wxWebViewDefaultURLStr, wxDefaultPosition,
                     wxSize(PX(RELNOTES_WIDTH), PX(RELNOTES_HEIGHT)));
  if (m_webBrowser)
    m_webBrowser->EnableContextMenu(false);

  m_heading = new wxStaticText(this, wxID_ANY, "");
  SetHeadingFont(m_heading);
  m_mainAreaSizer->Add(m_heading,
                       wxSizerFlags(0).Expand().Border(wxBOTTOM, PX(10)));

  m_message = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition,
                               wxSize(PX(MESSAGE_AREA_WIDTH), -1));
  m_mainAreaSizer->Add(m_message, wxSizerFlags(0).Expand());

  m_progress = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition,
                           wxSize(PX(MESSAGE_AREA_WIDTH), PX(16)));
  m_progressLabel = new wxStaticText(this, wxID_ANY, "");
  m_mainAreaSizer->Add(
      m_progress, wxSizerFlags(0).Expand().Border(wxTOP | wxBOTTOM, PX(10)));
  m_mainAreaSizer->Add(m_progressLabel, wxSizerFlags(0).Expand());
  m_mainAreaSizer->AddStretchSpacer(1);

  m_releaseNotesSizer = new wxBoxSizer(wxVERTICAL);

  wxStaticText* notesLabel =
      new wxStaticText(this, wxID_ANY, _("Release notes:"));
  SetBoldFont(notesLabel);
  m_releaseNotesSizer->Add(notesLabel, wxSizerFlags().Border(wxTOP, PX(10)));
  if (m_webBrowser)
    m_releaseNotesSizer->Add(m_webBrowser,
                             wxSizerFlags().Expand().Proportion(1));

  m_mainAreaSizer->Add(m_releaseNotesSizer,
                       // proportion=10000 to overcome stretch spacer above
                       wxSizerFlags(10000).Expand());

  m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);

  m_updateButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
  m_updateButtonsSizer->Add(
      new wxButton(this, ID_SKIP_VERSION, _("Skip this version")),
      wxSizerFlags().Border(wxRIGHT, PX(20)));
  m_updateButtonsSizer->AddStretchSpacer(1);
  m_updateButtonsSizer->Add(
      new wxButton(this, ID_REMIND_LATER, _("Remind me later")),
      wxSizerFlags().Border(wxRIGHT, PX(10)));
  m_updateButtonsSizer->Add(
      m_installButton = new wxButton(this, ID_INSTALL, _("Install update")),
      wxSizerFlags());
  m_buttonSizer->Add(m_updateButtonsSizer, wxSizerFlags(1));

  m_closeButtonSizer = new wxBoxSizer(wxHORIZONTAL);
  m_closeButton = new wxButton(this, wxID_CANCEL);
  m_closeButtonSizer->AddStretchSpacer(1);
  m_closeButtonSizer->Add(m_closeButton, wxSizerFlags(0).Border(wxLEFT));
  m_buttonSizer->Add(m_closeButtonSizer, wxSizerFlags(1));

  m_runInstallerButtonSizer = new wxBoxSizer(wxHORIZONTAL);
  // TODO: make this "Install and relaunch"
  m_runInstallerButton =
      new wxButton(this, ID_RUN_INSTALLER, _("Close Vivaldi and install now"));
  m_runInstallerButtonSizer->AddStretchSpacer(1);
  m_runInstallerButtonSizer->Add(m_runInstallerButton,
                                 wxSizerFlags(0).Border(wxLEFT));
  m_buttonSizer->Add(m_runInstallerButtonSizer, wxSizerFlags(1));

  m_mainAreaSizer->Add(m_buttonSizer,
                       wxSizerFlags(0).Expand().Border(wxTOP, PX(10)));

  UpdateLayout();

  Bind(wxEVT_CLOSE_WINDOW, &UpdateDialog::OnClose, this);
  Bind(wxEVT_TIMER, &UpdateDialog::OnTimer, this);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED, &UpdateDialog::OnCloseButton, this,
       wxID_CANCEL);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED, &UpdateDialog::OnSkipVersion, this,
       ID_SKIP_VERSION);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED, &UpdateDialog::OnRemindLater, this,
       ID_REMIND_LATER);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED, &UpdateDialog::OnInstall, this,
       ID_INSTALL);
  Bind(wxEVT_COMMAND_BUTTON_CLICKED, &UpdateDialog::OnRunInstaller, this,
       ID_RUN_INSTALLER);
}

void UpdateDialog::ShowAsMainWindow() {
  Freeze();
  if (!IsShown()) {
    CenterWindowOnHostApplication(this);
  }
  Show();
  Thaw();
  Raise();
}

void UpdateDialog::EnablePulsing(bool enable) {
  if (enable && !m_timer.IsRunning())
    m_timer.Start(100);
  else if (!enable && m_timer.IsRunning())
    m_timer.Stop();
}

void UpdateDialog::OnTimer(wxTimerEvent&) {
  m_progress->Pulse();
}

void UpdateDialog::OnCloseButton(wxCommandEvent&) {
  Close();
}

void UpdateDialog::OnClose(wxCloseEvent&) {
  SetBusyCursor(false);

  // We need to call this, because by default, wxDialog doesn't
  // destroy itself in Close().
  Destroy();
}

void UpdateDialog::OnSkipVersion(wxCommandEvent&) {
  Settings::WriteConfigValue(ConfigKey::kSkipThisVersion, m_appcast.Version);
  Close();
}

void UpdateDialog::OnRemindLater(wxCommandEvent&) {
  // Just abort the update. Next time it's scheduled to run,
  // the user will be prompted.
  Close();
}

void UpdateDialog::OnInstall(wxCommandEvent&) {
  if (!m_appcast.HasDownload()) {
    wxLaunchDefaultBrowser(m_appcast.WebBrowserURL, wxBROWSER_NEW_WINDOW);
    Close();
  } else {
    StateDownloading();
  }
}

void UpdateDialog::OnRunInstaller(wxCommandEvent& event) {
  SetBusyCursor(true);
  m_runInstallerButton->Disable();

  m_message->SetLabel(_("Launching the installer..."));
  g_delegate->WinsparkleLaunchInstaller();
}

bool UpdateDialog::SetMessage(const wxString& text, int width) {
  if (m_message->GetLabel() == text)
    return false;
  m_message->SetLabel(text);
  m_message->Wrap(PX(width));
  return true;
}

void UpdateDialog::StateCheckingUpdates() {
  LayoutChangesGuard guard(this);

  SetMessage(_("Checking for updates..."));

  m_closeButton->SetLabel(_("Cancel"));
  EnablePulsing(true);

  HIDE(m_heading);
  SHOW(m_progress);
  HIDE(m_progressLabel);
  SHOW(m_closeButtonSizer);
  HIDE(m_runInstallerButtonSizer);
  HIDE(m_releaseNotesSizer);
  HIDE(m_updateButtonsSizer);
  MakeResizable(false);
}

void UpdateDialog::StateNoUpdateFound() {
  LayoutChangesGuard guard(this);

  m_heading->SetLabel(_("You're up to date!"));

  wxString msg =
      wxString::Format(_("%s %s is currently the newest version available."),
                       GetConfig().app_name, GetConfig().app_version);

  SetMessage(msg);

  m_closeButton->SetLabel(
      l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_CLOSE_BUTTON));
  m_closeButton->SetFocus();
  EnablePulsing(false);

  SHOW(m_heading);
  HIDE(m_progress);
  HIDE(m_progressLabel);
  SHOW(m_closeButtonSizer);
  HIDE(m_runInstallerButtonSizer);
  HIDE(m_releaseNotesSizer);
  HIDE(m_updateButtonsSizer);
  MakeResizable(false);
}

void UpdateDialog::StateUpdateError(Error error) {
  SetBusyCursor(false);

  LayoutChangesGuard guard(this);

  m_heading->SetLabel(
      l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_ERROR_TITLE));

  int message_id = 0;
  switch (error.kind()) {
    case Error::kNone:
      NOTREACHED();
      break;
    case Error::kCancelled:
      message_id = IDS_UPDATE_NOTIFICATION_ERROR_CANCEL;
      break;
    // Treat format errors as network one.
    case Error::kFormat:
    case Error::kNetwork:
      message_id = IDS_UPDATE_NOTIFICATION_ERROR_NETWORK;
      break;
    case Error::kStorage:
      message_id = IDS_UPDATE_NOTIFICATION_ERROR_STORAGE;
      break;
    case Error::kExec:
      message_id = IDS_UPDATE_NOTIFICATION_ERROR_EXEC;
      break;
    case Error::kVerify:
      message_id = IDS_UPDATE_NOTIFICATION_ERROR_VERIFY;
      break;
  }

  std::string msg =
      message_id ? l10n_util::GetStringUTF8(message_id) : std::string();
  if (error.kind() != Error::kCancelled) {
    msg += "\n\n";
    msg += l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_ERROR_DETAILS);
    msg += "\n\n";
    msg += error.message();
  }
  SetMessage(msg);

  m_closeButton->SetLabel(
      l10n_util::GetStringUTF8(IDS_UPDATE_NOTIFICATION_CLOSE_BUTTON));
  m_closeButton->SetFocus();
  EnablePulsing(false);

  SHOW(m_heading);
  HIDE(m_progress);
  HIDE(m_progressLabel);
  SHOW(m_closeButtonSizer);
  HIDE(m_runInstallerButtonSizer);
  HIDE(m_releaseNotesSizer);
  HIDE(m_updateButtonsSizer);
  MakeResizable(false);
}

void UpdateDialog::StateUpdateAvailable(Appcast appcast) {
  m_appcast = std::move(appcast);

  const bool showRelnotes =
      m_appcast.ReleaseNotesURL.is_valid() && m_webBrowser;

  {
    LayoutChangesGuard guard(this);

    const wxString appname = GetConfig().app_name;
    wxString ver_my = GetConfig().app_version;
    wxString ver_new = m_appcast.Version;
    if (ver_my == ver_new) {
      ver_my = wxString::Format("%s (%s)", ver_my, GetConfig().app_version);
      ver_new = wxString::Format("%s (%s)", ver_new, m_appcast.Version);
    }

    m_heading->SetLabel(
        wxString::Format(_("A new version of %s is available!"), appname));

    if (!m_appcast.HasDownload())
      m_installButton->SetLabel(_("Get update"));

    SetMessage(wxString::Format(_("%s %s is now available (you have %s). Would "
                                  "you like to download it now?"),
                                appname, ver_new, ver_my),
               showRelnotes ? RELNOTES_WIDTH : MESSAGE_AREA_WIDTH);

    EnablePulsing(false);

    m_installButton->SetFocus();

    SHOW(m_heading);
    HIDE(m_progress);
    HIDE(m_progressLabel);
    HIDE(m_closeButtonSizer);
    HIDE(m_runInstallerButtonSizer);
    SHOW(m_updateButtonsSizer);
    DoShowElement(m_releaseNotesSizer, showRelnotes);
    MakeResizable(showRelnotes);
  }

  // Only show the release notes now that the layout was updated, as it may
  // take some time to load the MSIE control:
  if (showRelnotes) {
    m_webBrowser->LoadURL(wxString(m_appcast.ReleaseNotesURL.spec()));
  }
}

void UpdateDialog::StateDownloading() {
  LayoutChangesGuard guard(this);

  SetMessage(_("Downloading update..."));

  m_closeButton->SetLabel(_("Cancel"));
  EnablePulsing(false);

  HIDE(m_heading);
  SHOW(m_progress);
  SHOW(m_progressLabel);
  SHOW(m_closeButtonSizer);
  HIDE(m_runInstallerButtonSizer);
  HIDE(m_releaseNotesSizer);
  HIDE(m_updateButtonsSizer);
  MakeResizable(false);

  g_delegate->WinsparkleStartDownload();
}

void UpdateDialog::DownloadProgress(DownloadReport report) {
  wxString label;
  switch (report.kind) {
    case DownloadReport::kConnected:
      DCHECK(report.downloaded_length == 0);

      // Ensure that we show a progress in case we recovered from failed delta
      // verification and update and switched to downloading the full update.
      SHOW(m_progressLabel);
      FALLTHROUGH;
    case DownloadReport::kMoreData: {
      int total = report.content_length;
      int downloaded = report.downloaded_length;
      if (total) {
        if (m_progress->GetRange() != total) {
          m_progress->SetRange(total);
        }
        m_progress->SetValue(downloaded);
        label = wxString::Format(
            // TRANSLATORS: This is the progress of a download, e.g. "3 MB of 12
            // MB".
            _("%s of %s"),
            wxFileName::GetHumanReadableSize(downloaded, "", 1, wxSIZE_CONV_SI),
            wxFileName::GetHumanReadableSize(total, "", 1, wxSIZE_CONV_SI));
      } else {
        m_progress->Pulse();
        label =
            wxFileName::GetHumanReadableSize(downloaded, "", 1, wxSIZE_CONV_SI);
      }
      if (label != m_progressLabel->GetLabel()) {
        m_progressLabel->SetLabel(label);
      }
      break;
    }
    case DownloadReport::kVerificationStart:
      if (!SetMessage(_("Verifying update...")))
        return;
      HIDE(m_progressLabel);
      break;
    case DownloadReport::kDeltaExtraction:
      if (!SetMessage(_("Extracting update...")))
        return;
      break;
  }

  Refresh();
  Update();
}

void UpdateDialog::StateDownloaded() {
  HIDE(m_progressLabel);
  m_closeButton->Disable();

  LayoutChangesGuard guard(this);

  SetMessage(_("Ready to install. Vivaldi will be closed and relaunched."));

  m_progress->SetRange(1);
  m_progress->SetValue(1);

  m_runInstallerButton->Enable();
  m_runInstallerButton->SetFocus();

  HIDE(m_heading);
  SHOW(m_progress);
  // HIDE(m_progressLabel);
  HIDE(m_closeButtonSizer);
  SHOW(m_runInstallerButtonSizer);
  HIDE(m_releaseNotesSizer);
  HIDE(m_updateButtonsSizer);
  MakeResizable(false);
}

/*--------------------------------------------------------------------------*
                             Inter-thread messages
 *--------------------------------------------------------------------------*/

const int MSG_BRING_TO_FOCUS = wxNewId();

// Show "Checking for updates..." window
const int MSG_SHOW_CHECKING_UPDATES = wxNewId();

// Notify the UI about done version check.
const int MSG_UPDATE_CHECK_DONE = wxNewId();

// Inform the UI about download progress
const int MSG_DOWNLOAD_PROGRESS = wxNewId();

// Inform the UI that update download finished
const int MSG_DOWNLOAD_RESULT = wxNewId();

// Inform the UI that the installer successfully started.
const int MSG_STARTED_INSTALLER = wxNewId();

/*--------------------------------------------------------------------------*
                                Application
 *--------------------------------------------------------------------------*/

struct UpdateCheckResult {
  Error error;
  Appcast appcast;
};

class App : public wxApp {
 public:
  App();

  // Sends a message with ID @a msg to the app. This should be called from
  // UIThreadAccess.
  template <typename T>
  void SendMsg(int msg, const T& data) {
    wxThreadEvent* event = new wxThreadEvent(wxEVT_COMMAND_THREAD, msg);
    event->SetPayload(data);
    wxQueueEvent(this, event);
  }

  void SendMsg(int msg) {
    wxThreadEvent* event = new wxThreadEvent(wxEVT_COMMAND_THREAD, msg);
    wxQueueEvent(this, event);
  }

 private:
  bool OnInit() override;
  wxLayoutDirection GetLayoutDirection() const override;

  void InitUpdateDialog();

  void OnWindowClose(wxCloseEvent& event);
  void OnBringToFocus(wxThreadEvent& event);
  void OnShowCheckingUpdates(wxThreadEvent& event);
  void OnUpdateCheckDone(wxThreadEvent& event);
  void OnUpdateError(wxThreadEvent& event);
  void OnDownloadProgress(wxThreadEvent& event);
  void OnDownloadResult(wxThreadEvent& event);
  void OnStartedInstaller(wxThreadEvent& event);

 private:
  UpdateDialog* update_dialog_ = nullptr;
};

IMPLEMENT_APP_NO_MAIN(App)

App::App() {
  // Keep the wx "main" thread running even without windows. This greatly
  // simplifies threads handling, because we don't have to correctly
  // implement wx-thread restarting.
  //
  // Note that this only works if we don't explicitly call ExitMainLoop(),
  // except in reaction to win_sparkle_cleanup()'s message.
  // win_sparkle_cleanup() relies on the availability of wxApp instance and
  // if the event loop terminated, wxEntry() would return and wxApp instance
  // would be destroyed.
  //
  // Also note that this is efficient, because if there are no windows, the
  // thread will sleep waiting for a new event. We could safe some memory
  // by shutting the thread down when it's no longer needed, though.
  SetExitOnFrameDelete(false);

  // Bind events to their handlers:
  Bind(wxEVT_COMMAND_THREAD, &App::OnBringToFocus, this, MSG_BRING_TO_FOCUS);
  Bind(wxEVT_COMMAND_THREAD, &App::OnShowCheckingUpdates, this,
       MSG_SHOW_CHECKING_UPDATES);
  Bind(wxEVT_COMMAND_THREAD, &App::OnUpdateCheckDone, this,
       MSG_UPDATE_CHECK_DONE);
  Bind(wxEVT_COMMAND_THREAD, &App::OnDownloadProgress, this,
       MSG_DOWNLOAD_PROGRESS);
  Bind(wxEVT_COMMAND_THREAD, &App::OnDownloadResult, this, MSG_DOWNLOAD_RESULT);
  Bind(wxEVT_COMMAND_THREAD, &App::OnStartedInstaller, this,
       MSG_STARTED_INSTALLER);
}

bool App::OnInit() {
  if (!wxApp::OnInit())
    return false;

  wxString language = GetConfig().locale;
  language.Replace("-", "_");

  wxTranslations* trans = new wxTranslations();
  wxTranslations::Set(trans);

  trans->SetLoader(new WinsparkleTranslationsLoader());
  trans->SetLanguage(language);
  trans->AddCatalog("winsparkle");

  return true;
}

wxLayoutDirection App::GetLayoutDirection() const {
  wxString lang = wxTranslations::Get()->GetBestTranslation("winsparkle");
  const wxLanguageInfo* info = wxLocale::FindLanguageInfo(lang);
  return info ? info->LayoutDirection : wxLayout_Default;
}

void App::InitUpdateDialog() {
  if (!update_dialog_) {
    update_dialog_ = new UpdateDialog();
    update_dialog_->Bind(wxEVT_CLOSE_WINDOW, &App::OnWindowClose, this);
  }
}

void App::OnWindowClose(wxCloseEvent& event) {
  update_dialog_ = nullptr;
  g_delegate->WinsparkleOnUIClose();
  event.Skip();
}

void App::OnBringToFocus(wxThreadEvent& event) {
  // update_dialog_ can be null if the event was posted before the user closed
  // the update UI. Just ignore the event in that case.
  if (update_dialog_) {
    update_dialog_->ShowAsMainWindow();
  }
}

void App::OnShowCheckingUpdates(wxThreadEvent& event) {
  InitUpdateDialog();
  update_dialog_->StateCheckingUpdates();
  update_dialog_->ShowAsMainWindow();
}

void App::OnUpdateCheckDone(wxThreadEvent& event) {
  UpdateCheckResult result(event.GetPayload<UpdateCheckResult>());
  InitUpdateDialog();
  if (result.error) {
    update_dialog_->StateUpdateError(std::move(result.error));
  } else if (!result.appcast.IsValid()) {
    update_dialog_->StateNoUpdateFound();
  } else {
    update_dialog_->StateUpdateAvailable(std::move(result.appcast));
  }
  update_dialog_->ShowAsMainWindow();
}

void App::OnDownloadProgress(wxThreadEvent& event) {
  if (!update_dialog_) {
    // update_dialog_ can be null if the user closed the UI after the progress
    // message was posted. Ignore it if so.
    return;
  }
  DownloadReport report(event.GetPayload<DownloadReport>());
  update_dialog_->DownloadProgress(std::move(report));
}

void App::OnDownloadResult(wxThreadEvent& event) {
  if (!update_dialog_) {
    // update_dialog_ can be null if the user closed the update UI when the
    // background thread just finished preparing the installer to launch.
    // Re-open if so.
    InitUpdateDialog();
  }

  Error download_error(event.GetPayload<Error>());
  if (download_error) {
    update_dialog_->StateUpdateError(std::move(download_error));
  } else {
    update_dialog_->StateDownloaded();
  }
  update_dialog_->ShowAsMainWindow();
}

void App::OnStartedInstaller(wxThreadEvent& event) {
  Error installer_error(event.GetPayload<Error>());
  if (installer_error && installer_error.kind() != Error::kCancelled) {
    InitUpdateDialog();
    update_dialog_->StateUpdateError(std::move(installer_error));
    update_dialog_->ShowAsMainWindow();
    return;
  }
  if (update_dialog_) {
    update_dialog_->Close();
  }
}

/*--------------------------------------------------------------------------*
                             winsparkle::UI class
 *--------------------------------------------------------------------------*/

// This thread is only created when needed -- in most cases, it isn't. Once it
// is created, it runs indefinitely (without wasting CPU time -- it sleeps
// waiting for incoming messages).
struct UIThread : public DetachedThread {
  void Run() override;
};

// helper for accessing the UI thread
class UIThreadAccess {
 public:
  UIThreadAccess() { EnterCriticalSection(ui_thread_lock()); }

  ~UIThreadAccess() { LeaveCriticalSection(ui_thread_lock()); }

  winsparkle::App* App() { return GetGlobalState().active_app; }

  winsparkle::App& EnsureApp() {
    GlobalState& global_state = GetGlobalState();
    if (!global_state.active_app) {
      StartUIThread();
      DCHECK(global_state.active_app);
    }
    return *global_state.active_app;
  }

  static void Init() {
    // Ensure the global state is initialized.
    (void)GetGlobalState();
  }

  static void OnUIThreadStarted(winsparkle::App& app) {
    GlobalState& global_state = GetGlobalState();
    DCHECK(!global_state.active_app);
    DCHECK(global_state.start_event);
    global_state.active_app = &app;

    // After this the parent thread resumes.
    SetEvent(global_state.start_event);
  }

 private:
  struct GlobalState {
    GlobalState() { InitializeCriticalSection(&ui_thread_lock); }
    ~GlobalState() { DeleteCriticalSection(&ui_thread_lock); }
    DISALLOW_COPY_AND_ASSIGN(GlobalState);

    // The event to wait for UI thread initialization.
    HANDLE start_event = nullptr;

    winsparkle::App* active_app = nullptr;
    CRITICAL_SECTION ui_thread_lock;
  };

  static CRITICAL_SECTION* ui_thread_lock() {
    return &GetGlobalState().ui_thread_lock;
  }

  static GlobalState& GetGlobalState() {
    static base::NoDestructor<GlobalState> global_state;
    return *global_state;
  }

  static void StartUIThread() {
    GlobalState& global_state = GetGlobalState();
    global_state.start_event =
        CreateEvent(nullptr,  // default security attributes
                    false,    // auto-reset
                    false,    // initially non-signaled
                    nullptr   // anonymous
        );
    CHECK(global_state.start_event);
    DetachedThread::Start(std::make_unique<UIThread>());

    // Thread has started, wait until it initializes the App.
    WaitForSingleObject(global_state.start_event, INFINITE);
    DCHECK(global_state.active_app);
    CloseHandle(global_state.start_event);
  }
};

void UIThread::Run() {
  // The code must fallow through OnUIThreadStarted() to
  // ensure that the parent thread is waken up.

  // IMPLEMENT_WXWIN_MAIN does this as the first thing
  wxDISABLE_DEBUG_SUPPORT();

  // We do this before wxEntry() explicitly, even though wxEntry() would
  // do it too, so that we know when wx is initialized and can signal
  // run_wx_gui_from_dll() about it *before* starting the event loop.
  wxInitializer wxinit;

  // We cannot recover from wxWidget initialization errors.
  CHECK(wxinit.IsOk());
  UIThreadAccess::OnUIThreadStarted(wxGetApp());

  // Run the app:
  wxMSWDisableSettingHighDPIAware();
  HINSTANCE h_instance = GetModuleHandle(NULL);
  wxEntry(h_instance);
}

}  // namespace

/*static*/
void UI::Init(UIDelegate& delegate) {
  g_delegate = &delegate;
  UIThreadAccess::Init();
}

/* static */
void UI::BringToFocus() {
  UIThreadAccess uit;
  App& app = uit.EnsureApp();
  app.SendMsg(MSG_BRING_TO_FOCUS);
}

/*static*/
void UI::NotifyCheckingUpdates() {
  UIThreadAccess uit;
  App& app = uit.EnsureApp();
  app.SendMsg(MSG_SHOW_CHECKING_UPDATES);
}

/*static*/
void UI::NotifyUpdateCheckDone(const Appcast* appcast, const Error& error) {
  UIThreadAccess uit;
  App& app = uit.EnsureApp();
  UpdateCheckResult result;
  if (error) {
    result.error = error;
  } else if (appcast) {
    result.appcast = *appcast;
  }
  app.SendMsg(MSG_UPDATE_CHECK_DONE, result);
}

/*static*/
void UI::NotifyDownloadProgress(const DownloadReport& report) {
  UIThreadAccess uit;
  App& app = uit.EnsureApp();
  app.SendMsg(MSG_DOWNLOAD_PROGRESS, report);
}

/*static*/
void UI::NotifyDownloadResult(const Error& download_error) {
  UIThreadAccess uit;
  App& app = uit.EnsureApp();
  app.SendMsg(MSG_DOWNLOAD_RESULT, download_error);
}

/*static*/
void UI::NotifyStartedInstaller(const Error& error) {
  UIThreadAccess uit;
  App& app = uit.EnsureApp();
  app.SendMsg(MSG_STARTED_INSTALLER, error);
}

}  // namespace winsparkle
