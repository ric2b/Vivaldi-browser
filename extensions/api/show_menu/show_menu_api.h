//
// Copyright (c) 2014-2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_
#define EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/favicon_base/favicon_types.h"
#include "components/renderer_context_menu/render_view_context_menu_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/show_menu.h"
#include "third_party/icu/source/i18n/unicode/coll.h"

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}

namespace content {
class BrowserContext;
}

namespace favicon {
class FaviconService;
}

namespace favicon_base {
struct FaviconImageResult;
}

namespace vivaldi {
class VivaldiContextMenu;
}

namespace extensions {

class BookmarkSorter {
 public:
  ~BookmarkSorter();

  BookmarkSorter(vivaldi::show_menu::SortField sort_field,
                 vivaldi::show_menu::SortOrder sort_order_test,
                 bool group_folders);
  void sort(std::vector<bookmarks::BookmarkNode*>& vector);
  void setGroupFolders(bool group_folders) {group_folders_ = group_folders;}
  bool isManualOrder() const
      { return sort_field_ == vivaldi::show_menu::SORT_FIELD_NONE; }

 private:
  bool fallbackToTitleSort(const icu::Collator* collator,
                           bookmarks::BookmarkNode *b1,
                           bookmarks::BookmarkNode *b2,
                           int l1,
                           int l2);
  bool fallbackToDateSort(bookmarks::BookmarkNode *b1,
                          bookmarks::BookmarkNode *b2,
                          int l1,
                          int l2);

