// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browser_state/model/chrome_browser_state_manager_impl.h"

#import <stdint.h>

#import <utility>

#import "base/check_deref.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "components/optimization_guide/core/optimization_guide_features.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/browser_state/model/chrome_browser_state_impl.h"
#import "ios/chrome/browser/browser_state/model/constants.h"
#import "ios/chrome/browser/browser_state/model/off_the_record_chrome_browser_state_impl.h"
#import "ios/chrome/browser/browser_state_metrics/model/browser_state_metrics.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/page_info/about_this_site_service_factory.h"
#import "ios/chrome/browser/plus_addresses/model/plus_address_service_factory.h"
#import "ios/chrome/browser/push_notification/model/push_notification_browser_state_service_factory.h"
#import "ios/chrome/browser/segmentation_platform/model/segmentation_platform_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/account_consistency_service_factory.h"
#import "ios/chrome/browser/signin/model/account_reconcilor_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/supervised_user/model/child_account_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/list_family_members_service_factory.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_service_factory.h"
#import "ios/chrome/browser/unified_consent/model/unified_consent_service_factory.h"

// vivaldi
#import "ios/chrome/browser/browser_state/vivaldi_post_browser_state_init.h"
// End Vivaldi

namespace {

int64_t ComputeFilesSize(const base::FilePath& directory,
                         const base::FilePath::StringType& pattern) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int64_t running_size = 0;
  base::FileEnumerator iter(directory, false, base::FileEnumerator::FILES,
                            pattern);
  while (!iter.Next().empty()) {
    running_size += iter.GetInfo().GetSize();
  }
  return running_size;
}

// Simple task to log the size of the browser state at `path`.
void BrowserStateSizeTask(const base::FilePath& path) {
  const int64_t kBytesInOneMB = 1024 * 1024;

  int64_t size = ComputeFilesSize(path, FILE_PATH_LITERAL("*"));
  int size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.HistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("History*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TotalHistorySize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Cookies"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.CookiesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Bookmarks"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.BookmarksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Favicons"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.FaviconsSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Top Sites"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.TopSitesSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Visited Links"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.VisitedLinksSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Web Data"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.WebDataSize", size_MB);

  size = ComputeFilesSize(path, FILE_PATH_LITERAL("Extension*"));
  size_MB = static_cast<int>(size / kBytesInOneMB);
  UMA_HISTOGRAM_COUNTS_10000("Profile.ExtensionSize", size_MB);
}

// Gets the user data directory.
base::FilePath GetUserDataDir() {
  base::FilePath user_data_dir;
  bool result = base::PathService::Get(ios::DIR_USER_DATA, &user_data_dir);
  DCHECK(result);
  return user_data_dir;
}

}  // namespace

ChromeBrowserStateManagerImpl::ChromeBrowserStateManagerImpl() {}

ChromeBrowserStateManagerImpl::~ChromeBrowserStateManagerImpl() {}

ChromeBrowserState*
ChromeBrowserStateManagerImpl::GetLastUsedBrowserStateDeprecatedDoNotUse() {
  ChromeBrowserState* browser_state =
      GetBrowserStateByName(GetLastUsedBrowserStateName());
  CHECK(browser_state);
  return browser_state;
}

ChromeBrowserState* ChromeBrowserStateManagerImpl::GetBrowserStateByName(
    const std::string& name) {
  // If the browser state is already loaded, just return it.
  auto iter = browser_states_.find(name);
  if (iter != browser_states_.end()) {
    DCHECK(iter->second.get());
    return iter->second.get();
  }

  return nullptr;
}

ChromeBrowserState* ChromeBrowserStateManagerImpl::GetBrowserStateByPath(
    const base::FilePath& path) {
  DCHECK_EQ(path.DirName(), GetUserDataDir());
  return GetBrowserStateByName(path.BaseName().AsUTF8Unsafe());
}

std::string ChromeBrowserStateManagerImpl::GetLastUsedBrowserStateName() const {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  DCHECK(local_state);
  std::string last_used_browser_state_name =
      local_state->GetString(prefs::kBrowserStateLastUsed);
  if (last_used_browser_state_name.empty()) {
    last_used_browser_state_name = kIOSChromeInitialBrowserState;
  }
  CHECK(!last_used_browser_state_name.empty());
  return last_used_browser_state_name;
}

BrowserStateInfoCache*
ChromeBrowserStateManagerImpl::GetBrowserStateInfoCache() {
  if (!browser_state_info_cache_) {
    browser_state_info_cache_ = std::make_unique<BrowserStateInfoCache>(
        GetApplicationContext()->GetLocalState(), GetUserDataDir());
  }
  return browser_state_info_cache_.get();
}

std::vector<ChromeBrowserState*>
ChromeBrowserStateManagerImpl::GetLoadedBrowserStates() {
  std::vector<ChromeBrowserState*> loaded_browser_states;
  for (const auto& pair : browser_states_) {
    loaded_browser_states.push_back(pair.second.get());
  }
  return loaded_browser_states;
}

