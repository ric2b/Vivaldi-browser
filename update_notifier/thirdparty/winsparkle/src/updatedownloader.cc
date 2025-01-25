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

#include "base/vivaldi_switches.h"
#include "installer/util/vivaldi_install_constants.h"
#include "installer/win/vivaldi_install_l10n.h"
#include "update_notifier/thirdparty/winsparkle/src/appcast.h"
#include "update_notifier/thirdparty/winsparkle/src/config.h"
#include "update_notifier/thirdparty/winsparkle/src/download.h"
#include "update_notifier/update_notifier_switches.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/util_constants.h"
#include "crypto/sha2.h"

// Windows must be included before Softpub. So prevent clang-format from sorting
// it with other headers by using blank lines around it.
#include <Windows.h>

#include <Softpub.h>
#include <WinTrust.h>
#include <io.h>
#include <lmcons.h>
#include <wincrypt.h>

// Link with Windows libraries
#pragma comment(lib, "wintrust")
#pragma comment(lib, "crypt32.lib")

namespace vivaldi_update_notifier {

/*--------------------------------------------------------------------------*
                                  helpers
 *--------------------------------------------------------------------------*/

namespace {

const wchar_t kExpandExe[] = L"expand.exe";

// TODO(igor@vivaldi.com): Figure out how to use installer::kChromeArchive here.
// That constant is defined in //chrome/installer/setup:lib static library  but
// adding that as a dependency may increase the binary size significantly.
const wchar_t kChromeArchive[] = L"vivaldi.7z";

const wchar_t kSetupExe[] = L"setup.exe";

#ifndef OFFICIAL_BUILD
// To avoid asking for a HTTP password for Soprano builds we download them from
// a special location remapped from the original and which requires an extra
// HTTP header instead of the password. Without the header HTTP reports a
// not-found error.
const std::pair<std::string_view, std::string_view> kSopranoDownloadRemap[] =
    {
        {"https://vivaldi.com/sopranos/",
         "https://vivaldi.com/sopranos-update/"},
        {"https://sopranos.vivaldi.com/",
         "https://sopranos-update.vivaldi.com/"},
};

const char kSopranoHeaderName[] = "X-Vivaldi-Update";
const char kSopranoHeaderValue[] = "soprano";
#endif

// We cancel the delta and switch to the full download if delta extraction and
// setup reconstruction runs over this time limit.
constexpr base::TimeDelta kDeltaExtractionLimit = base::Minutes(3);

// Max limit to unpack the full installer. The value is arbitrary to have a
// sanity limit.
constexpr base::TimeDelta kUnpackTimeLimit = base::Hours(24);

// Get a temporary directory with a stable, installation-specific name to hold
// the directory with an unpredictable name. The stable name allows to remove
// all temporary directories when cleaning leftovers from the previous run.
base::FilePath GetTempDirHolder(Error& error) {
  if (error)
    return base::FilePath();

  base::FilePath os_tmp_dir;
  if (!base::GetTempDir(&os_tmp_dir)) {
    error.set(Error::kStorage, "Failed to get a temporary directory");
    return base::FilePath();
  }

  std::wstring temp_subdir;
  if (g_mode == UpdateMode::kNetworkInstall) {
    temp_subdir = L"VivaldiInstall";
  } else {
    // Generate a stable name using a hash of the installation directory.
    base::FilePath temp_dir_holder;
    std::wstring install_hash = GetInstallHash(/*for_task_scheduler=*/false);
    if (install_hash.empty()) {
      error.set(Error::kStorage, "Failed to derive the installation hash");
      return base::FilePath();
    }
    temp_subdir = L"VivaldiUpdate-" + install_hash;
  }
  return os_tmp_dir.Append(temp_subdir);
}

base::FilePath CreateUniqueDownloadDir(Error& error) {
  base::FilePath temp_dir_holder = GetTempDirHolder(error);
  if (!CreateDirectory(temp_dir_holder)) {
    error.set(Error::kStorage, "Failed to create a directory - " +
                                   base::WideToUTF8(temp_dir_holder.value()));
    return base::FilePath();
  }
  base::FilePath tmp_dir;
  if (!CreateTemporaryDirInDir(temp_dir_holder, std::wstring(), &tmp_dir)) {
    error.set(Error::kStorage, "Failed to create a temporary directory in " +
                                   base::WideToUTF8(temp_dir_holder.value()));
    return base::FilePath();
  }
  return tmp_dir;
}

base::FilePath DownloadUrl(GURL url,
                           const base::FilePath& tmpdir,
                           DownloadReport& report,
                           DownloadUpdateDelegate& delegate,
                           Error& error) {
  FileDownloader downloader;
#ifndef OFFICIAL_BUILD
  for (auto const& i : kSopranoDownloadRemap) {
    if (base::StartsWith(url.spec(), i.first)) {
      size_t nprefix = i.first.length();
      std::string remapped_url(i.second.data(), i.second.length());
      remapped_url.append(url.spec().data() + nprefix,
                          url.spec().length() - nprefix);
      url = GURL(remapped_url);
      downloader.SetHeader(kSopranoHeaderName, kSopranoHeaderValue);
      break;
    }
  }
#endif
  downloader.Connect(url, error);
  if (error)
    return base::FilePath();
  report.kind = DownloadReport::kConnected;
  report.content_length = downloader.GetContentLength();
  report.downloaded_length = 0;
  delegate.SendReport(report, error);
  if (error)
    return base::FilePath();

  base::FilePath download_path =
      tmpdir.Append(base::UTF8ToWide(downloader.GetFileName()));
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
    for (auto& delta : appcast.Deltas) {
      if (!delta.DownloadURL.is_valid())
        continue;
      if (g_app_version != delta.DeltaFrom)
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

// Check that the file is signed by some party and that signature is trusted by
// Windows. The party can be an arbitrary entity that can sign.
LONG CheckTrustedSignature(const std::wstring& file_path) {
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
  WinTrustData.dwUIChoice = WTD_UI_NONE;
  WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;
  WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
  WinTrustData.hWVTStateData = NULL;
  WinTrustData.pwszURLReference = NULL;
  WinTrustData.dwUIContext = 0;
  WinTrustData.pFile = &FileData;

  LONG lStatus = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

  // Any hWVTStateData must be released by a call with close.
  WinTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
  WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);

  return lStatus;
}

// Read the name of the subject that signs the build.
//
// This is based on
// https://stackoverflow.com/questions/1072540/winverifytrust-to-check-for-a-specific-signature/46770722#46770722
// and
// https://docs.microsoft.com/en-US/troubleshoot/windows/win32/get-information-authenticode-signed-executables
//
std::wstring ReadSigningSubjectName(const std::wstring& file_path,
                                    Error& error) {
  if (error)
    return std::wstring();

  HCERTSTORE hStore = nullptr;
  HCRYPTMSG hMsg = nullptr;
  const CERT_CONTEXT* cert_context = nullptr;
  CMSG_SIGNER_INFO* signer_info = nullptr;
  wchar_t* name = nullptr;

  std::wstring subject;
  do {
    DWORD encoding = 0;
    DWORD content_type = 0;
    DWORD format_type = 0;
    BOOL status =
        CryptQueryObject(CERT_QUERY_OBJECT_FILE, file_path.c_str(),
                         CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                         CERT_QUERY_FORMAT_FLAG_BINARY, 0, &encoding,
                         &content_type, &format_type, &hStore, &hMsg, nullptr);
    if (!status) {
      error.set(Error::kVerify, LastWin32Error("CryptQueryObject"));
      break;
    }

    // Get the size of and then signing info itself.
    DWORD info_size = 0;
    status =
        CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, nullptr, &info_size);
    if (!status) {
      error.set(Error::kVerify, LastWin32Error("CryptMsgGetParam"));
      break;
    }
    signer_info = static_cast<CMSG_SIGNER_INFO*>(LocalAlloc(LPTR, info_size));
    CHECK(signer_info);
    status = CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, signer_info,
                              &info_size);
    if (!status) {
      error.set(Error::kVerify, LastWin32Error("CryptMsgGetParam"));
      break;
    }

    CERT_INFO CertInfo = {};
    CertInfo.Issuer = signer_info->Issuer;
    CertInfo.SerialNumber = signer_info->SerialNumber;
    constexpr DWORD kEncoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
    cert_context = CertFindCertificateInStore(
        hStore, kEncoding, 0, CERT_FIND_SUBJECT_CERT, &CertInfo, nullptr);
    if (!cert_context) {
      error.set(Error::kVerify, LastWin32Error("CertFindCertificateInStore"));
      break;
    }

    // Get the size of and then the subject name itself.
    DWORD name_size = CertGetNameString(
        cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, nullptr, nullptr, 0);
    if (name_size <= 1) {
      error.set(Error::kVerify,
                "Failed CertGetNameString(CERT_NAME_SIMPLE_DISPLAY_TYPE)");
      break;
    }
    name = static_cast<wchar_t*>(LocalAlloc(LPTR, name_size * sizeof(wchar_t)));
    CHECK(name);
    name_size = CertGetNameString(cert_context, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                  0, nullptr, name, name_size);
    if (name_size <= 1) {
      error.set(Error::kVerify,
                "Failed CertGetNameString(CERT_NAME_SIMPLE_DISPLAY_TYPE)");
      break;
    }
    subject = name;

  } while (false);

