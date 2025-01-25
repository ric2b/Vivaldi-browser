#include "ui/vivaldi_side_panel_coordinator.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "extensions/schema/browser_action_utilities.h"
#include "extensions/tools/vivaldi_tools.h"

using namespace extensions;

namespace vivaldi {

SidePanelCoordinator::~SidePanelCoordinator() {
  Profile* p = GetProfile();
  DCHECK(p);
  if (!p)
    return;
  SidePanelService* side_panel_service = extensions::SidePanelService::Get(p);
  DCHECK(side_panel_service);
  if (side_panel_service) {
    side_panel_service->RemoveObserver(this);
  }
}

SidePanelCoordinator::SidePanelCoordinator(BrowserWindowInterface* browser)
    : browser_(browser) {
  Profile* p = GetProfile();
  DCHECK(p);
  if (!p)
    return;
  SidePanelService* side_panel_service = extensions::SidePanelService::Get(p);
  DCHECK(side_panel_service);
  if (side_panel_service) {
    side_panel_service->AddObserver(this);
  }
}

Profile * SidePanelCoordinator::GetProfile() {
  return browser_->GetProfile();
}

void SidePanelCoordinator::Close() {}

void SidePanelCoordinator::Show(
    SidePanelEntryId entry_id,
    std::optional<SidePanelOpenTrigger> open_trigger) {}

void SidePanelCoordinator::Show(
    SidePanelEntryKey entry_key,
    std::optional<SidePanelOpenTrigger> open_trigger) {
  auto extension_id = entry_key.extension_id();
  if (extension_id) {
    namespace action_utils = ::extensions::vivaldi::extension_action_utils;
    ::vivaldi::BroadcastEvent(
        action_utils::OnSidePanelActionRequested::kEventName,
        action_utils::OnSidePanelActionRequested::Create(*extension_id, "show"),
        GetProfile());
  }
}

void SidePanelCoordinator::Toggle(SidePanelEntryKey key,
                                  SidePanelOpenTrigger open_trigger) {}

void SidePanelCoordinator::OpenInNewTab() {}

void SidePanelCoordinator::UpdatePinState() {}

std::optional<SidePanelEntryId> SidePanelCoordinator::GetCurrentEntryId()
    const {
  return std::optional<SidePanelEntryId>();
}

bool SidePanelCoordinator::IsSidePanelShowing() const {
  return false;
}

content::WebContents* SidePanelCoordinator::GetWebContentsForTest(
    SidePanelEntryId id) {
  return nullptr;
}

bool SidePanelCoordinator::IsSidePanelEntryShowing(
    const SidePanelEntryKey& entry_key) const {
  return false;
}

void SidePanelCoordinator::OnPanelOptionsChanged(
    const ExtensionId& extension_id,
    const api::side_panel::PanelOptions& updated_options) {
  namespace action_utils = ::extensions::vivaldi::extension_action_utils;
  const Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension) {
    return;
  }

  action_utils::SidePanelOptions options;
  options.tab_id = updated_options.tab_id;

  if (updated_options.path) {
    options.resource_url =
        extension->GetResourceURL(*updated_options.path).spec();
    options.path = updated_options.path;
  }
  options.enabled = updated_options.enabled;

  ::vivaldi::BroadcastEvent(
      action_utils::OnSidePanelOptionChanged::kEventName,
      action_utils::OnSidePanelOptionChanged::Create(extension_id, options),
      GetProfile());
}

void SidePanelCoordinator::OnSidePanelServiceShutdown() {}

}  // namespace vivaldi
