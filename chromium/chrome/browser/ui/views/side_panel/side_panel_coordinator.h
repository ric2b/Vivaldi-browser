// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "ui/actions/actions.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_observer.h"

class Browser;
class BrowserView;

namespace actions {
class ActionItem;
}  // namespace actions

namespace views {
class ImageButton;
class ToggleImageButton;
class View;
}  // namespace views

// Class used to manage the state of side-panel content. Clients should manage
// side-panel visibility using this class rather than explicitly showing/hiding
// the side-panel View.
// This class is also responsible for consolidating multiple SidePanelEntry
// classes across multiple SidePanelRegistry instances, potentially merging them
// into a single unified side panel.
// Existence and value of registries' active_entry() determines which entry is
// visible for a given tab where the order of precedence is contextual
// registry's active_entry() then global registry's. These values are reset when
// the side panel is closed and |last_active_global_entry_id_| is used to
// determine what entry is seen when the panel is reopened.
class SidePanelCoordinator final : public SidePanelRegistryObserver,
                                   public TabStripModelObserver,
                                   public views::ViewObserver,
                                   public PinnedToolbarActionsModel::Observer,
                                   public SidePanelUI,
                                   public ToolbarActionsModel::Observer {
 public:
  explicit SidePanelCoordinator(BrowserView* browser_view);
  SidePanelCoordinator(const SidePanelCoordinator&) = delete;
  SidePanelCoordinator& operator=(const SidePanelCoordinator&) = delete;
  ~SidePanelCoordinator() override;

  void TearDownPreBrowserViewDestruction();

  static SidePanelRegistry* GetGlobalSidePanelRegistry(Browser* browser);

  // SidePanelUI:
  void Show(SidePanelEntry::Id entry_id,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger =
                std::nullopt) override;
  void Show(SidePanelEntry::Key entry_key,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger =
                std::nullopt) override;
  void Close() override;
  void Toggle(SidePanelEntryKey key,
              SidePanelUtil::SidePanelOpenTrigger open_trigger) override;
  void OpenInNewTab() override;
  void UpdatePinState() override;
  std::optional<SidePanelEntry::Id> GetCurrentEntryId() const override;
  bool IsSidePanelShowing() const override;
  bool IsSidePanelEntryShowing(
      const SidePanelEntry::Key& entry_key) const override;
  void SetNoDelaysForTesting(bool no_delays_for_testing) override;

  // Returns the web contents in a side panel if one exists.
  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;
  void DisableAnimationsForTesting() override;

  // TODO(crbug.com/40851017): Move this method to `SidePanelUI` after
  // decoupling `SidePanelEntry` from views.
  bool IsSidePanelEntryShowing(const SidePanelEntry* entry) const;

  // Re-runs open new tab URL check and sets button state to enabled/disabled
  // accordingly.
  void UpdateNewTabButtonState();

  void UpdateHeaderPinButtonState();

  SidePanelEntry* GetCurrentSidePanelEntryForTesting() {
    return current_entry_.get();
  }

  actions::ActionItem* GetActionItem(SidePanelEntry::Key entry_key);

  views::ToggleImageButton* GetHeaderPinButtonForTesting() {
    return header_pin_button_;
  }

  SidePanelEntry* GetLoadingEntryForTesting() const;

  void AddSidePanelViewStateObserver(SidePanelViewStateObserver* observer);

  void RemoveSidePanelViewStateObserver(SidePanelViewStateObserver* observer);

  void Close(bool supress_animations);

 private:
  friend class SidePanelCoordinatorTest;
  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           ShowEmptyUserNoteSidePanel);
  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           PopulateUserNoteSidePanel);

  // Unlike `Show()` which takes in a SidePanelEntry's id or key, this version
  // should only be used for the rare case when we need to show a particular
  // entry instead of letting GetEntryForKey() decide for us.
  void Show(SidePanelEntry* entry,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger =
                std::nullopt,
            bool supress_animations = false);
  void OnClosed();

  // Returns the corresponding entry for `entry_key` or a nullptr if this key is
  // not registered in the currently observed registries. This looks through the
  // active contextual registry first, then the global registry.
  SidePanelEntry* GetEntryForKey(const SidePanelEntry::Key& entry_key) const;

  SidePanelEntry* GetActiveContextualEntryForKey(
      const SidePanelEntry::Key& entry_key) const;

  // Returns the current loading entry or nullptr if none exists.
  SidePanelEntry* GetLoadingEntry() const;

  // Returns whether the global entry with the same key as `entry_key` is
  // showing.
  bool IsGlobalEntryShowing(const SidePanelEntry::Key& entry_key) const;

  // Creates header and SidePanelEntry content container within the side panel.
  void InitializeSidePanel();

  // Removes existing SidePanelEntry contents from the side panel if any exist
  // and populates the side panel with the provided SidePanelEntry and
  // `content_view` if provided, otherwise get the content_view from the
  // provided SidePanelEntry.
  void PopulateSidePanel(
      bool supress_animations,
      SidePanelEntry* entry,
      std::optional<std::unique_ptr<views::View>> content_view);

  // Clear cached views for registry entries for global and contextual
  // registries.
  void ClearCachedEntryViews();

  void UpdatePanelIconAndTitle(const ui::ImageModel& icon,
                               const std::u16string& text,
                               const bool should_show_title_text,
                               const bool is_extension);

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_from) override;

  // PinnedToolbarActionsModel::Observer:
  void OnActionAdded(const actions::ActionId& id) override {}
  void OnActionRemoved(const actions::ActionId& id) override {}
  void OnActionMoved(const actions::ActionId& id,
                     int from_index,
                     int to_index) override {}
  void OnActionsChanged() override;

  SidePanelRegistry* GetActiveContextualRegistry() const;

  std::unique_ptr<views::View> CreateHeader();

  // Returns the new entry to be shown after the active entry is deregistered,
  // or nullptr if no suitable entry is found. Called from
  // `OnEntryWillDeregister()` when there's an active entry being shown in the
  // side panel.
  SidePanelEntry* GetNewActiveEntryOnDeregister(
      SidePanelRegistry* deregistering_registry,
      const SidePanelEntry::Key& key);

  // Returns the new entry to be shown after the active tab has changed, or
  // nullptr if no suitable entry is found. Called from
  // `OnTabStripModelChanged()` when there's an active entry being shown in the
  // side panel.
  SidePanelEntry* GetNewActiveEntryOnTabChanged();

  void NotifyPinnedContainerOfActiveStateChange(SidePanelEntryKey key,
                                                bool is_active);

  void MaybeQueuePinPromo();
  void ShowPinPromo();
  void MaybeEndPinPromo(bool pinned);

  // SidePanelRegistryObserver:
  void OnEntryRegistered(SidePanelRegistry* registry,
                         SidePanelEntry* entry) override;
  void OnEntryWillDeregister(SidePanelRegistry* registry,
                             SidePanelEntry* entry) override;
  void OnRegistryDestroying(SidePanelRegistry* registry) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // ToolbarActionsModel::Observer
  void OnToolbarActionAdded(const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionRemoved(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarActionUpdated(
      const ToolbarActionsModel::ActionId& id) override {}
  void OnToolbarModelInitialized() override {}
  void OnToolbarPinnedActionsChanged() override;

  // When true, prevent loading delays when switching between side panel
  // entries.
  bool no_delays_for_testing_ = false;

  // Timestamp of when the side panel was opened. Updated when the side panel is
  // triggered to be opened, not when visibility changes. These can differ due
  // to delays for loading content. This is used for metrics.
  base::TimeTicks opened_timestamp_;

  const raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_;
  raw_ptr<SidePanelRegistry> global_registry_;

  // current_entry_ tracks the entry that currently has its view hosted by the
  // side panel. It is necessary as current_entry_ may belong to a contextual
  // registry that is swapped out (during a tab switch for e.g.). In such
  // situations we may still need a reference to the entry corresponding to the
  // hosted view so we can cache and clean up appropriately before switching in
  // the new entry.
  // Use a weak pointer so that current side panel entry can be reset
  // automatically if the entry is destroyed.
  base::WeakPtr<SidePanelEntry> current_entry_;

  // Used to update icon in the side panel header.
  raw_ptr<views::ImageView, AcrossTasksDanglingUntriaged> panel_icon_ = nullptr;

  // Used to update the displayed title in the side panel header.
  raw_ptr<views::Label, AcrossTasksDanglingUntriaged> panel_title_ = nullptr;

  // Used to update the visibility of the 'Open in New Tab' header button.
  raw_ptr<views::ImageButton, AcrossTasksDanglingUntriaged>
      header_open_in_new_tab_button_ = nullptr;

  // Used to update the visibility of the pin header button.
  raw_ptr<views::ToggleImageButton, AcrossTasksDanglingUntriaged>
      header_pin_button_ = nullptr;

  // Provides delay on pinning promo.
  base::OneShotTimer pin_promo_timer_;

  // Set to the appropriate pin promo for the current side panel entry, or null
  // if none. (Not set if e.g. already pinned.)
  raw_ptr<const base::Feature> pending_pin_promo_ = nullptr;

  base::ScopedObservation<ToolbarActionsModel, ToolbarActionsModel::Observer>
      extensions_model_observation_{this};

  base::ObserverList<SidePanelViewStateObserver> view_state_observers_;

  base::ScopedMultiSourceObservation<SidePanelRegistry,
                                     SidePanelRegistryObserver>
      registry_observations_{this};

  base::ScopedObservation<PinnedToolbarActionsModel,
                          PinnedToolbarActionsModel::Observer>
      pinned_model_observation_{this};
};

namespace base {

// Since SidePanelCoordinator defines custom method names to add and remove
// observers, we need define a new trait customization to use
// `base::ScopedObservation` and `base::ScopedMultiSourceObservation`.
// See `base/scoped_observation_traits.h` for more details.
template <>
struct ScopedObservationTraits<SidePanelCoordinator,
                               SidePanelViewStateObserver> {
  static void AddObserver(SidePanelCoordinator* source,
                          SidePanelViewStateObserver* observer) {
    source->AddSidePanelViewStateObserver(observer);
  }
  static void RemoveObserver(SidePanelCoordinator* source,
                             SidePanelViewStateObserver* observer) {
    source->RemoveSidePanelViewStateObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
