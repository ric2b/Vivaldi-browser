#include "ui/vivaldi_side_panel_coordinator.h"
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/schema/browser_action_utilities.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"

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

SidePanelCoordinator::SidePanelCoordinator(VivaldiBrowserWindow* browser_window)
    : browser_window_(browser_window) {
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
  return browser_window_->GetProfile();
}

void SidePanelCoordinator::Show(
    absl::optional<SidePanelEntryId> entry_id,
    absl::optional<SidePanelOpenTrigger> open_trigger) {}

void SidePanelCoordinator::Show(
    SidePanelEntryKey entry_key,
    absl::optional<SidePanelOpenTrigger> open_trigger) {}

void SidePanelCoordinator::Close() {}

void SidePanelCoordinator::Toggle() {}

void SidePanelCoordinator::Toggle(SidePanelEntryKey key,
                                  SidePanelOpenTrigger open_trigger) {}

void SidePanelCoordinator::OpenInNewTab() {}

void SidePanelCoordinator::UpdatePinState() {}

absl::optional<SidePanelEntryId> SidePanelCoordinator::GetCurrentEntryId()
    const {
  return absl::optional<SidePanelEntryId>();
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
  namespace exu = ::extensions::vivaldi::extension_action_utils;
  const Extension* extension =
      extensions::ExtensionRegistry::Get(GetProfile())
          ->GetExtensionById(extension_id,
                             extensions::ExtensionRegistry::ENABLED);
  if (!extension) {
    return;
  }

  exu::SidePanelOptions options;
  options.tab_id = updated_options.tab_id;

  if (updated_options.path) {
    options.resource_url =
        extension->GetResourceURL(*updated_options.path).spec();
    options.path = updated_options.path;
  }
  options.enabled = updated_options.enabled;

  ::vivaldi::BroadcastEvent(
      exu::OnSidePanelOptionChanged::kEventName,
      exu::OnSidePanelOptionChanged::Create(extension_id, options),
      GetProfile());
}

void SidePanelCoordinator::OnSidePanelServiceShutdown() {}

}  // namespace vivaldi
