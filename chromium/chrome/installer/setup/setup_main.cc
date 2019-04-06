// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_main.h"

#include <windows.h>

#include <msi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_storage.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/process_startup_helper.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/archive_patch_helper.h"
#include "chrome/installer/setup/buildflags.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_crash_reporting.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_install_details.h"
#include "chrome/installer/setup/setup_singleton.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/setup/user_experiment.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/delete_old_versions.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/google_update_util.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crash/content/app/crash_switches.h"
#include "components/crash/content/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"

#include <vector>

#include "chrome/installer/util/util_constants.h"

#include "app/vivaldi_version_info.h"
#include "installer/util/vivaldi_install_dialog.h"
#include "installer/util/vivaldi_install_util.h"
#include "installer/util/vivaldi_progress_dialog.h"

#pragma comment(lib, "ComCtl32.lib")

using installer::InstallerState;
using installer::InstallationState;
using installer::MasterPreferences;
using installer::Product;
using installer::ProductState;

namespace {

const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";
const wchar_t kDisplayVersion[] = L"DisplayVersion";
const wchar_t kMsiDisplayVersionOverwriteDelay[] = L"10";  // seconds as string
const wchar_t kMsiProductIdPrefix[] = L"EnterpriseProduct";

// Overwrite an existing DisplayVersion as written by the MSI installer
// with the real version number of Chrome.
LONG OverwriteDisplayVersion(const base::string16& path,
                             const base::string16& value,
                             REGSAM wowkey) {
  base::win::RegKey key;
  LONG result = 0;
  base::string16 existing;
  if ((result = key.Open(HKEY_LOCAL_MACHINE, path.c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE | wowkey))
      != ERROR_SUCCESS) {
    VLOG(1) << "Skipping DisplayVersion update because registry key " << path
            << " does not exist in "
            << (wowkey == KEY_WOW64_64KEY ? "64" : "32") << "bit hive";
    return result;
  }
  if ((result = key.ReadValue(kDisplayVersion, &existing)) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " not found under " << path;
    return result;
  }
  if ((result = key.WriteValue(kDisplayVersion, value.c_str()))
      != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " could not be written under " << path;
    return result;
  }
  VLOG(1) << "Set DisplayVersion at " << path << " to " << value
          << " from " << existing;
  return ERROR_SUCCESS;
}

LONG OverwriteDisplayVersions(const base::string16& product,
                              const base::string16& value) {
  // The version is held in two places.  Frist change it in the MSI Installer
  // registry entry.  It is held under a "squashed guid" key.
  base::string16 reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\"
      L"%ls\\Products\\%ls\\InstallProperties",
      kSystemPrincipalSid, InstallUtil::GuidToSquid(product).c_str());
  LONG result1 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);

  // The display version also exists under the Unininstall registry key with
  // the original guid.  Check both WOW64_64 and WOW64_32.
  reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{%ls}",
      product.c_str());
  // Consider the operation a success if either of these succeeds.
  LONG result2 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);
  LONG result3 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_32KEY);

  return result1 != ERROR_SUCCESS ? result1 :
      (result2 != ERROR_SUCCESS ? result3 : ERROR_SUCCESS);
}

void DelayedOverwriteDisplayVersions(const base::FilePath& setup_exe,
                                     const std::string& id,
                                     const base::Version& version) {
  // This process has to be able to exit so we launch ourselves with
  // instructions on what to do and then return.
  base::CommandLine command_line(setup_exe);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionProduct,
                                 id);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionValue,
                                 version.GetString());
  command_line.AppendSwitchNative(installer::switches::kDelay,
                                  kMsiDisplayVersionOverwriteDelay);

  base::LaunchOptions launch_options;
  launch_options.force_breakaway_from_job_ = true;
  base::Process writer = base::LaunchProcess(command_line, launch_options);
  if (!writer.IsValid()) {
    PLOG(ERROR) << "Failed to set DisplayVersion: "
                << "could not launch subprocess to make desired changes."
                << " <<" << command_line.GetCommandLineString() << ">>";
  }
}

// Returns NULL if no compressed archive is available for processing, otherwise
// returns a patch helper configured to uncompress and patch.
std::unique_ptr<installer::ArchivePatchHelper> CreateChromeArchiveHelper(
    const base::FilePath& setup_exe,
    const base::CommandLine& command_line,
    const installer::InstallerState& installer_state,
    const base::FilePath& working_directory,
    installer::UnPackConsumer consumer) {
  // A compressed archive is ordinarily given on the command line by the mini
  // installer. If one was not given, look for chrome.packed.7z next to the
  // running program.
  base::FilePath compressed_archive(
      command_line.GetSwitchValuePath(installer::switches::kInstallArchive));
  bool compressed_archive_specified = !compressed_archive.empty();
  if (!compressed_archive_specified) {
    compressed_archive = setup_exe.DirName().Append(
        installer::kChromeCompressedArchive);
  }

  // Fail if no compressed archive is found.
  if (!base::PathExists(compressed_archive)) {
    if (compressed_archive_specified) {
      LOG(ERROR) << installer::switches::kInstallArchive << "="
                 << compressed_archive.value() << " not found.";
    }
    return std::unique_ptr<installer::ArchivePatchHelper>();
  }

  // chrome.7z is either extracted directly from the compressed archive into the
  // working dir or is the target of patching in the working dir.
  base::FilePath target(working_directory.Append(installer::kChromeArchive));
  DCHECK(!base::PathExists(target));

  // Specify an empty path for the patch source since it isn't yet known that
  // one is needed. It will be supplied in UncompressAndPatchChromeArchive if it
  // is.
  return std::unique_ptr<installer::ArchivePatchHelper>(
      new installer::ArchivePatchHelper(working_directory, compressed_archive,
                                        base::FilePath(), target, consumer));
}

// Returns the MSI product ID from the ClientState key that is populated for MSI
// installs.  This property is encoded in a value name whose format is
// "EnterpriseProduct<GUID>" where <GUID> is the MSI product id.  <GUID> is in
// the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.  The id will be returned if
// found otherwise this method will return an empty string.
//
// This format is strange and its provenance is shrouded in mystery but it has
// the data we need, so use it.
base::string16 FindMsiProductId(const InstallerState& installer_state,
                                const Product& product) {
  HKEY reg_root = installer_state.root_key();
  BrowserDistribution* dist = product.distribution();
  DCHECK(dist);

  base::win::RegistryValueIterator value_iter(
      reg_root, dist->GetStateKey().c_str(), KEY_WOW64_32KEY);
  for (; value_iter.Valid(); ++value_iter) {
    base::string16 value_name(value_iter.Name());
    if (base::StartsWith(value_name, kMsiProductIdPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return value_name.substr(arraysize(kMsiProductIdPrefix) - 1);
    }
  }
  return base::string16();
}

void VivaldiUpdateDeltaPatchStatus(bool successful) {
  std::wstring key_name(vivaldi::constants::kVivaldiKey);
  key_name.append(L"\\");
  key_name.append(vivaldi::constants::kVivaldiAutoUpdate);
  base::win::RegKey key(HKEY_CURRENT_USER, key_name.c_str(), KEY_ALL_ACCESS);
  if (key.Valid())
    key.WriteValue(vivaldi::constants::kVivaldiDeltaPatchFailed,
                   (successful) ? L"0" : L"1");
}

// Workhorse for producing an uncompressed archive (chrome.7z) given a
// chrome.packed.7z containing either a patch file based on the version of
// chrome being updated or the full uncompressed archive. Returns true on
// success, in which case |archive_type| is populated based on what was found.
// Returns false on failure, in which case |install_status| contains the error
// code and the result is written to the registry (via WriteInstallerResult).
bool UncompressAndPatchChromeArchive(
    const installer::InstallationState& original_state,
    const installer::InstallerState& installer_state,
    installer::ArchivePatchHelper* archive_helper,
    installer::ArchiveType* archive_type,
    installer::InstallStatus* install_status,
    const base::Version& previous_version) {
  installer_state.SetStage(installer::UNCOMPRESSING);
  base::TimeTicks start_time = base::TimeTicks::Now();

  if (!archive_helper->Uncompress(NULL)) {
    *install_status = installer::UNCOMPRESSION_FAILED;
    installer_state.WriteInstallerResult(*install_status,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         NULL);
    VivaldiUpdateDeltaPatchStatus(false);
    return false;
  }
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  bool has_full_archive = base::PathExists(archive_helper->target());
  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.HasArchivePatch.background",
                          !has_full_archive);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Setup.Install.HasArchivePatch", !has_full_archive);
  }

  // Short-circuit if uncompression produced the uncompressed archive rather
  // than a patch file.
  if (has_full_archive) {
    *archive_type = installer::FULL_ARCHIVE_TYPE;
    // Uncompression alone hopefully takes less than 3 minutes even on slow
    // machines.
    if (installer_state.is_background_mode()) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Setup.Install.UncompressFullArchiveTime.background", elapsed_time);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Setup.Install.UncompressFullArchiveTime", elapsed_time);
    }
    return true;
  }

  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UncompressArchivePatchTime.background", elapsed_time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UncompressArchivePatchTime", elapsed_time);
  }

  // Find the installed version's archive to serve as the source for patching.
  base::FilePath patch_source(installer::FindArchiveToPatch(original_state,
                                                            installer_state,
                                                            previous_version));
  if (patch_source.empty()) {
    LOG(ERROR) << "Failed to find archive to patch.";
    *install_status = installer::DIFF_PATCH_SOURCE_MISSING;
    installer_state.WriteInstallerResult(*install_status,
                                         IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
                                         NULL);
    VivaldiUpdateDeltaPatchStatus(false);
    return false;
  }
  archive_helper->set_patch_source(patch_source);

  // Patch application sometimes takes a very long time, so use 100 buckets for
  // up to an hour.
  start_time = base::TimeTicks::Now();
  installer_state.SetStage(installer::PATCHING);
  if (!archive_helper->ApplyPatch()) {
    *install_status = installer::APPLY_DIFF_PATCH_FAILED;
    installer_state.WriteInstallerResult(
        *install_status, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, NULL);
    VivaldiUpdateDeltaPatchStatus(false);
    return false;
  }

  // Record patch time only if it was successful.
  elapsed_time = base::TimeTicks::Now() - start_time;
  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Setup.Install.ApplyArchivePatchTime.background", elapsed_time);
  } else {
    UMA_HISTOGRAM_LONG_TIMES(
        "Setup.Install.ApplyArchivePatchTime", elapsed_time);
  }

  *archive_type = installer::INCREMENTAL_ARCHIVE_TYPE;
  VivaldiUpdateDeltaPatchStatus(true);
  return true;
}

