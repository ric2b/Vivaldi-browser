// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_process_finder_win.h"

#include <shellapi.h>
#include <string>
#include <tlhelp32.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/message_window.h"
#include "base/win/metro.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#include "chrome/browser/policy/policy_path_parser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"


namespace {

int timeout_in_milliseconds = 20 * 1000;

// The following is copied from net/base/escape.cc. We don't want to depend on
// net here because this gets compiled into chrome.exe to facilitate
// fast-rendezvous (see https://codereview.chromium.org/14617003/).

// TODO(koz): Move these functions out of net/base/escape.cc into base/escape.cc
// so we can depend on it directly.

// BEGIN COPY from net/base/escape.cc

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
struct Charmap {
  bool Contains(unsigned char c) const {
    return ((map[c >> 5] & (1 << (c & 31))) != 0);
  }

  uint32 map[8];
};

const char kHexString[] = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20.
std::string Escape(const std::string& text, const Charmap& charmap,
                   bool use_plus) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
static const Charmap kQueryCharmap = {{
  0xffffffffL, 0xfc00987dL, 0x78000001L, 0xb8000001L,
  0xffffffffL, 0xffffffffL, 0xffffffffL, 0xffffffffL
}};

std::string EscapeQueryParamValue(const std::string& text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

// END COPY from net/base/escape.cc

}  // namespace

namespace chrome {

void KillVivaldiProcesses(std::vector<DWORD>& process_ids) {
  if (process_ids.empty())
    return;

  std::wstring cmd_line_string(L"taskkill.exe /F");
  std::vector<DWORD>::iterator it;
  for (it = process_ids.begin(); it != process_ids.end(); it++) {
    DWORD pid = *it;
    cmd_line_string += base::StringPrintf(L" /PID %d", pid);
  }

  scoped_ptr<wchar_t[]> cmd_line(new wchar_t[cmd_line_string.length() + 1]);
  std::copy(cmd_line_string.begin(), cmd_line_string.end(), cmd_line.get());
  cmd_line[cmd_line_string.length()] = 0;

  STARTUPINFO si = { sizeof(si) };
  PROCESS_INFORMATION pi = { 0 };

  if (CreateProcess(NULL, cmd_line.get(), NULL, NULL, FALSE, CREATE_NO_WINDOW,
      NULL, NULL, &si, &pi)) {
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  }
}

typedef BOOL(WINAPI *LPQueryFullProcessImageName)(
    HANDLE hProcess, DWORD dwFlags, LPWSTR lpExeName, PDWORD lpdwSize);

static LPQueryFullProcessImageName fpQueryFullProcessImageName = NULL;

bool LoadQueryFullProcessImageNameFunc()
{
  HMODULE hDLL = LoadLibrary(L"kernel32.dll");
  if (!hDLL)
    return false;

  fpQueryFullProcessImageName =
    (LPQueryFullProcessImageName)GetProcAddress(hDLL,
      "QueryFullProcessImageNameW");

  if (!fpQueryFullProcessImageName)
    return false;

  return true;
}

void GetRunningVivaldiProcesses(const std::wstring& path,
    std::vector<DWORD>& process_ids) {

  if (!fpQueryFullProcessImageName) {
    if (!LoadQueryFullProcessImageNameFunc())
      return;
  }

  process_ids.clear();
  PROCESSENTRY32 entry = { sizeof(PROCESSENTRY32) };
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
  if (snapshot && Process32First(snapshot, &entry)) {
    while (Process32Next(snapshot, &entry)) {
      if (!wcsicmp(entry.szExeFile, L"vivaldi.exe")) {
        if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
          HANDLE process =
            OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, entry.th32ProcessID);
          if (process) {
            wchar_t process_image_name[MAX_PATH] = { 0 };
            DWORD size = MAX_PATH;
            if (fpQueryFullProcessImageName(process, 0, process_image_name,
                &size)) {
              std::wstring proc_path(process_image_name);
              std::wstring::size_type pos = proc_path.rfind(L"\\vivaldi.exe");
              if (pos != std::wstring::npos)
                proc_path = proc_path.substr(0, pos);

              if (proc_path != path ||
                  GetCurrentProcessId() == entry.th32ProcessID) {
                CloseHandle(process);
                continue;
              }
            }
            CloseHandle(process);
          }
        }
        process_ids.push_back(entry.th32ProcessID);
      }
    }
  }
  if (snapshot)
    CloseHandle(snapshot);
}

void AttemptToKillTheUndead() {
  base::FilePath exe_path;
  PathService::Get(base::DIR_EXE, &exe_path);

  std::vector<DWORD> process_ids;
  GetRunningVivaldiProcesses(exe_path.value(), process_ids);
  if (!process_ids.empty())
    KillVivaldiProcesses(process_ids);
}

HWND FindRunningChromeWindow(const base::FilePath& user_data_dir) {
  return base::win::MessageWindow::FindWindow(user_data_dir.value());
}

NotifyChromeResult AttemptToNotifyRunningChrome(HWND remote_window,
                                                bool fast_start) {
  DCHECK(remote_window);
  DWORD process_id = 0;
  DWORD thread_id = GetWindowThreadProcessId(remote_window, &process_id);
  if (!thread_id || !process_id)
    return NOTIFY_FAILED;

  base::CommandLine command_line(*base::CommandLine::ForCurrentProcess());
  command_line.AppendSwitchASCII(
      switches::kOriginalProcessStartTime,
      base::Int64ToString(
          base::CurrentProcessInfo::CreationTime().ToInternalValue()));

  if (fast_start)
    command_line.AppendSwitch(switches::kFastStart);

  // Send the command line to the remote chrome window.
  // Format is "START\0<<<current directory>>>\0<<<commandline>>>".
  std::wstring to_send(L"START\0", 6);  // want the NULL in the string.
  base::FilePath cur_dir;
  if (!base::GetCurrentDirectory(&cur_dir))
    return NOTIFY_FAILED;
  to_send.append(cur_dir.value());
  to_send.append(L"\0", 1);  // Null separator.
  to_send.append(command_line.GetCommandLineString());
  to_send.append(L"\0", 1);  // Null separator.

  // Allow the current running browser window to make itself the foreground
  // window (otherwise it will just flash in the taskbar).
  ::AllowSetForegroundWindow(process_id);

  COPYDATASTRUCT cds;
  cds.dwData = 0;
  cds.cbData = static_cast<DWORD>((to_send.length() + 1) * sizeof(wchar_t));
  cds.lpData = const_cast<wchar_t*>(to_send.c_str());

  DWORD_PTR result = 0;

  if (::SendMessageTimeout(remote_window, WM_COPYDATA, NULL,
                           reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG,
                           timeout_in_milliseconds, &result)) {
    return result ? NOTIFY_SUCCESS : NOTIFY_FAILED;
  }

  // It is possible that the process owning this window may have died by now.
  if (!::IsWindow(remote_window)) {
    AttemptToKillTheUndead();
    return NOTIFY_FAILED;
  }

  AttemptToKillTheUndead();
  // If the window couldn't be notified but still exists, assume it is hung.
  return NOTIFY_WINDOW_HUNG;
}

base::TimeDelta SetNotificationTimeoutForTesting(base::TimeDelta new_timeout) {
  base::TimeDelta old_timeout =
      base::TimeDelta::FromMilliseconds(timeout_in_milliseconds);
  timeout_in_milliseconds = new_timeout.InMilliseconds();
  return old_timeout;
}

}  // namespace chrome
