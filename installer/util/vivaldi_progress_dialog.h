// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_PROGRESS_DIALOG_H_
#define INSTALLER_UTIL_VIVALDI_PROGRESS_DIALOG_H_

#include <windows.h>
#include "base/macros.h"

namespace installer {

class VivaldiProgressDialog {
 public:
  explicit VivaldiProgressDialog(HINSTANCE instance);
  virtual ~VivaldiProgressDialog();

  bool ShowModeless();
  void FinishProgress(int delay);  // if delay == 0; terminate dialog
                                   // immediately, otherwise set progress to
                                   // 100% and delay termination 'delay'
                                   // millisecs

  void SetProgressValue(int percentage);
  void SetMarqueeMode(bool marquee_mode);

 private:
  static INT_PTR CALLBACK DlgProc(HWND hdlg,
                                  UINT msg,
                                  WPARAM wparam,
                                  LPARAM lparam);
  static DWORD WINAPI DlgThreadProc(LPVOID lpParam);

 private:
  HINSTANCE instance_;
  HWND hdlg_;
  HWND hwnd_progress_;
  HANDLE thread_handle_;
  DWORD thread_id_;
  HANDLE dlg_event_;
  static VivaldiProgressDialog* this_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiProgressDialog);
};

}  // namespace installer

#endif  // INSTALLER_UTIL_VIVALDI_PROGRESS_DIALOG_H_
