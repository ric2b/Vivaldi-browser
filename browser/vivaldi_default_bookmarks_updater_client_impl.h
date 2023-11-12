// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef BROWSER_VIVALDI_DEFAULT_BOOKMARKS_CLIENT_IMPL_H_
#define BROWSER_VIVALDI_DEFAULT_BOOKMARKS_CLIENT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "browser/vivaldi_default_bookmarks.h"

class Profile;

namespace vivaldi_default_bookmarks {
class UpdaterClientImpl: public UpdaterClient {
 public:
  ~UpdaterClientImpl() override;
  UpdaterClientImpl(const UpdaterClientImpl&) = delete;
  UpdaterClientImpl& operator=(const UpdaterClientImpl&) = delete;

  static std::unique_ptr<UpdaterClientImpl> Create(Profile* profile);

  bookmarks::BookmarkModel* GetBookmarkModel() override;
  FaviconServiceGetter GetFaviconServiceGetter() override;
  PrefService* GetPrefService() override;
  const std::string& GetApplicationLocale() override;

 private:
  UpdaterClientImpl(Profile* profile);
  const raw_ptr<Profile> profile_;
};
}

#endif  // BROWSER_VIVALDI_DEFAULT_BOOKMARKS_CLIENT_IMPL_H_