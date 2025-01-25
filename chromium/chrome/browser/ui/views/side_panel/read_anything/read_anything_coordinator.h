// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_

#include <memory>
#include <string>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"

class Browser;
class SidePanelRegistry;
namespace views {
class View;
}  // namespace views

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingCoordinator
//
//  A class that coordinates the Read Anything feature. This class registers
//  itself as a SidePanelEntry.
//  The coordinator acts as the external-facing API for the Read Anything
//  feature. Classes outside this feature should make calls to the coordinator.
//  This class has the same lifetime as the browser.
//
class ReadAnythingCoordinator : public BrowserUserData<ReadAnythingCoordinator>,
                                public SidePanelEntryObserver,
                                public TabStripModelObserver,
                                public content::WebContentsObserver,
                                public BrowserListObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void Activate(bool active) {}
    virtual void OnActivePageDistillable(bool distillable) {}
    virtual void OnCoordinatorDestroyed() = 0;
  };

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);
  explicit ReadAnythingCoordinator(Browser* browser);
  ~ReadAnythingCoordinator() override;

  void AddObserver(ReadAnythingCoordinator::Observer* observer);
  void RemoveObserver(ReadAnythingCoordinator::Observer* observer);

  void OnReadAnythingSidePanelEntryShown();
  void OnReadAnythingSidePanelEntryHidden();

  void ActivePageDistillableForTesting();
  void ActivePageNotDistillableForTesting();

 private:
  friend class BrowserUserData<ReadAnythingCoordinator>;
  friend class ReadAnythingCoordinatorTest;
  friend class ReadAnythingCoordinatorScreen2xDataCollectionModeTest;

  void CreateAndRegisterEntriesForExistingWebContents(
      TabStripModel* tab_strip_model);
  void CreateAndRegisterEntryForWebContents(content::WebContents* web_contents);

  // Starts the delay for showing the IPH after the tab has changed.
  void StartPageChangeDelay();
  // Occurs when the timer set when changing tabs is finished.
  void OnTabChangeDelayComplete();

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // Callback passed to SidePanelCoordinator. This function creates the
  // container view and all its child views and returns it.
  std::unique_ptr<views::View> CreateContainerView();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void DidStopLoading() override;
  void PrimaryPageChanged(content::Page& page) override;

  content::WebContents* GetActiveWebContents() const;

  // Decides whether the active page is distillable and alerts observers. Also,
  // attempts to show or hide in product help for reading mode.
  void ActivePageDistillable();
  void ActivePageNotDistillable();
  bool IsActivePageDistillable() const;

  void InstallGDocsHelperExtension();
  void RemoveGDocsHelperExtension();
  // The number of active local side panel that is currently shown in the
  // browser. If there is no active local side panel (count is 0) after a
  // timeout, we can safely remove the gdocs helper extension.
  int active_local_side_panel_count_ = 0;
  // Start a timer when the user leaves a local side panel. If they switch to
  // another local side panel before it expires, keep the extension installed;
  // otherwise, uninstall it. This prevents frequent
  // installations/uninstallations.
  base::RepeatingTimer local_side_panel_switch_delay_timer_;
  void OnLocalSidePanelSwitchDelayTimeout();

  std::string default_language_code_;

  base::ObserverList<Observer> observers_;

  bool post_tab_change_delay_complete_ = true;
  base::RetainingOneShotTimer delay_timer_;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  base::WeakPtrFactory<ReadAnythingCoordinator> weak_ptr_factory_{this};

  BROWSER_USER_DATA_KEY_DECL();
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