void RecordNumDeleteOldVersionsAttempsBeforeAbort(int num_attempts) {
  UMA_HISTOGRAM_COUNTS_100(
      "Setup.Install.NumDeleteOldVersionsAttemptsBeforeAbort", num_attempts);
}

// Repetitively attempts to delete all files that belong to old versions of
// Chrome from |install_dir|. Waits 15 seconds before the first attempt and 5
// minutes after each unsuccessful attempt. Returns when no files that belong to
// an old version of Chrome remain or when another process tries to acquire the
// SetupSingleton.
installer::InstallStatus RepeatDeleteOldVersions(
    const base::FilePath& install_dir,
    const installer::SetupSingleton& setup_singleton) {
  constexpr int kMaxNumAttempts = 12;
  int num_attempts = 0;

  while (num_attempts < kMaxNumAttempts) {
    // Wait 15 seconds before the first attempt because trying to delete old
    // files right away is likely to fail. Indeed, this is called in 2
    // occasions:
    // - When the installer fails to delete old files after a not-in-use update:
    //   retrying immediately is likely to fail again.
    // - When executables are successfully renamed on Chrome startup or
    //   shutdown: old files can't be deleted because Chrome is still in use.
    // Wait 5 minutes after an unsuccessful attempt because retrying immediately
    // is likely to fail again.
    const base::TimeDelta max_wait_time = num_attempts == 0
                                              ? base::TimeDelta::FromSeconds(15)
                                              : base::TimeDelta::FromMinutes(5);
    if (setup_singleton.WaitForInterrupt(max_wait_time)) {
      VLOG(1) << "Exiting --delete-old-versions process because another "
                 "process tries to acquire the SetupSingleton.";
      RecordNumDeleteOldVersionsAttempsBeforeAbort(num_attempts);
      return installer::SETUP_SINGLETON_RELEASED;
    }

    const bool priority_was_changed_to_background =
        base::Process::Current().SetProcessBackgrounded(true);
    const bool delete_old_versions_success =
        installer::DeleteOldVersions(install_dir);
    if (priority_was_changed_to_background)
      base::Process::Current().SetProcessBackgrounded(false);
    ++num_attempts;

    if (delete_old_versions_success) {
      VLOG(1) << "Successfully deleted all old files from "
                 "--delete-old-versions process.";
      UMA_HISTOGRAM_COUNTS_100(
          "Setup.Install.NumDeleteOldVersionsAttemptsBeforeSuccess",
          num_attempts);
      return installer::DELETE_OLD_VERSIONS_SUCCESS;
    } else if (num_attempts == 1) {
      VLOG(1) << "Failed to delete all old files from --delete-old-versions "
                 "process. Will retry every five minutes.";
    }
  }

  VLOG(1) << "Exiting --delete-old-versions process after retrying too many "
             "times to delete all old files.";
  DCHECK_EQ(num_attempts, kMaxNumAttempts);
  RecordNumDeleteOldVersionsAttempsBeforeAbort(num_attempts);
  return installer::DELETE_OLD_VERSIONS_TOO_MANY_ATTEMPTS;
}

// This function is called when --rename-chrome-exe option is specified on
// setup.exe command line. This function assumes an in-use update has happened
// for Chrome so there should be a file called new_chrome.exe on the file
// system and a key called 'opv' in the registry. This function will move
// new_chrome.exe to chrome.exe and delete 'opv' key in one atomic operation.
// This function also deletes elevation policies associated with the old version
// if they exist. |setup_exe| is the path to the current executable.
installer::InstallStatus RenameChromeExecutables(
    const base::FilePath& setup_exe,
    const InstallationState& original_state,
    InstallerState* installer_state) {
  const base::FilePath &target_path = installer_state->target_path();
  base::FilePath chrome_exe(target_path.Append(installer::kChromeExe));
  base::FilePath chrome_new_exe(target_path.Append(installer::kChromeNewExe));
  base::FilePath chrome_old_exe(target_path.Append(installer::kChromeOldExe));

  // Create a temporary backup directory on the same volume as chrome.exe so
  // that moving in-use files doesn't lead to trouble.
  installer::SelfCleaningTempDir temp_path;
  if (!temp_path.Initialize(target_path.DirName(),
                            installer::kInstallTempDir)) {
    PLOG(ERROR) << "Failed to create Temp directory "
                << target_path.DirName()
                       .Append(installer::kInstallTempDir).value();
    return installer::RENAME_FAILED;
  }
  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  // Move chrome.exe to old_chrome.exe, then move new_chrome.exe to chrome.exe.
  install_list->AddMoveTreeWorkItem(chrome_exe.value(),
                                    chrome_old_exe.value(),
                                    temp_path.path().value(),
                                    WorkItem::ALWAYS_MOVE);
  install_list->AddMoveTreeWorkItem(chrome_new_exe.value(),
                                    chrome_exe.value(),
                                    temp_path.path().value(),
                                    WorkItem::ALWAYS_MOVE);
  install_list->AddDeleteTreeWorkItem(chrome_new_exe, temp_path.path());

  // Add work items to delete Chrome's "opv", "cpv", and "cmd" values.
  // TODO(grt): Clean this up; https://crbug.com/577816.
  HKEY reg_root = installer_state->root_key();
  base::string16 version_key;
  version_key = installer_state->product().distribution()->GetVersionKey();
  install_list->AddDeleteRegValueWorkItem(reg_root, version_key,
                                          KEY_WOW64_32KEY,
                                          google_update::kRegOldVersionField);
  install_list->AddDeleteRegValueWorkItem(
      reg_root, version_key, KEY_WOW64_32KEY,
      google_update::kRegCriticalVersionField);
  install_list->AddDeleteRegValueWorkItem(reg_root, version_key,
                                          KEY_WOW64_32KEY,
                                          google_update::kRegRenameCmdField);
  // old_chrome.exe is still in use in most cases, so ignore failures here.
  install_list->AddDeleteTreeWorkItem(chrome_old_exe, temp_path.path())
      ->set_best_effort(true);

  installer::InstallStatus ret = installer::RENAME_SUCCESSFUL;
  if (install_list->Do()) {
    installer::LaunchDeleteOldVersionsProcess(setup_exe, *installer_state);
  } else {
    LOG(ERROR) << "Renaming of executables failed. Rolling back any changes.";
    install_list->Rollback();
    ret = installer::RENAME_FAILED;
  }
  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory " << temp_path.path().value();

  return ret;
}

// Checks for compatibility between the current state of the system and the
// desired operation.
// Also blocks simultaneous user-level and system-level installs.  In the case
// of trying to install user-level Chrome when system-level exists, the
// existing system-level Chrome is launched.
// When the pre-install conditions are not satisfied, the result is written to
// the registry (via WriteInstallerResult), |status| is set appropriately, and
// false is returned.
bool CheckPreInstallConditions(const InstallationState& original_state,
                               const InstallerState& installer_state,
                               installer::InstallStatus* status) {
  if (installer_state.is_vivaldi()) {
    return true;
  }

  if (!installer_state.system_install()) {
    // This is a user-level installation. Make sure that we are not installing
    // on top of an existing system-level installation.
    const Product& product = installer_state.product();
    BrowserDistribution* browser_dist = product.distribution();
    DCHECK_EQ(BrowserDistribution::GetDistribution(), browser_dist);

    const ProductState* user_level_product_state =
        original_state.GetProductState(false);
    const ProductState* system_level_product_state =
        original_state.GetProductState(true);

    // Allow upgrades to proceed so that out-of-date versions are not left
    // around.
    if (user_level_product_state)
      return true;

    // This is a new user-level install...

    if (system_level_product_state) {
      // ... and the product already exists at system-level.
      LOG(ERROR) << "Already installed version "
                 << system_level_product_state->version().GetString()
                 << " at system-level conflicts with this one at user-level.";
      // Instruct Google Update to launch the existing system-level Chrome.
      // There should be no error dialog.
      base::FilePath install_path(
          installer::GetChromeInstallPath(true /* system_install */));
      if (install_path.empty()) {
        // Give up if we failed to construct the install path.
        *status = installer::OS_ERROR;
        installer_state.WriteInstallerResult(*status, IDS_INSTALL_OS_ERROR_BASE,
                                             nullptr);
      } else {
        *status = installer::EXISTING_VERSION_LAUNCHED;
        base::FilePath chrome_exe = install_path.Append(installer::kChromeExe);
        base::CommandLine cmd(chrome_exe);
        cmd.AppendSwitch(switches::kForceFirstRun);
        installer_state.WriteInstallerResult(
            *status, IDS_INSTALL_EXISTING_VERSION_LAUNCHED_BASE, nullptr);
        VLOG(1) << "Launching existing system-level chrome instead.";
        base::LaunchProcess(cmd, base::LaunchOptions());
      }
      return false;
    }
  }

  return true;
}

