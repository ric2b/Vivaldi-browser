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
}

namespace extensions {

class BookmarkContextMenuAPI : public BrowserContextKeyedAPI {
 public:
  explicit BookmarkContextMenuAPI(content::BrowserContext* context);
  ~BookmarkContextMenuAPI() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>*
        GetFactoryInstance();

  // Functions that will send events to JS.
  static void SendActivated(content::BrowserContext* browser_context,
                            int id,
                            int event_state);
  static void SendAction(content::BrowserContext* browser_context,
                         int id,
                         int index,
                         int action);
  static void SendOpen(content::BrowserContext* browser_context, int64_t id);
  static void SendClose(content::BrowserContext* browser_context);
  static void SendHover(content::BrowserContext* browser_context,
                        const std::string& url);

 private:
  friend class BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BookmarkContextMenuAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Hover url as reported by menu code. Cached here to avoid repeated events
  // with same value
  std::string hover_url_;
};

// Opens a bookmark context menu
class BookmarkContextMenuShowFunction :
    public UIThreadExtensionFunction,
    public ::vivaldi::VivaldiBookmarkMenuObserver {
 public:
  DECLARE_EXTENSION_FUNCTION(
        "bookmarkContextMenu.show", BOOKMARKCONTEXTMENU_SHOW)
  BookmarkContextMenuShowFunction();

 private:
  ~BookmarkContextMenuShowFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  // vivaldi::VivaldiBookmarkMenuObserver
  void BookmarkMenuClosed(::vivaldi::VivaldiBookmarkMenu* menu) override;

  std::string Open(const std::string& id);

  ::vivaldi::BookmarkMenuParams menuParams_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuShowFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_BOOKMARK_CONTEXT_MENU_API_H_
