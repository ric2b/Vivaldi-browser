// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_mediator.h"

#import "base/ios/ios_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/common/bookmark_pref_names.h"
#import "components/browsing_data/core/browsing_data_utils.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper.h"
#import "components/language/ios/browser/ios_language_detection_tab_helper_observer_bridge.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/profile_metrics/browser_profile_type.h"
#import "components/reading_list/core/reading_list_model.h"
#import "components/reading_list/ios/reading_list_model_bridge_observer.h"
#import "components/search_engines/template_url_service.h"
#import "components/supervised_user/core/common/features.h"
#import "components/supervised_user/core/common/supervised_user_constants.h"
#import "components/sync/service/sync_service.h"
#import "components/translate/core/browser/translate_manager.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_model_bridge_observer.h"
#import "ios/chrome/browser/commerce/model/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/find_in_page/model/abstract_find_tab_helper.h"
#import "ios/chrome/browser/follow/model/follow_browser_agent.h"
#import "ios/chrome/browser/follow/model/follow_menu_updater.h"
#import "ios/chrome/browser/follow/model/follow_tab_helper.h"
#import "ios/chrome/browser/follow/model/follow_util.h"
#import "ios/chrome/browser/intents/intents_donation_helper.h"
#import "ios/chrome/browser/iph_for_new_chrome_user/model/tab_based_iph_browser_agent.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/ntp/shared/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/policy/ui_bundled/user_policy_util.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/settings/model/sync/utils/identity_error_util.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/activity_service_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/overflow_menu_customization_commands.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/public/commands/popup_menu_commands.h"
#import "ios/chrome/browser/shared/public/commands/price_notifications_commands.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/reading_list_add_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/text_zoom_commands.h"
#import "ios/chrome/browser/shared/public/commands/whats_new_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_capabilities.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/destination_usage_history/destination_usage_history.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_constants.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_metrics.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_orderer.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/overflow_menu_swift.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_utils.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_browser_agent.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ios/web/common/user_agent.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "components/bookmarks/vivaldi_bookmark_kit.h"
#import "ios/translate/vivaldi_ios_translate_client.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_constants.h"
#import "ios/ui/context_menu/vivaldi_context_menu_constants.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/vivaldi_overflow_menu/vivaldi_oveflow_menu_constants.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

using base::RecordAction;
using base::UmaHistogramEnumeration;
using base::UserMetricsAction;
using experimental_flags::IsSpotlightDebuggingEnabled;

namespace {

// Approximate number of visible page actions by default.
const unsigned int kDefaultVisiblePageActionCount = 3u;

// Struct used to count and store the number of active WhatsNew badges,
// as the FET does not support showing multiple badges for the same FET feature
// at the same time.
struct WhatsNewActiveMenusData : public base::SupportsUserData::Data {
  // The number of active menus.
  int activeMenus = 0;

  // Key to use for this type in SupportsUserData
  static constexpr char key[] = "WhatsNewActiveMenusData";
};

typedef void (^Handler)(void);

OverflowMenuFooter* CreateOverflowMenuManagedFooter(
    int nameID,
    int linkID,
    NSString* accessibilityIdentifier,
    NSString* imageName,
    Handler handler) {
  NSString* name = l10n_util::GetNSString(nameID);
  NSString* link = l10n_util::GetNSString(linkID);
  return [[OverflowMenuFooter alloc] initWithName:name
                                             link:link
                                            image:[UIImage imageNamed:imageName]
                          accessibilityIdentifier:accessibilityIdentifier
                                          handler:handler];
}

}  // namespace

@interface OverflowMenuMediator () <BookmarkModelBridgeObserver,
                                    CRWWebStateObserver,
                                    FollowMenuUpdater,
                                    IOSLanguageDetectionTabHelperObserving,
                                    OverflowMenuDestinationProvider,
                                    OverlayPresenterObserving,
                                    PrefObserverDelegate,
                                    ReadingListModelBridgeObserver,
                                    SearchEngineObserving,
                                    WebStateListObserving> {
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;

  // Observer for the content area overlay events
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;

  // Bridge to register for bookmark changes.
  std::unique_ptr<BookmarkModelBridge> _bookmarkModelBridge;

  // Bridge to register for reading list model changes.
  std::unique_ptr<ReadingListModelBridge> _readingListModelBridge;

  // Bridge to get notified of the language detection event.
  std::unique_ptr<language::IOSLanguageDetectionTabHelperObserverBridge>
      _iOSLanguageDetectionTabHelperObserverBridge;

  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;
  // Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}

// The current web state.
@property(nonatomic, assign) web::WebState* webState;

// Whether or not the menu has been dismissed. Sometimes, the menu takes some
// time to dismiss after requesting dismissal, leading to errors were menu
// options are selected twice or at the wrong times (see crbug.com/1500367)
@property(nonatomic, assign) BOOL menuHasBeenDismissed;

// Whether an overlay is currently presented over the web content area.
@property(nonatomic, assign) BOOL webContentAreaShowingOverlay;

// Whether the web content is currently being blocked.
@property(nonatomic, assign) BOOL contentBlocked;

@property(nonatomic, strong) OverflowMenuDestination* bookmarksDestination;
@property(nonatomic, strong) OverflowMenuDestination* downloadsDestination;
@property(nonatomic, strong) OverflowMenuDestination* historyDestination;
@property(nonatomic, strong) OverflowMenuDestination* passwordsDestination;
@property(nonatomic, strong)
    OverflowMenuDestination* priceNotificationsDestination;
@property(nonatomic, strong) OverflowMenuDestination* readingListDestination;
@property(nonatomic, strong) OverflowMenuDestination* recentTabsDestination;
@property(nonatomic, strong) OverflowMenuDestination* settingsDestination;
@property(nonatomic, strong) OverflowMenuDestination* siteInfoDestination;
@property(nonatomic, strong) OverflowMenuDestination* whatsNewDestination;
@property(nonatomic, strong)
    OverflowMenuDestination* spotlightDebuggerDestination;

@property(nonatomic, strong) OverflowMenuActionGroup* appActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* pageActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* helpActionsGroup;
@property(nonatomic, strong) OverflowMenuActionGroup* editActionsGroup;

@property(nonatomic, strong) OverflowMenuAction* reloadAction;
@property(nonatomic, strong) OverflowMenuAction* stopLoadAction;
@property(nonatomic, strong) OverflowMenuAction* openTabAction;
@property(nonatomic, strong) OverflowMenuAction* openIncognitoTabAction;
@property(nonatomic, strong) OverflowMenuAction* openNewWindowAction;

@property(nonatomic, strong) OverflowMenuAction* clearBrowsingDataAction;
@property(nonatomic, strong) OverflowMenuAction* followAction;
@property(nonatomic, strong) OverflowMenuAction* addBookmarkAction;
@property(nonatomic, strong) OverflowMenuAction* editBookmarkAction;
@property(nonatomic, strong) OverflowMenuAction* readLaterAction;
@property(nonatomic, strong) OverflowMenuAction* translateAction;
@property(nonatomic, strong) OverflowMenuAction* requestDesktopAction;
@property(nonatomic, strong) OverflowMenuAction* requestMobileAction;
@property(nonatomic, strong) OverflowMenuAction* findInPageAction;
@property(nonatomic, strong) OverflowMenuAction* textZoomAction;

@property(nonatomic, strong) OverflowMenuAction* reportIssueAction;
@property(nonatomic, strong) OverflowMenuAction* helpAction;
@property(nonatomic, strong) OverflowMenuAction* shareChromeAction;

@property(nonatomic, strong) OverflowMenuAction* editActionsAction;
@property(nonatomic, strong) OverflowMenuAction* lensOverlayAction;

// Vivaldi
@property(nonatomic, strong) OverflowMenuActionGroup* vivaldiActionsGroup;

@property(nonatomic, strong) OverflowMenuDestination* shareDestination;
@property(nonatomic, strong) OverflowMenuDestination* textZoomDestination;
@property(nonatomic, strong) OverflowMenuDestination* findInPageDestination;
@property(nonatomic, strong) OverflowMenuDestination* desktopSiteDestination;

@property(nonatomic, strong) OverflowMenuAction* addPageToAction;
@property(nonatomic, strong) OverflowMenuAction* editPageAction;
@property(nonatomic, strong) OverflowMenuAction* bookmarksAction;
@property(nonatomic, strong) OverflowMenuAction* notesAction;
@property(nonatomic, strong) OverflowMenuAction* historyAction;
@property(nonatomic, strong) OverflowMenuAction* readingListAction;
@property(nonatomic, strong) OverflowMenuAction* downloadsAction;
// End Vivaldi

@end

@implementation OverflowMenuMediator

- (instancetype)init {
  self = [super init];
  if (self) {
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
  }
  return self;
}

- (void)disconnect {
  // Remove the model link so the other deallocations don't update the model
  // and thus the UI as the UI is dismissing.
  self.model = nil;
  self.menuOrderer = nil;

  self.webContentAreaOverlayPresenter = nullptr;

  if (_engagementTracker) {
    if (self.readingListDestination.badge != BadgeTypeNone) {
      _engagementTracker->Dismissed(
          feature_engagement::kIPHBadgedReadingListFeature);
    }

    if (self.whatsNewDestination.badge != BadgeTypeNone) {
      // Check if this is the last active menu with WhatsNew badge and dismiss
      // the FET if so.
      WhatsNewActiveMenusData* data = static_cast<WhatsNewActiveMenusData*>(
          _engagementTracker->GetUserData(WhatsNewActiveMenusData::key));
      if (data) {
        data->activeMenus--;
        if (data->activeMenus <= 0) {
          _engagementTracker->Dismissed(
              feature_engagement::kIPHWhatsNewUpdatedFeature);
          _engagementTracker->RemoveUserData(WhatsNewActiveMenusData::key);
        }
      } else {
        _engagementTracker->Dismissed(
            feature_engagement::kIPHWhatsNewUpdatedFeature);
      }
    }

    _engagementTracker = nullptr;
  }

  self.followBrowserAgent = nullptr;

  self.webState = nullptr;
  self.webStateList = nullptr;

  self.bookmarkModel = nullptr;
  self.readingListModel = nullptr;
  self.profilePrefs = nullptr;
  self.localStatePrefs = nullptr;

  self.syncService = nullptr;
  _searchEngineObserver.reset();
}

#pragma mark - Property getters/setters

- (void)setModel:(OverflowMenuModel*)model {
  _model = model;
  if (_model) {
    [self initializeModel];
    [self updateModelItemsState];
    // Any state that is required for re-ordering the menu overall (e.g. badges)
    // must be ready by this point. After this, the only order-based changes
    // that will be observed are those that show/hide whole destinations.
    [_menuOrderer reorderDestinationsForInitialMenu];
    [self updateModel];
  }
}

- (void)setMenuOrderer:(OverflowMenuOrderer*)menuOrderer {
  if (self.menuOrderer.destinationProvider == self) {
    self.menuOrderer.destinationProvider = nil;
  }
  if (self.menuOrderer.actionProvider == self) {
    self.menuOrderer.actionProvider = nil;
  }
  _menuOrderer = menuOrderer;
  self.menuOrderer.destinationProvider = self;
  self.menuOrderer.actionProvider = self;

  [self updateModel];
}

- (void)setIsIncognito:(BOOL)isIncognito {
  _isIncognito = isIncognito;
  [self updateModel];
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserver.get());

    if (self.followAction) {
      FollowTabHelper* followTabHelper =
          FollowTabHelper::FromWebState(_webState);
      if (followTabHelper) {
        followTabHelper->RemoveFollowMenuUpdater();
      }
    }
  }

  _webState = webState;

  if (_webState) {
    [self updateModel];
    _webState->AddObserver(_webStateObserver.get());

    // Observe the language::IOSLanguageDetectionTabHelper for `_webState`.
    _iOSLanguageDetectionTabHelperObserverBridge =
        std::make_unique<language::IOSLanguageDetectionTabHelperObserverBridge>(
            language::IOSLanguageDetectionTabHelper::FromWebState(_webState),
            self);

    FollowTabHelper* followTabHelper = FollowTabHelper::FromWebState(_webState);
    if (followTabHelper) {
      followTabHelper->SetFollowMenuUpdater(self);
    }
  }
}