// Initializes |temp_path| to "Temp" within the target directory, and
// |unpack_path| to a random directory beginning with "source" within
// |temp_path|. Returns false on error.
bool CreateTemporaryAndUnpackDirectories(
    const InstallerState& installer_state,
    installer::SelfCleaningTempDir* temp_path,
    base::FilePath* unpack_path) {
  DCHECK(temp_path && unpack_path);

  if (!temp_path->Initialize(installer_state.target_path().DirName(),
                             installer::kInstallTempDir)) {
    PLOG(ERROR) << "Could not create temporary path.";
    return false;
  }
  VLOG(1) << "Created path " << temp_path->path().value();

  if (!base::CreateTemporaryDirInDir(temp_path->path(),
                                     installer::kInstallSourceDir,
                                     unpack_path)) {
    PLOG(ERROR) << "Could not create temporary path for unpacked archive.";
    return false;
  }

  return true;
}

installer::InstallStatus UninstallProduct(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    bool remove_all,
    bool force_uninstall,
    const Product& product) {
  const ProductState* product_state =
      original_state.GetProductState(installer_state.system_install());
  if (product_state != NULL) {
    VLOG(1) << "version on the system: "
            << product_state->version().GetString();
  } else if (!force_uninstall) {
    LOG(ERROR) << product.distribution()->GetDisplayName()
               << " not found for uninstall.";
    return installer::CHROME_NOT_INSTALLED;
  }

  return installer::UninstallProduct(
      original_state, installer_state, setup_exe, product, remove_all,
      force_uninstall, cmd_line);
}

installer::InstallStatus UninstallProducts(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line) {
  DCHECK_EQ(BrowserDistribution::GetDistribution(),
            installer_state.product().distribution());
  // System-level Chrome will be launched via this command if its program gets
  // set below.
  base::CommandLine system_level_cmd(base::CommandLine::NO_PROGRAM);

  if (cmd_line.HasSwitch(installer::switches::kSelfDestruct) &&
      !installer_state.system_install()) {
    const base::FilePath system_exe_path(
        installer::GetChromeInstallPath(true).Append(installer::kChromeExe));
    system_level_cmd.SetProgram(system_exe_path);
  }

  installer::InstallStatus install_status = installer::UNINSTALL_SUCCESSFUL;
  const bool force = cmd_line.HasSwitch(installer::switches::kForceUninstall);
  const bool remove_all = !cmd_line.HasSwitch(
      installer::switches::kDoNotRemoveSharedItems);

  install_status =
      UninstallProduct(original_state, installer_state, setup_exe, cmd_line,
                       remove_all, force, installer_state.product());

  installer::CleanUpInstallationDirectoryAfterUninstall(
      original_state, installer_state, setup_exe, &install_status);

  // The app and vendor dirs may now be empty. Make a last-ditch attempt to
  // delete them.
  installer::DeleteChromeDirectoriesIfEmpty(installer_state.target_path());

  // Trigger Active Setup if it was requested for the chrome product. This needs
  // to be done after the UninstallProduct calls as some of them might
  // otherwise terminate the process launched by TriggerActiveSetupCommand().
  if (cmd_line.HasSwitch(installer::switches::kTriggerActiveSetup))
    InstallUtil::TriggerActiveSetupCommand();

  if (!system_level_cmd.GetProgram().empty())
    base::LaunchProcess(system_level_cmd, base::LaunchOptions());

  // Tell Google Update that an uninstall has taken place if this install did
  // not originate from the MSI. Google Update has its own logic relating to
  // MSI-driven uninstalls that conflicts with this. Ignore the return value:
  // success or failure of Google Update has no bearing on the success or
  // failure of Chrome's uninstallation.
  if (!installer_state.is_msi())
    google_update::UninstallGoogleUpdate(installer_state.system_install());

  return install_status;
}

installer::InstallStatus InstallProducts(
    const InstallationState& original_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line,
    const MasterPreferences& prefs,
    InstallerState* installer_state,
    base::FilePath* installer_directory) {
  DCHECK(installer_state);
  const bool system_install = installer_state->system_install();
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  installer::ArchiveType archive_type = installer::UNKNOWN_ARCHIVE_TYPE;
  installer_state->SetStage(installer::PRECONDITIONS);
  // Remove any legacy "-multifail" or "-stage:*" values from the product's
  // "ap" value.
  BrowserDistribution::GetDistribution()->UpdateInstallStatus(
      system_install, archive_type, install_status);

  // Drop to background processing mode if the process was started below the
  // normal process priority class. This is done here because InstallProducts-
  // Helper has read-only access to the state and because the action also
  // affects everything else that runs below.
  bool entered_background_mode = installer::AdjustProcessPriority();
  installer_state->set_background_mode(entered_background_mode);
  VLOG_IF(1, entered_background_mode) << "Entered background processing mode.";

  if (CheckPreInstallConditions(original_state, *installer_state,
                                &install_status)) {
    VLOG(1) << "Installing to " << installer_state->target_path().value();
    install_status = InstallProductsHelper(
        original_state, setup_exe, cmd_line, prefs, *installer_state,
        installer_directory, &archive_type);
  } else {
    // CheckPreInstallConditions must set the status on failure.
    DCHECK_NE(install_status, installer::UNKNOWN_STATUS);
  }

  // Delete the master preferences file if present. Note that we do not care
  // about rollback here and we schedule for deletion on reboot if the delete
  // fails. As such, we do not use DeleteTreeWorkItem.
  if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
    base::FilePath prefs_path(cmd_line.GetSwitchValuePath(
        installer::switches::kInstallerData));
    if (!base::DeleteFile(prefs_path, false)) {
      LOG(ERROR) << "Failed deleting master preferences file "
                 << prefs_path.value()
                 << ", scheduling for deletion after reboot.";
      ScheduleFileSystemEntityForDeletion(prefs_path);
    }
  }

  installer_state->product().distribution()->UpdateInstallStatus(
      system_install, archive_type, install_status);

  return install_status;
}

installer::InstallStatus ShowEULADialog(const base::string16& inner_frame) {
  VLOG(1) << "About to show EULA";
  base::string16 eula_path = installer::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer::EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  installer::EulaHTMLDialog dlg(eula_path, inner_frame);
  installer::EulaHTMLDialog::Outcome outcome = dlg.ShowModal();
  if (installer::EulaHTMLDialog::REJECTED == outcome) {
    LOG(ERROR) << "EULA rejected or EULA failure";
    return installer::EULA_REJECTED;
  }
  if (installer::EulaHTMLDialog::ACCEPTED_OPT_IN == outcome) {
    VLOG(1) << "EULA accepted (opt-in)";
    return installer::EULA_ACCEPTED_OPT_IN;
  }
  VLOG(1) << "EULA accepted (no opt-in)";
  return installer::EULA_ACCEPTED;
}

// Creates the sentinel indicating that the EULA was required and has been
// accepted.
bool CreateEULASentinel(BrowserDistribution* dist) {
  base::FilePath eula_sentinel;
  if (!InstallUtil::GetEULASentinelFilePath(&eula_sentinel))
    return false;

  return (base::CreateDirectory(eula_sentinel.DirName()) &&
          base::WriteFile(eula_sentinel, "", 0) != -1);
}

installer::InstallStatus RegisterDevChrome(
    const InstallationState& original_state,
    const InstallerState& installer_state,
    const base::FilePath& setup_exe,
    const base::CommandLine& cmd_line) {
  BrowserDistribution* chrome_dist = BrowserDistribution::GetDistribution();

  // Only proceed with registering a dev chrome if no real Chrome installation
  // of the same distribution are present on this system.
  const ProductState* existing_chrome = original_state.GetProductState(false);
  if (!existing_chrome)
    existing_chrome = original_state.GetProductState(true);
  if (existing_chrome) {
    static const wchar_t kPleaseUninstallYourChromeMessage[] =
        L"You already have a full-installation (non-dev) of %1ls, please "
        L"uninstall it first using Add/Remove Programs in the control panel.";
    base::string16 name(chrome_dist->GetDisplayName());
    base::string16 message(
        base::StringPrintf(kPleaseUninstallYourChromeMessage, name.c_str()));

    LOG(ERROR) << "Aborting operation: another installation of " << name
               << " was found, as a last resort (if the product is not present "
                  "in Add/Remove Programs), try executing: "
               << existing_chrome->uninstall_command().GetCommandLineString();
    MessageBox(NULL, message.c_str(), NULL, MB_ICONERROR);
    return installer::INSTALL_FAILED;
  }

  base::FilePath chrome_exe(
      cmd_line.GetSwitchValuePath(installer::switches::kRegisterDevChrome));
  if (chrome_exe.empty())
    chrome_exe = setup_exe.DirName().Append(installer::kChromeExe);
  if (!chrome_exe.IsAbsolute())
    chrome_exe = base::MakeAbsoluteFilePath(chrome_exe);

  installer::InstallStatus status = installer::FIRST_INSTALL_SUCCESS;
  if (base::PathExists(chrome_exe)) {
    Product chrome(chrome_dist);

    // Create the Start menu shortcut and pin it to the Win7+ taskbar.
    ShellUtil::ShortcutProperties shortcut_properties(ShellUtil::CURRENT_USER);
    chrome.AddDefaultShortcutProperties(chrome_exe, &shortcut_properties);
    shortcut_properties.set_pin_to_taskbar(true);
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, chrome_dist,
        shortcut_properties, ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS);

    // Register Chrome at user-level and make it default.
    if (ShellUtil::CanMakeChromeDefaultUnattended()) {
      ShellUtil::MakeChromeDefault(chrome_dist, ShellUtil::CURRENT_USER,
                                   chrome_exe, true);
    } else {
      ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_dist, chrome_exe);
    }
  } else {
    LOG(ERROR) << "Path not found: " << chrome_exe.value();
    status = installer::INSTALL_FAILED;
  }
  return status;
}