  if (name)
    LocalFree(name);
  if (signer_info)
    LocalFree(signer_info);
  if (cert_context)
    CertFreeCertificateContext(cert_context);
  if (hStore)
    CertCloseStore(hStore, 0);
  if (hMsg)
    CryptMsgClose(hMsg);

  return subject;
}

void VerifyEmbeddedSignature(const base::FilePath& file_path,
                             DownloadReport& report,
                             DownloadUpdateDelegate& delegate,
                             Error& error) {
  if (error)
    return;
  report.kind = DownloadReport::kVerificationStart;
  delegate.SendReport(report, error);

  std::wstring subject = ReadSigningSubjectName(file_path.value(), error);
  if (error)
    return;

    // For sopranos we need to support self-signed installer in non-official
    // builds.
#if !defined(OFFICIAL_BUILD)
  bool self_signed = false;
#endif
  if (subject != vivaldi_update_notifier::kVivaldiSubjectName) {
#if !defined(OFFICIAL_BUILD)
    if (subject == vivaldi_update_notifier::kVivaldiTestSubjectName) {
      self_signed = true;
    } else
#endif
    {
      error.set(
          Error::kVerify,
          std::string("Certificate contains an unexpected subject name - ") +
              base::WideToUTF8(subject));
      return;
    }
  }

  LONG status = CheckTrustedSignature(file_path.value());
  if (status == ERROR_SUCCESS)
    return;
#if !defined(OFFICIAL_BUILD)
  if (self_signed) {
    constexpr LONG kUntrustedRoot = 0x800B0109;
    if (status == kUntrustedRoot) {
      LOG(INFO) << "The expected untrusted root certificate when "
                   "checking the signature of a self-signed build.";

    } else {
      LOG(WARNING) << base::StringPrintf(
          "An unexpected signature verification error for a self-signed build, "
          "error=0x%lx",
          status);
    }
    return;
  }
#endif
  error.set(Error::kVerify,
            base::StringPrintf(
                "Failed to verify a downloaded file signature, status=0x%lx",
                status));
}

// Run a helper process until it exits
void RunHelperProcess(const base::CommandLine& cmdline,
                      const base::LaunchOptions& options,
                      base::Time timeout_time,
                      DownloadReport& report,
                      DownloadUpdateDelegate& delegate,
                      Error& error) {
  if (error)
    return;
  DLOG(INFO) << "Launching in " << options.current_directory << "\n"
             << cmdline.GetCommandLineString();
  base::Process process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    error.set(
        Error::kExec,
        "Failed to run " + base::WideToUTF8(cmdline.GetCommandLineString()));
    return;
  }
  int exit_code = 0;
  for (;;) {
    // Wait for 2 seconds so we can check if the user canceled the download.
    bool done = process.WaitForExitWithTimeout(base::Seconds(2), &exit_code);
    if (done)
      break;
    delegate.SendReport(report, error);
    if (error) {
      DLOG(INFO) << "The process was cancelled";
      process.Terminate(1, false);
      return;
    }
    if (timeout_time < base::Time::Now()) {
      error.set(Error::kExec,
                "Timed out wating for the process to finish - " +
                    base::WideToUTF8(cmdline.GetCommandLineString()));
      process.Terminate(1, false);
      return;
    }
  }
  if (exit_code != 0) {
    error.set(Error::kStorage,
              base::WideToUTF8(cmdline.GetCommandLineString()) +
                  " terminated with non-zero exit code - " +
                  std::to_string(exit_code));
  }
}

