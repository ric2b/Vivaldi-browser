/*
 *  This file is part of WinSparkle (https://winsparkle.org)
 *
 *  Copyright (C) 2012-2016 Vaclav Slavik
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

#include "update_notifier/thirdparty/winsparkle/src/updatedownloader.h"

#include "update_notifier/thirdparty/winsparkle/src/appcast.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/thirdparty/winsparkle/src/download.h"
#include "update_notifier/thirdparty/winsparkle/src/settings.h"
#include "update_notifier/thirdparty/winsparkle/src/updatechecker.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

// Windows must be included before Softpub. So prevent clang-format from sorting
// it with other headers by using blank lines around it.
#include <Windows.h>

#include <Softpub.h>
#include <WinTrust.h>
#include <io.h>
#include <rpc.h>
#include <shellapi.h>
#include <time.h>

// Link with Windows libraries
#pragma comment(lib, "wintrust")

namespace winsparkle {

/*--------------------------------------------------------------------------*
                                  helpers
 *--------------------------------------------------------------------------*/

namespace {

const wchar_t kExpandExe[] = L"expand.exe";

// TODO(igor@vivaldi.com): Consider using base::CreateNewTempDirectory() from
// base::file_util.h. This is not used that that uses ScopedBlockingCall() and
// it is not clear if that can be used with threads not created with Chromium
// API.
base::FilePath CreateUniqueTempDirectory(Error& error) {
  if (error)
    return base::FilePath();

  // We need to put downloaded updates into a directory of their own, because
  // if we put it in $TMP, some DLLs could be there and interfere with the
  // installer.
  //
  // This code creates a new randomized directory name and tries to create it;
  // this process is repeated if the directory already exists.
  wchar_t tmpdir[MAX_PATH + 1];
  if (GetTempPath(MAX_PATH + 1, tmpdir) == 0) {
    error.set(
        Error::kStorage,
        LastWin32Error("GetTempPath", "Cannot create temporary directory"));
    return base::FilePath();
  }

  for (;;) {
    std::wstring dir(tmpdir);
    dir += L"Update-";

    UUID uuid;
    RPC_STATUS status = UuidCreate(&uuid);
    if (status != RPC_S_OK && status != RPC_S_UUID_LOCAL_ONLY) {
      error.set(Error::kStorage, LastWin32Error("UuidCreate"));
      return base::FilePath();
    }
    RPC_WSTR uuidStr = nullptr;
    status = UuidToString(&uuid, &uuidStr);
    if (status != RPC_S_OK) {
      error.set(Error::kStorage, LastWin32Error("UuidToString"));
      return base::FilePath();
    }
    dir += reinterpret_cast<wchar_t*>(uuidStr);
    RpcStringFree(&uuidStr);

    if (CreateDirectory(dir.c_str(), NULL))
      return base::FilePath(dir);
    if (GetLastError() != ERROR_ALREADY_EXISTS) {
      error.set(Error::kStorage,
                LastWin32Error("CreateDirectory",
                               "Cannot create temporary directory"));
      return base::FilePath();
    }
  }
}

base::FilePath DownloadUrl(const GURL& url,
                           const base::FilePath& tmpdir,
                           DownloadReport& report,
                           DownloadUpdateDelegate& delegate,
                           Error& error) {
  FileDownloader downloader(url, 0, error);
  if (error)
    return base::FilePath();
  report.kind = DownloadReport::kConnected;
  report.content_length = downloader.GetContentLength();
  report.downloaded_length = 0;
  delegate.SendReport(report, error);
  if (error)
    return base::FilePath();

  base::FilePath download_path =
      tmpdir.Append(base::UTF8ToUTF16(downloader.GetFileName()));
  FILE* file = _wfopen(download_path.value().c_str(), L"wb");
  if (!file) {
    error.set(Error::kStorage, "Cannot open update file " +
                                   download_path.AsUTF8Unsafe() +
                                   " for writing");
    return base::FilePath();
  }

  // From this point the code must not return until properly closing and
  // deleteing on errors the file.
  while (downloader.FetchData(error)) {
    size_t n = downloader.GetDataLength();
    report.kind = DownloadReport::kMoreData;
    report.downloaded_length = downloader.GetTotalReadLength();
    delegate.SendReport(report, error);
    if (error)
      break;

    size_t written_items = fwrite(downloader.GetData(), n, 1, file);
    if (written_items != 1) {
      error.set(Error::kStorage, "Cannot write " +
                                     std::to_string(report.downloaded_length) +
                                     " bytes to the update file " +
                                     download_path.AsUTF8Unsafe());
      break;
    }
  }

  if (!error && downloader.GetTotalReadLength() == 0) {
    error.set(Error::kNetwork, "No data was downloaded for the update file");
  }

  if (error) {
    // Do not consume space for failed download. Although we will try to
    // delete, another application could have openned the file preventing
    // its deletion.
    (void)_chsize(_fileno(file), 0);
    fclose(file);
    DeleteFile(download_path.value().c_str());
    return base::FilePath();
  }
  fclose(file);

  return download_path;
}

GURL FindDeltaURL(const Appcast& appcast) {
  GURL delta_url;
  if (!appcast.Deltas.empty()) {
    // Download the delta update file if there is a valid download URL
    const std::string currentVersion =
        base::UTF16ToUTF8(GetConfig().app_version);
    for (auto& delta : appcast.Deltas) {
      if (!delta.DownloadURL.is_valid())
        continue;
      if (CompareVersions(currentVersion, delta.DeltaFrom) != 0)
        continue;
      // Check that URL path ends with ".cab".
      if (!base::EndsWith(delta.DownloadURL.PathForRequestPiece(), ".cab"))
        continue;
      delta_url = delta.DownloadURL;
      break;
    }
  }
  return delta_url;
}

void VerifyEmbeddedSignature(const std::wstring& file_path,
                             DownloadReport& report,
                             DownloadUpdateDelegate& delegate,
                             Error& error) {
  if (error)
    return;
  report.kind = DownloadReport::kVerificationStart;
  delegate.SendReport(report, error);
  if (error)
    return;

  GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  WINTRUST_FILE_INFO FileData;
  memset(&FileData, 0, sizeof(FileData));
  FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
  FileData.pcwszFilePath = file_path.c_str();
  FileData.hFile = NULL;
  FileData.pgKnownSubject = NULL;

  WINTRUST_DATA WinTrustData;
  memset(&WinTrustData, 0, sizeof(WinTrustData));
  WinTrustData.cbStruct = sizeof(WinTrustData);
  WinTrustData.pPolicyCallbackData = NULL;
  WinTrustData.pSIPClientData = NULL;
  WinTrustData.dwUIChoice = WTD_UI_NOGOOD;  // WTD_UI_NONE;
  WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;
  WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
  WinTrustData.hWVTStateData = NULL;
  WinTrustData.pwszURLReference = NULL;
  WinTrustData.dwUIContext = 0;
  WinTrustData.pFile = &FileData;

  LONG lStatus = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);
  bool is_trusted = (lStatus == ERROR_SUCCESS);

