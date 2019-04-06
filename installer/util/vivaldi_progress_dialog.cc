// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#include "installer/util/vivaldi_progress_dialog.h"
#include <CommCtrl.h>
#include "base/logging.h"
#include "chrome/installer/setup/setup_resource.h"


namespace installer {

VivaldiProgressDialog* VivaldiProgressDialog::this_ = NULL;

VivaldiProgressDialog::VivaldiProgressDialog(HINSTANCE instance)
    : instance_(instance),
      hdlg_(NULL),
      hwnd_progress_(NULL),
      thread_handle_(NULL),
      thread_id_(0) {
  this_ = this;
  dlg_event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
  DCHECK(dlg_event_);
}

VivaldiProgressDialog::~VivaldiProgressDialog() {}

bool VivaldiProgressDialog::ShowModeless() {
  thread_handle_ = CreateThread(NULL, 0, DlgThreadProc, this, 0, &thread_id_);
  if (thread_handle_ == NULL)
    return false;

  WaitForSingleObject(dlg_event_, INFINITE);  // wait for dialog creation
  return true;
}

void VivaldiProgressDialog::FinishProgress(int delay) {
  if (hdlg_ == NULL)
    return;
  SetMarqueeMode(false);
  if (delay > 0) {
    SetProgressValue(100);
    Sleep(delay);
  }
  SendMessage(hdlg_, WM_CLOSE, 0, 0);
  WaitForSingleObject(dlg_event_, INFINITE);  // wait for dialog termination
}

void VivaldiProgressDialog::SetProgressValue(int percentage) {
  if (hwnd_progress_)
    SendMessage(hwnd_progress_, PBM_SETPOS, percentage, 0);
}

void VivaldiProgressDialog::SetMarqueeMode(bool marquee_mode) {
  if (hwnd_progress_) {
    DWORD style = GetWindowLong(hwnd_progress_, GWL_STYLE);
    if (marquee_mode) {
      style |= PBS_MARQUEE;
      SetWindowLong(hwnd_progress_, GWL_STYLE, style);
      SendMessage(hwnd_progress_, PBM_SETMARQUEE, 1, 10);
    } else {
      SendMessage(hwnd_progress_, PBM_SETMARQUEE, 0, 0);
      style &= ~PBS_MARQUEE;
      SetWindowLong(hwnd_progress_, GWL_STYLE, style);
    }
  }
}

// static
INT_PTR CALLBACK VivaldiProgressDialog::DlgProc(HWND hdlg,
                                                UINT msg,
                                                WPARAM wparam,
                                                LPARAM lparam) {
  switch (msg) {
    case WM_INITDIALOG:
      this_->hdlg_ = hdlg;
      this_->hwnd_progress_ = GetDlgItem(hdlg, IDC_PROGRESS1);
      this_->SetMarqueeMode(true);
      SetForegroundWindow(hdlg);
      SetEvent(this_->dlg_event_);
      return (INT_PTR)TRUE;
    case WM_CLOSE:
      EndDialog(hdlg, LOWORD(wparam));
      SetEvent(this_->dlg_event_);
      this_->hdlg_ = NULL;
      return (INT_PTR)TRUE;
    case WM_COMMAND:
      if (LOWORD(wparam) == IDCANCEL) {
        EndDialog(hdlg, LOWORD(wparam));
        SetEvent(this_->dlg_event_);
        this_->hdlg_ = NULL;
        return (INT_PTR)TRUE;
      }
      break;
  }
  return (INT_PTR)FALSE;
}

// static
DWORD WINAPI VivaldiProgressDialog::DlgThreadProc(LPVOID lparam) {
  IsGUIThread(TRUE);  // make sure we have a UI thread with a message loop
  DialogBox(this_->instance_, MAKEINTRESOURCE(IDD_DIALOG2), NULL, DlgProc);
  return 0;
}

}  // namespace installer