- (void)setWebStateList:(WebStateList*)webStateList {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());

    _iOSLanguageDetectionTabHelperObserverBridge.reset();
  }

  _webStateList = webStateList;

  self.webState = (_webStateList) ? _webStateList->GetActiveWebState() : nil;

  if (_webStateList) {
    _webStateList->AddObserver(_webStateListObserver.get());
  }
}

- (void)setBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel {
  _bookmarkModelBridge.reset();

  _bookmarkModel = bookmarkModel;

  if (bookmarkModel) {
    _bookmarkModelBridge =
        std::make_unique<BookmarkModelBridge>(self, bookmarkModel);
  }

  [self updateModel];
}

- (void)setReadingListModel:(ReadingListModel*)readingListModel {
  _readingListModelBridge.reset();

  _readingListModel = readingListModel;

  if (readingListModel) {
    _readingListModelBridge =
        std::make_unique<ReadingListModelBridge>(self, readingListModel);
  }

  [self updateModel];
}

- (void)setProfilePrefs:(PrefService*)profilePrefs {
  _prefObserverBridge.reset();
  _prefChangeRegistrar.reset();

  _profilePrefs = profilePrefs;

  if (_profilePrefs) {
    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(profilePrefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    _prefObserverBridge->ObserveChangesForPreference(
        bookmarks::prefs::kEditBookmarksEnabled, _prefChangeRegistrar.get());
  }
}

- (void)setEngagementTracker:(feature_engagement::Tracker*)engagementTracker {
  _engagementTracker = engagementTracker;

  if (!engagementTracker) {
    return;
  }

  [self updateModel];
}

- (void)setWebContentAreaOverlayPresenter:
    (OverlayPresenter*)webContentAreaOverlayPresenter {
  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->RemoveObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay = NO;
  }

  _webContentAreaOverlayPresenter = webContentAreaOverlayPresenter;

  if (_webContentAreaOverlayPresenter) {
    _webContentAreaOverlayPresenter->AddObserver(
        _overlayPresenterObserver.get());
    self.webContentAreaShowingOverlay =
        _webContentAreaOverlayPresenter->IsShowingOverlayUI();
  }
}

- (void)setWebContentAreaShowingOverlay:(BOOL)webContentAreaShowingOverlay {
  if (_webContentAreaShowingOverlay == webContentAreaShowingOverlay)
    return;
  _webContentAreaShowingOverlay = webContentAreaShowingOverlay;
  [self updateModel];
}

- (void)setSyncService:(syncer::SyncService*)syncService {
  _syncService = syncService;

  if (!syncService) {
    return;
  }

  [self updateModel];
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (_templateURLService) {
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    [self searchEngineChanged];
  } else {
    _searchEngineObserver.reset();
  }
}

#pragma mark - Model Creation

- (void)initializeModel {
  __weak __typeof(self) weakSelf = self;

  // Bookmarks destination.
  self.bookmarksDestination = [self newBookmarksDestination];

  // Downloads destination.
  self.downloadsDestination = [self newDownloadsDestination];

  // History destination.
  self.historyDestination = [self newHistoryDestination];

  // Passwords destination.
  self.passwordsDestination = [self newPasswordsDestination];

  // Price Tracking destination.
  self.priceNotificationsDestination = [self newPriceNotificationsDestination];

  // Reading List destination.
  self.readingListDestination = [self newReadingListDestination];

  // Recent Tabs destination.
  self.recentTabsDestination = [self newRecentTabsDestination];

  // Settings destination.
  self.settingsDestination = [self newSettingsDestination];

  self.spotlightDebuggerDestination = [self newSpotlightDebuggerDestination];

  // WhatsNew destination.
  self.whatsNewDestination = [self newWhatsNewDestination];

  // Site Info destination.
  self.siteInfoDestination = [self newSiteInfoDestination];

  if (IsVivaldiRunning()) {
    self.shareDestination = [self newShareDestination];
    self.textZoomDestination = [self newZoomTextDestination];
    self.findInPageDestination = [self newFindInPageDestination];
    self.desktopSiteDestination =
        [self userAgentType] != web::UserAgentType::DESKTOP ?
            [self newDesktopSiteDestination] : [self newMobileSiteDestination];
  } // End Vivaldi

  [self logTranslateAvailability];

  self.reloadAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_RELOAD
                                    actionType:overflow_menu::ActionType::Reload

#if defined(VIVALDI_BUILD)
                                    symbolName:vOverflowReload
                                  systemSymbol:NO
#else
                                    symbolName:kArrowClockWiseSymbol
                                  systemSymbol:NO
#endif // End Vivaldi

                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuReload
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf reload];
                                       }];

  self.stopLoadAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_STOP
                                    actionType:overflow_menu::ActionType::Reload

#if defined(VIVALDI_BUILD)
                                    symbolName:vOverflowStop
                                  systemSymbol:NO
#else
                                    symbolName:kXMarkSymbol
                                  systemSymbol:YES
#endif // End Vivaldi

                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuStop
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf stopLoading];
                                       }];

  self.openTabAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_NEW_TAB
                                    actionType:overflow_menu::ActionType::NewTab

#if defined(VIVALDI_BUILD)
                                    symbolName:vOverflowNewTab
                                  systemSymbol:NO
#else
                                    symbolName:kPlusInCircleSymbol
                                  systemSymbol:YES
#endif // End Vivaldi

                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuNewTabId
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf openTab];
                                       }];

  self.openIncognitoTabAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB
                              actionType:overflow_menu::ActionType::
                                             NewIncognitoTab

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowNewPrivateTab
                            systemSymbol:NO
#else
                              symbolName:kIncognitoSymbol
                            systemSymbol:NO
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuNewIncognitoTabId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf openIncognitoTab];
                                 }];

  self.openNewWindowAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_NEW_WINDOW
                              actionType:overflow_menu::ActionType::NewWindow

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowNewWindow
                            systemSymbol:NO
#else
                              symbolName:kNewWindowActionSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuNewWindowId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf openNewWindow];
                                 }];

  self.clearBrowsingDataAction = [self newClearBrowsingDataAction];

  if (GetFollowActionState(self.webState) != FollowActionStateHidden) {
    OverflowMenuAction* action = [self newFollowAction];

    action.enabled = NO;
    self.followAction = action;
  }

  self.addBookmarkAction = [self newAddBookmarkAction];

  NSString* editBookmarkHideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_EDIT_BOOKMARK);
  self.editBookmarkAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_EDIT_BOOKMARK
                              actionType:overflow_menu::ActionType::Bookmark

#if defined(VIVALDI_BUILD)
                                    symbolName:vOverflowEditBookmark
                                  systemSymbol:NO
#else
                              symbolName:kEditActionSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuEditBookmark
                            hideItemText:editBookmarkHideItemText
                                 handler:^{
                                   [weakSelf addOrEditBookmark];
                                 }];

  self.readLaterAction = [self newReadLaterAction];

  self.translateAction = [self newTranslateAction];

  self.requestDesktopAction = [self newRequestDesktopAction];

  NSString* requestMobileHideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_MOBILE_SITE);
  self.requestMobileAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_REQUEST_MOBILE_SITE
                              actionType:overflow_menu::ActionType::DesktopSite

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowMobileSite
                            systemSymbol:NO
#else
                              symbolName:kIPhoneSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:YES
                         accessibilityID:kToolsMenuRequestMobileId
                            hideItemText:requestMobileHideItemText
                                 handler:^{
                                   [weakSelf requestMobileSite];
                                 }];

  self.findInPageAction = [self newFindInPageAction];

  self.textZoomAction = [self newTextZoomAction];

#if defined(VIVALDI_BUILD)
   self.reportIssueAction =
       [self createOverflowMenuActionWithNameID:IDS_IOS_FEEDBACK_MENU_TITLE
                                     actionType:overflow_menu::ActionType::
                                                    ReportAnIssue
                                    symbolName:vOverflowShareFeedback
                                  systemSymbol:NO
#else
  self.reportIssueAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_OPTIONS_REPORT_AN_ISSUE
                                    actionType:overflow_menu::ActionType::
                                                   ReportAnIssue
                                    symbolName:kWarningSymbol
                                  systemSymbol:YES
#endif // End Vivaldi

                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuReportAnIssueId
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf reportAnIssue];
                                       }];

  self.helpAction =
      [self createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_HELP_MOBILE
                                    actionType:overflow_menu::ActionType::Help

#if defined(VIVALDI_BUILD)
                                    symbolName:vOverflowHelp
                                  systemSymbol:NO
#else
                                    symbolName:kHelpSymbol
                                  systemSymbol:YES
#endif // End Vivaldi

                              monochromeSymbol:NO
                               accessibilityID:kToolsMenuHelpId
                                  hideItemText:nil
                                       handler:^{
                                         [weakSelf openHelp];
                                       }];

  self.shareChromeAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_OVERFLOW_MENU_SHARE_CHROME
                              actionType:overflow_menu::ActionType::ShareChrome

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowShare
                            systemSymbol:NO
#else
                              symbolName:kShareSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuShareChromeId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf shareChromeApp];
                                 }];

  self.editActionsAction = [self
      createOverflowMenuActionWithNameID:IDS_IOS_OVERFLOW_MENU_EDIT_ACTIONS
                              actionType:overflow_menu::ActionType::EditActions
                              symbolName:nil
                            systemSymbol:NO
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuEditActionsId
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf beginCustomization];
                                 }];
  if (IsLensOverlayAvailable()) {
    self.lensOverlayAction = [self openLensOverlayAction];
  }
  self.editActionsAction.automaticallyUnhighlight = NO;
  self.editActionsAction.useButtonStyling = YES;

  // The app actions vary based on page state, so they are set in
  // `-updateModel`.
  self.appActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"app_actions"
                                                 actions:@[]
                                                  footer:nil];

  // The page actions vary based on page state, so they are set in
  // `-updateModel`.
  self.pageActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"page_actions"
                                                 actions:@[]
                                                  footer:nil];
  self.menuOrderer.pageActionsGroup = self.pageActionsGroup;

  // Footer and actions vary based on state, so they're set in -updateModel.
  self.helpActionsGroup =
      [[OverflowMenuActionGroup alloc] initWithGroupName:@"help_actions"
                                                 actions:@[]
                                                  footer:nil];

  self.editActionsGroup = [[OverflowMenuActionGroup alloc]
      initWithGroupName:@"edit_actions"
                actions:@[ self.editActionsAction ]
                 footer:nil];

  if (IsVivaldiRunning()) {
    NSMutableArray* actionGroups = [[NSMutableArray alloc] init];

    self.addPageToAction = [self vivaldiAddPageToAction];
    self.editPageAction = [self vivaldiEditPageAction];

    // The action group for vivaldi that contains the panel items and downloads.
    self.bookmarksAction = [self vivaldiBookmarksAction];
    self.notesAction = [self vivaldiNotesAction];
    self.historyAction = [self vivaldiHistoryAction];
    self.readingListAction = [self vivaldiReadingListAction];
    self.downloadsAction = [self vivaldiDownloadsAction];

    NSArray<OverflowMenuAction*>* vivaldiActions = @[
      self.bookmarksAction,
      self.notesAction,
      self.historyAction,
      self.readingListAction,
      self.downloadsAction
    ];

    self.vivaldiActionsGroup =
        [[OverflowMenuActionGroup alloc] initWithGroupName:@"vivaldi_actions"
                                                   actions:vivaldiActions
                                                    footer:nil];
    self.menuOrderer.vivaldiActionsGroup = self.vivaldiActionsGroup;

    [actionGroups
        addObjectsFromArray:@[ self.appActionsGroup,
                               self.pageActionsGroup,
                               self.vivaldiActionsGroup,
                               self.editActionsGroup,
                               self.helpActionsGroup]];
    self.model.actionGroups = actionGroups;
    return;
  } // End Vivaldi

  self.model.actionGroups = @[
    self.appActionsGroup, self.pageActionsGroup, self.editActionsGroup,
    self.helpActionsGroup
  ];
}

- (OverflowMenuAction*)newFollowAction {
  return [self
      createOverflowMenuActionWithName:l10n_util::GetNSString(
                                           IDS_IOS_TOOLS_MENU_CUSTOMIZE_FOLLOW)
                            actionType:overflow_menu::ActionType::Follow
                            symbolName:kPlusSymbol
                          systemSymbol:YES
                      monochromeSymbol:NO
                       accessibilityID:kToolsMenuFollow
                          hideItemText:
                              l10n_util::GetNSStringF(
                                  IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_FOLLOW, u"")
                               handler:^{
                               }];
}

