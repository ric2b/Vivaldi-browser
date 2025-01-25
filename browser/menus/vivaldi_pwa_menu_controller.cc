//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#include "browser/menus/vivaldi_pwa_menu_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "browser/vivaldi_runtime_feature.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/web_applications/web_app_dialog_utils.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/gfx/text_elider.h"

constexpr size_t kMaxAppNameLength = 30;

namespace vivaldi {

// Returns the appropriate menu label for the IDC_INSTALL_PWA command if
// available.
// Copied from app_menu_model.cc.
std::optional<std::u16string> GetInstallPWAAppMenuItemName(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return std::nullopt;
  std::u16string app_name =
      webapps::AppBannerManager::GetInstallableWebAppName(web_contents);
  if (app_name.empty())
    return std::nullopt;
  return l10n_util::GetStringFUTF16(IDS_INSTALL_TO_OS_LAUNCH_SURFACE, app_name);
}

PWAMenuController::PWAMenuController(Browser* browser) : browser_(browser) {}

void PWAMenuController::PopulateModel(ui::SimpleMenuModel* menu_model) {
  std::optional<webapps::AppId> pwa = web_app::GetWebAppForActiveTab(browser_);
  if (pwa) {
    auto* provider =
        web_app::WebAppProvider::GetForWebApps(browser_->profile());
    menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    menu_model->AddItem(
        IDC_OPEN_IN_PWA_WINDOW,
        l10n_util::GetStringFUTF16(
            IDS_OPEN_IN_APP_WINDOW,
            gfx::TruncateString(
                base::UTF8ToUTF16(provider->registrar_unsafe().GetAppShortName(*pwa)),
                kMaxAppNameLength, gfx::CHARACTER_BREAK)));
  } else {
    std::optional<std::u16string> install_pwa_item_name =
        GetInstallPWAAppMenuItemName(browser_);
    if (install_pwa_item_name) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      menu_model->AddItem(IDC_INSTALL_PWA, *install_pwa_item_name);
    }
    menu_model->AddItemWithStringId(IDC_CREATE_SHORTCUT,
                                    IDS_ADD_TO_OS_LAUNCH_SURFACE);
  }
}

bool PWAMenuController::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_INSTALL_PWA;
}

std::u16string PWAMenuController::GetLabelForCommandId(int command_id) const {
  if (command_id == IDC_INSTALL_PWA) {
    return GetInstallPWAAppMenuItemName(browser_).value();
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
      web_app::CreateWebAppFromCurrentWebContents(
          browser_,
          web_app::WebAppInstallFlow::kCreateShortcut);
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