//
// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.
//
#ifndef EXTENSIONS_API_BOOKMARK_CONTEXT_MENU_API_H_
#define EXTENSIONS_API_BOOKMARK_CONTEXT_MENU_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/bookmark_context_menu.h"
#include "ui/vivaldi_context_menu.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class BookmarkContextMenuAPI : public BrowserContextKeyedAPI,
                               public EventRouter::Observer {
 public:
  explicit BookmarkContextMenuAPI(content::BrowserContext* context);
  ~BookmarkContextMenuAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>*
        GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // Functions that will send events to JS.
  void OnActivated(int id, int event_state);
  void OnAction(int id, int index, int action);
  void OnOpen();
  void OnClose();
  void OnHover(const std::string& url);

 private:
  friend class BrowserContextKeyedAPIFactory<BookmarkContextMenuAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BookmarkContextMenuAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<class BookmarkContextMenuEventRouter> event_router_;

  // Hover url as reported by menu code. Cached here to avoid repeated events
  // with same value
  std::string hover_url_;
};


// Helper to send events as described in bookmark_context_menu.json
class BookmarkContextMenuEventRouter {
 public:
  explicit BookmarkContextMenuEventRouter(Profile* profile);
  ~BookmarkContextMenuEventRouter();

  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);

 private:
  content::BrowserContext* browser_context_;
  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuEventRouter);
};


// Opens a bookmark context menu
class BookmarkContextMenuShowFunction :
    public ChromeAsyncExtensionFunction,
    public ::vivaldi::VivaldiBookmarkMenuObserver {
 public:
  DECLARE_EXTENSION_FUNCTION(
        "bookmarkContextMenu.show", BOOKMARKCONTEXTMENU_SHOW)
  BookmarkContextMenuShowFunction();

  // vivaldi::VivaldiBookmarkMenuObserver
  void BookmarkMenuClosed(::vivaldi::VivaldiBookmarkMenu* menu) override;

 protected:
  ~BookmarkContextMenuShowFunction() override;

  // ChromeAsyncExtensionFunction
  bool RunAsync() override;

 private:
  std::unique_ptr<vivaldi::bookmark_context_menu::Show::Params> params_;
  Profile* profile_;
  ::vivaldi::BookmarkMenuParams menuParams_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuShowFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_BOOKMARK_CONTEXT_MENU_API_H_
