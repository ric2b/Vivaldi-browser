// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_BOOKMARKS_BOOKMARKS_PRIVATE_API_H_
#define EXTENSIONS_API_BOOKMARKS_BOOKMARKS_PRIVATE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"

namespace extensions {

class BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bookmarksPrivate.updateSpeedDialsForWindowsJumplist",
      BOOKMARKSPRIVATE_UPDATESPEEDDIALSFORWINDOWSJUMPLIST);

  BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction();

 protected:
  ~BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction() override;

 private:
  // BookmarksFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(
      BookmarksPrivateUpdateSpeedDialsForWindowsJumplistFunction);
};

class BookmarksPrivateEmptyTrashFunction : public BookmarksFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bookmarksPrivate.emptyTrash",
                             BOOKMARKSPRIVATE_EMPTYTRASH);

  BookmarksPrivateEmptyTrashFunction();

 protected:
  ~BookmarksPrivateEmptyTrashFunction() override;

 private:
  // BookmarksFunction:
  bool RunOnReady() override;

  DISALLOW_COPY_AND_ASSIGN(BookmarksPrivateEmptyTrashFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_BOOKMARKS_VIVALDI_BOOKMARKS_API_H_
