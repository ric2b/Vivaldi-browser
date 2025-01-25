// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/accessibility/embedded_a11y_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller_utils.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_tab_helper.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/accessibility/reading/distillable_pages.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

base::TimeDelta kDelaySeconds = base::Seconds(2);

}  // namespace

ReadAnythingCoordinator::ReadAnythingCoordinator(Browser* browser)
    : BrowserUserData<ReadAnythingCoordinator>(*browser),
      delay_timer_(FROM_HERE,
                   kDelaySeconds,
                   base::BindRepeating(
                       &ReadAnythingCoordinator::OnTabChangeDelayComplete,
                       base::Unretained(this))) {
  browser->tab_strip_model()->AddObserver(this);
  Observe(GetActiveWebContents());
  if (features::IsReadAnythingLocalSidePanelEnabled()) {
    CreateAndRegisterEntriesForExistingWebContents(browser->tab_strip_model());
  }

  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    BrowserList::GetInstance()->AddObserver(this);
  }

  if (features::IsReadAnythingDocsIntegrationEnabled()) {
    EmbeddedA11yExtensionLoader::GetInstance()->Init();
  }
}

ReadAnythingCoordinator::~ReadAnythingCoordinator() {
  local_side_panel_switch_delay_timer_.Stop();

  if (features::IsReadAnythingDocsIntegrationEnabled()) {
    RemoveGDocsHelperExtension();
  }

  // Inform observers when |this| is destroyed so they can do their own cleanup.
  for (Observer& obs : observers_) {
    obs.OnCoordinatorDestroyed();
  }

  // Deregister Read Anything from the global side panel registry. This removes
  // Read Anything as a side panel entry observer.
  Browser* browser = &GetBrowser();

  // Deregisters the Read Anything side panel if it is not local. When a side
  // panel entry is global, it has the same lifetime as the browser.
  if (!features::IsReadAnythingLocalSidePanelEnabled()) {
    // TODO(dcheng): The SidePanelRegistry is *also* a BrowserUserData. During
    // Browser destruction, no other BrowserUserData instances are available, so
    // this may be null. In general, this is a bit of a code smell, and the code
    // should be refactored to avoid this situation.
    SidePanelRegistry* global_registry =
        SidePanelCoordinator::GetGlobalSidePanelRegistry(browser);
    if (global_registry) {
      global_registry->Deregister(
          SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
    }
  }

  if (features::IsDataCollectionModeForScreen2xEnabled()) {
    BrowserList::GetInstance()->RemoveObserver(this);
  }
  browser->tab_strip_model()->RemoveObserver(this);
  Observe(nullptr);
}

void ReadAnythingCoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      base::BindRepeating(&ReadAnythingCoordinator::CreateContainerView,
                          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  global_registry->Register(std::move(side_panel_entry));
}

void ReadAnythingCoordinator::CreateAndRegisterEntriesForExistingWebContents(
    TabStripModel* tab_strip_model) {
  for (int index = 0; index < tab_strip_model->GetTabCount(); ++index) {
    CreateAndRegisterEntryForWebContents(
        tab_strip_model->GetWebContentsAt(index));
  }
}

void ReadAnythingCoordinator::CreateAndRegisterEntryForWebContents(
    content::WebContents* web_contents) {
  CHECK(web_contents);
  ReadAnythingTabHelper* tab_helper =
      ReadAnythingTabHelper::FromWebContents(web_contents);
  CHECK(tab_helper);
  tab_helper->CreateAndRegisterEntry();
}

void ReadAnythingCoordinator::AddObserver(
    ReadAnythingCoordinator::Observer* observer) {
  observers_.AddObserver(observer);
}

void ReadAnythingCoordinator::RemoveObserver(
    ReadAnythingCoordinator::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingCoordinator::OnEntryShown(SidePanelEntry* entry) {
  DCHECK(entry->key().id() == SidePanelEntry::Id::kReadAnything);
  OnReadAnythingSidePanelEntryShown();
}

void ReadAnythingCoordinator::OnEntryHidden(SidePanelEntry* entry) {
  DCHECK(entry->key().id() == SidePanelEntry::Id::kReadAnything);
  OnReadAnythingSidePanelEntryHidden();
}

void ReadAnythingCoordinator::OnReadAnythingSidePanelEntryShown() {
  for (Observer& obs : observers_) {
    obs.Activate(true);
  }

  if (!features::IsReadAnythingDocsIntegrationEnabled()) {
    return;
  }

  if (!features::IsReadAnythingLocalSidePanelEnabled()) {
    InstallGDocsHelperExtension();
    return;
  }

  active_local_side_panel_count_++;
  InstallGDocsHelperExtension();
}

void ReadAnythingCoordinator::OnReadAnythingSidePanelEntryHidden() {
  for (Observer& obs : observers_) {
    obs.Activate(false);
  }

  if (!features::IsReadAnythingDocsIntegrationEnabled()) {
    return;
  }

  if (!features::IsReadAnythingLocalSidePanelEnabled()) {
    RemoveGDocsHelperExtension();
    return;
  }

  active_local_side_panel_count_--;
  local_side_panel_switch_delay_timer_.Stop();
  local_side_panel_switch_delay_timer_.Start(
      FROM_HERE, base::Seconds(30),
      base::BindRepeating(
          &ReadAnythingCoordinator::OnLocalSidePanelSwitchDelayTimeout,
          weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<views::View> ReadAnythingCoordinator::CreateContainerView() {
  Browser* browser = &GetBrowser();
  auto web_view =
      std::make_unique<ReadAnythingSidePanelWebView>(browser->profile());

  return std::move(web_view);
}

void ReadAnythingCoordinator::StartPageChangeDelay() {
  // Reset the delay status.
  post_tab_change_delay_complete_ = false;
  // Cancel any existing page change delay and start again.
  delay_timer_.Reset();
}

void ReadAnythingCoordinator::OnTabChangeDelayComplete() {
  CHECK(!post_tab_change_delay_complete_);
  post_tab_change_delay_complete_ = true;
  auto* web_contents = GetActiveWebContents();
  // Web contents should be checked before starting the delay, and the timer
  // will be canceled if the user navigates or leaves the tab.
  CHECK(web_contents);
  if (!web_contents->IsLoading()) {
    // Ability to show was already checked before timer was started.
    ActivePageDistillable();
  }
}

void ReadAnythingCoordinator::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // If the Read Anything side panel is local, creates and registers a side
  // panel entry for each tab.
  if (features::IsReadAnythingLocalSidePanelEnabled()) {
    if (change.type() == TabStripModelChange::Type::kInserted) {
      for (const auto& inserted_tab : change.GetInsert()->contents) {
        CreateAndRegisterEntryForWebContents(inserted_tab.contents);
      }
    }
    if (change.type() == TabStripModelChange::Type::kReplaced) {
      content::WebContents* new_contents = change.GetReplace()->new_contents;
      if (new_contents) {
        CreateAndRegisterEntryForWebContents(new_contents);
      }
    }
  }
  if (!selection.active_tab_changed()) {
    return;
  }
  Observe(GetActiveWebContents());
  if (IsActivePageDistillable()) {
    StartPageChangeDelay();
  } else {
    ActivePageNotDistillable();
  }
}

void ReadAnythingCoordinator::DidStopLoading() {
  if (!post_tab_change_delay_complete_) {
    return;
  }
  if (IsActivePageDistillable()) {
    ActivePageDistillable();
  } else {
    ActivePageNotDistillable();
  }
}

void ReadAnythingCoordinator::PrimaryPageChanged(content::Page& page) {
  // On navigation, cancel any running delays.
  delay_timer_.Stop();

  if (!IsActivePageDistillable()) {
    // On navigation, if we shouldn't show the IPH hide it. Otherwise continue
    // to show it.
    ActivePageNotDistillable();
  }
}

content::WebContents* ReadAnythingCoordinator::GetActiveWebContents() const {
  return GetBrowser().tab_strip_model()->GetActiveWebContents();
}

bool ReadAnythingCoordinator::IsActivePageDistillable() const {
  auto* web_contents = GetActiveWebContents();
  if (!web_contents) {
    return false;
  }

  auto url = web_contents->GetLastCommittedURL();

  for (const std::string& distillable_domain : a11y::GetDistillableDomains()) {
    // If the url's domain is found in distillable domains AND the url has a
    // filename (i.e. it is not a home page or sub-home page), show the promo.
    if (url.DomainIs(distillable_domain) && !url.ExtractFileName().empty()) {
      return true;
    }
  }
  return false;
}

void ReadAnythingCoordinator::ActivePageNotDistillable() {
  GetBrowser().window()->CloseFeaturePromo(
      feature_engagement::kIPHReadingModeSidePanelFeature);
  for (Observer& obs : observers_) {
    obs.OnActivePageDistillable(false);
  }
}

void ReadAnythingCoordinator::ActivePageDistillable() {
  GetBrowser().window()->MaybeShowFeaturePromo(
      feature_engagement::kIPHReadingModeSidePanelFeature);
  for (Observer& obs : observers_) {
    obs.OnActivePageDistillable(true);
  }
}

void ReadAnythingCoordinator::InstallGDocsHelperExtension() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EmbeddedA11yManagerLacros::GetInstance()->SetReadingModeEnabled(true);
#else
  EmbeddedA11yExtensionLoader::GetInstance()->InstallExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId,
      extension_misc::kReadingModeGDocsHelperExtensionPath,
      extension_misc::kReadingModeGDocsHelperManifestFilename,
      /*should_localize=*/false);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void ReadAnythingCoordinator::RemoveGDocsHelperExtension() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  EmbeddedA11yManagerLacros::GetInstance()->SetReadingModeEnabled(false);
#else
  EmbeddedA11yExtensionLoader::GetInstance()->RemoveExtensionWithId(
      extension_misc::kReadingModeGDocsHelperExtensionId);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void ReadAnythingCoordinator::ActivePageNotDistillableForTesting() {
  ActivePageNotDistillable();
}

void ReadAnythingCoordinator::ActivePageDistillableForTesting() {
  ActivePageDistillable();
}

void ReadAnythingCoordinator::OnBrowserSetLastActive(Browser* browser) {
  if (!features::IsDataCollectionModeForScreen2xEnabled() ||
      browser != &GetBrowser()) {
    return;
  }
  // This code is called as part of a screen2x data generation workflow, where
  // the browser is opened by a CLI and the read-anything side panel is
  // automatically opened. Therefore we force the UI to show right away, as in
  // tests.
  auto* side_panel_ui = browser->GetFeatures().side_panel_ui();
  if (side_panel_ui->GetCurrentEntryId() != SidePanelEntryId::kReadAnything) {
    side_panel_ui->SetNoDelaysForTesting(true);  // IN-TEST
    side_panel_ui->Show(SidePanelEntryId::kReadAnything);
  }
}

void ReadAnythingCoordinator::OnLocalSidePanelSwitchDelayTimeout() {
  if (active_local_side_panel_count_ > 0) {
    return;
  }

  RemoveGDocsHelperExtension();
}

BROWSER_USER_DATA_KEY_IMPL(ReadAnythingCoordinator);
