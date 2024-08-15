//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "browser/menus/vivaldi_pwa_link_menu_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "browser/menus/vivaldi_render_view_context_menu.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace vivaldi {

PWALinkMenuController::PWALinkMenuController(
    VivaldiRenderViewContextMenu* rv_context_menu,
    Profile* profile)
    : rv_context_menu_(rv_context_menu), profile_(profile) {}

PWALinkMenuController::~PWALinkMenuController() = default;

// Content in this function is taken from
// RenderViewContextMenu::AppendOpenInWebAppLinkItems
void PWALinkMenuController::Populate(Browser* browser,
                                     std::u16string label,
                                     ui::SimpleMenuModel* menu_model) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_))
    return;

  std::optional<webapps::AppId> app_id =
      web_app::FindInstalledAppWithUrlInScope(profile_,
                                              rv_context_menu_->GetLinkUrl());
  if (!app_id)
    return;

  int open_in_app_string_id;
  if (browser && browser->app_name() ==
                     web_app::GenerateApplicationNameFromAppId(*app_id)) {
    open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP_SAMEAPP;
  } else {
    open_in_app_string_id = IDS_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP;
  }

  auto* const provider = web_app::WebAppProvider::GetForWebApps(profile_);
  menu_model->AddItem(
      IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP,
      l10n_util::GetStringFUTF16(
          open_in_app_string_id,
          base::UTF8ToUTF16(provider->registrar_unsafe().GetAppShortName(*app_id))));

  gfx::Image icon = gfx::Image::CreateFrom1xBitmap(
      provider->icon_manager().GetFavicon(*app_id));
  menu_model->SetIcon(menu_model->GetItemCount() - 1,
                      ui::ImageModel::FromImage(icon));
}

}  // namespace vivaldi