  vivaldi::show_menu::SortField sort_field_;
  vivaldi::show_menu::SortOrder sort_order_;
  bool group_folders_;
  std::unique_ptr<icu::Collator> collator_;
};

class ShowMenuCreateFunction;

class VivaldiMenuController : public ui::SimpleMenuModel::Delegate,
                              public bookmarks::BookmarkModelObserver {
 public:
  class Delegate {
   public:
    virtual void OnMenuActivated(int command_id, int event_flags) = 0;
    virtual void OnMenuCanceled()= 0;
  };

  struct BookmarkFolder {
    int node_id;
    int menu_id;
    bool complete;
  };

  struct BookmarkSupport {
    enum Icons {
      kUrl = 0,
      kFolder = 1,
      kSpeeddial = 2,
      kMax = 3,
    };
    BookmarkSupport();
    ~BookmarkSupport();
    const gfx::Image& iconForNode(const bookmarks::BookmarkNode* node) const;
    base::string16 add_label;
    std::vector<gfx::Image> icons;
    bool observer_enabled;
  };

  VivaldiMenuController(Delegate* delegate,
                        std::vector<vivaldi::show_menu::MenuItem>* menu_items);
  ~VivaldiMenuController() override;

  void Show(content::WebContents* web_contents,
            const content::ContextMenuParams& menu_params);

  // ui::SimpleMenuModel::Delegate
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  bool GetIconForCommandId(int command_id, gfx::Image* icon) const override;
  void CommandIdHighlighted(int command_id) override;
  void ExecuteCommand(int command_id, int event_flags) override;
  void MenuWillShow(ui::SimpleMenuModel* source) override;
  void MenuClosed(ui::SimpleMenuModel* source) override;

  // bookmarks::BookmarkModelObserver
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override {}
  void BookmarkNodeMoved(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* old_parent,
                                 int old_index,
                                 const bookmarks::BookmarkNode* new_parent,
                                 int new_index) override {}
  void BookmarkNodeAdded(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* parent,
                                 int index) override {}
  void BookmarkNodeRemoved(
      bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* parent,
      int old_index,
      const bookmarks::BookmarkNode* node,
      const std::set<GURL>& no_longer_bookmarked) override {}
  void BookmarkNodeChanged(bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkNodeFaviconChanged(bookmarks::BookmarkModel* model,
                                  const bookmarks::BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(bookmarks::BookmarkModel* model,
      const bookmarks::BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(bookmarks::BookmarkModel* model,
                                const std::set<GURL>& removed_urls) override {}

 private:
  const vivaldi::show_menu::MenuItem* getItemByCommandId(int command_id) const;
  void PopulateModel(const vivaldi::show_menu::MenuItem* menuitem,
                     ui::SimpleMenuModel* menu_model,
                     int parent_id);
  void SanitizeModel(ui::SimpleMenuModel* menu_model);
  void PopulateBookmarks(const bookmarks::BookmarkNode* node,
                         bookmarks::BookmarkModel* model,
                         int parent_id,
                         bool is_top_folder,
                         int offset_in_folder,
                         ui::SimpleMenuModel* menu_model);
  void PopulateBookmarkFolder(ui::SimpleMenuModel* menu_model,
                              int node_id,
                              int offset);
  bookmarks::BookmarkModel* GetBookmarkModel();
  void EnableBookmarkObserver(bool enable);
  bool IsBookmarkSeparator(const bookmarks::BookmarkNode* node);
  bool HasDeveloperTools();
  bool IsDeveloperTools(int command_id) const;
  void HandleDeveloperToolsCommand(int command_id);
  const Extension* GetExtension() const;
  void LoadFavicon(int command_id, const std::string& url);
  void OnFaviconDataAvailable(
      int command_id,
      const favicon_base::FaviconImageResult& image_result);

  // Loading favicons
  base::CancelableTaskTracker cancelable_task_tracker_;
  favicon::FaviconService* favicon_service_;

  Delegate* delegate_;
  content::ContextMenuParams menu_params_;
  std::vector<vivaldi::show_menu::MenuItem>* menu_items_;  // Not owned by us.
  ui::SimpleMenuModel menu_model_;
  std::unique_ptr<::vivaldi::VivaldiContextMenu> menu_;
  std::vector<std::unique_ptr<ui::SimpleMenuModel>> models_;
  base::string16 separator_;
  base::string16 separator_description_;
  std::unique_ptr<BookmarkSorter> bookmark_sorter_;
  content::WebContents* web_contents_;  // Not owned by us.
  Profile* profile_;
  std::map<int, std::string*> url_map_;
  std::map<ui::SimpleMenuModel*, BookmarkFolder> bookmark_menu_model_map_;
  // State variables to reduce lookups in url_map_
  int current_highlighted_id_;
  bool is_url_highlighted_;
  // Initial selection
  int initial_selected_id_;
  bool is_shown_;
  BookmarkSupport bookmark_support_;
};

// Based on BookmarkEventRouter, send command events to the javascript.
class CommandEventRouter {
 public:
  explicit CommandEventRouter(Profile* profile);
  ~CommandEventRouter();

  // Helper to actually dispatch an event to extension listeners.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

 private:
  content::BrowserContext* browser_context_;
  DISALLOW_COPY_AND_ASSIGN(CommandEventRouter);
};

class ShowMenuAPI : public BrowserContextKeyedAPI,
                    public EventRouter::Observer {
 public:
  explicit ShowMenuAPI(content::BrowserContext* context);
  ~ShowMenuAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ShowMenuAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  void CommandExecuted(int command_id, const std::string& parameter = "");
  void OnOpen();
  void OnUrlHighlighted(const std::string& url);
  void OnBookmarkActivated(int id, int event_flags);
  void OnAddBookmark(int id);

 private:
  friend class BrowserContextKeyedAPIFactory<ShowMenuAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ShowMenuAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<CommandEventRouter> command_event_router_;
};

class ShowMenuCreateFunction : public ChromeAsyncExtensionFunction,
                               public VivaldiMenuController::Delegate {
 public:
  DECLARE_EXTENSION_FUNCTION("showMenu.create", SHOWMENU_CREATE)
  ShowMenuCreateFunction();

  // VivaldiMenuController::Delegate
  void OnMenuActivated(int command_id, int event_flags) override;
  void OnMenuCanceled() override;

 protected:
  ~ShowMenuCreateFunction() override;

  // ChromeAsyncExtensionFunction
  bool RunAsync() override;

 private:
  std::unique_ptr<vivaldi::show_menu::Create::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(ShowMenuCreateFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_SHOW_MENU_SHOW_MENU_API_H_
