// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/mv2_experiment_stage.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/commerce/product_specifications_entry_point_controller.h"
#include "chrome/browser/ui/extensions/mv2_disabled_dialog_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_declutter_controller.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/session_service_tab_group_sync_observer.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_features.h"
#include "chrome/browser/ui/toasts/toast_service.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_coordinator.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/lens/lens_features.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/saved_tab_groups/features.h"

#include "app/vivaldi_apptools.h"
#include "ui/vivaldi_side_panel_coordinator.h"

namespace {

// This is the generic entry point for test code to stub out browser window
// functionality. It is called by production code, but only used by tests.
BrowserWindowFeatures::BrowserWindowFeaturesFactory& GetFactory() {
  static base::NoDestructor<BrowserWindowFeatures::BrowserWindowFeaturesFactory>
      factory;
  return *factory;
}

}  // namespace

// static
std::unique_ptr<BrowserWindowFeatures>
BrowserWindowFeatures::CreateBrowserWindowFeatures() {
  if (GetFactory()) {
    CHECK_IS_TEST();
    return GetFactory().Run();
  }
  // Constructor is protected.
  return base::WrapUnique(new BrowserWindowFeatures());
}

BrowserWindowFeatures::~BrowserWindowFeatures() = default;

// static
void BrowserWindowFeatures::ReplaceBrowserWindowFeaturesForTesting(
    BrowserWindowFeaturesFactory factory) {
  BrowserWindowFeatures::BrowserWindowFeaturesFactory& f = GetFactory();
  f = std::move(factory);
}

void BrowserWindowFeatures::Init(BrowserWindowInterface* browser) {
  // Avoid passing `browser` directly to features. Instead, pass the minimum
  // necessary state or controllers necessary.
  // Ping erikchen for assistance. This comment will be deleted after there are
  // 10+ features.
  //
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL) {
    product_specifications_entry_point_controller_ =
        std::make_unique<commerce::ProductSpecificationsEntryPointController>(
            browser);

    if (browser->GetProfile()->IsRegularProfile() &&
        tab_groups::IsTabGroupsSaveV2Enabled() &&
        browser->GetTabStripModel()->SupportsTabGroups()) {
      session_service_tab_group_sync_observer_ =
          std::make_unique<tab_groups::SessionServiceTabGroupSyncObserver>(
              browser->GetProfile(), browser->GetTabStripModel(),
              browser->GetSessionID());
    }

    if (features::IsTabstripDeclutterEnabled() &&
        browser->GetProfile()->IsRegularProfile()) {
      tab_declutter_controller_ =
          std::make_unique<tabs::TabDeclutterController>(
              browser->GetTabStripModel());
    }

    if (vivaldi::IsVivaldiRunning()) {
      CHECK(!side_panel_coordinator_);
      vivaldi_side_panel_coordinator_ =
        std::make_unique<vivaldi::SidePanelCoordinator>(browser);
    } // End Vivaldi
  }

  // The LensOverlayEntryPointController is constructed for all browser types
  // but is only initialized for normal browser windows. This simplifies the
  // logic for code shared by both normal and non-normal windows.
  lens_overlay_entry_point_controller_ =
      std::make_unique<lens::LensOverlayEntryPointController>();

  tab_strip_model_ = browser->GetTabStripModel();
}

void BrowserWindowFeatures::InitPostWindowConstruction(Browser* browser) {
  // Features that are only enabled for normal browser windows (e.g. a window
  // with an omnibox and a tab strip). By default most features should be
  // instantiated in this block.
  if (browser->is_type_normal()) {
    if (IsChromeLabsEnabled()) {
      chrome_labs_coordinator_ =
          std::make_unique<ChromeLabsCoordinator>(browser);
    }

    // TODO(b/350508658): Ideally, we don't pass in a reference to browser as
    // per the guidance in the comment above. However, currently, we need
    // browser to properly determine if the lens overlay is enabled.
    // Cannot be in Init since needs to listen to the fullscreen controller
    // which is initialized after Init.
    if (lens::features::IsLensOverlayEnabled()) {
      lens_overlay_entry_point_controller_->Initialize(
          browser, browser->command_controller());
    }

    auto* experiment_manager =
        extensions::ManifestV2ExperimentManager::Get(browser->profile());
    if (experiment_manager &&
        experiment_manager->GetCurrentExperimentStage() ==
            extensions::MV2ExperimentStage::kDisableWithReEnable) {
      mv2_disabled_dialog_controller_ =
          std::make_unique<extensions::Mv2DisabledDialogController>(browser);
    }

    if (base::FeatureList::IsEnabled(toast_features::kToastFramework)) {
      toast_service_ = std::make_unique<ToastService>(browser);
    }
  }
}

void BrowserWindowFeatures::InitPostBrowserViewConstruction(
    BrowserView* browser_view) {
  // TODO(crbug.com/346148093): Move SidePanelCoordinator construction to Init.
  // TODO(crbug.com/346148554): Do not create a SidePanelCoordinator for most
  // browser.h types
  // Conceptually, SidePanelCoordinator handles the "model" whereas
  // BrowserView::unified_side_panel_ handles the "ui". When we stop making this
  // for most browser.h types, we should also stop making the
  // unified_side_panel_.
  side_panel_coordinator_ =
      std::make_unique<SidePanelCoordinator>(browser_view);
}

void BrowserWindowFeatures::TearDownPreBrowserViewDestruction() {
  // TODO(crbug.com/346148093): This logic should not be gated behind a
  // conditional.
  if (side_panel_coordinator_) {
    side_panel_coordinator_->TearDownPreBrowserViewDestruction();
  }

  if (mv2_disabled_dialog_controller_) {
    mv2_disabled_dialog_controller_->TearDown();
  }
}

SidePanelUI* BrowserWindowFeatures::side_panel_ui() {
  // This is only set for normal browser windows.
  if (vivaldi_side_panel_coordinator_) {
    return vivaldi_side_panel_coordinator_.get();
  }
  return side_panel_coordinator_.get();
}

ToastController* BrowserWindowFeatures::toast_controller() {
  return toast_service_ ? toast_service_->toast_controller() : nullptr;
}

BrowserWindowFeatures::BrowserWindowFeatures() = default;
