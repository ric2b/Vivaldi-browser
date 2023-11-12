// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/chrome/browser/browser_state/vivaldi_post_browser_state_init.h"

#import "base/memory/raw_ptr.h"
#import "browser/removed_partners_tracker.h"
#import "browser/vivaldi_default_bookmarks.h"
#import "components/keyed_service/core/service_access_type.h"
#import "ios/ad_blocker/adblock_rule_service_factory.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/notes/notes_factory.h"

namespace vivaldi_default_bookmarks {
namespace {
class UpdaterClientImpl : public UpdaterClient {
 public:
  ~UpdaterClientImpl() override;
  UpdaterClientImpl(const UpdaterClientImpl&) = delete;
  UpdaterClientImpl& operator=(const UpdaterClientImpl&) = delete;

  static std::unique_ptr<UpdaterClientImpl> Create(
      ChromeBrowserState* browser_state);

  bookmarks::BookmarkModel* GetBookmarkModel() override;
  FaviconServiceGetter GetFaviconServiceGetter() override;
  PrefService* GetPrefService() override;
  const std::string& GetApplicationLocale() override;

 private:
  UpdaterClientImpl(ChromeBrowserState* browser_state);
  const raw_ptr<ChromeBrowserState> browser_state_;
};

/*static*/
std::unique_ptr<UpdaterClientImpl> UpdaterClientImpl::Create(
    ChromeBrowserState* browser_state) {
  // Allow to upgrade bookmarks even with a private profile as a command line
  // switch can trigger the first window in Vivaldi to be incognito one. So
  // get the original recording profile.
  return std::unique_ptr<UpdaterClientImpl>(
      new UpdaterClientImpl(browser_state->GetOriginalChromeBrowserState()));
}

UpdaterClientImpl::UpdaterClientImpl(ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}
UpdaterClientImpl::~UpdaterClientImpl() = default;

bookmarks::BookmarkModel* UpdaterClientImpl::GetBookmarkModel() {
  return ios::LocalOrSyncableBookmarkModelFactory::
              GetForBrowserState(browser_state_);
}

PrefService* UpdaterClientImpl::GetPrefService() {
  return browser_state_->GetPrefs();
}

const std::string& UpdaterClientImpl::GetApplicationLocale() {
  return GetApplicationContext()->GetApplicationLocale();
}

FaviconServiceGetter UpdaterClientImpl::GetFaviconServiceGetter() {
  auto get_favicon_service = [](ChromeBrowserState* browser_state) {
    return ios::FaviconServiceFactory::GetForBrowserState(
        browser_state, ServiceAccessType::IMPLICIT_ACCESS);
  };
  return base::BindRepeating(get_favicon_service, browser_state_);
}
}  // namespace
}  // namespace vivaldi_default_bookmarks

namespace vivaldi {
void PostBrowserStateInit(ChromeBrowserState* browser_state) {
  vivaldi_partners::RemovedPartnersTracker::Create(
      browser_state->GetPrefs(), ios::LocalOrSyncableBookmarkModelFactory::
                                      GetForBrowserState(browser_state));

  vivaldi_default_bookmarks::UpdatePartners(
      vivaldi_default_bookmarks::UpdaterClientImpl::Create(browser_state));

  NotesModelFactory::GetForBrowserState(browser_state);
  adblock_filter::RuleServiceFactory::GetForBrowserState(browser_state);
}
}  // namespace vivaldi