bool CheckCanApplyDelta(const base::FilePath& setup_exe) {
  if (!base::PathExists(setup_exe)) {
    LOG(WARNING) << "Setup executable to run a delta update does not exist - "
                 << setup_exe;
    return false;
  }
  base::FilePath archive = setup_exe.DirName().Append(kChromeArchive);
  if (!base::PathExists(archive)) {
    LOG(WARNING) << "Archive to apply the delta to does not exist - "
                 << archive;
    return false;
  }

  std::string delta_version_failed =
      ReadRegistryIem(RegistryItem::kDeltaPatchFailed);
  if (delta_version_failed == "1") {
    LOG(WARNING) << "Refusing delta as the installer failed to run the delta "
                    "update the last time";
    return false;
  }
  return true;
}

// Extract the cabinet file (CAB) which contains 7z file for full Vivaldi
// installation. If the archive also contains a separated delta for setup exe,
// apply that delta to the current setup.exe to get a patched file in the
// temporary folder and set setup_exe to its path.
void ExpandDeltaArchive(const base::FilePath& file_path,
                        base::FilePath& setup_exe,
                        base::FilePath& vivaldi_delta,
                        DownloadReport& report,
                        DownloadUpdateDelegate& delegate,
                        Error& error) {
  if (error)
    return;
  report.kind = DownloadReport::kUnpacking;
  delegate.SendReport(report, error);
  if (error)
    return;

  base::Time timeout_time = base::Time::Now() + kDeltaExtractionLimit;

  base::CommandLine cmdline((base::FilePath(kExpandExe)));
  cmdline.AppendArgNative(L"-R");
  cmdline.AppendArgNative(L"-f:*");
  cmdline.AppendArgPath(file_path.BaseName());
  cmdline.AppendArgNative(L".");  // expand to current dir

  base::LaunchOptions launch_options;
  launch_options.current_directory = file_path.DirName();
  launch_options.start_hidden = true;

  RunHelperProcess(cmdline, launch_options, timeout_time, report, delegate,
                   error);
  if (error)
    return;

  // Strip .cab extenssion to get the first archive member.
  vivaldi_delta = file_path.RemoveFinalExtension();
  if (!base::PathExists(vivaldi_delta)) {
    error.set(Error::kFormat,
              "The delta archive without " +
                  base::WideToUTF8(vivaldi_delta.BaseName().value()) +
                  " member");
    return;
  }

  // Check if we get a patch for setup.exe. Its name is setup.version.delta.7z,
  // where the part after the setup is the same as for the main archive.
  base::FilePath setup_delta;
  std::wstring vivaldi_delta_name = vivaldi_delta.BaseName().value();
  std::wstring::size_type dot = vivaldi_delta_name.find('.');
  if (dot == std::wstring::npos) {
    error.set(Error::kFormat,
              "Unexpected format of delta archive - " +
                  base::WideToUTF8(vivaldi_delta.BaseName().value()));
    return;
  }

  std::wstring setup_delta_name = L"setup";
  setup_delta_name.append(vivaldi_delta_name, dot);
  setup_delta = vivaldi_delta.DirName().Append(setup_delta_name);
  if (!base::PathExists(setup_delta)) {
    // Use setup.exe from the current installation to do the update.
    return;
  }

  // Launch setup.exe to create a new setup.exe in the temporary folder.

  base::FilePath new_setup_exe = vivaldi_delta.DirName().Append(kSetupExe);
  if (base::PathExists(new_setup_exe)) {
    error.set(Error::kFormat,
              "The delta archive contains unexpected member - " +
                  base::WideToUTF8(new_setup_exe.BaseName().value()));
    return;
  }

  cmdline = base::CommandLine(setup_exe);
  cmdline.AppendSwitchPath(installer::switches::kUpdateSetupExe, setup_delta);
  cmdline.AppendSwitchPath(installer::switches::kNewSetupExe, new_setup_exe);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          installer::switches::kEnableLogging)) {
    cmdline.AppendSwitch(installer::switches::kVerboseLogging);
  }

  launch_options = base::LaunchOptions();
  launch_options.start_hidden = true;

  RunHelperProcess(cmdline, launch_options, timeout_time, report, delegate,
                   error);
  if (error)
    return;

  setup_exe = std::move(new_setup_exe);
}

