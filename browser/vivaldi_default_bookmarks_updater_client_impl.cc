// Copyright 2022 Vivaldi Technologies. All rights reserved.

#include "browser/vivaldi_default_bookmarks_updater_client_impl.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace vivaldi_default_bookmarks {
/*static*/
std::unique_ptr<UpdaterClientImpl> UpdaterClientImpl::Create(Profile* profile) {
  if (profile->IsGuestSession() || profile->IsSystemProfile()) {
    LOG(ERROR) << "Attempt to update bookmarks from a guest window, or from a "
                  "system-profile.";
    return nullptr;
  }

  // Allow to upgrade bookmarks even with a private profile as a command line
  // switch can trigger the first window in Vivaldi to be incognito one. So
  // get the original recording profile.
  return std::unique_ptr<UpdaterClientImpl>(
      new UpdaterClientImpl(profile->GetOriginalProfile()));
}

UpdaterClientImpl::UpdaterClientImpl(Profile* profile) : profile_(profile) {}
UpdaterClientImpl::~UpdaterClientImpl() = default;

bookmarks::BookmarkModel* UpdaterClientImpl::GetBookmarkModel() {
  return BookmarkModelFactory::GetForBrowserContext(profile_);
}

PrefService* UpdaterClientImpl::GetPrefService() {
  return profile_->GetPrefs();
}

const std::string& UpdaterClientImpl::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

FaviconServiceGetter UpdaterClientImpl::GetFaviconServiceGetter() {
  auto get_favicon_service =
      [](base::WeakPtr<Profile> profile) -> favicon::FaviconService* {
    if (!profile)
      return nullptr;
    return FaviconServiceFactory::GetForProfile(
        profile.get(), ServiceAccessType::IMPLICIT_ACCESS);
  };
  return base::BindRepeating(get_favicon_service, profile_->GetWeakPtr());
}
}  // namespace vivaldi_default_bookmarks