// This method processes any command line options that make setup.exe do
// various tasks other than installation (renaming chrome.exe, showing eula
// among others). This function returns true if any such command line option
// has been found and processed (so setup.exe should exit at that point).
bool HandleNonInstallCmdLineOptions(const base::FilePath& setup_exe,
                                    const base::CommandLine& cmd_line,
                                    InstallationState* original_state,
                                    InstallerState* installer_state,
                                    int* exit_code) {
  // This option is independent of all others so doesn't belong in the if/else
  // block below.
  if (cmd_line.HasSwitch(installer::switches::kDelay)) {
    const std::string delay_seconds_string(
        cmd_line.GetSwitchValueASCII(installer::switches::kDelay));
    int delay_seconds;
    if (base::StringToInt(delay_seconds_string, &delay_seconds) &&
        delay_seconds > 0) {
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(delay_seconds));
    }
  }

  // TODO(gab): Add a local |status| variable which each block below sets;
  // only determine the |exit_code| from |status| at the end (this will allow
  // this method to validate that
  // (!handled || status != installer::UNKNOWN_STATUS)).
  bool handled = true;
  // TODO(tommi): Split these checks up into functions and use a data driven
  // map of switch->function.
  if (cmd_line.HasSwitch(installer::switches::kUpdateSetupExe)) {
    // this commandline switch is handled elsewhere if we are started
    // with the --vivaldi switch. See InstallProductsHelper().
    if (installer_state->is_vivaldi())
      return false;

    installer_state->SetStage(installer::UPDATING_SETUP);
    installer::InstallStatus status = installer::SETUP_PATCH_FAILED;
    // If --update-setup-exe command line option is given, we apply the given
    // patch to current exe, and store the resulting binary in the path
    // specified by --new-setup-exe. But we need to first unpack the file
    // given in --update-setup-exe.
    base::ScopedTempDir temp_path;
    if (!temp_path.CreateUniqueTempDir()) {
      PLOG(ERROR) << "Could not create temporary path.";
    } else {
      base::FilePath compressed_archive(cmd_line.GetSwitchValuePath(
          installer::switches::kUpdateSetupExe));
      VLOG(1) << "Opening archive " << compressed_archive.value();
      if (installer::ArchivePatchHelper::UncompressAndPatch(
              temp_path.GetPath(), compressed_archive, setup_exe,
              cmd_line.GetSwitchValuePath(installer::switches::kNewSetupExe),
              installer::UnPackConsumer::SETUP_EXE_PATCH)) {
        status = installer::NEW_VERSION_UPDATED;
      }
      if (!temp_path.Delete()) {
        // PLOG would be nice, but Delete() doesn't leave a meaningful value in
        // the Windows last-error code.
        LOG(WARNING) << "Scheduling temporary path "
                     << temp_path.GetPath().value()
                     << " for deletion at reboot.";
        ScheduleDirectoryForDeletion(temp_path.GetPath());
      }
    }

    *exit_code = InstallUtil::GetInstallReturnCode(status);
    if (*exit_code) {
      LOG(WARNING) << "setup.exe patching failed.";
      installer_state->WriteInstallerResult(
          status, IDS_SETUP_PATCH_FAILED_BASE, NULL);
    }
  } else if (cmd_line.HasSwitch(installer::switches::kShowEula)) {
    // Check if we need to show the EULA. If it is passed as a command line
    // then the dialog is shown and regardless of the outcome setup exits here.
    base::string16 inner_frame =
        cmd_line.GetSwitchValueNative(installer::switches::kShowEula);
    *exit_code = ShowEULADialog(inner_frame);

    if (installer::EULA_REJECTED != *exit_code) {
      if (GoogleUpdateSettings::SetEULAConsent(
              *original_state, BrowserDistribution::GetDistribution(), true)) {
        CreateEULASentinel(BrowserDistribution::GetDistribution());
      }
    }
  } else if (cmd_line.HasSwitch(installer::switches::kConfigureUserSettings)) {
    // NOTE: Should the work done here, on kConfigureUserSettings, change:
    // kActiveSetupVersion in install_worker.cc needs to be increased for Active
    // Setup to invoke this again for all users of this install.
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    if (installer_state->system_install()) {
      bool force =
          cmd_line.HasSwitch(installer::switches::kForceConfigureUserSettings);
      installer::HandleActiveSetupForBrowser(*installer_state, force);
      status = installer::INSTALL_REPAIRED;
    } else {
      LOG(DFATAL)
          << "--configure-user-settings is incompatible with user-level";
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterDevChrome)) {
    installer::InstallStatus status = RegisterDevChrome(
        *original_state, *installer_state, setup_exe, cmd_line);
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser)) {
    installer::InstallStatus status = installer::UNKNOWN_STATUS;
    const Product& chrome_install = installer_state->product();
    // If --register-chrome-browser option is specified, register all Chrome
    // protocol/file associations, as well as register it as a valid browser for
    // Start Menu->Internet shortcut. This switch will also register Chrome as a
    // valid handler for a set of URL protocols that Chrome may become the
    // default handler for, either by the user marking Chrome as the default
    // browser, through the Windows Default Programs control panel settings, or
    // through website use of registerProtocolHandler. These protocols are found
    // in ShellUtil::kPotentialProtocolAssociations.  The
    // --register-url-protocol will additionally register Chrome as a potential
    // handler for the supplied protocol, and is used if a website registers a
    // handler for a protocol not found in
    // ShellUtil::kPotentialProtocolAssociations.  These options should only be
    // used when setup.exe is launched with admin rights. We do not make any
    // user specific changes with this option.
    DCHECK(IsUserAnAdmin());
    base::FilePath chrome_exe(cmd_line.GetSwitchValuePath(
        installer::switches::kRegisterChromeBrowser));
    base::string16 suffix;
    if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    if (cmd_line.HasSwitch(installer::switches::kRegisterURLProtocol)) {
      base::string16 protocol = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterURLProtocol);
      // ShellUtil::RegisterChromeForProtocol performs all registration
      // done by ShellUtil::RegisterChromeBrowser, as well as registering
      // with Windows as capable of handling the supplied protocol.
      if (ShellUtil::RegisterChromeForProtocol(chrome_install.distribution(),
                                               chrome_exe, suffix, protocol,
                                               false))
        status = installer::IN_USE_UPDATED;
    } else {
      if (ShellUtil::RegisterChromeBrowser(chrome_install.distribution(),
                                           chrome_exe, suffix, false))
        status = installer::IN_USE_UPDATED;
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kDeleteOldVersions) ||
             cmd_line.HasSwitch(installer::switches::kRenameChromeExe)) {
    std::unique_ptr<installer::SetupSingleton> setup_singleton(
        installer::SetupSingleton::Acquire(
            cmd_line, MasterPreferences::ForCurrentProcess(), original_state,
            installer_state));
    if (!setup_singleton) {
      *exit_code = installer::SETUP_SINGLETON_ACQUISITION_FAILED;
    } else if (cmd_line.HasSwitch(installer::switches::kDeleteOldVersions)) {
      *exit_code = RepeatDeleteOldVersions(installer_state->target_path(),
                                           *setup_singleton);
    } else {
      DCHECK(cmd_line.HasSwitch(installer::switches::kRenameChromeExe));
      *exit_code =
          RenameChromeExecutables(setup_exe, *original_state, installer_state);
    }
  } else if (cmd_line.HasSwitch(
                 installer::switches::kRemoveChromeRegistration)) {
    // This is almost reverse of --register-chrome-browser option above.
    // Here we delete Chrome browser registration. This option should only
    // be used when setup.exe is launched with admin rights. We do not
    // make any user specific changes in this option.
    base::string16 suffix;
    if (cmd_line.HasSwitch(
            installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    installer::InstallStatus tmp = installer::UNKNOWN_STATUS;
    const Product& chrome_install = installer_state->product();
    installer::DeleteChromeRegistrationKeys(*installer_state,
                                            chrome_install.distribution(),
                                            HKEY_LOCAL_MACHINE, suffix, &tmp);
    *exit_code = tmp;
  } else if (cmd_line.HasSwitch(installer::switches::kOnOsUpgrade)) {
    const Product& chrome_install = installer_state->product();
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    std::unique_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(setup_exe));
    const base::Version installed_version(
        base::UTF16ToUTF8(version_info->product_version()));
    if (installed_version.IsValid()) {
      installer::HandleOsUpgradeForBrowser(*installer_state, chrome_install,
                                           installed_version);
      status = installer::INSTALL_REPAIRED;
    } else {
      LOG(DFATAL) << "Failed to extract product version from "
                  << setup_exe.value();
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kUserExperiment)) {
    installer::RunUserExperiment(cmd_line,
                                 MasterPreferences::ForCurrentProcess(),
                                 original_state, installer_state);
    exit_code = 0;
  } else if (cmd_line.HasSwitch(installer::switches::kPatch)) {
    const std::string patch_type_str(
        cmd_line.GetSwitchValueASCII(installer::switches::kPatch));
    const base::FilePath input_file(
        cmd_line.GetSwitchValuePath(installer::switches::kInputFile));
    const base::FilePath patch_file(
        cmd_line.GetSwitchValuePath(installer::switches::kPatchFile));
    const base::FilePath output_file(
        cmd_line.GetSwitchValuePath(installer::switches::kOutputFile));

    if (patch_type_str == installer::kCourgette) {
      *exit_code = installer::CourgettePatchFiles(input_file,
                                                  patch_file,
                                                  output_file);
    } else if (patch_type_str == installer::kBsdiff) {
      *exit_code = installer::BsdiffPatchFiles(input_file,
                                               patch_file,
                                               output_file);
#if BUILDFLAG(ZUCCHINI)
    } else if (patch_type_str == installer::kZucchini) {
      *exit_code =
          installer::ZucchiniPatchFiles(input_file, patch_file, output_file);
#endif  // BUILDFLAG(ZUCCHINI)
    } else {
      *exit_code = installer::PATCH_INVALID_ARGUMENTS;
    }
  } else if (cmd_line.HasSwitch(installer::switches::kReenableAutoupdates)) {
    // setup.exe has been asked to attempt to reenable updates for Chrome.
    bool updates_enabled = GoogleUpdateSettings::ReenableAutoupdates();
    *exit_code = updates_enabled ? installer::REENABLE_UPDATES_SUCCEEDED :
                                   installer::REENABLE_UPDATES_FAILED;
  } else if (cmd_line.HasSwitch(
                 installer::switches::kSetDisplayVersionProduct)) {
    const base::string16 registry_product(
        cmd_line.GetSwitchValueNative(
            installer::switches::kSetDisplayVersionProduct));
    const base::string16 registry_value(
        cmd_line.GetSwitchValueNative(
            installer::switches::kSetDisplayVersionValue));
    *exit_code = OverwriteDisplayVersions(registry_product, registry_value);
#if defined(GOOGLE_CHROME_BUILD)
  } else if (cmd_line.HasSwitch(installer::switches::kStoreDMToken)) {
    // Write the specified token to the registry, overwriting any already
    // existing value.
    base::string16 token_switch_value =
        cmd_line.GetSwitchValueNative(installer::switches::kStoreDMToken);
    base::Optional<std::string> token;
    if (!(token = installer::DecodeDMTokenSwitchValue(token_switch_value)) ||
        !installer::StoreDMToken(*token)) {
      *exit_code = installer::STORE_DMTOKEN_FAILED;
    } else {
      *exit_code = installer::STORE_DMTOKEN_SUCCESS;
    }
#endif
  } else {
    handled = false;
  }

  return handled;
}

}  // namespace