void AddInstallArguments(base::CommandLine& cmdline) {
  if (!g_install_dir.empty()) {
    cmdline.AppendSwitchPath(vivaldi::constants::kVivaldiInstallDir,
                             g_install_dir);
  }

  const std::wstring& language = vivaldi::GetInstallerLanguage();
  if (!language.empty()) {
    cmdline.AppendSwitchNative(vivaldi::constants::kVivaldiLanguage, language);
  }
  if (g_mode != UpdateMode::kNetworkInstall) {
    cmdline.AppendSwitch(vivaldi::constants::kVivaldiUpdate);
    if (g_mode == UpdateMode::kSilentUpdate) {
      cmdline.AppendSwitch(switches::kVivaldiSilentUpdate);
    }
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          installer::switches::kEnableLogging)) {
    cmdline.AppendSwitch(installer::switches::kVerboseLogging);
  }
}

// Return null without setting the error to proceed to the full download.
std::unique_ptr<InstallerLaunchData> TryDeltaDownload(
    const Appcast& appcast,
    const base::FilePath& tmpdir,
    DownloadUpdateDelegate& delegate,
    Error& error) {
  if (error)
    return nullptr;

  GURL delta_url = FindDeltaURL(appcast);
  if (!delta_url.is_valid())
    return nullptr;

  base::FilePath setup_exe = GetSetupExePath();
  if (!CheckCanApplyDelta(setup_exe))
    return nullptr;

  LOG(INFO) << "Downloading a delta update from " << delta_url.spec();

  Error delta_error;
  DownloadReport report;
  report.delta = true;

  base::FilePath delta_path =
      DownloadUrl(delta_url, tmpdir, report, delegate, delta_error);
  VerifyEmbeddedSignature(delta_path, report, delegate, delta_error);

  base::FilePath vivaldi_delta;
  ExpandDeltaArchive(delta_path, setup_exe, vivaldi_delta, report, delegate,
                     delta_error);
  if (delta_error) {
    if (delta_error.kind() == Error::kCancelled) {
      // Propagate the cancellation.
      error = std::move(delta_error);
    } else {
      LOG(ERROR) << delta_error.log_message();
    }
    return nullptr;
  }

  LOG(INFO) << "Delta was downloaded and successfully extracted.";
  base::CommandLine cmdline(setup_exe);
  AddInstallArguments(cmdline);
  cmdline.AppendSwitchASCII(installer::switches::kPreviousVersion,
                            g_app_version.GetString());
  cmdline.AppendSwitchPath(installer::switches::kInstallArchive, vivaldi_delta);
  return std::make_unique<InstallerLaunchData>(true, appcast.Version,
                                               std::move(cmdline));
}

