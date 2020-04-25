//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
//
#include "ui/vivaldi_bookmark_menu_mac.h"

#include "app/vivaldi_commands.h"
#include "app/vivaldi_resources.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/utf_string_conversions.h"
#include "browser/menus/bookmark_sorter.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/profiles/profile.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_menu_bridge.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/prefs/pref_service.h"
#include "prefs/vivaldi_gen_prefs.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkNode;

static base::string16 Separator;
static base::string16 SeparatorDescription;

namespace vivaldi {

class SortFlags {
 public:
  SortFlags() {
    field_ = ::vivaldi::BookmarkSorter::FIELD_NONE;
    order_ = ::vivaldi::BookmarkSorter::ORDER_NONE;
  }
  void InitFromPrefs();

  ::vivaldi::BookmarkSorter::SortField field_;
  ::vivaldi::BookmarkSorter::SortOrder order_;
};

void SortFlags::InitFromPrefs() {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  Profile* profile = appController.lastProfile;
  if (profile) {
    PrefService* pref_service = profile->GetPrefs();
    const base::DictionaryValue* dict = pref_service->GetDictionary(
        vivaldiprefs::kBookmarksManagerSorting);
    if (dict) {
      const base::Value* val =
          dict->FindKeyOfType("sortField", base::Value::Type::STRING);
      if (val) {
        const std::string sortField = val->GetString();
        if (sortField == "manually")
          field_ = ::vivaldi::BookmarkSorter::FIELD_NONE;
        else if (sortField == "title")
          field_ = ::vivaldi::BookmarkSorter::FIELD_TITLE;
        else if (sortField == "url")
          field_ = ::vivaldi::BookmarkSorter::FIELD_URL;
        else if (sortField == "nickname")
          field_ = ::vivaldi::BookmarkSorter::FIELD_NICKNAME;
        else if (sortField == "description")
          field_ = ::vivaldi::BookmarkSorter::FIELD_DESCRIPTION;
      }
      val = dict->FindKeyOfType("sortOrder", base::Value::Type::INTEGER);
      if (val) {
        const int sortOrder = val->GetInt();
        if (sortOrder == 1)
          order_ = ::vivaldi::BookmarkSorter::ORDER_NONE;
        else if (sortOrder == 2)
          order_ = ::vivaldi::BookmarkSorter::ORDER_ASCENDING;
        else if (sortOrder == 3)
          order_ = ::vivaldi::BookmarkSorter::ORDER_DESCENDING;
      }
    }
  }
}

bool IsBookmarkSeparator(const bookmarks::BookmarkNode* node) {
  if (Separator.length() == 0) {
    Separator = base::UTF8ToUTF16("---");
    SeparatorDescription = base::UTF8ToUTF16("separator");
  }
  return node->GetTitle().compare(Separator) == 0  &&
     node->GetDescription().compare(SeparatorDescription) == 0;
}

void ClearBookmarkMenu() {
  AppController* appController = static_cast<AppController*>([NSApp delegate]);
  if ([appController bookmarkMenuBridge])
    [appController bookmarkMenuBridge]->ClearBookmarkMenu();
}

void GetBookmarkNodes(const BookmarkNode* node,
                      std::vector<bookmarks::BookmarkNode*>& nodes) {
  SortFlags flags;
  flags.InitFromPrefs();
  vivaldi::BookmarkSorter sorter(flags.field_, flags.order_, true);

  nodes.reserve(node->children().size());
  for (auto& it: node->children()) {
    BookmarkNode* child =
        const_cast<bookmarks::BookmarkNode*>(it.get());
    if (flags.order_ == ::vivaldi::BookmarkSorter::ORDER_NONE ||
        !IsBookmarkSeparator(child)) {
      nodes.push_back(child);
    }
  }

  sorter.sort(nodes);
}

void AddAddTabToBookmarksMenuItem(const BookmarkNode* node, NSMenu* menu) {

  base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
          initWithTitle:l10n_util::GetNSString(
              IDS_VIV_ADD_ACTIVE_TAB_TO_BOOKMARKS)
                 action:nil
          keyEquivalent:@""]);
  [item setTag:IDC_VIV_ADD_ACTIVE_TAB_TO_BOOKMARKS];
  AppController* app_controller = static_cast<AppController*>([NSApp delegate]);
  [item setTarget:app_controller];
  [app_controller setVivaldiMenuItemAction:item];
  [item setRepresentedObject:[NSString stringWithFormat:@"%lld", node->id()]];
  [menu addItem:item];
}

void OnClearBookmarkMenu(NSMenu* menu, NSMenuItem* item) {
  if ([item tag] == IDC_VIV_ADD_ACTIVE_TAB_TO_BOOKMARKS)
    [menu removeItem:item];
}

}  // namespace vivaldi