namespace installer {

InstallStatus InstallProductsHelper(const InstallationState& original_state,
                                    const base::FilePath& setup_exe,
                                    const base::CommandLine& cmd_line,
                                    const MasterPreferences& prefs,
                                    const InstallerState& installer_state,
                                    base::FilePath* installer_directory,
                                    ArchiveType* archive_type) {
  DCHECK(archive_type);
  bool patch_install = false;
  const bool system_install = installer_state.system_install();
  InstallStatus install_status = UNKNOWN_STATUS;

  // Create a temp folder where we will unpack Chrome archive. If it fails,
  // then we are doomed, so return immediately and no cleanup is required.
  SelfCleaningTempDir temp_path;
  base::FilePath unpack_path;
  if (!CreateTemporaryAndUnpackDirectories(installer_state, &temp_path,
                                           &unpack_path)) {
    installer_state.WriteInstallerResult(TEMP_DIR_FAILED,
                                         IDS_INSTALL_TEMP_DIR_FAILED_BASE,
                                         NULL);
    return TEMP_DIR_FAILED;
  }

  // Uncompress and optionally patch the archive if an uncompressed archive was
  // not specified on the command line and a compressed archive is found.
  *archive_type = UNKNOWN_ARCHIVE_TYPE;
  base::FilePath uncompressed_archive(cmd_line.GetSwitchValuePath(
      switches::kUncompressedArchive));
  if (uncompressed_archive.empty()) {
    base::Version previous_version;
    if (cmd_line.HasSwitch(installer::switches::kPreviousVersion)) {
      previous_version = base::Version(cmd_line.GetSwitchValueASCII(
          installer::switches::kPreviousVersion));
    }
    patch_install = previous_version.IsValid();
    std::unique_ptr<ArchivePatchHelper> archive_helper(
        CreateChromeArchiveHelper(
            setup_exe, cmd_line, installer_state, unpack_path,
            (previous_version.IsValid()
                 ? UnPackConsumer::CHROME_ARCHIVE_PATCH
                 : UnPackConsumer::COMPRESSED_CHROME_ARCHIVE)));
    if (archive_helper) {
      VLOG(1) << "Installing Chrome from compressed archive "
              << archive_helper->compressed_archive().value();
      if (!UncompressAndPatchChromeArchive(original_state,
                                           installer_state,
                                           archive_helper.get(),
                                           archive_type,
                                           &install_status,
                                           previous_version)) {
        DCHECK_NE(install_status, UNKNOWN_STATUS);
        return install_status;
      }
      uncompressed_archive = archive_helper->target();
      DCHECK(!uncompressed_archive.empty());
    } else {
      if (patch_install) {
        // The delta patch archive is invalid or missing, so bail out here.
        LOG(ERROR) << "Cannot patch Vivaldi without a valid (delta) archive.";
        installer_state.WriteInstallerResult(
            INVALID_ARCHIVE, IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
        VivaldiUpdateDeltaPatchStatus(false);
        return INVALID_ARCHIVE;
      }
    }
  }
  // Check for an uncompressed archive alongside the current executable if one
  // was not given or generated.
  if (uncompressed_archive.empty())
    uncompressed_archive = setup_exe.DirName().Append(kChromeArchive);

  if (*archive_type == UNKNOWN_ARCHIVE_TYPE) {
    // An archive was not uncompressed or patched above.
    if (uncompressed_archive.empty() ||
        !base::PathExists(uncompressed_archive)) {
      LOG(ERROR) << "Cannot install Vivaldi without an uncompressed archive.";
      installer_state.WriteInstallerResult(
          INVALID_ARCHIVE, IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
      return INVALID_ARCHIVE;
    }
    *archive_type = FULL_ARCHIVE_TYPE;
  }

  // Unpack the uncompressed archive.
  installer_state.SetStage(UNPACKING);
  base::TimeTicks start_time = base::TimeTicks::Now();
  UnPackStatus unpack_status = UNPACK_NO_ERROR;
  int32_t ntstatus = 0;
  DWORD lzma_result = UnPackArchive(uncompressed_archive, unpack_path, NULL,
                                    &unpack_status, &ntstatus);
  RecordUnPackMetrics(unpack_status, ntstatus,
                      UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE);
  if (lzma_result) {
    installer_state.WriteInstallerResult(
        UNPACKING_FAILED,
        IDS_INSTALL_UNCOMPRESSION_FAILED_BASE,
        NULL);
    return UNPACKING_FAILED;
  }

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;
  if (installer_state.is_background_mode()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UnpackFullArchiveTime.background", elapsed_time);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Setup.Install.UnpackFullArchiveTime", elapsed_time);
  }

  VLOG(1) << "unpacked to " << unpack_path.value();
  base::FilePath src_path(
      unpack_path.Append(kInstallSourceChromeDir));
  std::unique_ptr<base::Version> installer_version(
      GetMaxVersionFromArchiveDir(src_path));
  if (!installer_version.get()) {
    LOG(ERROR) << "Did not find any valid version in installer.";
    install_status = INVALID_ARCHIVE;
    installer_state.WriteInstallerResult(install_status,
        IDS_INSTALL_INVALID_ARCHIVE_BASE, NULL);
  } else {
    VLOG(1) << "version to install: " << installer_version->GetString();
    bool proceed_with_installation = true;

    if (!IsDowngradeAllowed(prefs)) {
      const Product& product = installer_state.product();
      const ProductState* product_state =
          original_state.GetProductState(system_install);
      if (product_state != NULL &&
          (product_state->version().CompareTo(*installer_version) > 0)) {
        LOG(ERROR) << "Higher version of "
                   << product.distribution()->GetDisplayName()
                   << " is already installed.";
        // NOTE(jarle@vivaldi): If standalone we allow downgrading.
        if (!installer_state.is_standalone()) {
          int message_id = IDS_INSTALL_HIGHER_VERSION_BASE;
          proceed_with_installation = false;
          install_status = HIGHER_VERSION_EXISTS;
          installer_state.WriteInstallerResult(install_status, message_id,
              NULL);
        }
      }
    }

    if (proceed_with_installation) {
      base::FilePath prefs_source_path(cmd_line.GetSwitchValueNative(
          switches::kInstallerData));
      install_status = InstallOrUpdateProduct(
          original_state, installer_state, setup_exe, uncompressed_archive,
          temp_path.path(), src_path, prefs_source_path, prefs,
          *installer_version);

      int install_msg_base = IDS_INSTALL_FAILED_BASE;
      base::FilePath chrome_exe;
      base::string16 quoted_chrome_exe;
      if (install_status == SAME_VERSION_REPAIR_FAILED) {
        install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_BASE;
      } else if (install_status != INSTALL_FAILED) {
        if (installer_state.target_path().empty()) {
          // If we failed to construct install path, it means the OS call to
          // get %ProgramFiles% or %AppData% failed. Report this as failure.
          install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
          install_status = OS_ERROR;
        } else {
          chrome_exe = installer_state.target_path().Append(kChromeExe);
          quoted_chrome_exe = L"\"" + chrome_exe.value() + L"\"";
          install_msg_base = 0;
        }
      }

      installer_state.SetStage(FINISHING);

      const Product& chrome_install = installer_state.product();

      bool do_not_register_for_update_launch = false;
      prefs.GetBool(master_preferences::kDoNotRegisterForUpdateLaunch,
                    &do_not_register_for_update_launch);

      bool write_chrome_launch_string =
          (!do_not_register_for_update_launch &&
           install_status != IN_USE_UPDATED);

      installer_state.WriteInstallerResult(install_status, install_msg_base,
          write_chrome_launch_string ? &quoted_chrome_exe : NULL);
      // TODO(jarle@vivaldi.com): REMOVE THIS:
      // rename the "Profile" folder to "User Data" for standalone builds if
      // the "Profile" folder exists
      if (installer_state.is_standalone() &&
          (install_status == FIRST_INSTALL_SUCCESS ||
          install_status == INSTALL_REPAIRED ||
          install_status == NEW_VERSION_UPDATED ||
          install_status == IN_USE_UPDATED)) {
        std::wstring::size_type pos =
          std::wstring(
            installer_state.target_path().value()).rfind(L"\\Application");
        if (pos != std::wstring::npos) {
          std::wstring base =
            std::wstring(installer_state.target_path().value()).substr(0, pos);
          base::FilePath old_profile_dir(base);
          old_profile_dir = old_profile_dir.AppendASCII("Profile");
          base::FilePath new_user_data_dir(base);
          new_user_data_dir = new_user_data_dir.AppendASCII("User Data");
          if (base::DirectoryExists(old_profile_dir)) {
            if (!::MoveFileEx(old_profile_dir.value().c_str(),
              new_user_data_dir.value().c_str(), MOVEFILE_WRITE_THROUGH)) {
              DWORD error = ::GetLastError();
              LOG(WARNING) <<
                "Failed to rename old Profile folder to User Data. Error="
                << error;
              base::string16 message(base::StringPrintf(
                  L"Failed to rename 'Profile' folder. Error=%u", error));
              MessageBox(NULL, message.c_str(), L"Vivaldi Installer",
                  MB_OK | MB_ICONWARNING);
            } else {
              // relax for a sec to be 100% sure that the rename has been
              // flushed to disk ...
              Sleep(1000);
            }
          }
        }
      }
      if (!installer_state.is_vivaldi()) {
      if (install_status == FIRST_INSTALL_SUCCESS) {
        VLOG(1) << "First install successful.";
        // We never want to launch Chrome in system level install mode.
        bool do_not_launch_chrome = false;
        prefs.GetBool(master_preferences::kDoNotLaunchChrome,
                      &do_not_launch_chrome);
        if (!system_install && !do_not_launch_chrome)
          chrome_install.LaunchChrome(installer_state.target_path());
      } else if ((install_status == NEW_VERSION_UPDATED) ||
                 (install_status == IN_USE_UPDATED)) {
        const Product& chrome = installer_state.product();
        DCHECK_NE(chrome_exe.value(), base::string16());
        RemoveChromeLegacyRegistryKeys(chrome.distribution(), chrome_exe);
      }
      }
      // For Vivaldi, if this is a standalone install, write the stp.viv file
      // to the same location as the executable.
      if (installer_state.is_standalone() &&
          (install_status == FIRST_INSTALL_SUCCESS ||
          install_status == INSTALL_REPAIRED ||
          install_status == NEW_VERSION_UPDATED ||
          install_status == IN_USE_UPDATED)) {
        std::string content_str("// Vivaldi Standalone");
        base::FilePath stp_viv_path =
            installer_state.target_path().
                Append(installer::kStandaloneProfileHelper);
        int size = static_cast<int>(content_str.size());
        if (base::WriteFile(stp_viv_path,
                            content_str.c_str(),
                            size) == size) {
          VLOG(1) << "Successfully wrote: " << stp_viv_path.value();
        } else {
          PLOG(ERROR) << "Error writing: " << stp_viv_path.value();
          return installer::INSTALL_FAILED;
        }
      }
      // For Vivaldi, if this is a patch install, we will try to patch
      // setup.exe as well.
      if (patch_install &&
          cmd_line.HasSwitch(installer::switches::kUpdateSetupExe)) {
        install_status = installer::SETUP_PATCH_FAILED;
        // If --update-setup-exe command line option is given, we apply
        // the given patch to current exe, and store the resulting binary in
        // the path specified by --new-setup-exe. But we need to first unpack
        // the file given in --update-setup-exe.
        base::ScopedTempDir temp_path;
        if (!temp_path.CreateUniqueTempDir()) {
          PLOG(ERROR) << "Could not create temporary path.";
        } else {
          base::FilePath compressed_archive(cmd_line.GetSwitchValuePath(
              installer::switches::kUpdateSetupExe));
          VLOG(1) << "Opening archive " << compressed_archive.value();
          if (installer::ArchivePatchHelper::UncompressAndPatch(
                  temp_path.GetPath(), compressed_archive, setup_exe,
                  cmd_line.GetSwitchValuePath(
                      installer::switches::kNewSetupExe),
                  installer::UnPackConsumer::SETUP_EXE_PATCH)) {
            install_status = installer::NEW_VERSION_UPDATED;
          }
          if (!temp_path.Delete()) {
            // PLOG would be nice, but Delete() doesn't leave a meaningful
            // value in the Windows last-error code.
            LOG(WARNING) << "Scheduling temporary path "
                          << temp_path.GetPath().value()
                          << " for deletion at reboot.";
            ScheduleDirectoryForDeletion(temp_path.GetPath());
          }
        }

        int exit_code = InstallUtil::GetInstallReturnCode(install_status);
        if (exit_code) {
          LOG(WARNING) << "setup.exe patching failed.";
          installer_state.WriteInstallerResult(
              install_status, IDS_SETUP_PATCH_FAILED_BASE, NULL);
          return install_status;
        }
      }

      bool force_launch_vivaldi_on_successful_install =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            vivaldi::constants::kVivaldiForceLaunch);
      if (force_launch_vivaldi_on_successful_install &&
          (install_status == FIRST_INSTALL_SUCCESS ||
          install_status == INSTALL_REPAIRED ||
          install_status == NEW_VERSION_UPDATED ||
          install_status == IN_USE_UPDATED)) {
        base::FilePath vivaldi_path =
            installer_state.target_path().Append(installer::kChromeExe);
        base::string16 new_features_url;

// Show new features tab only for official final builds.
#if defined(OFFICIAL_BUILD) && \
    (BUILD_VERSION(VIVALDI_RELEASE) == VIVALDI_BUILD_PUBLIC_RELEASE)
        if (installer_state.is_vivaldi_update())
          new_features_url = vivaldi::GetNewFeaturesUrl(*installer_version);
#endif
        // We need to use the custom ShellExecuteFromExplorer to avoid
        // launching vivaldi.exe with elevated privileges.
        // The setup.exe process could be elevated.
        VLOG(1) << "Launching: " << vivaldi_path.value()
            << ", is_standalone() = " << installer_state.is_standalone()
            << ", install_status = " << static_cast<int>(install_status);
        vivaldi::ShellExecuteFromExplorer(vivaldi_path,
                                          new_features_url,
                                          base::FilePath(),
                                          L"");
      }
    }
  }
  // For Vivaldi, launch the cleanup process here and not before we patch
  // the setup.exe (see install.cc).
  if (installer_state.is_vivaldi()) {
    const base::FilePath new_version_setup_path =
        installer_state.GetInstallerDirectory(*installer_version)
            .Append(setup_exe.BaseName());
    installer::LaunchDeleteOldVersionsProcess(new_version_setup_path,
                                              installer_state);
  }

  // If the installation completed successfully...
  VivaldiUpdateDeltaPatchStatus(true);
  if (InstallUtil::GetInstallReturnCode(install_status) == 0) {
    // Update the DisplayVersion created by an MSI-based install.
    base::FilePath master_preferences_file(
      installer_state.target_path().AppendASCII(
          installer::kDefaultMasterPrefs));
    std::string install_id;
    if (prefs.GetString(installer::master_preferences::kMsiProductId,
                        &install_id)) {
      // A currently active MSI install will have specified the master-
      // preferences file on the command-line that includes the product-id.
      // We must delay the setting of the DisplayVersion until after the
      // grandparent "msiexec" process has exited.
      base::FilePath new_setup =
          installer_state.GetInstallerDirectory(*installer_version)
          .Append(kSetupExe);
      DelayedOverwriteDisplayVersions(
          new_setup, install_id, *installer_version);
    } else {
      // Only when called by the MSI installer do we need to delay setting
      // the DisplayVersion.  In other runs, such as those done by the auto-
      // update action, we set the value immediately.
      const Product& chrome = installer_state.product();
      // Get the app's MSI Product-ID from an entry in ClientState.
      base::string16 app_guid = FindMsiProductId(installer_state, chrome);
      if (!app_guid.empty()) {
        OverwriteDisplayVersions(
            app_guid, base::UTF8ToUTF16(installer_version->GetString()));
      }
    }
    // Return the path to the directory containing the newly installed
    // setup.exe and uncompressed archive if the caller requested it.
    if (installer_directory) {
      *installer_directory =
          installer_state.GetInstallerDirectory(*installer_version);
    }
  }

  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory " << temp_path.path().value();

  return install_status;
}

}  // namespace installer