- (OverflowMenuAction*)newAddBookmarkAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText = l10n_util::GetNSString(
      IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_ADD_TO_BOOKMARKS);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_ADD_TO_BOOKMARKS
                              actionType:overflow_menu::ActionType::Bookmark

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowAddBookmark
                            systemSymbol:NO
#else
                              symbolName:kAddBookmarkActionSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuAddToBookmarks
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf addOrEditBookmark];
                                 }];
}

- (OverflowMenuAction*)openLensOverlayAction {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_CONTENT_CONTEXT_OPENLENSOVERLAY
                              actionType:overflow_menu::ActionType::LensOverlay
                              symbolName:kCameraLensSymbol
                            systemSymbol:NO
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuOpenLensOverlay
                            hideItemText:nil
                                 handler:^{
                                   [weakSelf startLensOverlay];
                                 }];
}

- (OverflowMenuAction*)newReadLaterAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_READING_LIST);
  return [self
      createOverflowMenuActionWithNameID:
          IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST
                              actionType:overflow_menu::ActionType::ReadingList

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowAddReadingList
                            systemSymbol:NO
#else
                              symbolName:kReadLaterActionSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuReadLater
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf addToReadingList];
                                 }];
}

- (OverflowMenuAction*)newClearBrowsingDataAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText = l10n_util::GetNSString(
      IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_CLEAR_BROWSING_DATA);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_CLEAR_BROWSING_DATA
                              actionType:overflow_menu::ActionType::
                                             ClearBrowsingData

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowClearHistory
                            systemSymbol:NO
#else
                              symbolName:kTrashSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuClearBrowsingData
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf openClearBrowsingData];
                                 }];
}

- (OverflowMenuAction*)newTranslateAction {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_TRANSLATE
                              actionType:overflow_menu::ActionType::Translate
#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowTranslate
#else
                              symbolName:kTranslateSymbol
#endif // End Vivaldi
                            systemSymbol:NO
                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuTranslateId
                            hideItemText:
                                l10n_util::GetNSString(
                                    IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_TRANSLATE)
                                 handler:^{
                                   [weakSelf translatePage];
                                 }];
}

- (OverflowMenuAction*)newRequestDesktopAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_DESKTOP_SITE);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_REQUEST_DESKTOP_SITE
                              actionType:overflow_menu::ActionType::DesktopSite

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowDesktopSite
                            systemSymbol:NO
#else
                              symbolName:kDesktopSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:YES
                         accessibilityID:kToolsMenuRequestDesktopId
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf requestDesktopSite];
                                 }];
}

- (OverflowMenuAction*)newFindInPageAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_FIND_IN_PAGE);
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_FIND_IN_PAGE
                              actionType:overflow_menu::ActionType::FindInPage

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowFindInPage
                            systemSymbol:NO
#else
                              symbolName:kFindInPageActionSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuFindInPageId
                            hideItemText:hideItemText
                                 handler:^{
                                   [weakSelf openFindInPage];
                                 }];
}

- (OverflowMenuAction*)newTextZoomAction {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_TEXT_ZOOM
                              actionType:overflow_menu::ActionType::TextZoom

#if defined(VIVALDI_BUILD)
                              symbolName:vOverflowZoom
                            systemSymbol:NO
#else
                              symbolName:kZoomTextActionSymbol
                            systemSymbol:YES
#endif // End Vivaldi

                        monochromeSymbol:NO
                         accessibilityID:kToolsMenuTextZoom
                            hideItemText:
                                l10n_util::GetNSString(
                                    IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_TEXT_ZOOM)
                                 handler:^{
                                   [weakSelf openTextZoom];
                                 }];
}

- (OverflowMenuDestination*)newBookmarksDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_BOOKMARKS
                              destination:overflow_menu::Destination::Bookmarks

#if defined(VIVALDI_BUILD)
                               symbolName:vOverflowBookmarks
                             systemSymbol:NO
#else
                               symbolName:kBookmarksSymbol
                             systemSymbol:YES
#endif // End Vivaldi

                          accessibilityID:kToolsMenuBookmarksId
                                  handler:^{
                                    [weakSelf openBookmarks];
                                  }];
}

- (OverflowMenuDestination*)newHistoryDestination {
  __weak __typeof(self) weakSelf = self;
  return [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_HISTORY
                                 destination:overflow_menu::Destination::History

#if defined(VIVALDI_BUILD)
                                  symbolName:vOverflowHistory
                                systemSymbol:NO
#else
                                  symbolName:kHistorySymbol
                                systemSymbol:YES
#endif // End Vivaldi

                             accessibilityID:kToolsMenuHistoryId
                                     handler:^{
                                       [weakSelf openHistory];
                                     }];
}

- (OverflowMenuDestination*)newReadingListDestination {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_READING_LIST
                        destination:overflow_menu::Destination::ReadingList

#if defined(VIVALDI_BUILD)
                         symbolName:vOverflowReadingList
                       systemSymbol:NO
#else
                         symbolName:kReadingListSymbol
                       systemSymbol:NO
#endif // End Vivaldi

                    accessibilityID:kToolsMenuReadingListId
                            handler:^{
                              [weakSelf openReadingList];
                            }];
}

- (OverflowMenuDestination*)newPasswordsDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_PASSWORD_MANAGER
                              destination:overflow_menu::Destination::Passwords

#if defined(VIVALDI_BUILD)
                               symbolName:vOverflowPasswords
                             systemSymbol:NO
#else
                               symbolName:kPasswordSymbol
                             systemSymbol:NO
#endif // End Vivaldi

                          accessibilityID:kToolsMenuPasswordsId
                                  handler:^{
                                    [weakSelf openPasswords];
                                  }];
}

- (OverflowMenuDestination*)newDownloadsDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_DOWNLOADS
                              destination:overflow_menu::Destination::Downloads

#if defined(VIVALDI_BUILD)
                               symbolName:vOverflowDownloads
                             systemSymbol:NO
#else
                               symbolName:kDownloadSymbol
                             systemSymbol:YES
#endif // End Vivaldi

                          accessibilityID:kToolsMenuDownloadsId
                                  handler:^{
                                    [weakSelf openDownloads];
                                  }];
}

- (OverflowMenuDestination*)newRecentTabsDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_RECENT_TABS
                              destination:overflow_menu::Destination::RecentTabs

#if defined(VIVALDI_BUILD)
                               symbolName:vOverflowRecentTabs
                             systemSymbol:NO
#else
                               symbolName:kRecentTabsSymbol
                             systemSymbol:NO
#endif // End Vivaldi

                          accessibilityID:kToolsMenuOtherDevicesId
                                  handler:^{
                                    [weakSelf openRecentTabs];
                                  }];
}

- (OverflowMenuDestination*)newSiteInfoDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SITE_INFORMATION
                              destination:overflow_menu::Destination::SiteInfo

#if defined(VIVALDI_BUILD)
                               symbolName:vATBShield
                             systemSymbol:NO
                          accessibilityID:kToolsMenuSiteInformation
                                  handler:^{
                                    [weakSelf showSiteTrackerBlockerPrefs];
                                  }];
  destination.canBeHidden = YES;
#else
                               symbolName:kTunerSymbol
                             systemSymbol:NO
                          accessibilityID:kToolsMenuSiteInformation
                                  handler:^{
                                    [weakSelf openSiteInformation];
                                  }];

  destination.canBeHidden = NO;
#endif // End Vivaldi

  return destination;
}

- (OverflowMenuDestination*)newSettingsDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_SETTINGS
                              destination:overflow_menu::Destination::Settings

#if defined(VIVALDI_BUILD)
                               symbolName:vOverflowSettings
                             systemSymbol:NO
#else
                               symbolName:kSettingsSymbol
                             systemSymbol:YES
#endif // End Vivaldi

                          accessibilityID:kToolsMenuSettingsId
                                  handler:^{
                                    [weakSelf openSettings];
                                  }];
  destination.canBeHidden = NO;
  return destination;
}

- (OverflowMenuDestination*)newWhatsNewDestination {
  __weak __typeof(self) weakSelf = self;
  return
      [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_WHATS_NEW
                              destination:overflow_menu::Destination::WhatsNew
                               symbolName:kCheckmarkSealSymbol
                             systemSymbol:YES
                          accessibilityID:kToolsMenuWhatsNewId
                                  handler:^{
                                    [weakSelf openWhatsNew];
                                  }];
}

- (OverflowMenuDestination*)newSpotlightDebuggerDestination {
  __weak __typeof(self) weakSelf = self;
  return [self destinationForSpotlightDebugger:^{
    [weakSelf openSpotlightDebugger];
  }];
}

- (OverflowMenuDestination*)newPriceNotificationsDestination {
  __weak __typeof(self) weakSelf = self;
  return [self createOverflowMenuDestination:IDS_IOS_TOOLS_MENU_PRICE_TRACKING
                                 destination:overflow_menu::Destination::
                                                 PriceNotifications
                                  symbolName:kDownTrendSymbol
                                systemSymbol:NO
                             accessibilityID:kToolsMenuPriceNotifications
                                     handler:^{
                                       [weakSelf openPriceNotifications];
                                     }];
}

- (NSString*)hideItemTextForDestination:
    (overflow_menu::Destination)destination {
  switch (destination) {
    case overflow_menu::Destination::SiteInfo:
    case overflow_menu::Destination::Settings:
    case overflow_menu::Destination::SpotlightDebugger:
      // These items are unhideable.
      return nil;
    case overflow_menu::Destination::Bookmarks:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_BOOKMARKS);
    case overflow_menu::Destination::History:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_HISTORY);
    case overflow_menu::Destination::ReadingList:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_READING_LIST);
    case overflow_menu::Destination::Passwords:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_PASSWORDS);
    case overflow_menu::Destination::Downloads:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_DOWNLOADS);
    case overflow_menu::Destination::RecentTabs:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_RECENT_TABS);
    case overflow_menu::Destination::WhatsNew:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_WHATS_NEW);
    case overflow_menu::Destination::PriceNotifications:
      return l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_PRICE_NOTIFICATIONS);

#if defined(VIVALDI_BUILD)
    case overflow_menu::Destination::vShare:
      return nil;
    case overflow_menu::Destination::vZoomText:
      return l10n_util::GetNSString(
          IDS_VIVALDI_OVERFLOW_MENU_HIDE_DESTINATION_TEXT_ZOOM);
    case overflow_menu::Destination::vFindInPage:
      return l10n_util::GetNSString(
          IDS_VIVALDI_OVERFLOW_MENU_HIDE_DESTINATION_FIND_IN_PAGE);
    case overflow_menu::Destination::vDesktopSite:
      return [self userAgentType] != web::UserAgentType::DESKTOP ?
          l10n_util::GetNSString(
              IDS_VIVALDI_OVERFLOW_MENU_HIDE_DESTINATION_DESKTOP_SITE) :
          l10n_util::GetNSString(
              IDS_VIVALDI_OVERFLOW_MENU_HIDE_DESTINATION_MOBILE_SITE);
#endif // End Vivaldi

  }
}

#pragma mark - Model Creation Utilities

