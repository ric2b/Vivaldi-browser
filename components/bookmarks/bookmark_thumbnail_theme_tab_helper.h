// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_BOOKMARKS_BOOKMARK_THUMBNAIL_THEM_TAB_HELPER_H_
#define COMPONENTS_BOOKMARKS_BOOKMARK_THUMBNAIL_THEM_TAB_HELPER_H_

#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"


namespace vivaldi_bookmark_kit {

class BookmarkThumbnailThemeTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BookmarkThumbnailThemeTabHelper>,
      public bookmarks::BaseBookmarkModelObserver {
 public:
  ~BookmarkThumbnailThemeTabHelper() override;
  BookmarkThumbnailThemeTabHelper(const BookmarkThumbnailThemeTabHelper&) =
      delete;
  BookmarkThumbnailThemeTabHelper& operator=(
      const BookmarkThumbnailThemeTabHelper&) = delete;

 private:
  friend class content::WebContentsUserData<BookmarkThumbnailThemeTabHelper>;
  explicit BookmarkThumbnailThemeTabHelper(content::WebContents* contents);

  // Implementing bookmarks::BaseBookmarkModelObserver
  void BookmarkModelChanged() override {}
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;

  // content::WebContentsObserver implementation
  // Invoked when theme color is changed.
  void DidChangeThemeColor() override;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  void UpdateBookmarkThemeColor();

  raw_ptr<bookmarks::BookmarkModel> bookmark_model_ = nullptr;
  std::vector<GURL> redirect_chain_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};
}  // namespace vivaldi_bookmark_kit
#endif  // COMPONENTS_BOOKMARKS_BOOKMARK_THUMBNAIL_THEM_TAB_HELPER_H_