// Attempts to enable the SE_DEBUG privilege, which allows us to obtain
// tokens for processes of other users. This shouls always succeed when
// running elevated and always fail otherwise. This is ok, because we
// only want to handle processes of other users when elevated.
void EnableDebugPrivileges() {
  HANDLE process_token_handle;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES,
                       &process_token_handle) == FALSE)
    return;
  base::win::ScopedHandle process_token(process_token_handle);

  LUID locally_unique_id;

  if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &locally_unique_id) == FALSE)
    return;

  TOKEN_PRIVILEGES token_privileges;
  token_privileges.PrivilegeCount = 1;
  token_privileges.Privileges[0].Luid = locally_unique_id;
  token_privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

  ::AdjustTokenPrivileges(process_token.Get(), FALSE, &token_privileges, NULL,
                          NULL, NULL);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* command_line, int show_command) {
  // Check to see if the CPU is supported before doing anything else. There's
  // very little than can safely be accomplished if the CPU isn't supported
  // since dependent libraries (e.g., base) may use invalid instructions.
  if (!installer::IsProcessorSupported())
    return installer::CPU_NOT_SUPPORTED;

  // Persist histograms so they can be uploaded later. The storage directory is
  // created during installation when the main WorkItemList is evaluated so
  // disable storage directory creation in PersistentHistogramStorage.
  base::PersistentHistogramStorage persistent_histogram_storage(
      installer::kSetupHistogramAllocatorName,
      base::PersistentHistogramStorage::StorageDirManagement::kUseExisting);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, NULL);

  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    // Histogram storage is enabled at the very top of this wWinMain. Disable it
    // when this process is decicated to crashpad as there is no directory in
    // which to write them nor a browser to subsequently upload them.
    persistent_histogram_storage.Disable();
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess(), base::FilePath(),
        switches::kProcessType, switches::kUserDataDir);
  }

  // install_util uses chrome paths.
  chrome::RegisterPathProvider();

  const MasterPreferences& prefs = MasterPreferences::ForCurrentProcess();
  installer::InitInstallerLogging(prefs);

  base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  VLOG(1) << "Command Line: " << cmd_line.GetCommandLineString();

  InitializeInstallDetails(cmd_line, prefs);

  base::FilePath vivaldi_target_path(cmd_line.GetSwitchValuePath(
      vivaldi::constants::kVivaldiInstallDir));

  // if true, the vivaldi target path was provided on the cmd line
  const bool is_vivaldi_target_path_provided = !vivaldi_target_path.empty();

  const bool is_vivaldi = cmd_line.HasSwitch(vivaldi::constants::kVivaldi);
  const bool is_uninstall = cmd_line.HasSwitch(installer::switches::kUninstall);
  const bool is_vivaldi_update = cmd_line.HasSwitch(
      vivaldi::constants::kVivaldiUpdate);
  const bool is_standalone = cmd_line.HasSwitch(
      vivaldi::constants::kVivaldiStandalone);
  const bool is_silent =
      cmd_line.HasSwitch(vivaldi::constants::kVivaldiSilent);