// Creates an OverflowMenuDestination to be displayed in the destinations
// carousel.
- (OverflowMenuDestination*)
    createOverflowMenuDestination:(int)nameID
                      destination:(overflow_menu::Destination)destination
                       symbolName:(NSString*)symbolName
                     systemSymbol:(BOOL)systemSymbol
                  accessibilityID:(NSString*)accessibilityID
                          handler:(Handler)handler {
  __weak __typeof(self) weakSelf = self;

  NSString* name = l10n_util::GetNSString(nameID);

  auto handlerWithMetrics = ^{
    if (weakSelf.menuHasBeenDismissed) {
      return;
    }
    overflow_menu::RecordUmaActionForDestination(destination);

    [weakSelf.menuOrderer recordClickForDestination:destination];

    [weakSelf logFeatureEngagementEventForClickOnDestination:destination];

    handler();
  };

  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:name
                                         symbolName:symbolName
                                       systemSymbol:systemSymbol
                                   monochromeSymbol:NO
                            accessibilityIdentifier:accessibilityID
                                 enterpriseDisabled:NO
                                displayNewLabelIcon:NO
                                            handler:handlerWithMetrics];

  result.destination = static_cast<NSInteger>(destination);

  NSMutableArray<OverflowMenuLongPressItem*>* longPressItems =
      [[NSMutableArray alloc] init];

  NSString* hideItemText = [self hideItemTextForDestination:destination];
  if (hideItemText) {
    [longPressItems addObject:[[OverflowMenuLongPressItem alloc]
                                  initWithTitle:hideItemText
                                     symbolName:@"eye.slash"
                                        handler:^{
                                          [weakSelf
                                              hideDestination:destination];
                                        }]];
  }
  [longPressItems
      addObject:[[OverflowMenuLongPressItem alloc]
                    initWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_OVERFLOW_MENU_EDIT_ACTIONS)
                       symbolName:@"pencil"
                          handler:^{
                            [weakSelf beginCustomization];
                          }]];
  result.longPressItems = longPressItems;

  __weak __typeof(result) weakResult = result;
  result.onShownToggleCallback = ^{
    [weakSelf onShownToggledForDestination:weakResult];
  };

  return result;
}

// Creates an OverflowMenuAction with the given name to be displayed.
- (OverflowMenuAction*)
    createOverflowMenuActionWithName:(NSString*)name
                          actionType:(overflow_menu::ActionType)actionType
                          symbolName:(NSString*)symbolName
                        systemSymbol:(BOOL)systemSymbol
                    monochromeSymbol:(BOOL)monochromeSymbol
                     accessibilityID:(NSString*)accessibilityID
                        hideItemText:(NSString*)hideItemText
                             handler:(Handler)handler {
  Handler newHandler =
      [self fullOverflowMenuActionHandlerForActionType:actionType
                                               handler:handler];

  OverflowMenuAction* action =
      [[OverflowMenuAction alloc] initWithName:name
                                    symbolName:symbolName
                                  systemSymbol:systemSymbol
                              monochromeSymbol:monochromeSymbol
                       accessibilityIdentifier:accessibilityID
                            enterpriseDisabled:NO
                           displayNewLabelIcon:NO
                                       handler:newHandler];
  action.actionType = static_cast<NSInteger>(actionType);

  ActionRanking reorderableActions = [self basePageActions];

  if (IsVivaldiRunning()) {
      ActionRanking vivaldiActions = [self vivaldiActions];
      reorderableActions.insert(
          reorderableActions.end(),
          vivaldiActions.begin(), vivaldiActions.end());
  } // End Vivaldi

  // If this action is not reorderable, then don't add any longpress items.
  bool actionIsReorderable =
      std::find(reorderableActions.begin(), reorderableActions.end(),
                actionType) != reorderableActions.end();
  if (actionIsReorderable) {
    action.longPressItems =
        [self actionLongPressItemsForActionType:actionType
                                   hideItemText:hideItemText];
  }
  return action;
}

// Creates an OverflowMenuAction with the given nameID as a localized string ID
// to be displayed.
- (OverflowMenuAction*)
    createOverflowMenuActionWithNameID:(int)nameID
                            actionType:(overflow_menu::ActionType)actionType
                            symbolName:(NSString*)symbolName
                          systemSymbol:(BOOL)systemSymbol
                      monochromeSymbol:(BOOL)monochromeSymbol
                       accessibilityID:(NSString*)accessibilityID
                          hideItemText:(NSString*)hideItemText
                               handler:(Handler)handler {
  NSString* name = l10n_util::GetNSString(nameID);

  return [self createOverflowMenuActionWithName:name
                                     actionType:actionType
                                     symbolName:symbolName
                                   systemSymbol:systemSymbol
                               monochromeSymbol:monochromeSymbol
                                accessibilityID:accessibilityID
                                   hideItemText:hideItemText
                                        handler:handler];
}

// Adds any necessary additions to the handler for any specific action.
- (Handler)fullOverflowMenuActionHandlerForActionType:
               (overflow_menu::ActionType)actionType
                                              handler:(Handler)handler {
  __weak __typeof(self) weakSelf = self;
  return ^{
    if (weakSelf.menuHasBeenDismissed) {
      return;
    }
    [weakSelf logFeatureEngagementEventForClickOnAction:actionType];
    handler();
  };
}

// Returns the LongPress items for the given action and hide item text. Can
// be used if actions need to update their name after action creation, as
// the hide item text should correspond to the name.
- (NSArray<OverflowMenuLongPressItem*>*)
    actionLongPressItemsForActionType:(overflow_menu::ActionType)actionType
                         hideItemText:(NSString*)hideItemText {
  __weak __typeof(self) weakSelf = self;
  NSMutableArray<OverflowMenuLongPressItem*>* longPressItems =
      [[NSMutableArray alloc] init];
  if (hideItemText) {
    [longPressItems addObject:[[OverflowMenuLongPressItem alloc]
                                  initWithTitle:hideItemText
                                     symbolName:@"eye.slash"
                                        handler:^{
                                          [weakSelf hideActionType:actionType];
                                        }]];
  }
  [longPressItems
      addObject:[[OverflowMenuLongPressItem alloc]
                    initWithTitle:l10n_util::GetNSString(
                                      IDS_IOS_OVERFLOW_MENU_EDIT_ACTIONS)
                       symbolName:@"pencil"
                          handler:^{
                            [weakSelf
                                beginCustomizationFromActionType:actionType];
                          }]];
  return [longPressItems copy];
}

#pragma mark - Private

// Creates an OverflowMenuDestination for the Spotlight debugger.
- (OverflowMenuDestination*)destinationForSpotlightDebugger:(Handler)handler {
  OverflowMenuDestination* result =
      [[OverflowMenuDestination alloc] initWithName:@"Spotlight Debugger"
                                         symbolName:kSettingsSymbol
                                       systemSymbol:YES
                                   monochromeSymbol:NO
                            accessibilityIdentifier:@"Spotlight Debugger"
                                 enterpriseDisabled:NO
                                displayNewLabelIcon:NO
                                            handler:handler];
  result.destination =
      static_cast<NSInteger>(overflow_menu::Destination::SpotlightDebugger);
  return result;
}

- (DestinationRanking)baseDestinations {

  if (IsVivaldiRunning()) {
    std::vector<overflow_menu::Destination> destinations = {
      overflow_menu::Destination::vShare,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::vFindInPage,
      overflow_menu::Destination::Settings,
      overflow_menu::Destination::vZoomText,
      overflow_menu::Destination::History,
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::vDesktopSite,
    };
    return destinations;
  } else {
  std::vector<overflow_menu::Destination> destinations = {
      overflow_menu::Destination::Bookmarks,
      overflow_menu::Destination::History,
      overflow_menu::Destination::ReadingList,
      overflow_menu::Destination::Passwords,
      overflow_menu::Destination::Downloads,
      overflow_menu::Destination::RecentTabs,
      overflow_menu::Destination::SiteInfo,
      overflow_menu::Destination::Settings,
      overflow_menu::Destination::PriceNotifications,
      overflow_menu::Destination::WhatsNew,
  };

  return destinations;
  } // End Vivaldi

}

// Returns YES if the Overflow Menu should indicate an identity error.
- (BOOL)shouldIndicateIdentityError {
  if (!self.syncService) {
    return NO;
  }

  return ShouldIndicateIdentityErrorInOverflowMenu(self.syncService);
}

// Updates the model to match the current page state.
- (void)updateModel {
  // First update the items' states, and then update all the orders.
  [self updateModelItemsState];
  [self updateModelOrdering];
}

// Updates the state of the individual model items (actions, destinations,
// group footers).
- (void)updateModelItemsState {
  // If the model hasn't been created, there's no need to update.
  if (!self.model) {
    return;
  }

  bool hasMachineLevelPolicies =
      _browserPolicyConnector &&
      _browserPolicyConnector->HasMachineLevelPolicies();
  bool canFetchUserPolicies =
      _authenticationService && _profilePrefs &&
      CanFetchUserPolicy(_authenticationService, _profilePrefs);
  // Set footer (on last section), if any.
  web::BrowserState* browserState =
      self.webState ? self.webState->GetBrowserState() : nullptr;
  ProfileIOS* profile = ProfileIOS::FromBrowserState(browserState);
  if (hasMachineLevelPolicies || canFetchUserPolicies) {
    // Set the Enterprise footer if there are machine level or user level
    // (aka ProfileIOS level) policies.
    self.helpActionsGroup.footer = CreateOverflowMenuManagedFooter(
        IDS_IOS_TOOLS_MENU_ENTERPRISE_MANAGED,
        IDS_IOS_TOOLS_MENU_ENTERPRISE_LEARN_MORE, kTextMenuEnterpriseInfo,
        @"overflow_menu_footer_managed", ^{
          [self enterpriseLearnMore];
        });
  } else if (profile && supervised_user::IsSubjectToParentalControls(profile)) {
    self.helpActionsGroup.footer = CreateOverflowMenuManagedFooter(
        IDS_IOS_TOOLS_MENU_PARENT_MANAGED, IDS_IOS_TOOLS_MENU_PARENT_LEARN_MORE,
        kTextMenuFamilyLinkInfo, @"overflow_menu_footer_family_link", ^{
          [self parentLearnMore];
        });
  } else {
    self.helpActionsGroup.footer = nil;
  }

  // The "Add to Reading List" functionality requires JavaScript execution,
  // which is paused while overlays are displayed over the web content area.
  self.readLaterAction.enabled =
      !self.webContentAreaShowingOverlay && [self isCurrentURLWebURL];

  BOOL bookmarkEnabled =
      [self isCurrentURLWebURL] && [self isEditBookmarksEnabled];
  self.addBookmarkAction.enabled = bookmarkEnabled;
  self.editBookmarkAction.enabled = bookmarkEnabled;
  self.translateAction.enabled = [self isTranslateEnabled];
  self.findInPageAction.enabled = [self isFindInPageEnabled];
  self.textZoomAction.enabled = [self isTextZoomEnabled];
  self.requestDesktopAction.enabled =
      [self userAgentType] == web::UserAgentType::MOBILE;
  self.requestMobileAction.enabled =
      [self userAgentType] == web::UserAgentType::DESKTOP;

  // Enable/disable items based on enterprise policies.
  self.openTabAction.enterpriseDisabled =
      IsIncognitoModeForced(self.profilePrefs);
  self.openIncognitoTabAction.enterpriseDisabled =
      IsIncognitoModeDisabled(self.profilePrefs);

  if (IsLensOverlayAvailable()) {
    self.lensOverlayAction.enabled =
        search_engines::SupportsSearchImageWithLens(self.templateURLService) &&
        !IsCompactHeight(self.baseViewController.traitCollection);
  }

  if (IsVivaldiRunning()) {
    self.shareDestination.disabled = ![self currentWebPageSupportsSiteInfo];
    self.siteInfoDestination.disabled = ![self currentWebPageSupportsSiteInfo];
    self.textZoomDestination.disabled = ![self isTextZoomEnabled];
    self.findInPageDestination.disabled = ![self isFindInPageEnabled];
    self.desktopSiteDestination.disabled =
        ([self userAgentType] != web::UserAgentType::MOBILE &&
            [self userAgentType] != web::UserAgentType::DESKTOP);

    self.addPageToAction.submenuActions = [self addToSubmenuActions];
    self.editPageAction.submenuActions = [self editSubmenuActions];

    self.addPageToAction.enabled =
        bookmarkEnabled || self.readLaterAction.enabled;
    self.editPageAction.enabled = bookmarkEnabled;
  } // End Vivaldi

}