  // Any hWVTStateData must be released by a call with close.
  WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

  if (!is_trusted) {
    error.set(Error::kVerify,
              "Failed to verify a downloaded file signature, status=" +
                  std::to_string(lStatus));
  }
}

// Extract the cabinet file (CAB) which contains two 7z files
void ExpandDeltaArchive(const std::wstring& file_path_str,
                        DownloadReport& report,
                        DownloadUpdateDelegate& delegate,
                        Error& error) {
  if (error)
    return;
  report.kind = DownloadReport::kDeltaExtraction;
  delegate.SendReport(report, error);
  if (error)
    return;

  base::FilePath file_path(file_path_str);
  std::wstring args(L"-R -f:* ");

  // TODO(igor@vivaldi.com): Properly escape the file name according to Windows
  // rules in case it contains spaces.
  args.append(file_path.BaseName().value());
  args.append(L" .");  // expand to current dir

  std::wstring run_directory = file_path.DirName().value();

  SHELLEXECUTEINFOW sei = {0};
  sei.cbSize = sizeof(SHELLEXECUTEINFOW);
  sei.nShow = SW_HIDE;
  sei.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOCLOSEPROCESS;
  sei.lpFile = kExpandExe;
  sei.lpParameters = args.c_str();
  sei.lpDirectory = run_directory.c_str();
  if (!::ShellExecuteExW(&sei)) {
    error.set(Error::kExec,
              LastWin32Error("ShellExecuteExW", base::UTF16ToUTF8(sei.lpFile)));
    return;
  }
  for (;;) {
    // Wait for 2 seconds so we can check if the user canceled the download.
    DWORD status = ::WaitForSingleObject(sei.hProcess, 2000);
    if (status == WAIT_OBJECT_0)
      break;
    if (status != WAIT_TIMEOUT) {
      error.set(Error::kExec, LastWin32Error("WaitForSingleObject"));
      break;
    }
    delegate.SendReport(report, error);
    if (error) {
      ::TerminateProcess(sei.hProcess, 1);
      break;
    }
  }
  if (!error) {
    DWORD exit_code = 0;
    if (!::GetExitCodeProcess(sei.hProcess, &exit_code)) {
      error.set(Error::kExec, LastWin32Error("ShellExecuteExW"));
    } else if (exit_code != 0) {
      // Report as storage as this is most lickely reason for extract to fail.`
      error.set(Error::kStorage, base::UTF16ToUTF8(kExpandExe) +
                                     " terminated with non-zero exit code - " +
                                     std::to_string(exit_code));
    }
  }
  CloseHandle(sei.hProcess);
}

