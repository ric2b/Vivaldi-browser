// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/chrome/browser/browser_state/vivaldi_post_browser_state_init.h"

#import "base/memory/raw_ptr.h"
#import "browser/removed_partners_tracker.h"
#import "browser/search_engines/vivaldi_search_engines_updater.h"
#import "browser/vivaldi_default_bookmarks.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/ad_blocker/adblock_rule_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/model/favicon_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/notes/notes_factory.h"
#import "ios/translate/vivaldi_ios_translate_client.h"
#import "ios/translate/vivaldi_ios_translate_service.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace vivaldi_default_bookmarks {
namespace {
class UpdaterClientImpl : public UpdaterClient {
 public:
  ~UpdaterClientImpl() override;
  UpdaterClientImpl(const UpdaterClientImpl&) = delete;
  UpdaterClientImpl& operator=(const UpdaterClientImpl&) = delete;

  static std::unique_ptr<UpdaterClientImpl> Create(ProfileIOS* profile);

  bookmarks::BookmarkModel* GetBookmarkModel() override;
  FaviconServiceGetter GetFaviconServiceGetter() override;
  PrefService* GetPrefService() override;
  const std::string& GetApplicationLocale() override;

 private:
  UpdaterClientImpl(ProfileIOS* profile);
  const raw_ptr<ProfileIOS> profile_;
};

/*static*/
std::unique_ptr<UpdaterClientImpl> UpdaterClientImpl::Create(
    ProfileIOS* profile) {
  // Allow to upgrade bookmarks even with a private profile as a command line
  // switch can trigger the first window in Vivaldi to be incognito one. So
  // get the original recording profile.
  return std::unique_ptr<UpdaterClientImpl>(
      new UpdaterClientImpl(profile->GetOriginalProfile()));
}

UpdaterClientImpl::UpdaterClientImpl(ProfileIOS* profile)
    : profile_(profile) {}
UpdaterClientImpl::~UpdaterClientImpl() = default;

bookmarks::BookmarkModel* UpdaterClientImpl::GetBookmarkModel() {
  return ios::BookmarkModelFactory::GetForProfile(profile_);
}

PrefService* UpdaterClientImpl::GetPrefService() {
  return profile_->GetPrefs();
}

const std::string& UpdaterClientImpl::GetApplicationLocale() {
  return GetApplicationContext()->GetApplicationLocale();
}

FaviconServiceGetter UpdaterClientImpl::GetFaviconServiceGetter() {
  auto get_favicon_service = [](ProfileIOS* profile) {
    return ios::FaviconServiceFactory::GetForProfile(
        profile, ServiceAccessType::IMPLICIT_ACCESS);
  };
  return base::BindRepeating(get_favicon_service, profile_);
}
}  // namespace
}  // namespace vivaldi_default_bookmarks

namespace vivaldi {
void PostBrowserStateInit(ProfileIOS* profile) {
  vivaldi::SearchEnginesUpdater::UpdateSearchEngines(
      profile->GetSharedURLLoaderFactory());
  vivaldi::SearchEnginesUpdater::UpdateSearchEnginesPrompt(
      profile->GetSharedURLLoaderFactory());

  vivaldi_partners::RemovedPartnersTracker::Create(
      profile->GetPrefs(),
          ios::BookmarkModelFactory::GetForProfile(profile));

  vivaldi_default_bookmarks::UpdatePartners(
      vivaldi_default_bookmarks::UpdaterClientImpl::Create(profile));

  NotesModelFactory::GetForProfile(profile);
  adblock_filter::RuleServiceFactory::GetForProfile(profile);

  VivaldiIOSTranslateService::Initialize();
  VivaldiIOSTranslateClient::LoadTranslationScript();
}
}  // namespace vivaldi