// Updates the order of the items in each section or group.
- (void)updateModelOrdering {
  // If the model hasn't been created, there's no need to update.
  if (!self.model) {
    return;
  }

  [self.menuOrderer updateDestinations];

  NSMutableArray<OverflowMenuAction*>* appActions =
      [[NSMutableArray alloc] init];

  // The reload/stop action is only shown when the reload button is not in the
  // toolbar. The reload button is shown in the toolbar when the toolbar is not
  // split.
  if (IsSplitToolbarMode(self.baseViewController)) {
    OverflowMenuAction* reloadStopAction =
        ([self isPageLoading]) ? self.stopLoadAction : self.reloadAction;
    [appActions addObject:reloadStopAction];
  }

  [appActions
      addObjectsFromArray:@[ self.openTabAction, self.openIncognitoTabAction ]];

  if (base::ios::IsMultipleScenesSupported()) {
    [appActions addObject:self.openNewWindowAction];
  }

  self.appActionsGroup.actions = appActions;

  [self.menuOrderer updatePageActions];

  NSMutableArray<OverflowMenuAction*>* helpActions =
      [[NSMutableArray alloc] init];

  if (ios::provider::IsUserFeedbackSupported()) {
    [helpActions addObject:self.reportIssueAction];
  }

  [helpActions addObject:self.helpAction];

  if (IsVivaldiRunning()) {
    [helpActions addObject:self.reportIssueAction];
  } // End Vivaldi

  [helpActions addObject:self.shareChromeAction];

  if (IsVivaldiRunning()) {
    [self.menuOrderer updateVivaldiActions];
  } // End Vivaldi

  self.helpActionsGroup.actions = helpActions;
}

// Returns whether the page can be manually translated. If `forceMenuLogging` is
// true the translate client will log this result.
- (BOOL)canManuallyTranslate:(BOOL)forceMenuLogging {
  if (!self.webState) {
    return NO;
  }

#if defined(VIVALDI_BUILD)
  auto* translate_client =
      VivaldiIOSTranslateClient::FromWebState(self.webState);
#else
  auto* translate_client =
      ChromeIOSTranslateClient::FromWebState(self.webState);
#endif // End Vivaldi

  if (!translate_client) {
    return NO;
  }

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  DCHECK(translate_manager);
  return translate_manager->CanManuallyTranslate(forceMenuLogging);
}

// Returns whether translate is enabled on the current page.
- (BOOL)isTranslateEnabled {
  return [self canManuallyTranslate:NO];
}

// Determines whether or not translate is available on the page and logs the
// result. This method should only be called once per popup menu shown.
- (void)logTranslateAvailability {
  [self canManuallyTranslate:YES];
}

// Whether find in page is enabled.
- (BOOL)isFindInPageEnabled {
  if (!self.webState) {
    return NO;
  }

  auto* helper = GetConcreteFindTabHelperFromWebState(self.webState);
  return (helper && helper->CurrentPageSupportsFindInPage() &&
          !helper->IsFindUIActive());
}

// Whether or not text zoom is enabled for this page.
- (BOOL)isTextZoomEnabled {
  if (self.webContentAreaShowingOverlay) {
    return NO;
  }

  if (!self.webState) {
    return NO;
  }

  if (IsVivaldiRunning()) {
    web::NavigationItem* navItem =
        self.webState->GetNavigationManager()->GetVisibleItem();
    if (!navItem) {
      return NO;
    }
    const GURL& URL = navItem->GetURL();

    // Do not show Text Zoom for NTP.
    if (URL.spec() == kChromeUIAboutNewTabURL ||
        URL.spec() == kChromeUINewTabURL) {
      return NO;
    }
  } // End Vivadi

  FontSizeTabHelper* helper = FontSizeTabHelper::FromWebState(self.webState);
  return helper && helper->CurrentPageSupportsTextZoom() &&
         !helper->IsTextZoomUIActive();
}

// Returns YES if user is allowed to edit any bookmarks.
- (BOOL)isEditBookmarksEnabled {
  return self.profilePrefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled);
}

// Whether the page is currently loading.
- (BOOL)isPageLoading {
  return (self.webState) ? self.webState->IsLoading() : NO;
}

// Whether the current page is a web page.
- (BOOL)isCurrentURLWebURL {
  if (!self.webState) {
    return NO;
  }

  const GURL& URL = self.webState->GetLastCommittedURL();
  return URL.is_valid() && !web::GetWebClient()->IsAppSpecificURL(URL);
}

// Whether the current web page has available site info.
- (BOOL)currentWebPageSupportsSiteInfo {
  if (!self.webState) {
    return NO;
  }
  web::NavigationItem* navItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!navItem) {
    return NO;
  }
  const GURL& URL = navItem->GetURL();
  // Show site info for offline pages.
  if (reading_list::IsOfflineURL(URL)) {
    return YES;
  }
  // Do not show site info for NTP.
  if (URL.spec() == kChromeUIAboutNewTabURL ||
      URL.spec() == kChromeUINewTabURL) {
    return NO;
  }

  if (self.contentBlocked) {
    return NO;
  }

  return navItem->GetVirtualURL().is_valid();
}

// Returns the UserAgentType currently in use.
- (web::UserAgentType)userAgentType {
  if (!self.webState) {
    return web::UserAgentType::NONE;
  }
  web::NavigationItem* visibleItem =
      self.webState->GetNavigationManager()->GetVisibleItem();
  if (!visibleItem) {
    return web::UserAgentType::NONE;
  }

  return visibleItem->GetUserAgentType();
}

// Creates a follow action if needed, when the follow action state is not
// hidden.
- (OverflowMenuAction*)createFollowActionIfNeeded {
  // Returns nil if the follow action state is hidden.
  if (GetFollowActionState(self.webState) == FollowActionStateHidden) {
    return nil;
  }

  OverflowMenuAction* action = [self newFollowAction];
  action.enabled = NO;
  return action;
}

- (void)dismissMenu {
  self.menuHasBeenDismissed = YES;
  [self.popupMenuHandler dismissPopupMenuAnimated:YES];
}

// Possibly logs a feature engagement tracker event when the user clicks on a
// destination.
- (void)logFeatureEngagementEventForClickOnDestination:
    (overflow_menu::Destination)destination {
  if (DestinationWasInitiallyVisible(
          destination, self.model.destinations,
          self.menuOrderer.visibleDestinationsCount)) {
    return;
  }

  if (self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);
  }
}

// Possibly logs a feature engagement tracker event when the user clicks on an
// action.
- (void)logFeatureEngagementEventForClickOnAction:
    (overflow_menu::ActionType)action {
  if (ActionWasInitiallyVisible(action, self.pageActionsGroup.actions,
                                kDefaultVisiblePageActionCount)) {
    return;
  }

  if (self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kIOSOverflowMenuOffscreenItemUsed);
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webState:(web::WebState*)webState
    didStartNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidStartLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidStopLoading:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webState:(web::WebState*)webState
    didChangeLoadingProgress:(double)progress {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidChangeBackForwardState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDidChangeVisibleSecurityState:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  [self updateModel];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  self.webState = nullptr;
}

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  if (!status.active_web_state_change()) {
    return;
  }

  self.webState = status.new_active_web_state;
  if (self.webState && self.followAction) {
    FollowTabHelper* followTabHelper =
        FollowTabHelper::FromWebState(self.webState);
    if (followTabHelper) {
      followTabHelper->SetFollowMenuUpdater(self);
    }
  }
}

#pragma mark - BookmarkModelBridgeObserver

// If an added or removed bookmark is the same as the current url, update the
// toolbar so the star highlight is kept in sync.
- (void)didChangeChildrenForNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateModel];
}

// If all bookmarks are removed, update the toolbar so the star highlight is
// kept in sync.
- (void)bookmarkModelRemovedAllNodes {
  [self updateModel];
}

// In case we are on a bookmarked page before the model is loaded.
- (void)bookmarkModelLoaded {
  [self updateModel];
}

- (void)didChangeNode:(const bookmarks::BookmarkNode*)bookmarkNode {
  [self updateModel];
}
- (void)didMoveNode:(const bookmarks::BookmarkNode*)bookmarkNode
         fromParent:(const bookmarks::BookmarkNode*)oldParent
           toParent:(const bookmarks::BookmarkNode*)newParent {
  // No-op -- required by BookmarkModelBridgeObserver but not used.
}
- (void)didDeleteNode:(const bookmarks::BookmarkNode*)node
           fromFolder:(const bookmarks::BookmarkNode*)folder {
  [self updateModel];
}

#pragma mark - ReadingListModelBridgeObserver

- (void)readingListModelLoaded:(const ReadingListModel*)model {
  [self updateModel];
}

- (void)readingListModelDidApplyChanges:(const ReadingListModel*)model {
  [self updateModel];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self updateModel];
}

- (void)templateURLServiceShuttingDown:(TemplateURLService*)urlService {
  _templateURLService = nullptr;
}

#pragma mark - FollowMenuUpdater

- (void)updateFollowMenuItemWithWebPage:(WebPageURLs*)webPageURLs
                               followed:(BOOL)followed
                             domainName:(NSString*)domainName
                                enabled:(BOOL)enable {
  DCHECK(IsWebChannelsEnabled());
  self.followAction.enabled = enable;
  if (followed) {
    __weak __typeof(self) weakSelf = self;
    self.followAction.name = l10n_util::GetNSStringF(
        IDS_IOS_TOOLS_MENU_UNFOLLOW, base::SysNSStringToUTF16(domainName));
    self.followAction.symbolName = kXMarkSymbol;
    self.followAction.handler = [self
        fullOverflowMenuActionHandlerForActionType:overflow_menu::ActionType::
                                                       Follow
                                           handler:^{
                                             [weakSelf
                                                 unfollowWebPage:webPageURLs];
                                           }];
    NSString* hideItemText =
        l10n_util::GetNSStringF(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_UNFOLLOW,
                                base::SysNSStringToUTF16(domainName));
    self.followAction.longPressItems = [self
        actionLongPressItemsForActionType:overflow_menu::ActionType::Follow
                             hideItemText:hideItemText];
  } else {
    __weak __typeof(self) weakSelf = self;
    self.followAction.name = l10n_util::GetNSStringF(
        IDS_IOS_TOOLS_MENU_FOLLOW, base::SysNSStringToUTF16(domainName));
    self.followAction.symbolName = kPlusSymbol;
    self.followAction.handler = [self
        fullOverflowMenuActionHandlerForActionType:overflow_menu::ActionType::
                                                       Follow
                                           handler:^{
                                             [weakSelf
                                                 followWebPage:webPageURLs];
                                           }];
    NSString* hideItemText =
        l10n_util::GetNSStringF(IDS_IOS_OVERFLOW_MENU_HIDE_ACTION_FOLLOW,
                                base::SysNSStringToUTF16(domainName));
    self.followAction.longPressItems = [self
        actionLongPressItemsForActionType:overflow_menu::ActionType::Follow
                             hideItemText:hideItemText];
  }
}

#pragma mark - BrowserContainerConsumer

- (void)setContentBlocked:(BOOL)contentBlocked {
  if (_contentBlocked == contentBlocked) {
    return;
  }
  _contentBlocked = contentBlocked;
  [self updateModel];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == bookmarks::prefs::kEditBookmarksEnabled)
    [self updateModel];
}

#pragma mark - IOSLanguageDetectionTabHelperObserving

