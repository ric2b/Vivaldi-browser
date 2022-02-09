// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

// This must be included before other Windows headers.
#include <Windows.h>

#include <CommCtrl.h>

#include "base/win/current_module.h"

#include "app/vivaldi_commands.h"
#include "browser/win/vivaldi_utils.h"

#pragma comment(lib, "ComCtl32.lib")

namespace {

static const int TIMER_ID = 123;
static const int TIMEOUT_CHECK_DELAY = 500;

INT_PTR CALLBACK DialogProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
    case WM_CLOSE:
      DestroyWindow(dlg);
      return TRUE;

    case WM_DESTROY:
      return TRUE;

    case WM_COMMAND: {
      switch (LOWORD(wParam)) {
        case IDCANCEL:
          SendMessage(dlg, WM_CLOSE, 0, 0);
          PostQuitMessage(0);
          return TRUE;
      }
      break;
    }
    case WM_TIMER: {
      if (!vivaldi::IsVivaldiExiting()) {
        KillTimer(dlg, TIMER_ID);
        SendMessage(dlg, WM_CLOSE, 0, 0);
        PostQuitMessage(0);
        return TRUE;
      }
      break;
    }
  }
  return FALSE;
}

}  // namespace

bool OpenVivaldiRunningDialog() {
  HWND dlg;
  MSG msg;
  BOOL ret;

  InitCommonControls();
  dlg = CreateDialogParam(CURRENT_MODULE(), MAKEINTRESOURCE(IDD_EXIT_WAIT_DLG),
                          0, DialogProc, 0);
  if (!dlg) {
    int error = GetLastError();
    return error != S_OK;
  }

  ShowWindow(dlg, SW_SHOW);
  SendMessage(GetDlgItem(dlg, IDC_EXIT_PROGRESS), PBM_SETMARQUEE, 1, 0);
  SetTimer(dlg, TIMER_ID, TIMEOUT_CHECK_DELAY, NULL);

  while ((ret = GetMessage(&msg, 0, 0, 0)) != 0) {
    if (ret == -1)
      return false;

    if (!IsDialogMessage(dlg, &msg)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  return true;
}