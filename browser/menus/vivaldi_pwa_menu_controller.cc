//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser/menus/vivaldi_pwa_menu_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "browser/vivaldi_runtime_feature.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/gfx/text_elider.h"

constexpr size_t kMaxAppNameLength = 30;

namespace vivaldi {

// Returns the appropriate menu label for the IDC_INSTALL_PWA command if
// available. Copied from app_menu_model.cc
std::u16string GetInstallPWALabel(const Browser* browser) {
  // There may be no active web contents in tests.
  auto* const web_contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return std::u16string();
  }
  if (!web_app::CanCreateWebApp(browser)) {
    return std::u16string();
  }
  // Don't allow apps created from chrome-extension urls.
  if (web_contents->GetLastCommittedURL().SchemeIs("chrome-extension")) {
    return std::u16string();
  }

  // TODO(b/328077967): Support async nature of AppBannerManager pipeline runs
  // with the menu model instead of needing this workaround to verify if an
  // non-installable site is installed.
  const webapps::AppId* app_id =
      web_app::WebAppTabHelper::GetAppId(web_contents);
  web_app::WebAppProvider* const provider =
      web_app::WebAppProvider::GetForLocalAppsUnchecked(browser->profile());
  if (app_id &&
      provider->registrar_unsafe().GetInstallState(*app_id) ==
          web_app::proto::INSTALLED_WITH_OS_INTEGRATION &&
      provider->registrar_unsafe().GetAppUserDisplayMode(*app_id) !=
          web_app::mojom::UserDisplayMode::kBrowser) {
    return std::u16string();
  }

  std::u16string install_page_as_app_label =
      l10n_util::GetStringUTF16(IDS_INSTALL_DIY_TO_OS_LAUNCH_SURFACE);
  webapps::AppBannerManager* banner =
      webapps::AppBannerManager::FromWebContents(web_contents);
  if (!banner) {
    // Showing `Install Page as App` allows the user to refetch the manifest and
    // go through the install flow without relying on the AppBannerManager to
    // finish working.
    return install_page_as_app_label;
  }

  std::optional<webapps::InstallBannerConfig> install_config =
      banner->GetCurrentBannerConfig();
  if (!install_config) {
    // In some edge cases where the `AppBannerManager` pipeline hasn't run yet,
    // the information populated to be used for determining installability and
    // other parameters is not available. In this case, allow users to try
    // installability by refetching the manifest.
    return install_page_as_app_label;
  }
  CHECK_EQ(install_config->mode, webapps::AppBannerMode::kWebApp);
  webapps::InstallableWebAppCheckResult installable =
      banner->GetInstallableWebAppCheckResult();

  switch (installable) {
    case webapps::InstallableWebAppCheckResult::kUnknown:
      // Loading of the menu model is synchronous, so there could be a condition
      // where the `AppBannerManager` has not yet finished the pipeline while
      // the menu item has been triggered. In such a case,
      // `banner->GetInstallableWebAppCheckResult()` returns the default value
      // of `kUnknown`.
      // Show `Install Page as App` for that use-case, since that allows the
      // user to trigger the install flow to verify all the data required for
      // installability. The correct dialog will be shown to the user depending
      // on whether the app turns out to be installable or not.
      return install_page_as_app_label;
    case webapps::InstallableWebAppCheckResult::kNo_AlreadyInstalled:
      // Returning an empty string here allows the `launch page as app` field to
      // get populated in place of the `install` strings.
      return std::u16string();
    case webapps::InstallableWebAppCheckResult::kNo:
      return install_page_as_app_label;
    case webapps::InstallableWebAppCheckResult::kYes_ByUserRequest:
    case webapps::InstallableWebAppCheckResult::kYes_Promotable:
      std::u16string app_name = install_config->GetWebOrNativeAppName();
      if (app_name.empty()) {
        // Prefer showing `Install Page as App` here, as users can set the name
        // of the installed app on the DIY app dialog anyway.
        return install_page_as_app_label;
      }
      return l10n_util::GetStringFUTF16(
          IDS_INSTALL_TO_OS_LAUNCH_SURFACE,
          ui::EscapeMenuLabelAmpersands(app_name));
  }
}

PWAMenuController::PWAMenuController(Browser* browser) : browser_(browser) {}

void PWAMenuController::PopulateModel(ui::SimpleMenuModel* menu_model) {
  menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
  std::optional<webapps::AppId> pwa = web_app::GetWebAppForActiveTab(browser_);
  if (pwa) {
    auto* provider =
        web_app::WebAppProvider::GetForWebApps(browser_->profile());
    menu_model->AddItem(
        IDC_OPEN_IN_PWA_WINDOW,
        l10n_util::GetStringFUTF16(
            IDS_OPEN_IN_APP_WINDOW,
            gfx::TruncateString(
                base::UTF8ToUTF16(provider->registrar_unsafe().GetAppShortName(*pwa)),
                kMaxAppNameLength, gfx::CHARACTER_BREAK)));
  } else {
    std::u16string install_pwa_item_name = GetInstallPWALabel(browser_);
    if (!install_pwa_item_name.empty()) {
      menu_model->AddItem(IDC_INSTALL_PWA, install_pwa_item_name);
    }
  }
  // Always add entry for installing a shortcut.
  menu_model->AddItemWithStringId(IDC_CREATE_SHORTCUT,
                                  IDS_ADD_TO_OS_LAUNCH_SURFACE);
}

bool PWAMenuController::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_INSTALL_PWA;
}

std::u16string PWAMenuController::GetLabelForCommandId(int command_id) const {
  if (command_id == IDC_INSTALL_PWA) {
    return GetInstallPWALabel(browser_);
  } else {
    return std::u16string();
  }
}

bool PWAMenuController::IsCommand(int command_id) const {
  return command_id == IDC_OPEN_IN_PWA_WINDOW ||
         command_id == IDC_INSTALL_PWA || command_id == IDC_CREATE_SHORTCUT;
}

bool PWAMenuController::HandleCommand(int command_id) {
  switch (command_id) {
    case IDC_CREATE_SHORTCUT:
      chrome::CreateDesktopShortcutForActiveWebContents(browser_);
      return true;

    case IDC_INSTALL_PWA:
      web_app::CreateWebAppFromCurrentWebContents(
          browser_,
          web_app::WebAppInstallFlow::kInstallSite);
      return true;

    case IDC_OPEN_IN_PWA_WINDOW:
      web_app::ReparentWebAppForActiveTab(browser_);
      return true;
  }
  return false;
}

}  // namespace vivaldi