void ChromeBrowserStateManagerImpl::LoadBrowserStates() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  const base::Value::List& last_active_browser_states =
      CHECK_DEREF(local_state).GetList(prefs::kBrowserStatesLastActive);

  std::set<std::string> last_active_browser_states_set;
  for (const base::Value& browser_state_id : last_active_browser_states) {
    if (browser_state_id.is_string()) {
      last_active_browser_states_set.insert(browser_state_id.GetString());
    }
  }

  // Ensure that the last used BrowserState is loaded (since client code
  // does not expect GetLastUsedBrowserStateDeprecatedDoNotUse() to return
  // null).
  //
  // See https://crbug.com/345478758 for exemple of crashes happening when
  // the last used BrowserState is not loaded.
  last_active_browser_states_set.insert(GetLastUsedBrowserStateName());

  // Create and load test profiles if experiment enabling Switch Profile
  // developer UI is enabled.
  std::optional<int> load_test_profiles =
      experimental_flags::DisplaySwitchProfile();
  if (load_test_profiles.has_value()) {
    for (int i = 0; i < load_test_profiles; i++) {
      last_active_browser_states_set.insert("TestProfile" +
                                            base::NumberToString(i + 1));
    }
  }

  for (const std::string& browser_state_name : last_active_browser_states_set) {
    LoadBrowserState(browser_state_name, base::DoNothing());
  }
}

void ChromeBrowserStateManagerImpl::OnChromeBrowserStateCreationStarted(
    ChromeBrowserState* browser_state,
    ChromeBrowserState::CreationMode creation_mode) {
  DCHECK(browser_state);
}

void ChromeBrowserStateManagerImpl::OnChromeBrowserStateCreationFinished(
    ChromeBrowserState* browser_state,
    ChromeBrowserState::CreationMode creation_mode,
    bool is_new_browser_state,
    bool success) {
  DCHECK(browser_state);
  DCHECK(success);
}

void ChromeBrowserStateManagerImpl::LoadBrowserState(
    const std::string& name,
    BrowserStateLoadedCallback callback) {
  DCHECK(!base::Contains(browser_states_, name));

  auto [iter, inserted] = browser_states_.insert(std::make_pair(
      name, ChromeBrowserState::CreateBrowserState(
                GetUserDataDir().Append(name), name,
                ChromeBrowserState::CreationMode::kSynchronous, this)));
  DCHECK(inserted);
  DCHECK(iter != browser_states_.end());

  ChromeBrowserState* browser_state = iter->second.get();
  DCHECK(!browser_state->IsOffTheRecord());

  DoFinalInit(browser_state);
  std::move(callback).Run(browser_state);
}

void ChromeBrowserStateManagerImpl::DoFinalInit(
    ChromeBrowserState* browser_state) {
  DoFinalInitForServices(browser_state);
  AddBrowserStateToCache(browser_state);

  // Log the browser state size after a reasonable startup delay.
  base::FilePath path =
      browser_state->GetOriginalChromeBrowserState()->GetStatePath();
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&BrowserStateSizeTask, path), base::Seconds(112));

// vivaldi
  vivaldi::PostBrowserStateInit(browser_state);
// End vivaldi

  LogNumberOfBrowserStates(
      GetApplicationContext()->GetChromeBrowserStateManager());
}

void ChromeBrowserStateManagerImpl::DoFinalInitForServices(
    ChromeBrowserState* browser_state) {
  ios::AccountConsistencyServiceFactory::GetForBrowserState(browser_state);
  IdentityManagerFactory::GetForBrowserState(browser_state)
      ->OnNetworkInitialized();
  ios::AccountReconcilorFactory::GetForBrowserState(browser_state);
  // Initialization needs to happen after the browser context is available
  // because UnifiedConsentService's dependencies needs the URL context getter.
  UnifiedConsentServiceFactory::GetForBrowserState(browser_state);

  // Initialization needs to happen after the browser context is available
  // because because IOSChromeMetricsServiceAccessor requires browser_state
  // to be registered in the ChromeBrowserStateManager.
  if (optimization_guide::features::IsOptimizationHintsEnabled()) {
    OptimizationGuideServiceFactory::GetForBrowserState(browser_state)
        ->DoFinalInit(BackgroundDownloadServiceFactory::GetForBrowserState(
            browser_state));
  }
  segmentation_platform::SegmentationPlatformServiceFactory::GetForBrowserState(
      browser_state);

  PushNotificationBrowserStateServiceFactory::GetForBrowserState(browser_state);

  ChildAccountServiceFactory::GetForBrowserState(browser_state)->Init();
  SupervisedUserServiceFactory::GetForBrowserState(browser_state)->Init();
  ListFamilyMembersServiceFactory::GetForBrowserState(browser_state)->Init();

  // The AboutThisSiteService needs to be created at startup in order to
  // register its OptimizationType with OptimizationGuideDecider.
  AboutThisSiteServiceFactory::GetForBrowserState(browser_state);

  PlusAddressServiceFactory::GetForBrowserState(browser_state);
}

void ChromeBrowserStateManagerImpl::AddBrowserStateToCache(
    ChromeBrowserState* browser_state) {
  DCHECK(!browser_state->IsOffTheRecord());
  DCHECK_EQ(browser_state->GetStatePath().DirName(), GetUserDataDir());
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browser_state);
  const CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  const std::u16string username = base::UTF8ToUTF16(account_info.email);

  BrowserStateInfoCache* cache = GetBrowserStateInfoCache();
  const size_t browser_state_index =
      cache->GetIndexOfBrowserStateWithPath(browser_state->GetStatePath());
  if (browser_state_index != std::string::npos) {
    // The BrowserStateInfoCache's info must match the IdentityManager.
    cache->SetAuthInfoOfBrowserStateAtIndex(browser_state_index,
                                            account_info.gaia, username);
    return;
  }
  cache->AddBrowserState(browser_state->GetStatePath(), account_info.gaia,
                         username);
}