- (void)iOSLanguageDetectionTabHelper:
            (language::IOSLanguageDetectionTabHelper*)tabHelper
                 didDetermineLanguage:
                     (const translate::LanguageDetectionDetails&)details {
  [self updateModel];
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    willShowOverlayForRequest:(OverlayRequest*)request
          initialPresentation:(BOOL)initialPresentation {
  self.webContentAreaShowingOverlay = YES;
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  self.webContentAreaShowingOverlay = NO;
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  self.webContentAreaOverlayPresenter = nullptr;
}

#pragma mark - OverflowMenuDestinationProvider

- (OverflowMenuDestination*)destinationForDestinationType:
    (overflow_menu::Destination)destinationType {
  switch (destinationType) {
    case overflow_menu::Destination::Bookmarks:
      return self.bookmarksDestination;
    case overflow_menu::Destination::History:
      return (self.isIncognito) ? nil : self.historyDestination;
    case overflow_menu::Destination::ReadingList:
      // Set badges if necessary.
      if (self.engagementTracker &&
          self.engagementTracker->ShouldTriggerHelpUI(
              feature_engagement::kIPHBadgedReadingListFeature)) {
        self.readingListDestination.badge = BadgeTypePromo;
      }
      return self.readingListModel->loaded() ? self.readingListDestination
                                             : nil;
    case overflow_menu::Destination::Passwords:
      return self.passwordsDestination;
    case overflow_menu::Destination::Downloads:
      return self.downloadsDestination;
    case overflow_menu::Destination::RecentTabs:

      if (IsVivaldiRunning())
        return self.recentTabsDestination; // End Vivaldi

      return self.isIncognito ? nil : self.recentTabsDestination;

#if defined(VIVALDI_BUILD)
    case overflow_menu::Destination::vShare:
      return self.shareDestination;
    case overflow_menu::Destination::vZoomText:
      return self.textZoomDestination;
    case overflow_menu::Destination::vFindInPage:
      return self.findInPageDestination;
    case overflow_menu::Destination::vDesktopSite:
      return self.desktopSiteDestination;
#endif // End Vivaldi

    case overflow_menu::Destination::SiteInfo:

      if (IsVivaldiRunning()) {
        return self.siteInfoDestination;
      } // End Vivaldi

      return ([self currentWebPageSupportsSiteInfo]) ? self.siteInfoDestination
                                                     : nil;
    case overflow_menu::Destination::Settings:
      if ([self shouldIndicateIdentityError]) {
        self.settingsDestination.badge = BadgeTypeError;
      } else if (self.hasSettingsBlueDot) {
        self.settingsDestination.badge = BadgeTypePromo;
      }
      return self.settingsDestination;
    case overflow_menu::Destination::WhatsNew:
      // Possibly set the new label badge if it was never used before.
      if (self.whatsNewDestination.badge == BadgeTypeNone &&
          !WasWhatsNewUsed() && self.engagementTracker) {
        // First check if another active menu (e.g. in another window) has an
        // active badge. If so, just set the badge here without querying the
        // FET. Only query the FET if there is no currently active badge.
        WhatsNewActiveMenusData* data = static_cast<WhatsNewActiveMenusData*>(
            self.engagementTracker->GetUserData(WhatsNewActiveMenusData::key));
        if (data) {
          self.whatsNewDestination.badge = BadgeTypeNew;
          data->activeMenus++;
        } else if (self.engagementTracker->ShouldTriggerHelpUI(
                       feature_engagement::kIPHWhatsNewUpdatedFeature)) {
          std::unique_ptr<WhatsNewActiveMenusData> new_data =
              std::make_unique<WhatsNewActiveMenusData>();
          new_data->activeMenus++;
          self.engagementTracker->SetUserData(WhatsNewActiveMenusData::key,
                                              std::move(new_data));
          self.whatsNewDestination.badge = BadgeTypeNew;
        }
      }
      return self.whatsNewDestination;
    case overflow_menu::Destination::SpotlightDebugger:
      return self.spotlightDebuggerDestination;
    case overflow_menu::Destination::PriceNotifications:
      BOOL priceNotificationsActive =
          self.webState && IsPriceTrackingEnabled(ProfileIOS::FromBrowserState(
                               self.webState->GetBrowserState()));
      return (priceNotificationsActive) ? self.priceNotificationsDestination
                                        : nil;
  }
}

- (OverflowMenuDestination*)customizationDestinationForDestinationType:
    (overflow_menu::Destination)destinationType {
  switch (destinationType) {
    case overflow_menu::Destination::Bookmarks:
      return [self newBookmarksDestination];
    case overflow_menu::Destination::History:
      return [self newHistoryDestination];
    case overflow_menu::Destination::ReadingList:
      return [self newReadingListDestination];
    case overflow_menu::Destination::Passwords:
      return [self newPasswordsDestination];
    case overflow_menu::Destination::Downloads:
      return [self newDownloadsDestination];
    case overflow_menu::Destination::RecentTabs:
      return [self newRecentTabsDestination];
    case overflow_menu::Destination::SiteInfo:
      return [self newSiteInfoDestination];
    case overflow_menu::Destination::Settings:
      return [self newSettingsDestination];
    case overflow_menu::Destination::WhatsNew:
      return [self newWhatsNewDestination];
    case overflow_menu::Destination::SpotlightDebugger:
      return [self newSpotlightDebuggerDestination];
    case overflow_menu::Destination::PriceNotifications:
      return [self newPriceNotificationsDestination];

#if defined(VIVALDI_BUILD)
    case overflow_menu::Destination::vShare:
      return [self newShareDestination];
    case overflow_menu::Destination::vZoomText:
      return [self newZoomTextDestination];
    case overflow_menu::Destination::vFindInPage:
      return [self newFindInPageDestination];
    case overflow_menu::Destination::vDesktopSite:
      return [self newDesktopSiteDestination];
#endif // End Vivaldi

  }
}

- (void)destinationCustomizationCompleted {
  if (self.engagementTracker &&
      self.settingsDestination.badge == BadgeTypePromo) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kBlueDotOverflowMenuCustomized);
    [self.popupMenuHandler updateToolsMenuBlueDotVisibility];
  }
}

#pragma mark - OverflowMenuActionProvider

- (ActionRanking)basePageActions {
  if (IsVivaldiRunning()) {
    return {
      overflow_menu::ActionType::vAddPageTo,
      overflow_menu::ActionType::vEditPage,
      overflow_menu::ActionType::ClearBrowsingData,
      overflow_menu::ActionType::Translate,
      overflow_menu::ActionType::DesktopSite,
      overflow_menu::ActionType::FindInPage,
      overflow_menu::ActionType::TextZoom,
    };
  } // End Vivaldi

  ActionRanking actions = {
      overflow_menu::ActionType::Follow,
      overflow_menu::ActionType::Bookmark,
      overflow_menu::ActionType::ReadingList,
      overflow_menu::ActionType::ClearBrowsingData,
      overflow_menu::ActionType::Translate,
      overflow_menu::ActionType::DesktopSite,
      overflow_menu::ActionType::FindInPage,
      overflow_menu::ActionType::TextZoom,
  };

  if (IsLensOverlayAvailable()) {
    actions.push_back(overflow_menu::ActionType::LensOverlay);
  }

  return actions;
}

- (OverflowMenuAction*)actionForActionType:
    (overflow_menu::ActionType)actionType {
  switch (actionType) {
    case overflow_menu::ActionType::Reload:
      return ([self isPageLoading]) ? self.stopLoadAction : self.reloadAction;
    case overflow_menu::ActionType::NewTab:
      return self.openTabAction;
    case overflow_menu::ActionType::NewIncognitoTab:
      return self.openIncognitoTabAction;
    case overflow_menu::ActionType::NewWindow:
      return self.openNewWindowAction;
    case overflow_menu::ActionType::Follow: {
      // Try to create the followAction if there isn't one. It's possible that
      // sometimes when creating the model the followActionState is hidden so
      // the followAction hasn't been created but at the time when updating the
      // model, the followAction should be valid.
      if (!self.followAction) {
        self.followAction = [self createFollowActionIfNeeded];
        DCHECK(!self.followAction || self.webState != nullptr);
      }

      if (self.followAction) {
        FollowTabHelper* followTabHelper =
            FollowTabHelper::FromWebState(self.webState);
        if (followTabHelper) {
          followTabHelper->UpdateFollowMenuItem();
        }
      }
      return self.followAction;
    }
    case overflow_menu::ActionType::Bookmark: {
      BOOL pageIsBookmarked =
          self.webState && self.bookmarkModel &&
          self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL());
      return (pageIsBookmarked) ? self.editBookmarkAction
                                : self.addBookmarkAction;
    }
    case overflow_menu::ActionType::ReadingList:
      return self.readLaterAction;
    case overflow_menu::ActionType::ClearBrowsingData:
      // Showing the Clear Browsing Data Action would be confusing in incognito.
      return (self.isIncognito) ? nil : self.clearBrowsingDataAction;
    case overflow_menu::ActionType::Translate:
      return self.translateAction;
    case overflow_menu::ActionType::DesktopSite:
      return ([self userAgentType] != web::UserAgentType::DESKTOP)
                 ? self.requestDesktopAction
                 : self.requestMobileAction;
    case overflow_menu::ActionType::FindInPage:
      return self.findInPageAction;
    case overflow_menu::ActionType::TextZoom:
      return self.textZoomAction;
    case overflow_menu::ActionType::ReportAnIssue:
      return self.reportIssueAction;
    case overflow_menu::ActionType::Help:
      return self.helpAction;
    case overflow_menu::ActionType::ShareChrome:
      return self.shareChromeAction;
    case overflow_menu::ActionType::EditActions:
      return self.editActionsAction;
    case overflow_menu::ActionType::LensOverlay:
      return self.lensOverlayAction;

    // Vivaldi
    case overflow_menu::ActionType::vBookmarks:
      return self.bookmarksAction;
    case overflow_menu::ActionType::vNotes:
      return self.notesAction;
    case overflow_menu::ActionType::vHistory:
      return self.historyAction;
    case overflow_menu::ActionType::vReadingList:
      return self.readingListAction;
    case overflow_menu::ActionType::vDownloads:
      return self.downloadsAction;
    case overflow_menu::ActionType::vAddPageTo:
      return self.addPageToAction;
    case overflow_menu::ActionType::vEditPage:
      // Show edit action only if it has actions.
      return ([[self editSubmenuActions] count] > 0) ?
                  self.editPageAction : nil;
    case overflow_menu::ActionType::vStartPage:
      return nil;
    // End Vivaldi

  }
}

// Returns an action for the given `actionType` suitable for displaying in a
// customization UI. This means that it should not depend on any page state
// for things like choosing which variant of an action to show (e.g. Add to
// Bookmarks vs Edit Bookmarks) and shouldn't be enabled based on page state.
- (OverflowMenuAction*)customizationActionForActionType:
    (overflow_menu::ActionType)actionType {
  switch (actionType) {
    // These actions should not be customizable.
    case overflow_menu::ActionType::Reload:
    case overflow_menu::ActionType::NewTab:
    case overflow_menu::ActionType::NewIncognitoTab:
    case overflow_menu::ActionType::NewWindow:
    case overflow_menu::ActionType::ReportAnIssue:
    case overflow_menu::ActionType::Help:
    case overflow_menu::ActionType::ShareChrome:
    case overflow_menu::ActionType::EditActions:
      NOTREACHED();
    case overflow_menu::ActionType::Follow:
      return [self newFollowAction];
    case overflow_menu::ActionType::Bookmark:
      return [self newAddBookmarkAction];
    case overflow_menu::ActionType::ReadingList:
      return [self newReadLaterAction];
    case overflow_menu::ActionType::ClearBrowsingData:
      return [self newClearBrowsingDataAction];
    case overflow_menu::ActionType::Translate:
      return [self newTranslateAction];
    case overflow_menu::ActionType::DesktopSite:
      return [self newRequestDesktopAction];
    case overflow_menu::ActionType::FindInPage:
      return [self newFindInPageAction];
    case overflow_menu::ActionType::TextZoom:
      return [self newTextZoomAction];
    case overflow_menu::ActionType::LensOverlay:
      return [self openLensOverlayAction];

    // Vivaldi
    case overflow_menu::ActionType::vBookmarks:
      return [self vivaldiBookmarksAction];
    case overflow_menu::ActionType::vNotes:
      return [self vivaldiNotesAction];
    case overflow_menu::ActionType::vHistory:
      return [self vivaldiHistoryAction];
    case overflow_menu::ActionType::vReadingList:
      return [self vivaldiReadingListAction];
    case overflow_menu::ActionType::vDownloads:
      return [self vivaldiDownloadsAction];
    case overflow_menu::ActionType::vAddPageTo:
      return [self vivaldiAddPageToAction];
    case overflow_menu::ActionType::vEditPage:
      return [self vivaldiEditPageAction];
    case overflow_menu::ActionType::vStartPage:
      return nil;
    // End Vivaldi

  }
}

#pragma mark - Action handlers

// Dismisses the menu and reloads the current page.
- (void)reload {
  RecordAction(UserMetricsAction("MobileMenuReload"));
  self.tabBasedIPHBrowserAgent->NotifyMultiGestureRefreshEvent();
  [self dismissMenu];
  self.navigationAgent->Reload();
}

// Dismisses the menu and stops the current page load.
- (void)stopLoading {
  RecordAction(UserMetricsAction("MobileMenuStop"));
  [self dismissMenu];
  self.navigationAgent->StopLoading();
}

// Dismisses the menu and opens a new tab.
- (void)openTab {
  RecordAction(UserMetricsAction("MobileMenuNewTab"));
  RecordAction(UserMetricsAction("MobileTabNewTab"));

  [self dismissMenu];
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand commandWithIncognito:NO]];
}

