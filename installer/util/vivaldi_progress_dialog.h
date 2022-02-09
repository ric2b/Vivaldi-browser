// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.

#ifndef INSTALLER_UTIL_VIVALDI_PROGRESS_DIALOG_H_
#define INSTALLER_UTIL_VIVALDI_PROGRESS_DIALOG_H_

#include <Windows.h>

#include "base/synchronization/lock.h"

namespace installer {

class VivaldiProgressDialog {
 public:
  static void ShowModeless(HINSTANCE instance);
  static void SetProgress(int perecent);
  static void Finish();

 private:
  class ProgressThread;
  friend ProgressThread;

  VivaldiProgressDialog();
  ~VivaldiProgressDialog();
  VivaldiProgressDialog(const VivaldiProgressDialog&) = delete;
  VivaldiProgressDialog& operator=(const VivaldiProgressDialog&) = delete;

  static INT_PTR CALLBACK DlgProc(HWND hdlg,
                                  UINT msg,
                                  WPARAM wparam,
                                  LPARAM lparam);

  static void PostMessageToDialog(UINT msg, WPARAM wparam);

  void FinishProgressImpl(int delay);
  void CloseDialog();

  // Lock protecting the access to hdlg_
  base::Lock lock_;
  HWND hdlg_ = nullptr;
  HANDLE dlg_event_ = nullptr;
  static VivaldiProgressDialog* this_;
};

}  // namespace installer

#endif  // INSTALLER_UTIL_VIVALDI_PROGRESS_DIALOG_H_
