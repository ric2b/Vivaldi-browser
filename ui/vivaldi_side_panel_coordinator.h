#pragma once

#include <map>
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"

class BrowserWindowInterface;
class Profile;

namespace vivaldi {

class SidePanelCoordinator : public SidePanelUI,
                             public extensions::SidePanelService::Observer {
 public:
  SidePanelCoordinator(BrowserWindowInterface*);

  ~SidePanelCoordinator() override;

  void Show(
      SidePanelEntryId entry_id,
      std::optional<SidePanelOpenTrigger> open_trigger = std::nullopt) override;

  void Show(
      SidePanelEntryKey entry_key,
      std::optional<SidePanelOpenTrigger> open_trigger = std::nullopt) override;

  void Close() override;

  void Toggle(SidePanelEntryKey key,
              SidePanelOpenTrigger open_trigger) override;

  void OpenInNewTab() override;

  void UpdatePinState() override;

  std::optional<SidePanelEntryId> GetCurrentEntryId() const override;

  bool IsSidePanelShowing() const override;

  bool IsSidePanelEntryShowing(
      const SidePanelEntryKey& entry_key) const override;

  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;

  void OnPanelOptionsChanged(const extensions::ExtensionId& extension_id,
                             const extensions::api::side_panel::PanelOptions&
                                 updated_options) override;
  void OnSidePanelServiceShutdown() override;

  void DisableAnimationsForTesting() override {}

  void SetNoDelaysForTesting(bool no_delays_for_testing) override {}

 private:
  Profile * GetProfile();

  BrowserWindowInterface* browser_;
};

}  // namespace vivaldi
