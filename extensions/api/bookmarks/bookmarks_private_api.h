// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_BOOKMARKS_BOOKMARKS_PRIVATE_API_H_
#define EXTENSIONS_API_BOOKMARKS_BOOKMARKS_PRIVATE_API_H_

#include <set>

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using extensions::BrowserContextKeyedAPI;
using extensions::BrowserContextKeyedAPIFactory;

namespace extensions {

class MetaInfoChangeFilter;

class VivaldiBookmarksAPI : public bookmarks::BookmarkModelObserver,
                            public BrowserContextKeyedAPI {
 public:
  explicit VivaldiBookmarksAPI(content::BrowserContext* context);
  ~VivaldiBookmarksAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>*
  GetFactoryInstance();

 private:
  // bookmarks::BookmarkModelObserver
  void BookmarkModelLoaded(BookmarkModel* model, bool ids_reassigned) override;

  void BookmarkNodeMoved(BookmarkModel* model,
                         const BookmarkNode* old_parent,
                         size_t old_index,
                         const BookmarkNode* new_parent,
                         size_t new_index) override;
  void BookmarkNodeRemoved(BookmarkModel* model,
                           const BookmarkNode* parent,
                           size_t old_index,
                           const BookmarkNode* node,
                           const std::set<GURL>& no_longer_bookmarked) override;

  void BookmarkNodeAdded(BookmarkModel* model,
                         const BookmarkNode* parent,
                         size_t index) override {}

  // Invoked when the title or url of a node changes.
  void BookmarkNodeChanged(BookmarkModel* model,
                           const BookmarkNode* node) override;
  void OnWillChangeBookmarkMetaInfo(BookmarkModel* model,
                                    const BookmarkNode* node) override;
  void BookmarkMetaInfoChanged(BookmarkModel* model,
                               const BookmarkNode* node) override;
  void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                  const BookmarkNode* node) override;
  void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                     const BookmarkNode* node) override {}
  void BookmarkAllUserNodesRemoved(
      BookmarkModel* model,
      const std::set<GURL>& removed_urls) override {}

  friend class BrowserContextKeyedAPIFactory<VivaldiBookmarksAPI>;

  content::BrowserContext* browser_context_;

  bookmarks::BookmarkModel* bookmark_model_;

  std::unique_ptr<MetaInfoChangeFilter> change_filter_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VivaldiBookmarksAPI"; }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;
};

class BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bookmarksPrivate.updateSpeedDialsForWindowsJumplist",
      BOOKMARKSPRIVATE_UPDATESPEEDDIALSFORWINDOWSJUMPLIST)

  BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction() = default;

 private:
  ~BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction() override =
      default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class BookmarksPrivateEmptyTrashFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarksPrivate.emptyTrash",
                             BOOKMARKSPRIVATE_EMPTYTRASH)

  BookmarksPrivateEmptyTrashFunction() = default;

 private:
  ~BookmarksPrivateEmptyTrashFunction() override = default;

  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

class BookmarksPrivateUpdatePartnersFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarksPrivate.updatePartners",
                             BOOKMARKSPRIVATE_UPDATE_PARTNERS)

  BookmarksPrivateUpdatePartnersFunction() = default;

 private:
  ~BookmarksPrivateUpdatePartnersFunction() override = default;

  // BookmarksFunction:
  ResponseAction Run() override;

  void OnUpdatePartnersResult(bool ok,
                              bool no_version,
                              const std::string& locale);
};

class BookmarksPrivateIsCustomThumbnailFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarksPrivate.isCustomThumbnail",
                             BOOKMARKSPRIVATE_ISCUSTOMTHUMBNAIL)

  BookmarksPrivateIsCustomThumbnailFunction() = default;

 protected:
  ~BookmarksPrivateIsCustomThumbnailFunction() override = default;

 private:
  // BookmarksFunction:
  ResponseValue RunOnReady() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_API_BOOKMARKS_BOOKMARKS_PRIVATE_API_H_