// Dismisses the menu and opens a new incognito tab.
- (void)openIncognitoTab {
  RecordAction(UserMetricsAction("MobileMenuNewIncognitoTab"));
  [self dismissMenu];
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand commandWithIncognito:YES]];
}

// Dismisses the menu and opens a new window.
- (void)openNewWindow {
  RecordAction(UserMetricsAction("MobileMenuNewWindow"));
  [self dismissMenu];
  [self.applicationHandler
      openNewWindowWithActivity:ActivityToLoadURL(WindowActivityToolsOrigin,
                                                  GURL(kChromeUINewTabURL))];
}

// Dismisses the menu and opens the Clear Browsing Data screen.
- (void)openClearBrowsingData {
  RecordAction(UserMetricsAction("MobileMenuClearBrowsingData"));
  base::UmaHistogramEnumeration(
      browsing_data::kDeleteBrowsingDataDialogHistogram,
      browsing_data::DeleteBrowsingDataDialogAction::
          kMenuItemEntryPointSelected);

  [self dismissMenu];
  if (IsIosQuickDeleteEnabled()) {
    [self.quickDeleteHandler
        showQuickDeleteAndCanPerformTabsClosureAnimation:YES];
  } else {
    [self.settingsHandler showClearBrowsingDataSettings];
  }
}

// Follows the website corresponding to `webPage` and dismisses the menu.
- (void)followWebPage:(WebPageURLs*)webPage {
  // FollowBrowserAgent may be null after -disconnect has been called.
  FollowBrowserAgent* followBrowserAgent = self.followBrowserAgent;
  if (followBrowserAgent)
    followBrowserAgent->FollowWebSite(webPage, FollowSource::OverflowMenu);
  [self dismissMenu];
}

// Unfollows the website corresponding to `webPage` and dismisses the menu.
- (void)unfollowWebPage:(WebPageURLs*)webPage {
  // FollowBrowserAgent may be null after -disconnect has been called.
  FollowBrowserAgent* followBrowserAgent = self.followBrowserAgent;
  if (followBrowserAgent)
    followBrowserAgent->UnfollowWebSite(webPage, FollowSource::OverflowMenu);
  [self dismissMenu];
}

// Dismisses the menu and adds the current page as a bookmark or opens the
// bookmark edit screen if the current page is bookmarked.
- (void)addOrEditBookmark {
  RecordAction(UserMetricsAction("MobileMenuAddToOrEditBookmark"));
  // Dismissing the menu disconnects the mediator, so save anything cleaned up
  // there.
  web::WebState* currentWebState = self.webState;
  [self dismissMenu];
  if (!currentWebState) {
    return;
  }
  [self.bookmarksHandler bookmarkWithWebState:currentWebState];
}

// Dismisses the menu and adds the current page to the reading list.
- (void)addToReadingList {
  RecordAction(UserMetricsAction("MobileMenuReadLater"));

  web::WebState* webState = self.webState;
  if (!webState) {
    return;
  }

  // Fetching the canonical URL is asynchronous (and happen on a background
  // thread), so the operation can be started before the UI is dismissed.
  reading_list::AddToReadingListUsingCanonicalUrl(self.readingListBrowserAgent,
                                                  webState);

  [self dismissMenu];
}

// Dismisses the menu and starts translating the current page.
- (void)translatePage {
  base::RecordAction(UserMetricsAction("MobileMenuTranslate"));
  [self dismissMenu];
  [self.browserCoordinatorHandler showTranslate];
}

// Dismisses the menu and requests the desktop version of the current page
- (void)requestDesktopSite {
  RecordAction(UserMetricsAction("MobileMenuRequestDesktopSite"));
  [self dismissMenu];
  self.navigationAgent->RequestDesktopSite();
  [self.helpHandler
      presentInProductHelpWithType:InProductHelpType::kDefaultSiteView];
}

// Dismisses the menu and requests the mobile version of the current page
- (void)requestMobileSite {
  RecordAction(UserMetricsAction("MobileMenuRequestMobileSite"));
  [self dismissMenu];
  self.navigationAgent->RequestMobileSite();
}

// Dismisses the menu and opens Find In Page
- (void)openFindInPage {
  RecordAction(UserMetricsAction("MobileMenuFindInPage"));
  [self dismissMenu];
  [self.findInPageHandler openFindInPage];
}

// Dismisses the menu and opens Text Zoom
- (void)openTextZoom {
  RecordAction(UserMetricsAction("MobileMenuTextZoom"));
  [self dismissMenu];
  [self.textZoomHandler openTextZoom];
}

// Dismisses the menu and opens the Report an Issue screen.
- (void)reportAnIssue {
  RecordAction(UserMetricsAction("MobileMenuReportAnIssue"));
  [self dismissMenu];
  [self.applicationHandler
      showReportAnIssueFromViewController:self.baseViewController
                                   sender:UserFeedbackSender::ToolsMenu];
}

// Dismisses the menu and opens the help screen.
- (void)openHelp {
  RecordAction(UserMetricsAction("MobileMenuHelp"));
  [self dismissMenu];
  [self.browserCoordinatorHandler showHelpPage];
}

// Begins the action edit flow.
- (void)beginCustomization {
  // Clear the new badge if it's active.
  self.editActionsAction.displayNewLabelIcon = NO;
  self.editActionsAction.highlighted = NO;
  [self.overflowMenuCustomizationHandler showMenuCustomization];
}

- (void)beginCustomizationFromActionType:(overflow_menu::ActionType)actionType {
  [self.overflowMenuCustomizationHandler
      showMenuCustomizationFromActionType:actionType];
}

- (void)hideDestination:(overflow_menu::Destination)destination {
  DestinationCustomizationModel* destinationCustomizationModel =
      self.menuOrderer.destinationCustomizationModel;
  for (OverflowMenuDestination* menuDestination in destinationCustomizationModel
           .shownDestinations) {
    if (menuDestination.destination == static_cast<int>(destination)) {
      menuDestination.shown = NO;
    }
  }
  [self.menuOrderer commitDestinationsUpdate];
}

- (void)hideActionType:(overflow_menu::ActionType)actionType {
  ActionCustomizationModel* actionCustomizationModel =
      self.menuOrderer.actionCustomizationModel;
  for (OverflowMenuAction* action in actionCustomizationModel.shownActions) {
    if (action.actionType == static_cast<int>(actionType)) {
      action.shown = NO;
    }
  }

  if (IsVivaldiRunning()) {
    ActionCustomizationModel* vivaldiActionCustomizationModel =
         self.menuOrderer.vivaldiActionCustomizationModel;
     for (OverflowMenuAction* action in
          vivaldiActionCustomizationModel.shownActions) {
       if (action.actionType == static_cast<int>(actionType)) {
         action.shown = NO;
       }
     }
    [self.menuOrderer commitVivaldiActionsUpdate];
  } // End Vivaldi

  [self.menuOrderer commitActionsUpdate];
}

// Creates and opens the lens overlay UI.
- (void)startLensOverlay {
  RecordAction(UserMetricsAction("MobileMenuLensOverlay"));
  [self dismissMenu];
  [self.lensOverlayHandler
      createAndShowLensUI:YES
               entrypoint:LensOverlayEntrypoint::kOverflowMenu
               completion:nil];
}

#pragma mark - Destinations Handlers

// Dismisses the menu and opens bookmarks.
- (void)openBookmarks {
  [self dismissMenu];
  [self.browserCoordinatorHandler showBookmarksManager];
}

// Dismisses the menu and opens share sheet to share Chrome's app store link
- (void)shareChromeApp {
  [self dismissMenu];
  [self.activityServiceHandler shareChromeApp];
}

// Dismisses the menu and opens history.
- (void)openHistory {
  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHiOSHistoryOnOverflowMenuFeature) &&
      _engagementTracker) {
    _engagementTracker->NotifyEvent(
        feature_engagement::events::kHistoryOnOverflowMenuUsed);
  }
  [IntentDonationHelper donateIntent:IntentType::kViewHistory];
  [self dismissMenu];
  [self.applicationHandler showHistory];
}

// Dismisses the menu and opens reading list.
- (void)openReadingList {
  [self dismissMenu];
  [self.browserCoordinatorHandler showReadingList];
}

// Dismisses the menu and opens password list.
- (void)openPasswords {
  UmaHistogramEnumeration(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kChromeMenuItem);
  [self dismissMenu];
  [self.settingsHandler
      showSavedPasswordsSettingsFromViewController:self.baseViewController
                                  showCancelButton:NO];
}

// Dismisses the menu and opens price notifications list.
- (void)openPriceNotifications {
  RecordAction(UserMetricsAction("MobileMenuPriceNotifications"));
  _engagementTracker->NotifyEvent(
      feature_engagement::events::kPriceNotificationsUsed);
  [self dismissMenu];
  [self.priceNotificationHandler showPriceNotifications];
}

// Dismisses the menu and opens downloads.
- (void)openDownloads {
  [self dismissMenu];
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Download.OpenDownloadsFromMenu.PerProfileType",
                          type);
  [self.browserCoordinatorHandler showDownloadsFolder];
}

// Dismisses the menu and opens recent tabs.
- (void)openRecentTabs {
  [self dismissMenu];
  [self.browserCoordinatorHandler showRecentTabs];
}

// Dismisses the menu and shows page information.
- (void)openSiteInformation {

  if (![self currentWebPageSupportsSiteInfo])
    return; // End Vivaldi

  [self dismissMenu];
  [self.pageInfoHandler showPageInfo];
}

// Dismisses the menu and opens What's New.
- (void)openWhatsNew {
  [self dismissMenu];
  [self.whatsNewHandler showWhatsNew];
}

// Dismisses the menu and opens settings.
- (void)openSettings {
  if (!IsBlueDotOnToolsMenuButtoneEnabled() &&
      self.settingsDestination.badge == BadgeTypePromo &&
      self.engagementTracker) {
    self.engagementTracker->NotifyEvent(
        feature_engagement::events::kBlueDotPromoOverflowMenuDismissed);
    [self.popupMenuHandler updateToolsMenuBlueDotVisibility];
  }
  [self dismissMenu];
  profile_metrics::BrowserProfileType type =
      self.isIncognito ? profile_metrics::BrowserProfileType::kIncognito
                       : profile_metrics::BrowserProfileType::kRegular;
  UmaHistogramEnumeration("Settings.OpenSettingsFromMenu.PerProfileType", type);
  [self.applicationHandler
      showSettingsFromViewController:self.baseViewController
            hasDefaultBrowserBlueDot:(self.settingsDestination.badge ==
                                      BadgeTypePromo)];
}

- (void)enterpriseLearnMore {
  [self dismissMenu];
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand commandWithURLFromChrome:
                                             GURL(kChromeUIManagementURL)]];
}

- (void)parentLearnMore {
  [self dismissMenu];
  GURL familyLinkURL = GURL(supervised_user::kManagedByParentUiMoreInfoUrl);
  [self.applicationHandler
      openURLInNewTab:[OpenNewTabCommand
                          commandWithURLFromChrome:familyLinkURL]];
}

- (void)openSpotlightDebugger {
  DCHECK(IsSpotlightDebuggingEnabled());
  [self dismissMenu];
  [self.browserCoordinatorHandler showSpotlightDebugger];
}

// Make any necessary updates for when `destination`'s shown state is toggled.
- (void)onShownToggledForDestination:(OverflowMenuDestination*)destination {
  // If customization is not in progress, there's no need to update any UI.
  if (!self.menuOrderer.isDestinationCustomizationInProgress) {
    return;
  }

  overflow_menu::Destination destinationType =
      static_cast<overflow_menu::Destination>(destination.destination);
  overflow_menu::ActionType correspondingActionType;
  NSString* subtitle;
  switch (destinationType) {
    case overflow_menu::Destination::History:
    case overflow_menu::Destination::Passwords:
    case overflow_menu::Destination::Downloads:
    case overflow_menu::Destination::RecentTabs:
    case overflow_menu::Destination::SiteInfo:
    case overflow_menu::Destination::Settings:
    case overflow_menu::Destination::WhatsNew:
    case overflow_menu::Destination::SpotlightDebugger:
    case overflow_menu::Destination::PriceNotifications:
      // Most destinations have no corresponding destination and nothing special
      // to be done when their shown state is toggled.
      return;
    case overflow_menu::Destination::Bookmarks:
      correspondingActionType = overflow_menu::ActionType::Bookmark;
      subtitle = l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDDEN_BOOKMARKS_SUBTITLE);
      break;
    case overflow_menu::Destination::ReadingList:
      correspondingActionType = overflow_menu::ActionType::ReadingList;
      subtitle = l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDDEN_READING_LIST_SUBTITLE);
      break;

