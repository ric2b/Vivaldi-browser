//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef EXTENSIONS_API_BOOKMARK_CONTEXT_MENU_API_H_
#define EXTENSIONS_API_BOOKMARK_CONTEXT_MENU_API_H_

#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/schema/bookmark_context_menu.h"
#include "ui/vivaldi_context_menu.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class BookmarkContextMenuAPI : public BrowserContextKeyedAPI {
 public:
  explicit BookmarkContextMenuAPI(content::BrowserContext* context);
  ~BookmarkContextMenuAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>*
  GetFactoryInstance();

  // Functions that will send events to JS.
  static void SendOpen(content::BrowserContext* browser_context, int64_t id);
  static void SendClose(content::BrowserContext* browser_context);

 private:
  friend class BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BookmarkContextMenuAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

// Opens a bookmark context menu
class BookmarkContextMenuShowFunction
    : public ExtensionFunction,
      public ::vivaldi::BookmarkMenuContainer::Delegate,
      public ::vivaldi::VivaldiBookmarkMenuObserver {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarkContextMenu.show",
                             BOOKMARKCONTEXTMENU_SHOW)
  BookmarkContextMenuShowFunction();

 private:
  ~BookmarkContextMenuShowFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // vivaldi::VivaldiBookmarkMenuObserver
  void BookmarkMenuClosed(::vivaldi::VivaldiBookmarkMenu* menu) override;

  // vivaldi::BookmarkMenuContainer::Delegate
  void OnHover(const std::string& url) override;
  void OnOpenBookmark(int64_t bookmark_id, int event_state) override;
  void OnBookmarkAction(int64_t bookmark_id, int command) override;
  void OnOpenMenu(int64_t bookmark_id) override;

  std::string Open(content::WebContents* web_contents, const std::string& id);

  // Parsed data from the JS layer.
  std::optional<vivaldi::bookmark_context_menu::Show::Params> params_;
  std::unique_ptr<::vivaldi::BookmarkMenuContainer> bookmark_menu_container_;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_BOOKMARK_CONTEXT_MENU_API_H_
