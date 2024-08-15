// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "components/sandbox/vivaldi_module_name.h"

#include <windows.h>

#include "app/vivaldi_apptools.h"

#include "sandbox/win/src/interception.h"
#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/internal_types.h"

namespace sandbox {

#if defined(_WIN64) && defined(NDEBUG)

SANDBOX_INTERCEPT OriginalFunctions g_originals;

namespace {

typedef DWORD(WINAPI* GetModuleFileNameAFunction)(HMODULE hModule,
                                                  LPSTR lpFilename,
                                                  DWORD nSize);

typedef DWORD(WINAPI* GetModuleFileNameExAFunction)(HANDLE hProcess,
                                                    HMODULE hModule,
                                                    LPSTR lpFilename,
                                                    DWORD nSize);
typedef DWORD(WINAPI* GetModuleFileNameWFunction)(HMODULE hModule,
                                                  LPWSTR lpFilename,
                                                  DWORD nSize);

typedef DWORD(WINAPI* GetModuleFileNameExWFunction)(HANDLE hProcess,
                                                    HMODULE hModule,
                                                    LPWSTR lpFilename,
                                                    DWORD nSize);

const char vivaldi_exe_a[] = "vivaldi.exe";
const wchar_t vivaldi_exe_w[] = L"vivaldi.exe";
const DWORD vivaldi_exe_len = ARRAYSIZE(vivaldi_exe_a) - /* nul byte */ 1;

const char chrome_exe_a[] = "chrome.exe";
const wchar_t chrome_exe_w[] = L"chrome.exe";
const DWORD chrome_exe_len = ARRAYSIZE(chrome_exe_a) - /* nul byte */ 1;

DWORD PatchNameA(LPSTR lpFilename, DWORD len) {
  if (len >= vivaldi_exe_len) {
    const DWORD exe_string_start = len - vivaldi_exe_len;
    if (len == vivaldi_exe_len || lpFilename[exe_string_start - 1] == '\\' ||
        lpFilename[exe_string_start - 1] == '/') {
      if (lstrcmpA(lpFilename + exe_string_start, vivaldi_exe_a) == 0) {
        lstrcpyA(lpFilename + exe_string_start, chrome_exe_a);
        len = exe_string_start + chrome_exe_len;
      }
    }
  }

  return len;
}

DWORD PatchNameW(LPWSTR lpFilename, DWORD len) {
  if (len >= vivaldi_exe_len) {
    const DWORD exe_string_start = len - vivaldi_exe_len;
    if (len == vivaldi_exe_len || lpFilename[exe_string_start - 1] == '\\' ||
        lpFilename[exe_string_start - 1] == '/') {
      if (lstrcmpW(lpFilename + exe_string_start, vivaldi_exe_w) == 0) {
        lstrcpyW(lpFilename + exe_string_start, chrome_exe_w);
        len = exe_string_start + chrome_exe_len;
      }
    }
  }

  return len;
}

DWORD PatchGetModuleFileNameA(
    GetModuleFileNameAFunction orig_get_module_filename_a,
    LPSTR lpFilename,
    DWORD nSize) {
  DWORD len = orig_get_module_filename_a(nullptr, lpFilename, nSize);

  return PatchNameA(lpFilename, len);
}

DWORD PatchGetModuleFileNameW(
    GetModuleFileNameWFunction orig_get_module_filename_w,
    LPWSTR lpFilename,
    DWORD nSize) {
  DWORD len = orig_get_module_filename_w(nullptr, lpFilename, nSize);

  return PatchNameW(lpFilename, len);
}

DWORD PatchGetModuleFileNameExA(
    GetModuleFileNameExAFunction orig_get_module_filename_ex_a,
    HANDLE hProcess,
    LPSTR lpFilename,
    DWORD nSize) {
  DWORD len =
      orig_get_module_filename_ex_a(hProcess, nullptr, lpFilename, nSize);

  return PatchNameA(lpFilename, len);
}

DWORD PatchGetModuleFileNameExW(
    GetModuleFileNameExWFunction orig_get_module_filename_ex_w,
    HANDLE hProcess,
    LPWSTR lpFilename,
    DWORD nSize) {
  DWORD len =
      orig_get_module_filename_ex_w(hProcess, nullptr, lpFilename, nSize);

  return PatchNameW(lpFilename, len);
}

DWORD TargetGetModuleFileNameA(
    GetModuleFileNameAFunction orig_get_module_filename_a,
    HMODULE hModule,
    LPSTR lpFilename,
    DWORD nSize) {
  if (hModule == nullptr && lpFilename && nSize) {
    return PatchGetModuleFileNameA(orig_get_module_filename_a, lpFilename,
                                   nSize);
  }
  return orig_get_module_filename_a(hModule, lpFilename, nSize);
}

DWORD TargetGetModuleFileNameExA(
    GetModuleFileNameExAFunction orig_get_module_filename_ex_a,
    HANDLE hProcess,
    HMODULE hModule,
    LPSTR lpFilename,
    DWORD nSize) {
  if (hModule == nullptr && lpFilename && nSize &&
      hProcess == GetCurrentProcess()) {
    return PatchGetModuleFileNameExA(orig_get_module_filename_ex_a, hProcess,
                                     lpFilename, nSize);
  }
  return orig_get_module_filename_ex_a(hProcess, hModule, lpFilename, nSize);
}

DWORD TargetGetModuleFileNameW(
    GetModuleFileNameWFunction orig_get_module_filename_w,
    HMODULE hModule,
    LPWSTR lpFilename,
    DWORD nSize) {
  if (hModule == nullptr && lpFilename && nSize) {
    return PatchGetModuleFileNameW(orig_get_module_filename_w, lpFilename,
                                   nSize);
  }
  return orig_get_module_filename_w(hModule, lpFilename, nSize);
}

DWORD TargetGetModuleFileNameExW(
    GetModuleFileNameExWFunction orig_get_module_filename_ex_w,
    HANDLE hProcess,
    HMODULE hModule,
    LPWSTR lpFilename,
    DWORD nSize) {
  if (hModule == nullptr && lpFilename && nSize &&
      hProcess == GetCurrentProcess()) {
    return PatchGetModuleFileNameExW(orig_get_module_filename_ex_w, hProcess,
                                     lpFilename, nSize);
  }
  return orig_get_module_filename_ex_w(hProcess, hModule, lpFilename, nSize);
}

#if defined(_WIN64)
DWORD TargetGetModuleFileNameA64(HMODULE hModule,
                                 LPSTR lpFilename,
                                 DWORD nSize) {
  GetModuleFileNameAFunction orig_fn =
      reinterpret_cast<GetModuleFileNameAFunction>(
          g_originals.functions[GET_MODULE_FILENAME_A]);
  return TargetGetModuleFileNameA(orig_fn, hModule, lpFilename, nSize);
}

DWORD TargetGetModuleFileNameExA64(HANDLE hProcess,
                                   HMODULE hModule,
                                   LPSTR lpFilename,
                                   DWORD nSize) {
  GetModuleFileNameExAFunction orig_fn =
      reinterpret_cast<GetModuleFileNameExAFunction>(
          g_originals.functions[GET_MODULE_FILENAME_EX_A]);
  return TargetGetModuleFileNameExA(orig_fn, hProcess, hModule, lpFilename,
                                    nSize);
}
DWORD TargetGetModuleFileNameW64(HMODULE hModule,
                                 LPWSTR lpFilename,
                                 DWORD nSize) {
  GetModuleFileNameWFunction orig_fn =
      reinterpret_cast<GetModuleFileNameWFunction>(
          g_originals.functions[GET_MODULE_FILENAME_W]);
  return TargetGetModuleFileNameW(orig_fn, hModule, lpFilename, nSize);
}

DWORD TargetGetModuleFileNameExW64(HANDLE hProcess,
                                   HMODULE hModule,
                                   LPWSTR lpFilename,
                                   DWORD nSize) {
  GetModuleFileNameExWFunction orig_fn =
      reinterpret_cast<GetModuleFileNameExWFunction>(
          g_originals.functions[GET_MODULE_FILENAME_EX_W]);
  return TargetGetModuleFileNameExW(orig_fn, hProcess, hModule, lpFilename,
                                    nSize);
}
#endif

}  // namespace

#endif // outer win64

bool VivaldiSetupBasicInterceptions(InterceptionManager* manager) {
#if defined(_WIN64) && defined(NDEBUG)
  if (!vivaldi::IsVivaldiRunning())
    return true;

  static_assert(sizeof(HMODULE) + sizeof(LPWSTR) + sizeof(DWORD) <= 20);
  static_assert(
      sizeof(HANDLE) + sizeof(HMODULE) + sizeof(LPWSTR) + sizeof(DWORD) <= 28);
  if (!INTERCEPT_EAT(manager, kKerneldllName, GetModuleFileNameA,
                     GET_MODULE_FILENAME_A, 20) ||
      !INTERCEPT_EAT(manager, kKerneldllName, GetModuleFileNameW,
                     GET_MODULE_FILENAME_W, 20) ||
      !INTERCEPT_EAT(manager, kKerneldllName, GetModuleFileNameExA,
                     GET_MODULE_FILENAME_EX_A, 28) ||
      !INTERCEPT_EAT(manager, kKerneldllName, GetModuleFileNameExW,
                     GET_MODULE_FILENAME_EX_W, 28))
    return false;
#endif
  return true;
}

}  // namespace sandbox