#if defined(VIVALDI_BUILD)
    case overflow_menu::Destination::vShare:
    case overflow_menu::Destination::vZoomText:
    case overflow_menu::Destination::vFindInPage:
    case overflow_menu::Destination::vDesktopSite:
      return;
#endif // End Vivaldi

  }

  [self.menuOrderer customizationUpdateToggledShown:destination.shown
                                forLinkedActionType:correspondingActionType
                                     actionSubtitle:subtitle];
}

#pragma mark: VIVALDI

- (ActionRanking)vivaldiActions {
  return {
    overflow_menu::ActionType::vBookmarks,
    overflow_menu::ActionType::vNotes,
    overflow_menu::ActionType::vHistory,
    overflow_menu::ActionType::vReadingList,
    overflow_menu::ActionType::vDownloads,
  };
}

#pragma mark - Destinations
- (OverflowMenuDestination*)newShareDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:IDS_VIVALDI_TOOLS_MENU_SHARE
                              destination:overflow_menu::Destination::vShare
                               symbolName:vOverflowShare
                             systemSymbol:NO
                          accessibilityID:kToolsMenuShareId
                                  handler:^{
                                      [weakSelf showSharePage];
      }];
  destination.canBeHidden = NO;
  return destination;
}

- (OverflowMenuDestination*)newZoomTextDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:IDS_VIVALDI_TOOLS_MENU_ZOOM_TEXT
                              destination:overflow_menu::Destination::vZoomText
                               symbolName:vOverflowZoom
                             systemSymbol:NO
                          accessibilityID:vToolsMenuTextZoomId
                                  handler:^{
                                      [weakSelf openTextZoom];
      }];
  return destination;
}

- (OverflowMenuDestination*)newFindInPageDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:IDS_VIVALDI_TOOLS_MENU_FIND_IN_PAGE
                            destination:overflow_menu::Destination::vFindInPage
                               symbolName:vOverflowFindInPage
                             systemSymbol:NO
                          accessibilityID:vToolsMenuFindInPageId
                                  handler:^{
                                      [weakSelf openFindInPage];
      }];
  return destination;
}

- (OverflowMenuDestination*)newDesktopSiteDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:
                                IDS_VIVALDI_TOOLS_MENU_DESTINATION_DESKTOP_SITE
                            destination:overflow_menu::Destination::vDesktopSite
                               symbolName:vOverflowDesktopSite
                             systemSymbol:NO
                          accessibilityID:vToolsMenuDesktopSiteId
                                  handler:^{
                                      [weakSelf requestDesktopSite];
      }];
  return destination;
}

- (OverflowMenuDestination*)newMobileSiteDestination {
  __weak __typeof(self) weakSelf = self;
  OverflowMenuDestination* destination =
      [self createOverflowMenuDestination:
                                  IDS_VIVALDI_TOOLS_MENU_DESTINATION_MOBILE_SITE
                            destination:overflow_menu::Destination::vDesktopSite
                               symbolName:vOverflowMobileSite
                             systemSymbol:NO
                          accessibilityID:vToolsMenuMobileSiteId
                                  handler:^{
                                      [weakSelf requestMobileSite];
      }];
  return destination;
}

#pragma mark - Actions
- (OverflowMenuAction*)vivaldiBookmarksAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_BOOKMARKS);
  return [self
      createOverflowMenuActionWithNameID:IDS_VIVALDI_TOOLS_MENU_ACTION_BOOKMARKS
                                   actionType:
                            overflow_menu::ActionType::vBookmarks
                                   symbolName:vOverflowBookmarks
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:kToolsMenuBookmarksId
                                 hideItemText:hideItemText
                                      handler:^{
                                        [weakSelf openBookmarks];
                                                      }];
}

- (OverflowMenuAction*)vivaldiNotesAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTIONS_NOTES);
  return [self
      createOverflowMenuActionWithNameID:IDS_VIVALDI_TOOLS_MENU_NOTES
                                   actionType:
                            overflow_menu::ActionType::vNotes
                                   symbolName:vOverflowNotes
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:kToolsMenuNotesId
                                 hideItemText:hideItemText
                                      handler:^{
                                        [weakSelf openNotes];
                                                      }];
}

- (OverflowMenuAction*)vivaldiHistoryAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_HISTORY);
  return [self
      createOverflowMenuActionWithNameID:IDS_VIVALDI_TOOLS_MENU_ACTION_HISTORY
                                   actionType:
                            overflow_menu::ActionType::vHistory
                                   symbolName:vOverflowHistory
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:kToolsMenuHistoryId
                              hideItemText:hideItemText
                                      handler:^{
                                        [weakSelf openHistory];
                                                      }];
}

- (OverflowMenuAction*)vivaldiReadingListAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(
          IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_READING_LIST);
  return [self
      createOverflowMenuActionWithNameID:
                                IDS_VIVALDI_TOOLS_MENU_ACTION_READING_LIST
                                   actionType:
                            overflow_menu::ActionType::vReadingList
                                   symbolName:vOverflowReadingList
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:kToolsMenuReadingListId
                              hideItemText:hideItemText
                                      handler:^{
                                        [weakSelf openReadingList];
                                                      }];
}


- (OverflowMenuAction*)vivaldiDownloadsAction {
  __weak __typeof(self) weakSelf = self;
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_DESTINATION_DOWNLOADS);
  return [self
      createOverflowMenuActionWithNameID:IDS_VIVALDI_TOOLS_MENU_ACTION_DOWNLOADS
                                   actionType:
                            overflow_menu::ActionType::vDownloads
                                   symbolName:vOverflowDownloads
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:kToolsMenuDownloadsId
                              hideItemText:hideItemText
                                      handler:^{
                                        [weakSelf openDownloads];
                                                      }];
}

- (OverflowMenuAction*)vivaldiAddPageToAction {
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTIONS_ADD_TO);
  return [self
      createOverflowMenuActionWithNameID:
                                IDS_VIVALDI_TOOLS_MENU_ACTION_ADD_PAGE_TO
                                   actionType:
                            overflow_menu::ActionType::vAddPageTo
                                   symbolName:vOverflowAddTo
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:vToolsMenuAddPageToId
                                 hideItemText:hideItemText
                                      handler:nil];
}

- (OverflowMenuAction*)vivaldiEditPageAction {
  NSString* hideItemText =
      l10n_util::GetNSString(IDS_IOS_OVERFLOW_MENU_HIDE_ACTIONS_EDIT);
  return [self
      createOverflowMenuActionWithNameID:IDS_VIVALDI_TOOLS_MENU_ACTION_EDIT
                                   actionType:
                            overflow_menu::ActionType::vEditPage
                                   symbolName:vOverflowEdit
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:vToolsMenuEditPageId
                                 hideItemText:hideItemText
                                      handler:nil];
}

- (OverflowMenuAction*)vivaldiAddOrEditStartPageAction:(BOOL)isAdded {
  NSString* symbolName =
      isAdded ? vOverflowEditStartPage : vOverflowAddStartPage;
  NSString* accessibilityID =
      isAdded ? vToolsMenuEditStartPageId : vToolsMenuAddStartPageId;

  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:
                                      IDS_VIVALDI_TOOLS_MENU_ACTION_START_PAGE
                                   actionType:
                            overflow_menu::ActionType::vStartPage
                                   symbolName:symbolName
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:accessibilityID
                                 hideItemText:nil
                                      handler:^{
                                        [weakSelf addOrEditStartPage];
                                      }];
}

- (OverflowMenuAction*)vivaldiAddOrEditBookmarksAction:(BOOL)isBookmarked {
  NSString* symbolName =
      isBookmarked ? vOverflowEditBookmark : vOverflowAddBookmark;
  NSString* accessibilityID =
      isBookmarked ? kToolsMenuEditBookmark : kToolsMenuAddToBookmarks;
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_BOOKMARKS
                              actionType:overflow_menu::ActionType::Bookmark
                              symbolName:symbolName
                            systemSymbol:NO
                        monochromeSymbol:NO
                        accessibilityID:accessibilityID
                           hideItemText:nil
                                handler:^{
                                  [weakSelf addOrEditBookmark];
                                }];
}

- (OverflowMenuAction*)vivaldiAddToReadingListAction {
  __weak __typeof(self) weakSelf = self;
  return [self
      createOverflowMenuActionWithNameID:IDS_IOS_TOOLS_MENU_READING_LIST
                                   actionType:
                            overflow_menu::ActionType::ReadingList
                                   symbolName:vOverflowReadingList
                                 systemSymbol:NO
                             monochromeSymbol:NO
                              accessibilityID:kToolsMenuReadLater
                              hideItemText:nil
                                      handler:^{
                                        [weakSelf addToReadingList];
                                      }];
}

#pragma mark - Private

// Returns submenu actions for 'Add page to..' item depending on whether the
// active Web State URL is already added to start page or bookmarks.
- (NSArray<OverflowMenuAction*>*)addToSubmenuActions {
  NSMutableArray<OverflowMenuAction*>* addActions =
      [[NSMutableArray alloc] init];

  BOOL pageAddedToStartPage = self.webState && self.bookmarkModel &&
      vivaldi_bookmark_kit::IsURLAddedToStartPage(
          self.bookmarkModel, self.webState->GetVisibleURL());

  // Add to 'Add' action if page is not already added to startpage.
  if (!pageAddedToStartPage) {
    [addActions addObject:
        [self vivaldiAddOrEditStartPageAction:pageAddedToStartPage]];
  }

  BOOL pageIsBookmarked =
      self.webState && self.bookmarkModel &&
      self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL());

  // Add to 'Add' action if page is not already added to bookmark.
  if (!pageIsBookmarked) {
    [addActions addObject:
        [self vivaldiAddOrEditBookmarksAction:pageIsBookmarked]];
  }

  // Reading list should always be on the 'Add' action given that it has no
  // edit actions available.
  if (!self.webContentAreaShowingOverlay && [self isCurrentURLWebURL]) {
    [addActions addObject:[self vivaldiAddToReadingListAction]];
  }

  return addActions;
}

// Returns submenu actions for 'Edit' item depending on whether the
// active Web State URL is already added to start page or bookmarks.
- (NSArray<OverflowMenuAction*>*)editSubmenuActions {
  NSMutableArray<OverflowMenuAction*>* editActions =
      [[NSMutableArray alloc] init];

  BOOL pageAddedToStartPage = self.webState && self.bookmarkModel &&
      vivaldi_bookmark_kit::IsURLAddedToStartPage(
          self.bookmarkModel, self.webState->GetVisibleURL());

  // Add to 'Edit' action if page is already added to startpage.
  if (pageAddedToStartPage) {
    [editActions addObject:
        [self vivaldiAddOrEditStartPageAction:pageAddedToStartPage]];
  }

  BOOL pageIsBookmarked =
      self.webState && self.bookmarkModel &&
      self.bookmarkModel->IsBookmarked(self.webState->GetVisibleURL());

  // Add to 'Edit action if page is already added to bookmark.
  if (pageIsBookmarked) {
    [editActions addObject:
        [self vivaldiAddOrEditBookmarksAction:pageIsBookmarked]];
  }
  return editActions;
}

- (void)openNotes {
  [self.popupMenuHandler dismissPopupMenuAnimated:YES];
  [self.browserCoordinatorHandler showNotesManager];
}

- (void)showSharePage {
  [self.popupMenuHandler dismissPopupMenuAnimated:YES];
  [self.activityServiceHandler sharePage];
}

- (void)showSiteTrackerBlockerPrefs {
  [self.popupMenuHandler dismissPopupMenuAnimated:YES];
  [self.activityServiceHandler showTrackerBlockerManager];
}

- (void)addOrEditStartPage {
  // Dismissing the menu disconnects the mediator, so save anything cleaned up
  // there.
  web::WebState* currentWebState = self.webState;
  [self dismissMenu];
  if (!currentWebState) {
    return;
  }
  [self.bookmarksHandler createOrEditSpeedDialWithURL:currentWebState];
}

@end
