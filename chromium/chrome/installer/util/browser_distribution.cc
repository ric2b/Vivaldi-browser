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
#include "base/logging.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/google_chrome_distribution.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/non_updating_app_registration_data.h"

#include "base/command_line.h"
#include "chrome/installer/util/util_constants.h"
#include "installer/util/vivaldi_install_util.h"

namespace {

// The BrowserDistribution object is never freed.
BrowserDistribution* g_browser_distribution = NULL;

}  // namespace

BrowserDistribution::BrowserDistribution()
    : app_reg_data_(std::make_unique<NonUpdatingAppRegistrationData>(
#if defined(VIVALDI_BUILD)
          L"Software\\Vivaldi"
#else
          L"Software\\Chromium"
#endif
          )) {}

BrowserDistribution::BrowserDistribution(
    std::unique_ptr<AppRegistrationData> app_reg_data)
    : app_reg_data_(std::move(app_reg_data)) {}

BrowserDistribution::~BrowserDistribution() = default;

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
  dist = GetOrCreateBrowserDistribution<GoogleChromeDistribution>(
      &g_browser_distribution);
#else
  dist = GetOrCreateBrowserDistribution<BrowserDistribution>(
      &g_browser_distribution);
#endif

  return dist;
}

const AppRegistrationData& BrowserDistribution::GetAppRegistrationData() const {
  return *app_reg_data_;
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

base::string16 BrowserDistribution::GetDisplayName() {
  return GetShortcutName();
}

base::string16 BrowserDistribution::GetShortcutName() {
  // IDS_PRODUCT_NAME is automatically mapped to the mode-specific shortcut
  // name.
  return installer::GetLocalizedString(IDS_PRODUCT_NAME_BASE);
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

base::string16 BrowserDistribution::GetPublisherName() {
  return L"Vivaldi";
}

base::string16 BrowserDistribution::GetAppDescription() {
  return L"Browse the web";
}

// Vivaldi customization. For standalone installs,
// add the --user-data-dir argument.
base::string16 BrowserDistribution::GetArguments()
{
  base::string16 arguments;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(vivaldi::constants::kVivaldiStandalone) &&
      command_line.HasSwitch(vivaldi::constants::kVivaldiInstallDir)) {
    base::FilePath install_path = command_line.GetSwitchValuePath(
        vivaldi::constants::kVivaldiInstallDir).Append(L"User Data");
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

void BrowserDistribution::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type,
    installer::InstallStatus install_status) {
}
