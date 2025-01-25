// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_

#include "base/token.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace tab_groups {

class TabGroupSyncService;

// Class that maintains a relationship between the webcontents object of a tab
// and the saved tab group tab that exists for that tab. Listens to navigation
// events on the tab and performs actions to the tab group service, and when
// a sync navigation occurs, updates the local webcontents.
class SavedTabGroupWebContentsListener : public content::WebContentsObserver {
 public:
  SavedTabGroupWebContentsListener(content::WebContents* web_contents,
                                   const LocalTabID& token,
                                   TabGroupSyncService* service);
  SavedTabGroupWebContentsListener(content::WebContents* web_contents,
                                   content::NavigationHandle* navigation_handle,
                                   const LocalTabID& token,
                                   TabGroupSyncService* service);
  ~SavedTabGroupWebContentsListener() override;

  // If possible (see implementation for details) performs a naviagation of the
  // |web_contents_| and then stores the |navigation_handle_|. This method
  // should only be called when a navigation request comes in via sync, and
  // since DidFinishNavigation will be called when the navigation completes
  // created by this method, we save the navigation handle and prevent
  // DidFinishNavigation from running if it matches the navigation handle in
  // this method.
  void NavigateToUrl(const GURL& url);

  // Accessors.
  const LocalTabID& saved_tab_group_tab_id() const {
    return saved_tab_group_tab_id_;
  }
  content::WebContents* web_contents() const { return web_contents_; }

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidGetUserInteraction(const blink::WebInputEvent& event) override;

 private:
  // Clear and then update the |tab_redirect_chain_| for the navigation_handle's
  // entire redirect chain (from GetRedirectChain()). only performed if the nav
  // is a MainFrame navigation.
  void UpdateTabRedirectChain(content::NavigationHandle* navigation_handle);

  // Retrieves the SavedTabGroup that contains the tab with the id
  // |saved_tab_group_tab_id_|.
  const SavedTabGroup saved_group();

  // The saved tab group tab's ID.
  const LocalTabID saved_tab_group_tab_id_;

  // The webcontents for the Tab that is being listened to.
  const raw_ptr<content::WebContents> web_contents_ = nullptr;

  // The service used to query and manage SavedTabGroups.
  const raw_ptr<TabGroupSyncService> service_ = nullptr;

  // The NavigationHandle that resulted from the last sync update. Ignored by
  // `DidFinishNavigation` to prevent synclones.
  raw_ptr<content::NavigationHandle> handle_from_sync_update_ = nullptr;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_SAVED_TAB_GROUP_WEB_CONTENTS_LISTENER_H_
