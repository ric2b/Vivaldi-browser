// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_manager.h"

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/actions/chrome_actions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_action_callback.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/abseil-cpp/absl/memory/memory.h"
#include "ui/actions/actions.h"
#include "ui/base/ui_base_features.h"

namespace extensions {

namespace {

// The user data key used to store the ExtensionSidePanelManager for a browser.
const char kExtensionSidePanelManagerKey[] = "extension_side_panel_manager";

}  // namespace

ExtensionSidePanelManager::ExtensionSidePanelManager(
    Profile* profile,
    Browser* browser,
    content::WebContents* web_contents,
    SidePanelRegistry* registry,
    bool for_tab)
    : profile_(profile),
      browser_(browser),
      web_contents_(web_contents),
      registry_(registry),
      for_tab_(for_tab) {
  side_panel_registry_observation_.Observe(registry_);
  profile_observation_.Observe(profile);

  InitializeActions();
  RegisterExtensionEntries();
}

ExtensionSidePanelManager::~ExtensionSidePanelManager() = default;

// static
void ExtensionSidePanelManager::CreateForBrowser(
    Browser* browser,
    SidePanelRegistry* window_registry) {
  // Use absl::WrapUnique(new ExtensionSidePanelManager(...)) instead of
  // std::make_unique<ExtensionSidePanelManager> to access a private
  // constructor.
  auto manager = absl::WrapUnique(new ExtensionSidePanelManager(
      browser->profile(), browser, /*web_contents=*/nullptr, window_registry,
      /*for_tab=*/false));
  browser->SetUserData(kExtensionSidePanelManagerKey, std::move(manager));
}

// static
ExtensionSidePanelManager* ExtensionSidePanelManager::GetForBrowserForTesting(
    Browser* browser) {
  return static_cast<ExtensionSidePanelManager*>(
      browser->GetUserData(kExtensionSidePanelManagerKey));
}

// static
void ExtensionSidePanelManager::CreateForTab(Profile* profile,
                                             content::WebContents* web_contents,
                                             SidePanelRegistry* tab_registry) {
  DCHECK(web_contents);

  // Use absl::WrapUnique(new ExtensionSidePanelManager(...)) instead of
  // std::make_unique<ExtensionSidePanelManager> to access a private
  // constructor.
  auto manager = absl::WrapUnique(
      new ExtensionSidePanelManager(profile, /*browser=*/nullptr, web_contents,
                                    tab_registry, /*for_tab=*/true));
  web_contents->SetUserData(kExtensionSidePanelManagerKey, std::move(manager));
}

// static
ExtensionSidePanelManager* ExtensionSidePanelManager::GetForTabForTesting(
    content::WebContents* web_contents) {
  return static_cast<ExtensionSidePanelManager*>(
      web_contents->GetUserData(kExtensionSidePanelManagerKey));
}

ExtensionSidePanelCoordinator*
ExtensionSidePanelManager::GetExtensionCoordinatorForTesting(
    const ExtensionId& extension_id) {
  auto it = coordinators_.find(extension_id);
  return (it == coordinators_.end()) ? nullptr : it->second.get();
}

void ExtensionSidePanelManager::RegisterExtensionEntries() {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  extension_registry_observation_.Observe(extension_registry);

  for (const auto& extension : extension_registry->enabled_extensions()) {
    MaybeCreateExtensionSidePanelCoordinator(extension.get());
  }
}

void ExtensionSidePanelManager::InitializeActions() {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(profile_);
  for (const auto& extension : extension_registry->enabled_extensions()) {
    MaybeCreateActionItemForExtension(extension.get());
  }
}

void ExtensionSidePanelManager::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  MaybeCreateActionItemForExtension(extension);
  MaybeCreateExtensionSidePanelCoordinator(extension);
}

void ExtensionSidePanelManager::MaybeCreateActionItemForExtension(
    const Extension* extension) {
  if (!browser_ || !extension->permissions_data()->HasAPIPermission(
                       mojom::APIPermissionID::kSidePanel)) {
    return;
  }

  actions::ActionId extension_action_id =
      GetOrCreateActionIdForExtension(extension);
  BrowserActions* browser_actions = browser_->browser_actions();
  actions::ActionItem* extension_action_item =
      actions::ActionManager::Get().FindAction(
          extension_action_id, browser_actions->root_action_item());

  // Mark the action item as pinnable if it already exists.
  if (extension_action_item) {
    return;
  }

  // Create a new action item.
  actions::ActionItem* root_action_item = browser_actions->root_action_item();
  root_action_item->AddChild(
      actions::ActionItem::Builder(
          CreateToggleSidePanelActionCallback(
              SidePanelEntry::Key(SidePanelEntry::Id::kExtension,
                                  extension->id()),
              browser_))
          .SetText(base::UTF8ToUTF16(extension->short_name()))
          .SetActionId(extension_action_id)
          .SetProperty(actions::kActionItemPinnableKey, true)
          .Build());
}

actions::ActionId ExtensionSidePanelManager::GetOrCreateActionIdForExtension(
    const Extension* extension) {
  return actions::ActionIdMap::CreateActionId(
             SidePanelEntry::Key(SidePanelEntry::Id::kExtension,
                                 extension->id())
                 .ToString())
      .first;
}

void ExtensionSidePanelManager::MaybeRemoveActionItemForExtension(
    const Extension* extension) {
  if (browser_ && extension->permissions_data()->HasAPIPermission(
                      mojom::APIPermissionID::kSidePanel)) {
    BrowserActions* browser_actions = browser_->browser_actions();
    std::optional<actions::ActionId> extension_action_id =
        actions::ActionIdMap::StringToActionId(
            SidePanelEntry::Key(SidePanelEntry::Id::kExtension, extension->id())
                .ToString());
    CHECK(extension_action_id.has_value());
    actions::ActionItem* actionItem = actions::ActionManager::Get().FindAction(
        extension_action_id.value(), browser_actions->root_action_item());

    if (actionItem) {
      browser_actions->root_action_item()->RemoveChild(actionItem).reset();
    }
  }
}

void ExtensionSidePanelManager::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  auto it = coordinators_.find(extension->id());
  if (it != coordinators_.end()) {
    it->second->DeregisterEntry();
    coordinators_.erase(extension->id());
  }
  MaybeRemoveActionItemForExtension(extension);
}

void ExtensionSidePanelManager::OnRegistryDestroying(
    SidePanelRegistry* registry) {
  coordinators_.clear();
  side_panel_registry_observation_.Reset();
  registry_ = nullptr;
}

void ExtensionSidePanelManager::WillDiscard() {
  for (auto& it : coordinators_) {
    it.second->DeregisterEntry();
  }
}

void ExtensionSidePanelManager::OnProfileWillBeDestroyed(Profile* profile) {
  // Destroy all coordinators, since no functionality should remain once there's
  // no profile.
  coordinators_.clear();

  CHECK_EQ(profile_, profile);
  profile_observation_.Reset();
  profile_ = nullptr;
}

void ExtensionSidePanelManager::MaybeCreateExtensionSidePanelCoordinator(
    const Extension* extension) {
  if (extension->permissions_data()->HasAPIPermission(
          mojom::APIPermissionID::kSidePanel)) {
    coordinators_.emplace(
        extension->id(),
        std::make_unique<ExtensionSidePanelCoordinator>(
            profile_, browser_, web_contents_, extension, registry_, for_tab_));
  }
}

}  // namespace extensions