std::unique_ptr<InstallerLaunchData> DownloadFullInstaller(
    const Appcast& appcast,
    const base::FilePath& tmpdir,
    DownloadUpdateDelegate& delegate,
    Error& error) {
  if (error)
    return nullptr;

  LOG(INFO) << "Downloading a full installer from "
            << appcast.DownloadURL.spec();
  DownloadReport report;
  base::FilePath full_update_path =
      DownloadUrl(appcast.DownloadURL, tmpdir, report, delegate, error);
  VerifyEmbeddedSignature(full_update_path, report, delegate, error);
  if (error)
    return nullptr;

  // Extract setup.exe and the main archive from the installer.
  report.kind = DownloadReport::kUnpacking;
  delegate.SendReport(report, error);

  base::CommandLine unpack_cmdline(full_update_path);
  unpack_cmdline.AppendSwitch(vivaldi::constants::kVivaldiUnpack);
  base::LaunchOptions launch_options;
  launch_options.current_directory = tmpdir;
  launch_options.start_hidden = true;

  RunHelperProcess(unpack_cmdline, launch_options,
                   base::Time::Now() + kUnpackTimeLimit, report, delegate,
                   error);
  if (error)
    return nullptr;

  base::FilePath setup_exe = tmpdir.Append(kSetupExe);
  if (!base::PathExists(setup_exe)) {
    error.set(Error::kFormat, "The installer failed to unpack the main exe");
    return nullptr;
  }

  // We no longer need the installer file.
  base::DeleteFile(full_update_path);
  full_update_path = std::move(setup_exe);

  base::CommandLine cmdline(full_update_path);
  AddInstallArguments(cmdline);
  return std::make_unique<InstallerLaunchData>(false, appcast.Version,
                                               std::move(cmdline));
}

}  // namespace

