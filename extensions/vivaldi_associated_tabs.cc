// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/api/guest_view/parent_tab_user_data.h"

#include "extensions/vivaldi_associated_tabs.h"

namespace vivaldi {
namespace {
void DoRelatedMoves(std::vector<int> moved_tabs_vector) {
  using sessions::SessionTabHelper;
  using ::vivaldi::ParentTabUserData;

  std::set<int> moved_tabs(moved_tabs_vector.begin(), moved_tabs_vector.end());
  std::map<int, TabStripModel*> tab_id_to_tab_strip;

  // Find, where the parents are and remember their tab-strips.
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (!tab_strip)
      continue;

    for (int i = 0; i < tab_strip->count(); ++i) {
      auto * contents = tab_strip->GetWebContentsAt(i);
      int tab_id = SessionTabHelper::IdForTab(contents).id();
      if (tab_id == -1)
        continue;

      if (moved_tabs.count(tab_id)) {
        tab_id_to_tab_strip[tab_id] = tab_strip;
      }
    }
  }

  // Nothing to do, no parents with tab-strips.
  if (tab_id_to_tab_strip.empty())
    return;

  // Iterate over all tabs.
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (!tab_strip)
      continue;

    for (int i = 0; i < tab_strip->count(); ++i) {
      auto* contents = tab_strip->GetWebContentsAt(i);
      auto parent_id = ParentTabUserData::GetParentTabId(contents);
      // Only children are interesting (the tabs with parents).
      // parent_id == 0  means the parent is the main windows,
      // it is typically a side-panel.
      if (!parent_id || *parent_id == 0)
        continue;
      // Is this a child of one of the parents?
      auto it = tab_id_to_tab_strip.find(*parent_id);
      if (it == tab_id_to_tab_strip.end())
        continue;

      auto* target_tab_strip = it->second;

      // The child is already together with its parent in the tab-strip.
      if (target_tab_strip == tab_strip)
        continue;

      // Move the child to the tab-strip where the parent is.
      auto detached_tab = tab_strip->DetachTabAtForInsertion(i);
      target_tab_strip->InsertDetachedTabAt(target_tab_strip->count(),
                                            std::move(detached_tab), 0);
      // And repeat from index 0 since the tabs may changed their order.
      i = -1;
    }
  }
}

std::vector<int> FindAssociatedTabs(std::vector<int> parent_tabs_vector) {
  using sessions::SessionTabHelper;
  using ::vivaldi::ParentTabUserData;

  std::set<int> parent_tabs(parent_tabs_vector.begin(),
                            parent_tabs_vector.end());
  std::vector<int> res;

  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (!tab_strip)
      continue;
    for (int i = 0; i < tab_strip->count(); ++i) {
      auto * contents = tab_strip->GetWebContentsAt(i);
      auto parent_id = ParentTabUserData::GetParentTabId(contents);
      if (!parent_id || *parent_id == 0)
        continue;
      if (!parent_tabs.count(*parent_id))
        continue;
      res.push_back(SessionTabHelper::IdForTab(contents).id());
    }
  }
  return res;
}

void RemoveChildren(std::vector<int> parent_tabs) {
  using sessions::SessionTabHelper;
  using ::vivaldi::ParentTabUserData;

  std::set<int> tabs(parent_tabs.begin(), parent_tabs.end());
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (!tab_strip)
      continue;
    for (int i = 0; i < tab_strip->count(); ++i) {
      auto * contents = tab_strip->GetWebContentsAt(i);
      int tab_id = SessionTabHelper::IdForTab(contents).id();
      if (tabs.count(tab_id)) {
        tab_strip->DetachAndDeleteWebContentsAt(i);
        i = -1;
      }
    }
  }
}
} // namespace

void HandleAssociatedTabs(TabStripModel* tab_strip_model,
                          const TabStripModelChange& change) {
  using sessions::SessionTabHelper;
  using ::vivaldi::ParentTabUserData;

  if (change.type() == TabStripModelChange::kInserted) {
    std::vector<int> moved;
    auto* insert = change.GetInsert();
    if (insert) {
      for (auto& context_with_index : insert->contents) {
        // Ignore children tabs.
        if (ParentTabUserData::GetParentTabId(context_with_index.contents))
          continue;
        moved.push_back(
            SessionTabHelper::IdForTab(context_with_index.contents).id());
      }

      if (!moved.empty()) {
        content::GetUIThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(DoRelatedMoves, moved));
      }
    }
  } else if (change.type() == TabStripModelChange::kRemoved) {
    std::vector<int> removed;
    auto* remove = change.GetRemove();
    // Collect the tabId's of the removed tabs.
    for (auto& context_with_index : remove->contents) {
      if (context_with_index.remove_reason !=
          TabStripModelChange::RemoveReason::kDeleted)
        continue;
      // Not deleted, but detached to move...
      // Ignore children tabs.
      if (ParentTabUserData::GetParentTabId(context_with_index.contents))
        continue;
      removed.push_back(
          SessionTabHelper::IdForTab(context_with_index.contents).id());
    }

    // Collect the children of the tabs.
    auto children = FindAssociatedTabs(removed);
    if (!children.empty()) {
      // Due to the reentrancy check, we can't remove the children here.
      content::GetUIThreadTaskRunner()->PostTask(
          FROM_HERE, base::BindOnce(RemoveChildren, children));
    }
  }
}

void AddVivaldiTabItemsToEvent(content::WebContents* contents,
                               base::Value::Dict& object_args) {
  auto parent_tab_id = ::vivaldi::ParentTabUserData::GetParentTabId(contents);
  if (parent_tab_id) {
    object_args.Set("parentTabId", *parent_tab_id);
  }
}
} // namespace vivaldi
