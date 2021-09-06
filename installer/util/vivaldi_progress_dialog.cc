// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_progress_dialog.h"

#include <CommCtrl.h>

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/installer/setup/setup_resource.h"

#include "installer/win/detached_thread.h"

namespace installer {

VivaldiProgressDialog* VivaldiProgressDialog::this_ = NULL;

namespace {

constexpr UINT kSetProgressMessage = WM_APP;

HWND GetProgressControl(HWND hdlg) {
  return ::GetDlgItem(hdlg, IDC_PROGRESS1);
}

void SetMarqueeMode(HWND hdlg, bool marquee_mode) {
  HWND hwnd_progress = GetProgressControl(hdlg);
  if (!hwnd_progress)
    return;
  DWORD style = GetWindowLong(hwnd_progress, GWL_STYLE);
  if (marquee_mode) {
    style |= PBS_MARQUEE;
    ::SetWindowLong(hwnd_progress, GWL_STYLE, style);
    ::SendMessage(hwnd_progress, PBM_SETMARQUEE, 1, 10);
  } else {
    ::SendMessage(hwnd_progress, PBM_SETMARQUEE, 0, 0);
    style &= ~PBS_MARQUEE;
    ::SetWindowLong(hwnd_progress, GWL_STYLE, style);
  }
}

void SetProgressValue(HWND hdlg, int percentage) {
  HWND hwnd_progress = GetProgressControl(hdlg);
  if (!hwnd_progress)
    return;
  if (percentage >= 100) {
    SetMarqueeMode(hdlg, false);
  }
  ::SendMessage(hwnd_progress, PBM_SETPOS, percentage, 0);
}

}  // namespace

class VivaldiProgressDialog::ProgressThread : public vivaldi::DetachedThread {
 public:
  ProgressThread(HINSTANCE hInstance) : hInstance_(hInstance) {}
  void Run() override {
    // make sure we have a UI thread with a message loop.
    ::IsGUIThread(TRUE);
    ::DialogBox(hInstance_, MAKEINTRESOURCE(IDD_DIALOG2), NULL, DlgProc);
  }
  HINSTANCE hInstance_ = nullptr;
};

VivaldiProgressDialog::VivaldiProgressDialog() {
  dlg_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  CHECK(dlg_event_);
}

VivaldiProgressDialog::~VivaldiProgressDialog() {
  NOTREACHED();
}

/* static */
void VivaldiProgressDialog::ShowModeless(HINSTANCE hInstance) {
  if (!this_) {
    // This is never deleted. This way we do not need to wait for the the
    // progress thread to finish.
    this_ = new VivaldiProgressDialog();
  }
  vivaldi::DetachedThread::Start(std::make_unique<ProgressThread>(hInstance));
  WaitForSingleObject(this_->dlg_event_, INFINITE);  // wait for dialog creation
}

/* static */
void VivaldiProgressDialog::SetProgress(int percent) {
  PostMessageToDialog(kSetProgressMessage, percent);
}

/* static */
void VivaldiProgressDialog::Finish() {
  PostMessageToDialog(WM_CLOSE, 0);
}

/* static */
void VivaldiProgressDialog::PostMessageToDialog(UINT msg, WPARAM wparam) {
  if (!this_)
    return;

  base::AutoLock autolock(this_->lock_);

  // The user may have closed the dialog with Alt-F4 at this point.
  if (!this_->hdlg_)
    return;

  // Do not use SendMessage as that waits for a responce resulting in a deadlock
  // in CloseDialog().
  ::PostMessage(this_->hdlg_, msg, wparam, 0);
}

void VivaldiProgressDialog::CloseDialog() {
  HWND hdlg;
  {
    base::AutoLock autolock(lock_);
    hdlg = hdlg_;
    hdlg_ = nullptr;
  }
  ::EndDialog(hdlg, 0);
}

// static
INT_PTR CALLBACK VivaldiProgressDialog::DlgProc(HWND hdlg,
                                                UINT msg,
                                                WPARAM wparam,
                                                LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG:
      // We do not need to take a lock here to protect the dialog access as the
      // parent thread waits for the dlg_event_ event.
      this_->hdlg_ = hdlg;
      SetMarqueeMode(hdlg, true);
      SetForegroundWindow(hdlg);
      SetEvent(this_->dlg_event_);
      return (INT_PTR)TRUE;
    case WM_CLOSE:
      this_->CloseDialog();
      return (INT_PTR)TRUE;
    case WM_COMMAND:
      if (LOWORD(wparam) == IDCANCEL) {
        // React to Alt-F4
        this_->CloseDialog();
        return (INT_PTR)TRUE;
      }
      break;
    case kSetProgressMessage:
      SetProgressValue(hdlg, static_cast<int>(wparam));
      return (INT_PTR)TRUE;
  }
  return (INT_PTR)FALSE;
}

}  // namespace installer