const wchar_t kVivaldi[] = L" --vivaldi";
const wchar_t kVivaldiInstallDir[] = L" --vivaldi-install-dir=";
const wchar_t kVivaldiStandalone[] = L" --vivaldi-standalone";
const wchar_t kVivaldiUpdate[] = L" --vivaldi-update";
const wchar_t kVivaldiForceLaunch[] = L" --vivaldi-force-launch";
const wchar_t kPreviousVersion[] = L" --previous-version=";
const wchar_t kInstallArchive[] = L" --install-archive=";
const wchar_t kUpdateSetupExe[] = L" --update-setup-exe=";
const wchar_t kNewSetupExe[] = L" --new-setup-exe=";
const wchar_t kStpViv[] = L"stp.viv";

// Append to command_line an argument escaped according to Windows rules.
void AddEscapedArg(const std::wstring& value, std::wstring& command_line) {
  // TODO(igor@vivaldi.com): This assumes that value does not contain " or other
  // special characters. Figure out if Chromium has API to deal with all corner
  // cases.
  command_line += L'"';
  command_line += value;
  command_line += L'"';
}

// TODO(igor@vivaldi.com): This is based on base::PathExists() from
// base/files/file_util.h. Figure out if that can be used directly on WinSparkle
// thread that does not use Chromium infrastructure for threads.
bool PathExists(const base::FilePath& path) {
  return (GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

std::wstring GetInstallCommandLine(const Appcast& appcast) {
  std::wstring command_line = base::UTF8ToUTF16(appcast.InstallerArguments);

  const base::FilePath& exe_dir = GetConfig().exe_dir;
  const std::wstring& exe_dir_str = exe_dir.value();
  std::wstring::size_type pos = exe_dir_str.rfind(L"\\Application");
  if (pos != std::wstring::npos) {
    std::wstring install_path = exe_dir_str.substr(0, pos);
    command_line += kVivaldiInstallDir;
    AddEscapedArg(install_path, command_line);
  }

  base::FilePath stp_viv_path = exe_dir.Append(kStpViv);
  if (PathExists(stp_viv_path)) {
    command_line += kVivaldiStandalone;
  }

  command_line += kVivaldiUpdate;
  return command_line;
}

std::wstring GetDeltaCommandLine(const Appcast& appcast,
                                 base::FilePath downloaded_file,
                                 Error& error) {
  if (error)
    return std::wstring();

  std::wstring command_line = GetInstallCommandLine(appcast);

  // Strip .cab extenssion to get the first archive member.
  base::FilePath vivaldi_update_file = downloaded_file.RemoveFinalExtension();

  // TODO(igor@vivaldi.com): We do not verify that the vivaldi_update_file
  // exists. We should do it and switch to the full update if it does not.
  command_line += kVivaldi;
  command_line += kVivaldiForceLaunch;
  command_line += kPreviousVersion;
  command_line += GetConfig().app_version;
  command_line += kInstallArchive;
  AddEscapedArg(vivaldi_update_file.value(), command_line);

  // Check if we get a patch for setup.exe. Its name is setup.version.delta.7z,
  // where the part after the setup is the same as for the main archive.
  //
  // TODO(igor@vivaldi.com): If the archive name format is unexpected, switch to
  // full update rather than just ignoring the setup patch.
  base::FilePath setup_update_file;
  std::wstring vivaldi_update_name = vivaldi_update_file.BaseName().value();
  std::wstring::size_type dot = vivaldi_update_name.find('.');
  if (dot != std::wstring::npos) {
    std::wstring setup_update_name = L"setup";
    setup_update_name.append(vivaldi_update_name, dot);
    setup_update_file = vivaldi_update_file.DirName().Append(setup_update_name);
  }
  if (!setup_update_file.empty() && PathExists(setup_update_file)) {
    // delta
    command_line += kUpdateSetupExe;
    AddEscapedArg(setup_update_file.value(), command_line);

    // destination file
    base::FilePath new_setup_exe_dest =
        GetConfig().GetSetupExe(appcast.Version);
    command_line += kNewSetupExe;
    AddEscapedArg(new_setup_exe_dest.value(), command_line);
  }
  return command_line;
}

std::unique_ptr<InstallerLaunchData> DownloadUpdateImpl(
    const Appcast& appcast,
    const base::FilePath& tmpdir,
    DownloadUpdateDelegate& delegate,
    Error& error) {
  DownloadReport report;

  // Try to download a delta if any, but fallback to the main download on
  // errors.
  GURL delta_url = FindDeltaURL(appcast);
  if (delta_url.is_valid()) {
    // Check if the installer failed to run delta the last time and skip the
    // download if so.
    std::string delta_version_failed =
        Settings::ReadConfigValue(ConfigKey::kDeltaPatchFailed);
    if (delta_version_failed == "1") {
      delta_url = GURL();
    }
  }

  std::wstring delta_setup_exe;
  if (delta_url.is_valid()) {
    delta_setup_exe = GetConfig().GetSetupExe().value();
    if (!PathExists(base::FilePath(delta_setup_exe))) {
      delta_url = GURL();
      LogError("Setup executable to run a delta update does not exist - " +
               base::UTF16ToUTF8(delta_setup_exe));
    }
  }

  if (delta_url.is_valid()) {
    report.delta = true;
    Error delta_error;
    base::FilePath delta_path =
        DownloadUrl(delta_url, tmpdir, report, delegate, delta_error);
    VerifyEmbeddedSignature(delta_path.value(), report, delegate, delta_error);
    ExpandDeltaArchive(delta_path.value(), report, delegate, delta_error);
    std::wstring command_line =
        GetDeltaCommandLine(appcast, std::move(delta_path), delta_error);
    if (!delta_error) {
      auto launch_data = std::make_unique<InstallerLaunchData>();
      launch_data->delta = true;
      launch_data->version = appcast.Version;
      launch_data->exe_file = std::move(delta_setup_exe);
      launch_data->download_dir = tmpdir.value();
      launch_data->working_directory = launch_data->download_dir;
      launch_data->command_line = std::move(command_line);
      return launch_data;
    }
    if (delta_error.kind() == Error::kCancelled) {
      // Propagate the cancellation.
      error = std::move(delta_error);
      return nullptr;
    }
    LogError(delta_error);
  }

  report.delta = false;
  base::FilePath full_update_path =
      DownloadUrl(appcast.DownloadURL, tmpdir, report, delegate, error);
  VerifyEmbeddedSignature(full_update_path.value(), report, delegate, error);
  if (error)
    return nullptr;

  auto launch_data = std::make_unique<InstallerLaunchData>();
  launch_data->version = appcast.Version;
  launch_data->exe_file = full_update_path.value();
  launch_data->download_dir = tmpdir.value();
  launch_data->command_line = GetInstallCommandLine(appcast);
  return launch_data;
}

}  // namespace

std::unique_ptr<InstallerLaunchData> DownloadUpdate(
    const Appcast& appcast,
    DownloadUpdateDelegate& delegate,
    Error& error) {
  if (error)
    return nullptr;
  base::FilePath tmpdir = CreateUniqueTempDirectory(error);
  if (error)
    return nullptr;
  Settings::WriteConfigValue(ConfigKey::kUpdateTempDir, tmpdir.value());

  std::unique_ptr<InstallerLaunchData> launch_data =
      DownloadUpdateImpl(appcast, tmpdir, delegate, error);
  if (!launch_data) {
    CleanDownloadLeftovers(tmpdir.value());
  }
  return launch_data;
}

void RunInstaller(std::unique_ptr<InstallerLaunchData> launch_data,
                  Error& error) {
  if (error)
    return;

  if (launch_data->delta) {
    // Mark the delta as failed. The installer will clear the status on a
    // successfull delta installation.
    Settings::WriteConfigValue(ConfigKey::kDeltaPatchFailed, "1");
  }

  SHELLEXECUTEINFOW sei = {0};
  sei.cbSize = sizeof(SHELLEXECUTEINFOW);
  sei.nShow = SW_SHOWDEFAULT;
  sei.fMask = SEE_MASK_FLAG_NO_UI;

  sei.lpFile = launch_data->exe_file.c_str();
  if (!launch_data->working_directory.empty()) {
    sei.lpDirectory = launch_data->working_directory.c_str();
  }
  if (!launch_data->command_line.empty()) {
    sei.lpParameters = launch_data->command_line.c_str();
  }

  if (!::ShellExecuteExW(&sei)) {
    error.set(Error::kExec,
              LastWin32Error("ShellExecuteExW", base::UTF16ToUTF8(sei.lpFile)));
    return;
  }

  // We successfully started the installer, we should not remove the downloaded
  // files in the destructor.
  launch_data->download_dir = std::wstring();

  if (!launch_data->delta) {
    // Assume that we can run a delta again as we launched a full installer.
    //
    // TODO(igor@vivaldi.com): Call VivaldiUpdateDeltaPatchStatus(true) in the
    // installer after a successful installation of the full installer so we can
    // avoid doing it here as it is less reliable.
    Settings::WriteConfigValue(ConfigKey::kDeltaPatchFailed, "0");
  }
}

void CleanDownloadLeftovers(std::wstring tmpdir) {
  if (tmpdir.empty()) {
    tmpdir = Settings::ReadConfigValueW(ConfigKey::kUpdateTempDir);
    if (tmpdir.empty())
      return;
  }

  tmpdir.append(1, '\0');  // double NULL-terminate for SHFileOperation

  SHFILEOPSTRUCTW fos = {0};
  fos.wFunc = FO_DELETE;
  fos.pFrom = tmpdir.c_str();
  fos.fFlags = FOF_NO_UI |  // Vista+-only
               FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI;

  if (::SHFileOperationW(&fos) != 0) {
    // try another time, this is just a "soft" error
    LogError(LastWin32Error("SHFileOperationW",
                            "Failed to delete a temporary directory"));
  } else {
    Settings::WriteConfigValue(ConfigKey::kUpdateTempDir, std::wstring());
  }
}

InstallerLaunchData::InstallerLaunchData() = default;

InstallerLaunchData::~InstallerLaunchData() {
  if (!download_dir.empty()) {
    CleanDownloadLeftovers(std::move(download_dir));
  }
}

}  // namespace winsparkle
