// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a class that contains various method related to branding.
// It provides only default implementations of these methods. Usually to add
// specific branding, we will need to extend this class with a custom
// implementation.

#include "chrome/installer/util/browser_distribution.h"

#include <utility>

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_icon_resources_win.h"
#include "chrome/common/env_vars.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/google_chrome_sxs_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/non_updating_app_registration_data.h"

#include "chrome/installer/util/util_constants.h"

#include "installer/util/vivaldi_install_util.h"

using installer::MasterPreferences;

namespace {

const wchar_t kChromiumActiveSetupGuid[] =
    L"{9C142C0C-124C-4467-B117-EBCC62801D7B}";

const wchar_t kCommandExecuteImplUuid[] =
    L"{DAB968E0-3A13-4CCC-A3AF-85578ACBE9AB}";

// The BrowserDistribution objects are never freed.
BrowserDistribution* g_browser_distribution = NULL;

}  // namespace

BrowserDistribution::BrowserDistribution()
    : app_reg_data_(base::MakeUnique<NonUpdatingAppRegistrationData>(
#if defined(VIVALDI_BUILD)
          L"Software\\Vivaldi"
#else
          L"Software\\Chromium"
#endif
          )) {}

BrowserDistribution::BrowserDistribution(
    std::unique_ptr<AppRegistrationData> app_reg_data)
    : app_reg_data_(std::move(app_reg_data)) {}

BrowserDistribution::~BrowserDistribution() {}

template<class DistributionClass>
BrowserDistribution* BrowserDistribution::GetOrCreateBrowserDistribution(
    BrowserDistribution** dist) {
  if (!*dist) {
    DistributionClass* temp = new DistributionClass();
    if (base::subtle::NoBarrier_CompareAndSwap(
            reinterpret_cast<base::subtle::AtomicWord*>(dist), NULL,
            reinterpret_cast<base::subtle::AtomicWord>(temp)) != NULL)
      delete temp;
  }

  return *dist;
}

// static
BrowserDistribution* BrowserDistribution::GetDistribution() {
  BrowserDistribution* dist = NULL;

#if defined(GOOGLE_CHROME_BUILD)
  if (InstallUtil::IsChromeSxSProcess()) {
    dist = GetOrCreateBrowserDistribution<GoogleChromeSxSDistribution>(
        &g_browser_distribution);
  } else {
    dist = GetOrCreateBrowserDistribution<GoogleChromeDistribution>(
        &g_browser_distribution);
  }
#else
  dist = GetOrCreateBrowserDistribution<BrowserDistribution>(
      &g_browser_distribution);
#endif

  return dist;
}

const AppRegistrationData& BrowserDistribution::GetAppRegistrationData() const {
  return *app_reg_data_;
}

base::string16 BrowserDistribution::GetAppGuid() const {
  return app_reg_data_->GetAppGuid();
}

base::string16 BrowserDistribution::GetStateKey() const {
  return app_reg_data_->GetStateKey();
}

base::string16 BrowserDistribution::GetStateMediumKey() const {
  return app_reg_data_->GetStateMediumKey();
}

base::string16 BrowserDistribution::GetVersionKey() const {
  return app_reg_data_->GetVersionKey();
}

void BrowserDistribution::DoPostUninstallOperations(
    const base::Version& version, const base::FilePath& local_data_path,
    const base::string16& distribution_data) {
#if defined(VIVALDI_BUILD)
  vivaldi::DoPostUninstallOperations(version);
#endif
}

base::string16 BrowserDistribution::GetActiveSetupGuid() {
  return kChromiumActiveSetupGuid;
}

base::string16 BrowserDistribution::GetBaseAppName() {
  return L"Vivaldi";
}

base::string16 BrowserDistribution::GetDisplayName() {
  return GetShortcutName();
}

base::string16 BrowserDistribution::GetShortcutName() {
  return GetBaseAppName();
}

int BrowserDistribution::GetIconIndex() {
  return icon_resources::kApplicationIndex;
}

base::string16 BrowserDistribution::GetIconFilename() {
  return installer::kChromeExe;
}

base::string16 BrowserDistribution::GetStartMenuShortcutSubfolder(
    Subfolder subfolder_type) {
  switch (subfolder_type) {
    case SUBFOLDER_APPS:
      return installer::GetLocalizedString(IDS_APP_SHORTCUTS_SUBDIR_NAME_BASE);
    default:
      DCHECK_EQ(SUBFOLDER_CHROME, subfolder_type);
      return GetShortcutName();
  }
}

base::string16 BrowserDistribution::GetBaseAppId() {
  return L"Vivaldi";
}

base::string16 BrowserDistribution::GetBrowserProgIdPrefix() {
  // This used to be "ChromiumHTML", but was forced to become "ChromiumHTM"
  // because of http://crbug.com/153349.  See the declaration of this function
  // in the header file for more details.
  return L"VivaldiHTM";
}

base::string16 BrowserDistribution::GetBrowserProgIdDesc() {
  return L"Vivaldi HTML Document";
}


base::string16 BrowserDistribution::GetInstallSubDir() {
  return L"Vivaldi";
}

base::string16 BrowserDistribution::GetPublisherName() {
  return L"Vivaldi";
}

base::string16 BrowserDistribution::GetAppDescription() {
  return L"Browse the web";
}

// Vivaldi customization. For standalone installs, add the --user-data-dir argument.
base::string16 BrowserDistribution::GetArguments()
{
  base::string16 arguments;
  const base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(installer::switches::kVivaldiStandalone) && command_line.HasSwitch(installer::switches::kVivaldiInstallDir)) {
    base::FilePath install_path = command_line.GetSwitchValuePath(installer::switches::kVivaldiInstallDir).Append(L"User Data");
    arguments.assign(L"--user-data-dir=").append(install_path.value());
  }
  return arguments;
}

base::string16 BrowserDistribution::GetLongAppDescription() {
  const base::string16& app_description =
      installer::GetLocalizedString(IDS_PRODUCT_DESCRIPTION_BASE);
  return app_description;
}

std::string BrowserDistribution::GetSafeBrowsingName() {
  return "vivaldi";
}

base::string16 BrowserDistribution::GetDistributionData(HKEY root_key) {
  return L"";
}

base::string16 BrowserDistribution::GetRegistryPath() {
  return base::string16(L"Software\\").append(GetInstallSubDir());
}

base::string16 BrowserDistribution::GetUninstallRegPath() {
  return L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Vivaldi";
}

BrowserDistribution::DefaultBrowserControlPolicy
    BrowserDistribution::GetDefaultBrowserControlPolicy() {
  return DEFAULT_BROWSER_FULL_CONTROL;
}

bool BrowserDistribution::CanCreateDesktopShortcuts() {
  return true;
}

bool BrowserDistribution::GetChromeChannel(base::string16* channel) {
  return false;
}

base::string16 BrowserDistribution::GetCommandExecuteImplClsid() {
  return kCommandExecuteImplUuid;
}

void BrowserDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
}

bool BrowserDistribution::ShouldSetExperimentLabels() {
  return false;
}

bool BrowserDistribution::HasUserExperiments() {
  return false;
}