#if defined(VIVALDI_BUILD)
  // NOTE(jarle@vivaldi.com): From Chr-50, XP/Vista is unsupported.
  if (!InstallUtil::IsOSSupported()) {
    // TODO(jarle@vivaldi.com): Localize
    MessageBox(NULL, L"Vivaldi requires Windows 7 or higher.",
      NULL, MB_ICONINFORMATION | MB_SETFOREGROUND);
    return installer::OS_NOT_SUPPORTED;
  }
#endif
  installer::VivaldiInstallDialog::InstallType install_type;
  if (cmd_line.HasSwitch(installer::switches::kSystemLevel))
    install_type = installer::VivaldiInstallDialog::INSTALL_FOR_ALL_USERS;
  else if (is_standalone)
    install_type = installer::VivaldiInstallDialog::INSTALL_STANDALONE;
  else
    install_type = installer::VivaldiInstallDialog::INSTALL_FOR_CURRENT_USER;

  // for silent installs, make sure we have an install path
  if (is_silent && !is_vivaldi_target_path_provided) {
    int csidl = 0;

    if (install_type == installer::VivaldiInstallDialog::INSTALL_FOR_ALL_USERS)
      csidl = CSIDL_PROGRAM_FILES;
    else if (install_type ==
        installer::VivaldiInstallDialog::INSTALL_FOR_CURRENT_USER)
      csidl = CSIDL_LOCAL_APPDATA;

    wchar_t szPath[MAX_PATH];
    if (csidl && SUCCEEDED(::SHGetFolderPath(NULL, csidl, NULL, 0, szPath))) {
      vivaldi_target_path = base::FilePath(szPath).Append(L"Vivaldi");
      cmd_line.AppendSwitchPath(vivaldi::constants::kVivaldiInstallDir,
                                vivaldi_target_path);
    } else {
      LOG(ERROR) << "Vivaldi silent install failed: Install path empty.";
      return installer::INSTALL_FAILED;
    }
  }

  if (is_vivaldi && !(is_vivaldi_update || is_uninstall || is_silent)) {
    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize = sizeof(iccx);
    iccx.dwICC = ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_TREEVIEW_CLASSES |
        ICC_USEREX_CLASSES;
    ::InitCommonControlsEx(&iccx);

    installer::VivaldiInstallDialog dlg(instance,
                                        false,
                                        install_type,
                                        vivaldi_target_path);

    const installer::VivaldiInstallDialog::DlgResult dlg_result =
      dlg.ShowModal();
    if (dlg_result != installer::VivaldiInstallDialog::INSTALL_DLG_INSTALL) {
      VLOG(1) << "Vivaldi: install cancelled/failed.";
      return installer::INSTALL_FAILED;
    }

    const bool set_vivaldi_as_default = dlg.GetSetAsDefaultBrowser();
    if (set_vivaldi_as_default) {
      cmd_line.AppendSwitch(installer::switches::kMakeChromeDefault);
      VLOG(1) << "Vivaldi: set as default browser.";
    }

    base::FilePath path(dlg.GetDestinationFolder());
    cmd_line.AppendSwitchPath(vivaldi::constants::kVivaldiInstallDir, path);

    vivaldi_target_path = path;

    install_type = dlg.GetInstallType();
    switch (install_type)
    {
    case installer::VivaldiInstallDialog::INSTALL_FOR_CURRENT_USER:
      VLOG(1) << "Vivaldi: install for current user - install dir=" <<
        path.value();
      break;
    case installer::VivaldiInstallDialog::INSTALL_FOR_ALL_USERS:
      cmd_line.AppendSwitch(installer::switches::kSystemLevel);
      VLOG(1) << "Vivaldi: install for all users (system install).";
      break;
    case installer::VivaldiInstallDialog::INSTALL_STANDALONE:
      cmd_line.AppendSwitch(vivaldi::constants::kVivaldiStandalone);
      VLOG(1) << "Vivaldi: standalone install - install dir=" << path.value();
      break;
    default:
      NOTREACHED();
    }

    if (dlg.GetRegisterBrowser()) {
      cmd_line.AppendSwitch(vivaldi::constants::kVivaldiRegisterStandalone);
      VLOG(1) << "Vivaldi: register standalone browser.";
    }
  }

  installer::VivaldiProgressDialog progress_dlg(instance);

  EnableDebugPrivileges();

  base::FilePath vivaldi_exe_path(
      vivaldi_target_path.Append(installer::kInstallBinaryDir)
          .Append(installer::kChromeExe));

  if (is_vivaldi && !is_uninstall) {
    std::vector<base::FilePath::StringType> components;
    vivaldi_exe_path.GetComponents(&components);

    wchar_t path_sz[MAX_PATH];
    DWORD res = ::QueryDosDevice(components[0].c_str(),
                                 (LPWSTR)path_sz, MAX_PATH - 1);

    std::wstring target_path_str(path_sz);
    if (res && (target_path_str.find(L"\\??\\", 0, 4) != std::wstring::npos)) {
      // this is a virtual drive
      VLOG(1) << "Virtual drive: " << target_path_str;
      target_path_str.erase(0, 4);
      base::FilePath target_path = base::FilePath(target_path_str);
      for (size_t i = 2; i < components.size(); ++i) {
        target_path = target_path.Append(components[i]);
      }
      vivaldi_exe_path = target_path;
    }

    std::vector<base::win::ScopedHandle> vivaldi_processes(
        vivaldi::GetRunningProcessesForPath(vivaldi_exe_path));
    if (!vivaldi_processes.empty()) {
      vivaldi::KillProcesses(vivaldi_processes);

      const int MAX_WAIT_SECS = 10;
      for (int wait = MAX_WAIT_SECS * 10; wait > 0; --wait) {
        Sleep(100);
        vivaldi_processes =
            vivaldi::GetRunningProcessesForPath(vivaldi_exe_path);
        if (vivaldi_processes.empty())
          break;
      }

      int choice = IDRETRY;
      while (choice != IDCANCEL && !vivaldi_processes.empty()) {
        choice = MessageBox(
            NULL,
            L"Vivaldi is still running.\n"
            L"Please close all Vivaldi windows before continuing install.",
            L"Vivaldi Installer", MB_RETRYCANCEL | MB_ICONEXCLAMATION);
        vivaldi_processes =
            vivaldi::GetRunningProcessesForPath(vivaldi_exe_path);
      }
      if (choice == IDCANCEL) {
        VLOG(1) << "Vivaldi: install cancelled due to running instances.";
        return installer::INSTALL_FAILED;
      }
    }

    // enable auto-update
    std::wstring update_key(vivaldi::constants::kVivaldiKey);
    update_key.append(L"\\");
    update_key.append(vivaldi::constants::kVivaldiAutoUpdate);
    base::win::RegKey key(HKEY_CURRENT_USER,
        update_key.c_str(), KEY_ALL_ACCESS);
    if (key.Valid())
      key.WriteValue(L"Enabled", L"1");
  }

  bool system_install = false;
  prefs.GetBool(installer::master_preferences::kSystemLevel, &system_install);

  if (is_vivaldi) {
    if (is_vivaldi_update) {
      // Find the install type of the installed Vivaldi.
      // If installed, update the main install type.
      installer::VivaldiInstallDialog::InstallType inst_type;
      if (installer::VivaldiInstallDialog::IsVivaldiInstalled(
          vivaldi_target_path, &inst_type)) {
        install_type = inst_type;
      }
    }

    system_install = (install_type ==
        installer::VivaldiInstallDialog::INSTALL_FOR_ALL_USERS);
  }
  VLOG(1) << "system install is " << system_install;

  InstallationState original_state;
  original_state.Initialize();

  InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, original_state);

  VLOG(1) << "is_migrating_to_single is "
          << installer_state.is_migrating_to_single();

  persistent_histogram_storage.set_storage_base_dir(
      installer_state.target_path());

  if (!is_vivaldi) {
  installer::ConfigureCrashReporting(installer_state);
  installer::SetInitialCrashKeys(installer_state);
  installer::SetCrashKeysFromCommandLine(cmd_line);
  }

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(cmd_line);

  // Check to make sure current system is Win7 or later. If not, log
  // error message and get out.
  if (!InstallUtil::IsOSSupported()) {
    LOG(ERROR) << "Chrome only supports Windows 7 or later.";
    installer_state.WriteInstallerResult(
        installer::OS_NOT_SUPPORTED, IDS_INSTALL_OS_NOT_SUPPORTED_BASE, NULL);
    return installer::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded()) {
    installer_state.WriteInstallerResult(
        installer::OS_ERROR, IDS_INSTALL_OS_ERROR_BASE, NULL);
    return installer::OS_ERROR;
  }

  // Make sure system_level is supported if requested. For historical reasons,
  // system-level installs have never been supported for Chrome canary (SxS).
  // This is a brand-specific policy for this particular mode. In general,
  // system-level installation of secondary install modes is fully supported.
  if (!install_static::InstallDetails::Get().supports_system_level() &&
      (system_install ||
       cmd_line.HasSwitch(installer::switches::kSelfDestruct) ||
       cmd_line.HasSwitch(installer::switches::kRemoveChromeRegistration))) {
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }
  // Some switches only apply for modes that can be made the user's default
  // browser.
  if (!install_static::SupportsSetAsDefaultBrowser() &&
      (cmd_line.HasSwitch(installer::switches::kMakeChromeDefault) ||
       cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser))) {
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }
  // Some switches only apply for modes that support retention experiments.
  if (!install_static::SupportsRetentionExperiments() &&
      cmd_line.HasSwitch(installer::switches::kUserExperiment)) {
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }

  // Some command line options are no longer supported and must error out.
  if (installer::ContainsUnsupportedSwitch(cmd_line))
    return installer::UNSUPPORTED_OPTION;

  // A variety of installer operations require the path to the current
  // executable. Get it once here for use throughout these operations. Note that
  // the path service is the authoritative source for this path. One might think
  // that CommandLine::GetProgram would suffice, but it won't since
  // CreateProcess may have been called with a command line that is somewhat
  // ambiguous (e.g., an unquoted path with spaces, or a path lacking the file
  // extension), in which case CommandLineToArgv will not yield an argv with the
  // true path to the program at position 0.
  base::FilePath setup_exe;
  base::PathService::Get(base::FILE_EXE, &setup_exe);

  int exit_code = 0;
  if (HandleNonInstallCmdLineOptions(setup_exe, cmd_line, &original_state,
                                     &installer_state, &exit_code)) {
    return exit_code;
  }

  if (system_install && !IsUserAnAdmin()) {
    if (!cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      base::CommandLine new_cmd(base::CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer::switches::kRunAsAdmin);
      // If system_install became true due to an environment variable, append
      // it to the command line here since env vars may not propagate past the
      // elevation.
      if (!new_cmd.HasSwitch(installer::switches::kSystemLevel))
        new_cmd.AppendSwitch(installer::switches::kSystemLevel);

      // Do not show the Vivaldi installer UI twice.
      // Add the --vivaldi-update flag for the elevated process.
      if (new_cmd.HasSwitch(vivaldi::constants::kVivaldi) &&
         !new_cmd.HasSwitch(vivaldi::constants::kVivaldiUpdate)) {
        new_cmd.AppendSwitch(vivaldi::constants::kVivaldiUpdate);
      }

      if (new_cmd.HasSwitch(vivaldi::constants::kVivaldi) && !is_silent) {
        // Kill off the progress dialog here, since we are being relaunched.
        progress_dlg.FinishProgress(0);
      }

      DWORD exit_code = installer::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exit_code);
      return exit_code;
    } else {
      LOG(ERROR) << "Non admin user can not install system level Chrome.";
      installer_state.WriteInstallerResult(installer::INSUFFICIENT_RIGHTS,
          IDS_INSTALL_INSUFFICIENT_RIGHTS_BASE, NULL);
      return installer::INSUFFICIENT_RIGHTS;
    }
  }

  std::unique_ptr<installer::SetupSingleton> setup_singleton(
      installer::SetupSingleton::Acquire(cmd_line, prefs, &original_state,
                                         &installer_state));
  if (!setup_singleton) {
    installer_state.WriteInstallerResult(
        installer::SETUP_SINGLETON_ACQUISITION_FAILED,
        IDS_INSTALL_SINGLETON_ACQUISITION_FAILED_BASE, nullptr);
    return installer::SETUP_SINGLETON_ACQUISITION_FAILED;
  }

  if (is_vivaldi && !is_uninstall && !is_silent)
    progress_dlg.ShowModeless();

  base::FilePath installer_directory;
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  // If --uninstall option is given, uninstall the identified product(s)
  if (is_uninstall) {
    install_status =
        UninstallProducts(original_state, installer_state, setup_exe, cmd_line);
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    install_status =
        InstallProducts(original_state, setup_exe, cmd_line, prefs,
                        &installer_state, &installer_directory);
    DoLegacyCleanups(installer_state, install_status);

    // It may be time to kick off an experiment if this was a successful update
    // and Chrome was not in use (since the experiment only applies to inactive
    // installs).
    if (install_status == installer::NEW_VERSION_UPDATED &&
        installer::ShouldRunUserExperiment(installer_state)) {
      installer::BeginUserExperiment(
          installer_state, installer_directory.Append(setup_exe.BaseName()),
          !system_install);
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Setup.Install.Result", install_status,
                            installer::MAX_INSTALL_STATUS);

  // Dump peak memory usage.
  PROCESS_MEMORY_COUNTERS pmc;
  if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
    UMA_HISTOGRAM_MEMORY_KB("Setup.Install.PeakPagefileUsage",
                            base::saturated_cast<base::HistogramBase::Sample>(
                                pmc.PeakPagefileUsage / 1024));
    UMA_HISTOGRAM_MEMORY_KB("Setup.Install.PeakWorkingSetSize",
                            base::saturated_cast<base::HistogramBase::Sample>(
                                pmc.PeakWorkingSetSize / 1024));
  }

  int return_code = 0;
  // MSI demands that custom actions always return 0 (ERROR_SUCCESS) or it will
  // rollback the action. If we're uninstalling we want to avoid this, so always
  // report success, squashing any more informative return codes.
  if (!(installer_state.is_msi() && is_uninstall)) {
    // Note that we allow the status installer::UNINSTALL_REQUIRES_REBOOT
    // to pass through, since this is only returned on uninstall which is
    // never invoked directly by Google Update.
    return_code = InstallUtil::GetInstallReturnCode(install_status);
  }

  VLOG(1) << "Installation complete, returning: " << return_code;

  if (is_vivaldi && !is_uninstall && !is_silent) {
    progress_dlg.FinishProgress(return_code == 0 ? 1000 : 0);
  }

  return return_code;
}
