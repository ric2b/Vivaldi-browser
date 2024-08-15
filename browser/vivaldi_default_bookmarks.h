// Copyright 2019 Vivaldi Technologies. All rights reserved.
//

#ifndef BROWSER_VIVALDI_DEFAULT_BOOKMARKS_H_
#define BROWSER_VIVALDI_DEFAULT_BOOKMARKS_H_

#include "base/functional/callback.h"

class PrefService;

namespace favicon {
class FaviconService;
}

namespace bookmarks {
class BookmarkModel;
}

// Note: (prio@vivaldi.com) - Chromium is refactoring the bookmarks model on
// CR124 for iOS. This requires iOS to use LegacyBookmarkModel instead of
// BookmarkModel. This might be temporary or not, we don't know yet, depends on
// Chromium to finish their refactoring.
#if !BUILDFLAG(IS_IOS)
using VivaldiBookmarkModelType = bookmarks::BookmarkModel;
#else
class LegacyBookmarkModel;
using VivaldiBookmarkModelType = LegacyBookmarkModel;
#endif

namespace vivaldi_default_bookmarks {

using FaviconServiceGetter =
    base::RepeatingCallback<favicon::FaviconService*()>;

class UpdaterClient {
 public:
  virtual ~UpdaterClient();
  virtual VivaldiBookmarkModelType* GetBookmarkModel() = 0;
  virtual FaviconServiceGetter GetFaviconServiceGetter() = 0;
  virtual PrefService* GetPrefService() = 0;
  virtual const std::string& GetApplicationLocale() = 0;
};

extern bool g_bookmark_update_active;

using UpdateCallback = base::OnceCallback<
    void(bool ok, bool no_version, const std::string& locale)>;
void UpdatePartners(std::unique_ptr<UpdaterClient> client,
                    UpdateCallback callback = UpdateCallback());

}  // namespace vivaldi_default_bookmarks

#endif  // BROWSER_VIVALDI_DEFAULT_BOOKMARKS_H_