std::unique_ptr<InstallerLaunchData> DownloadUpdate(
    const Appcast& appcast,
    DownloadUpdateDelegate& delegate,
    Error& error) {
  if (error)
    return nullptr;
  base::FilePath tmpdir = CreateUniqueDownloadDir(error);

  std::unique_ptr<InstallerLaunchData> launch_data;
  if (g_mode != UpdateMode::kNetworkInstall) {
    launch_data = TryDeltaDownload(appcast, tmpdir, delegate, error);
  }
  if (!launch_data) {
    launch_data = DownloadFullInstaller(appcast, tmpdir, delegate, error);
  }
  if (!launch_data) {
    base::DeletePathRecursively(tmpdir);
  } else {
    launch_data->download_dir = std::move(tmpdir);
  }
  return launch_data;
}

base::Process RunInstaller(std::unique_ptr<InstallerLaunchData> launch_data,
                           Error& error) {
  if (error)
    return base::Process();

  if (launch_data->delta) {
    // Pre-mark the delta as failed. The installer will clear the status on a
    // successfull delta installation, or if that failed, after successfully
    // running a full installer.
    WriteRegistryItem(RegistryItem::kDeltaPatchFailed, "1");
  }

  LOG(INFO) << "Launching installer:\n"
            << launch_data->cmdline.GetCommandLineString();
  base::Process process =
      base::LaunchProcess(launch_data->cmdline, base::LaunchOptions());
  if (!process.IsValid()) {
    error.set(
        Error::kExec,
        "Failed to run " +
            base::WideToUTF8(launch_data->cmdline.GetCommandLineString()));
    return base::Process();
  }

  // We successfully started the installer, we should not remove the downloaded
  // files in the destructor.
  launch_data->download_dir = base::FilePath();
  return process;
}

void CleanDownloadLeftovers() {
  Error error;
  base::FilePath temp_dir_holder = GetTempDirHolder(error);
  DLOG(INFO) << "Removing download leftovers from " << temp_dir_holder;
  if (error) {
    LOG(ERROR) << error.log_message();
    return;
  }
  base::DeletePathRecursively(temp_dir_holder);
}

std::wstring GetInstallHash(bool for_task_scheduler) {
  std::wstring install_string;
  if (for_task_scheduler &&
      g_install_type == vivaldi::InstallType::kForAllUsers &&
      !DoesRunAsSystemService()) {
    // Windows Task Scheduler entries are shared among all users. Thus for
    // system installs with checks running from an ordinary user account we
    // derive the hash both from the user name and the installation path to
    // ensure hash uniqueness.
    wchar_t user_name[UNLEN + 1];
    DWORD n = std::size(user_name);
    if (!::GetUserName(user_name, &n)) {
      LOG(ERROR) << LastWin32Error("GetUserName");
      return std::wstring();
    }
    install_string += user_name;

    // Separate with a character that cannot present in user and file paths.
    install_string += L"|";
  }
  install_string += GetExeDir().value();
  std::string install_sha256 =
      crypto::SHA256HashString(base::WideToUTF8(install_string));
  DCHECK_EQ(install_sha256.length(), 32U);

  // We do not need cryptographically strong hash here, we just need to ensure
  // that the probability of a collision is small enough for non-deliberate
  // cases as things should not crash even when the hash coinsides. With 8 bytes
  // that probability is about 5.4E-20.
  //
  // TODO(igor@vivaldi.com): Remove base64 form. As it used for the persistent
  // path that is removed after the update downloader is restarted after an
  // update, it is no just a matter of removing the code.
  std::string hash;
  bool base64 = !for_task_scheduler;
  if (base64) {
    hash = base::Base64Encode(
        base::as_bytes(base::make_span(install_sha256.data(), 16UL)));
    // Use URL-safe encoding to avoid +/=
    DCHECK_EQ(hash.length(), 24U);
    hash.resize(22);  // strip '='
    base::ReplaceChars(hash, "+", "-", &hash);
    base::ReplaceChars(hash, "/", "_", &hash);
  } else {
    hash = base::HexEncode(
        base::as_bytes(base::make_span(install_sha256.data(), 8UL)));
  }
  return base::ASCIIToWide(base::ToLowerASCII(hash));
}

InstallerLaunchData::InstallerLaunchData(bool delta,
                                         const base::Version& version,
                                         base::CommandLine cmdline)
    : delta(delta), version(version), cmdline(std::move(cmdline)) {}

InstallerLaunchData::~InstallerLaunchData() {
  if (!download_dir.empty()) {
    base::DeletePathRecursively(download_dir);
  }
}

}  // namespace vivaldi_update_notifier
