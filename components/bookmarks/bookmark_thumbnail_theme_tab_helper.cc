// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "components/bookmarks/bookmark_thumbnail_theme_tab_helper.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "components/bookmarks/vivaldi_bookmark_kit.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace vivaldi_bookmark_kit {

WEB_CONTENTS_USER_DATA_KEY_IMPL(BookmarkThumbnailThemeTabHelper);

BookmarkThumbnailThemeTabHelper::BookmarkThumbnailThemeTabHelper(
    content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<BookmarkThumbnailThemeTabHelper>(*contents),
      bookmark_model_(BookmarkModelFactory::GetForBrowserContext(
          contents->GetBrowserContext())) {
  bookmark_model_->AddObserver(this);
}

BookmarkThumbnailThemeTabHelper::~BookmarkThumbnailThemeTabHelper() = default;

void BookmarkThumbnailThemeTabHelper::BookmarkModelLoaded(
    bool ids_reassigned) {
  UpdateBookmarkThemeColor();
}

void BookmarkThumbnailThemeTabHelper::BookmarkModelBeingDeleted() {
  bookmark_model_ = nullptr;
}

void BookmarkThumbnailThemeTabHelper::DidChangeThemeColor() {
  UpdateBookmarkThemeColor();
}

void BookmarkThumbnailThemeTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() &&
      !navigation_handle->HasCommitted())
    return;

  redirect_chain_ = navigation_handle->GetRedirectChain();
  UpdateBookmarkThemeColor();
}

void BookmarkThumbnailThemeTabHelper::WebContentsDestroyed() {
  bookmark_model_->RemoveObserver(this);
  bookmark_model_ = nullptr;
}

void BookmarkThumbnailThemeTabHelper::UpdateBookmarkThemeColor() {
  if (!bookmark_model_ || !bookmark_model_->loaded())
    return;

  std::optional<SkColor> theme_color = web_contents()->GetThemeColor();
  if (!theme_color)
    return;

  std::vector<base::raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodes;
  for (const GURL& url : redirect_chain_) {
    nodes = bookmark_model_->GetNodesByURL(url);
  }
  for (const auto node : nodes) {
    SetNodeThemeColor(bookmark_model_, node, *theme_color);
  }
}
}  // namespace vivaldi_bookmark_kit