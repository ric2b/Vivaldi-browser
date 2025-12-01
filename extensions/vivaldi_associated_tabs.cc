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

int IdForTab(content::WebContents* contents) {
  return sessions::SessionTabHelper::IdForTab(contents).id();
}

void DoRelatedMoves(std::vector<int> moved_tabs_vector) {
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
      int tab_id = IdForTab(contents);
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
      res.push_back(IdForTab(contents));
    }
  }
  return res;
}

void RemoveChildren(std::vector<int> parent_tabs) {
  using ::vivaldi::ParentTabUserData;

  std::set<int> tabs(parent_tabs.begin(), parent_tabs.end());
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    if (!tab_strip)
      continue;
    for (int i = 0; i < tab_strip->count(); ++i) {
      auto * contents = tab_strip->GetWebContentsAt(i);
      int tab_id = IdForTab(contents);
      if (tabs.count(tab_id)) {
        tabs.erase(tab_id);
        tab_strip->DetachAndDeleteWebContentsAt(i);
        i = -1;
      }
    }
  }
}

struct FoundTab {
  Browser * browser = nullptr;
  TabStripModel * tab_strip = nullptr;
  content::WebContents * contents = nullptr;
  int index = -1;

  void Delete() {
    tab_strip->DetachAndDeleteWebContentsAt(index);
  }
};

bool FindTab(int tab_id, FoundTab * result) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel *tab_strip = browser->tab_strip_model();
    if (!tab_strip)
      continue;
    for (int i = 0; i < tab_strip->count(); ++i) {
      auto * contents = tab_strip->GetWebContentsAt(i);
      if (!contents)
        continue;
      if (IdForTab(contents) == tab_id) {
        result->contents = contents;
        result->browser = browser;
        result->tab_strip = tab_strip;
        result->index = i;
        return true;
      }
    }
  }
  return false;
}

void HandleDetachedTabInternal(int tab_id) {
  FoundTab found_tab;
  if (!FindTab(tab_id, &found_tab))
    return;

  if (::vivaldi::ParentTabUserData::IsWebPanel(found_tab.contents)) {
    found_tab.Delete();
  }
}

} // namespace

void HandleDetachedTab(int tab_id) {
  if (tab_id == -1)
    return;

  // This function is called from the WebContents observer.
  // A WebContents should never be deleted while it is notifying observers.
  // Post the deletion to the UI thread to avoid the crash.
  content::GetUIThreadTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(HandleDetachedTabInternal, tab_id));
}

void HandleAssociatedTabs(TabStripModel* tab_strip_model,
                          const TabStripModelChange& change) {
  using ::vivaldi::ParentTabUserData;

  if (change.type() == TabStripModelChange::kInserted) {
    std::vector<int> moved;
    auto* insert = change.GetInsert();
    if (insert) {
      for (auto& context_with_index : insert->contents) {
        // Ignore children tabs.
        if (ParentTabUserData::GetParentTabId(context_with_index.contents))
          continue;
        moved.push_back(IdForTab(context_with_index.contents));
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
      removed.push_back(IdForTab(context_with_index.contents));
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
