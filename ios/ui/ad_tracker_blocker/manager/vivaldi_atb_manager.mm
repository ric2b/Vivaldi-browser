// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/ad_blocker/adblock_known_sources_handler.h"
#import "components/ad_blocker/adblock_rule_manager.h"
#import "components/ad_blocker/adblock_rule_service.h"
#import "components/ad_blocker/adblock_types.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/ad_blocker/adblock_rule_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager_bridge.h"
#import "ios/ui/ad_tracker_blocker/manager/vivaldi_atb_manager_helper.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_item.h"
#import "ios/ui/ad_tracker_blocker/vivaldi_atb_source_item.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"


using adblock_filter::KnownRuleSource;
using adblock_filter::KnownRuleSources;
using adblock_filter::KnownRuleSourcesHandler;
using adblock_filter::RuleGroup;
using adblock_filter::RuleManager;
using adblock_filter::RuleManager::kExemptList;
using adblock_filter::RuleManager::kProcessList;
using adblock_filter::RuleService;
using adblock_filter::RuleSourceCore;
using adblock_filter::ActiveRuleSource;

// Namespace
namespace {
// Converts NSString entered by the user to a GURL.
GURL ConvertUserDataToGURL(NSString* urlString) {
  if (urlString) {
    return url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                   std::string());
  } else {
    return GURL();
  }
}
}

@interface VivaldiATBManager()<VivaldiATBConsumer> {
  // Bridge to register for adblock backend changes.
  std::unique_ptr<vivaldi_adblocker::VivaldiATBManagerBridge> _bridge;
}
// The Browser in which blocker engine is active.
@property(nonatomic, assign) Browser* browser;
// The user's profile used.
@property(nonatomic, assign) ProfileIOS* profile;
// Rule service for ad blocker.
@property(nonatomic, assign) RuleService* ruleService;
// Rule manager for ad blocker.
@property(nonatomic, assign) RuleManager* ruleManager;
// Known rule source handler.
@property(nonatomic, assign) KnownRuleSourcesHandler* ruleSourceHandler;
// Helper class for the manager
@property(nonatomic, assign) VivaldiATBManagerHelper* managerHelper;

@end

@implementation VivaldiATBManager
@synthesize consumer = _consumer;
@synthesize browser = _browser;
@synthesize profile = _profile;
@synthesize ruleService = _ruleService;
@synthesize ruleManager = _ruleManager;
@synthesize ruleSourceHandler = _ruleSourceHandler;

#pragma mark - INITIALIZERS
- (instancetype)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    _browser = browser;
    _profile = _browser->GetProfile();
    _ruleService =
        adblock_filter::RuleServiceFactory::GetForProfile(_profile);
    _bridge.reset(new vivaldi_adblocker::VivaldiATBManagerBridge(
        self, _ruleService));

    [self initRuleManagerAndSourceHandler];
  }
  return self;
}

#pragma mark - PUBLIC METHODS

- (void)disconnect {
  self.consumer = nil;
  _bridge = nullptr;
}

#pragma mark - GETTERS

- (void)getSettingOptions {

  // The available setting options are fixed:
  // [No Blocking, Block Trackers, Block Ads and Trackers].

  // No blocking option
  NSString* noBlockingTitleString =
    l10n_util::GetNSString(IDS_LEVEL_NO_BLOCKING);
  NSString* noBlockingDescriptionString =
    l10n_util::GetNSString(IDS_LEVEL_NO_BLOCKING_DESCRIPTION);

  VivaldiATBItem* noBlockingOption =
    [[VivaldiATBItem alloc] initWithTitle:noBlockingTitleString
                                 subtitle:noBlockingDescriptionString
                                     type:ATBSettingNoBlocking];

  // Block trackers option
  NSString* blockTrackersTitleString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS);
  NSString* blockTrackersDescriptionString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS_DESCRIPTION);

  VivaldiATBItem* blockTrackersOption =
    [[VivaldiATBItem alloc]
      initWithTitle:blockTrackersTitleString
           subtitle:blockTrackersDescriptionString
               type:ATBSettingBlockTrackers];

  // Block trackers and ads option
  NSString* blockTrackersAndAdsTitleString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS_AND_ADS);
  NSString* blockTrackersAndAdsDescriptionString =
    l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS_AND_ADS_DESCRIPTION);

  VivaldiATBItem* blockTrackersAndAdsOption =
    [[VivaldiATBItem alloc]
      initWithTitle:blockTrackersAndAdsTitleString
           subtitle:blockTrackersAndAdsDescriptionString
               type:ATBSettingBlockTrackersAndAds];

  NSMutableArray* options =
    [[NSMutableArray alloc] initWithArray:@[noBlockingOption,
                                            blockTrackersOption,
                                            blockTrackersAndAdsOption]];

  SEL selector = @selector(didRefreshSettingOptions:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer didRefreshSettingOptions:options];
  }
}

- (ATBSettingType)globalBlockingSetting {
  ATBSettingType defaultBlocking = ATBSettingNoBlocking;
  if (!_ruleManager)
    return defaultBlocking;

  if ([self isTrackerBlockingEnabled]) {
    if ([self isAdBlockingEnabled])
      return ATBSettingBlockTrackersAndAds;
    else
      return ATBSettingBlockTrackers;
  }

  return defaultBlocking;
}

/// Returns setting for the give domain.
- (ATBSettingType)blockingSettingForDomain:(NSString*)domain {
  ATBSettingType defaultBlocking = [self globalBlockingSetting];
  if (!_ruleManager)
    return defaultBlocking;

  const GURL domainURL = ConvertUserDataToGURL(domain);
  if (!domainURL.is_valid()) {
    return defaultBlocking;
  }

  url::Origin originURL = url::Origin::Create(domainURL);

  BOOL isTrackingEnabled =
    !_ruleManager->IsExemptOfFiltering(RuleGroup::kTrackingRules, originURL);
  BOOL isAdblockingEnabled =
    !_ruleManager->IsExemptOfFiltering(RuleGroup::kAdBlockingRules, originURL);

  BOOL noTrackingEnabled = !isTrackingEnabled && !isAdblockingEnabled;
  BOOL adsAndTrackingEnabled = isTrackingEnabled && isAdblockingEnabled;

  if (noTrackingEnabled)
    return ATBSettingNoBlocking;

  if (adsAndTrackingEnabled)
    return ATBSettingBlockTrackersAndAds;

  if (isTrackingEnabled)
    return ATBSettingBlockTrackers;

  return defaultBlocking;
}

/// Returns the default settings.
-(BOOL)isTrackerBlockingEnabled {
  if (!_ruleManager)
    return NO;
  return _ruleManager->
      GetActiveExceptionList(RuleGroup::kTrackingRules) == kExemptList;
}

/// Returns the default settings.
-(BOOL)isAdBlockingEnabled {
  if (!_ruleManager)
    return NO;
  return _ruleManager->
      GetActiveExceptionList(RuleGroup::kAdBlockingRules) == kExemptList;
}

- (void)getAllExceptionsList {
  if (!_ruleManager)
    return;

  std::set<std::string> trackingBlockingList = _ruleManager->
      GetExceptions(RuleGroup::kTrackingRules, kProcessList);
  std::set<std::string> trackingExemptList = _ruleManager->
      GetExceptions(RuleGroup::kTrackingRules, kExemptList);
  std::set<std::string> adBlockingList = _ruleManager->
      GetExceptions(RuleGroup::kAdBlockingRules, kProcessList);
  std::set<std::string> adExemptList = _ruleManager->
      GetExceptions(RuleGroup::kAdBlockingRules, kExemptList);

  // Flatten the different lists to prepare a full collection of domain
  // alongside their rules.

  // Prepare the list for domain have tracker and ad blocking enabled.
  std::set<std::string> trackingAndAdBlockingList;
  std::set_intersection(
    trackingBlockingList.begin(), trackingBlockingList.end(),
    adBlockingList.begin(), adBlockingList.end(),
    std::inserter(trackingAndAdBlockingList, trackingAndAdBlockingList.begin())
                        );

  // Prepare the list for domain have tracker and ad blocking disabled.
  std::set<std::string> noBlockingList;
  std::set_intersection(trackingExemptList.begin(), trackingExemptList.end(),
                        adExemptList.begin(), adExemptList.end(),
                        std::inserter(noBlockingList, noBlockingList.begin()));

  // Prepare the list for domain have tracker enabled and ad blocking disabled.
  std::set<std::string> trackersBlockingList;
  std::set_intersection(
    trackingBlockingList.begin(), trackingBlockingList.end(),
    adExemptList.begin(), adExemptList.end(),
    std::inserter(trackersBlockingList, trackersBlockingList.begin())
                        );

  NSMutableArray<VivaldiATBItem*> *exceptionsList =
      [[NSMutableArray alloc] initWithArray:@[]];

  for (const auto &domain : trackingAndAdBlockingList) {
    VivaldiATBItem* item =
      [[VivaldiATBItem alloc]
       initWithTitle:[NSString stringWithUTF8String:domain.c_str()]
            subtitle:l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS_AND_ADS)
                type:ATBSettingBlockTrackersAndAds];
    [exceptionsList addObject:item];
  }

  for (const auto &domain : noBlockingList) {
    VivaldiATBItem* item =
      [[VivaldiATBItem alloc]
       initWithTitle:[NSString stringWithUTF8String:domain.c_str()]
            subtitle:l10n_util::GetNSString(IDS_LEVEL_NO_BLOCKING)
                type:ATBSettingNoBlocking];
    [exceptionsList addObject:item];
  }

  for (const auto &domain : trackersBlockingList) {
    VivaldiATBItem* item =
      [[VivaldiATBItem alloc]
       initWithTitle:[NSString stringWithUTF8String:domain.c_str()]
            subtitle:l10n_util::GetNSString(IDS_LEVEL_BLOCK_TRACKERS)
                type:ATBSettingBlockTrackers];
    [exceptionsList addObject:item];
  }

  // Sort the items alphabatically.
  NSArray* sortedExceptionsList = [exceptionsList sortedArrayUsingComparator:
                          ^NSComparisonResult(VivaldiATBItem *first,
                                              VivaldiATBItem *second) {
    return [VivaldiGlobalHelpers compare:first.title
                                  second:second.title];
  }];

  if (!self.consumer)
    return;
  SEL selector = @selector(didRefreshExceptionsList:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer didRefreshExceptionsList:sortedExceptionsList];
  }
}

- (void)getBlockerSourcesForSourceType:(ATBSourceType)sourceType {
  if (!_ruleSourceHandler || !_ruleManager)
    return;

  // Get the sources from backend.
  KnownRuleSources sources =  [self getSourcesWithType:sourceType];

  // Prepare the items for UI. We would prefer to create the data model here
  // so that we can use the model in different places without more computation.
  NSMutableArray *sourceList = [NSMutableArray array];

  for (auto it = sources.begin(); it != sources.end(); ++it) {
    uint32_t key = it->first;
    KnownRuleSource knownSource = it->second;
    VivaldiATBSourceItem* ruleSourceItem =
        [self getBlockerSourceForSourceId:key
                              knownSource:knownSource
                               sourceType:sourceType];
    [sourceList addObject:ruleSourceItem];
  }

  // Sort the items based on priority. Non removable items get higher priority.
  NSArray* sortedList = [sourceList sortedArrayUsingComparator:
                          ^NSComparisonResult(VivaldiATBSourceItem *first,
                                              VivaldiATBSourceItem *second) {
    if (first.list_priority < second.list_priority) {
      return (NSComparisonResult)NSOrderedAscending;
    } else if (first.list_priority > second.list_priority) {
      return (NSComparisonResult)NSOrderedDescending;
    }
    return [VivaldiGlobalHelpers compare:first.title
                                  second:second.title];
  }];

  if (!self.consumer)
    return;
  SEL selector = @selector(didRefreshSourcesList:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer didRefreshSourcesList:sortedList];
  }
}

- (VivaldiATBSourceItem*)getBlockerSourceForSourceId:(uint32_t)key
                                          sourceType:(ATBSourceType)sourceType {
  if (!_ruleSourceHandler || !_ruleManager)
    return nil;

  std::optional<KnownRuleSource> knownSource = _ruleSourceHandler->GetSource(
      sourceType == ATBSourceAds ?
      RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules,
      key);

  if (knownSource.has_value()) {
    VivaldiATBSourceItem* ruleSourceItem =
        [self getBlockerSourceForSourceId:key
                              knownSource:knownSource.value()
                               sourceType:sourceType];
    return ruleSourceItem;
  }

  return nil;
}

- (BOOL)isApplyingExceptionRules {
  if (_ruleService && _ruleService->IsLoaded()) {
    return _ruleService->IsApplyingIosRules(RuleGroup::kTrackingRules) ||
        _ruleService->IsApplyingIosRules(RuleGroup::kAdBlockingRules);
  }
  return NO;
}

#pragma mark - SETTERS
- (void)setExceptionFromBlockingType:(ATBSettingType)blockingType {
  switch (blockingType) {
    case ATBSettingNoBlocking:
      if ([self isTrackerBlockingEnabled])
        [self setTrackerBlockingEnabled:NO];
      if ([self isAdBlockingEnabled])
        [self setAdBlockingEnabled:NO];
      break;
    case ATBSettingBlockTrackers:
      if (![self isTrackerBlockingEnabled])
        [self setTrackerBlockingEnabled:YES];

      if ([self isAdBlockingEnabled])
        [self setAdBlockingEnabled:NO];
      break;
    case ATBSettingBlockTrackersAndAds:
      if (![self isTrackerBlockingEnabled])
        [self setTrackerBlockingEnabled:YES];

      if (![self isAdBlockingEnabled])
        [self setAdBlockingEnabled:YES];
      break;
    default: break;
  }
}

/// Adds exception for a given domain
- (void)setExceptionForDomain:(NSString*)domain
                 blockingType:(ATBSettingType)blockingType {
  switch (blockingType) {
    case ATBSettingNoBlocking:
      [self setTrackerBlockingExceptionForDomain:domain enableBlocking:NO];
      [self setAdBlockingExceptionForDomain:domain enableBlocking:NO];
      break;
    case ATBSettingBlockTrackers:
      [self setTrackerBlockingExceptionForDomain:domain enableBlocking:YES];
      [self setAdBlockingExceptionForDomain:domain enableBlocking:NO];
      break;
    case ATBSettingBlockTrackersAndAds:
      [self setTrackerBlockingExceptionForDomain:domain enableBlocking:YES];
      [self setAdBlockingExceptionForDomain:domain enableBlocking:YES];
      break;
    default: break;
  }
}

/// Removes exception for a given domain
- (void)removeExceptionForDomain:(NSString*)domain {
  if (!_ruleManager)
    return;

  NSString* host = [VivaldiGlobalHelpers hostOfURLString:domain];
  std::string hostString = base::SysNSStringToUTF8(host);

  _ruleManager->
      RemoveExceptionForDomain(RuleGroup::kTrackingRules,
                               kProcessList,
                               hostString);
  _ruleManager->
      RemoveExceptionForDomain(RuleGroup::kTrackingRules,
                               kExemptList,
                               hostString);
  _ruleManager->
      RemoveExceptionForDomain(RuleGroup::kAdBlockingRules,
                               kProcessList,
                               hostString);
  _ruleManager->
      RemoveExceptionForDomain(RuleGroup::kAdBlockingRules,
                               kExemptList,
                               hostString);
}

/// Add rule source for the given source type.
- (void)addRuleSource:(NSString*)source
           sourceType:(ATBSourceType)sourceType {
  if (!_ruleSourceHandler)
    return;

  const GURL sourceURL = ConvertUserDataToGURL(source);
  if (!sourceURL.is_valid()) {
    return;
  }
  _ruleSourceHandler->AddSource(
      sourceType == ATBSourceAds ?
      RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules,
      *RuleSourceCore::FromUrl(sourceURL));
}

/// Remove rule source of the given key and source type.
- (void)removeRuleSourceForKey:(uint32_t)key
                    sourceType:(ATBSourceType)sourceType {
  if (!_ruleSourceHandler)
    return;
  _ruleSourceHandler->RemoveSource(
      sourceType == ATBSourceAds ?
      RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules,
      key);
}

/// Enable rule source of the given key and source type.
- (void)enableRuleSourceForKey:(uint32_t)key
                    sourceType:(ATBSourceType)sourceType {
  if (!_ruleSourceHandler)
    return;
  _ruleSourceHandler->EnableSource(
      sourceType == ATBSourceAds ?
      RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules,
      key);
}

/// Disable rule source of the given key and source type.
- (void)disableRuleSourceForKey:(uint32_t)key
                     sourceType:(ATBSourceType)sourceType {
  if (!_ruleSourceHandler)
    return;
  _ruleSourceHandler->DisableSource(
      sourceType == ATBSourceAds ?
      RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules,
      key);
}

- (void)restoreRuleSourceForType:(ATBSourceType)sourceType {
  if (!_ruleSourceHandler)
    return;
  _ruleSourceHandler->ResetPresetSources(
      sourceType == ATBSourceAds ?
      RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules);
}

#pragma mark: - PRIVATE

- (void)initRuleManagerAndSourceHandler {
  if (_ruleService && _ruleService->IsLoaded()) {
    _ruleManager = _ruleService->GetRuleManager();
    _ruleSourceHandler = _ruleService->GetKnownSourcesHandler();
  }
}

/// Returns 'KnownRuleSources' for a given rule.
- (KnownRuleSources)getSourcesWithType:(ATBSourceType)sourceType {
  // Get the sources from backend.
  KnownRuleSources sources = _ruleSourceHandler->
      GetSources(sourceType == ATBSourceAds ?
                 RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules);
  return sources;
}

/// Used to notify the updated source list after any changes occured
/// on the backend.
- (void)refreshSourceList:(RuleGroup)group {
  ATBSourceType sourceType;
  if (group == RuleGroup::kTrackingRules)
    sourceType = ATBSourceTrackers;
  else if (group == RuleGroup::kAdBlockingRules)
    sourceType = ATBSourceAds;
  else
    sourceType = ATBSourceNone;

  [self getBlockerSourcesForSourceType:sourceType];
}

/// Returns the serialized VivaldiATBSourceItem created using
/// RuleSource to render associated info on the UI.
- (VivaldiATBSourceItem*)getBlockerSourceForSourceId:(uint32_t)key
     knownSource:(KnownRuleSource)knownSource
      sourceType:(ATBSourceType)sourceType {
  // Instantiate the model.
  VivaldiATBSourceItem* ruleSourceItem = [[VivaldiATBSourceItem alloc] init];

  ruleSourceItem.key = key;
  NSString* sourceURL = base::SysUTF8ToNSString(
      knownSource.core.is_from_url() ? knownSource.core.source_url().spec() :
      knownSource.core.source_file().AsUTF8Unsafe());

  VivaldiATBManagerHelper* managerHelper
      = [[VivaldiATBManagerHelper alloc] init];
  ruleSourceItem.title = [managerHelper titleForSourceForKey:sourceURL];
  ruleSourceItem.is_from_url = knownSource.core.is_from_url();
  ruleSourceItem.source_url = sourceURL;

  BOOL isEnabled = _ruleSourceHandler->IsSourceEnabled(
                       sourceType == ATBSourceAds ?
                       RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules,
                       key);
  ruleSourceItem.is_enabled = isEnabled;
  ruleSourceItem.is_default = !knownSource.removable;
  ruleSourceItem.is_loaded = NO;
  ruleSourceItem.list_priority = knownSource.removable ? 2 : 1;
  ruleSourceItem.is_fetching = NO;

  // Check if the source is enabled. If 'Yes' get more info from the engine.
  if (isEnabled) {
    std::optional<ActiveRuleSource> ruleSource =
        _ruleManager->GetRuleSource(
            sourceType == ATBSourceAds ?
            RuleGroup::kAdBlockingRules : RuleGroup::kTrackingRules,
            key);
    ruleSourceItem.last_update = ruleSource->last_update;
    ruleSourceItem.rules_list_checksum = ruleSource->rules_list_checksum;
    ruleSourceItem.is_fetching = ruleSource->is_fetching;
    ruleSourceItem.is_loaded = YES;

    std::string unsafeTitle = ruleSource->unsafe_adblock_metadata.title;
    if (!unsafeTitle.empty()) {
      ruleSourceItem.title = base::SysUTF8ToNSString(unsafeTitle);
    }
  } else {
    ruleSourceItem.is_fetching = NO;
  }

  return ruleSourceItem;
}

/// Sets the default tracker blocking settings.
- (void)setTrackerBlockingEnabled:(BOOL)enableBlocking {
  if (!_ruleManager)
    return;
  return _ruleManager->
      SetActiveExceptionList(RuleGroup::kTrackingRules,
                             enableBlocking ? kExemptList : kProcessList);
}

/// Sets the default ad blocking settings.
- (void)setAdBlockingEnabled:(BOOL)enableBlocking {
  if (!_ruleManager)
    return;
  return _ruleManager->
      SetActiveExceptionList(RuleGroup::kAdBlockingRules,
                             enableBlocking ? kExemptList : kProcessList);
}

/// Sets tracker blocking exceptions for a given domain based
/// on 'enableBlocking' value.
/// Removes any exceptions set first.
- (void)setTrackerBlockingExceptionForDomain:(NSString*)domain
                              enableBlocking:(BOOL)enableBlocking {
  if (!_ruleManager)
    return;

  NSString* host = [VivaldiGlobalHelpers hostOfURLString:domain];
  std::string hostString = base::SysNSStringToUTF8(host);

  _ruleManager->
      RemoveExceptionForDomain(RuleGroup::kTrackingRules,
                               enableBlocking ? kExemptList : kProcessList,
                               hostString);

  _ruleManager->
      AddExceptionForDomain(RuleGroup::kTrackingRules,
                            enableBlocking ? kProcessList : kExemptList,
                            hostString);
}

/// Sets ad blocking exceptions for a given domain based
/// on 'enableBlocking' value.
/// Removes any exceptions set first.
- (void)setAdBlockingExceptionForDomain:(NSString*)domain
                         enableBlocking:(BOOL)enableBlocking {
  if (!_ruleManager)
    return;

  NSString* host = [VivaldiGlobalHelpers hostOfURLString:domain];
  std::string hostString = base::SysNSStringToUTF8(host);

  _ruleManager->
      RemoveExceptionForDomain(RuleGroup::kAdBlockingRules,
                               enableBlocking ? kExemptList: kProcessList,
                               hostString);

  _ruleManager->
      AddExceptionForDomain(RuleGroup::kAdBlockingRules,
                            enableBlocking ? kProcessList : kExemptList,
                            hostString);
}

#pragma mark: - OBSERVERS
#pragma mark: - VivaldiATBConsumer

- (void)didRefreshSettingOptions:(NSArray*)options {
  // No op.
}

- (void)didRefreshExceptionsList:(NSArray*)exceptions {
  // No op.
}

- (void)didRefreshSourcesList:(NSArray*)sources {
  // No op.
}

- (void)ruleServiceStateDidLoad {
  [self initRuleManagerAndSourceHandler];
  SEL selector = @selector(ruleServiceStateDidLoad);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer ruleServiceStateDidLoad];
  }
}

- (void)rulesListDidStartApplying:(RuleGroup)group {
  SEL selector = @selector(rulesListDidStartApplying:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer rulesListDidStartApplying:group];
  }
}

- (void)rulesListDidEndApplying:(RuleGroup)group {
  SEL selector = @selector(rulesListDidEndApplying:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer rulesListDidEndApplying:group];
  }
}

- (void)ruleSourceDidUpdate:(uint32_t)key
                      group:(RuleGroup)group
                fetchResult:(ATBFetchResult)fetchResult {
  SEL selector = @selector(ruleSourceDidUpdate:group:fetchResult:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer ruleSourceDidUpdate:key
                                 group:group
                           fetchResult:fetchResult];
  }
}

- (void)ruleSourceDidRemove:(uint32_t)key
                      group:(RuleGroup)group {
  SEL selector = @selector(ruleSourceDidRemove:group:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer ruleSourceDidRemove:key
                                 group:group];
  }
}

- (void)exceptionListStateDidChange:(RuleGroup)group {
  SEL selector = @selector(exceptionListStateDidChange:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer exceptionListStateDidChange:group];
  }
  [self getSettingOptions];
  [self getAllExceptionsList];
}

- (void)exceptionListDidChange:(RuleGroup)group
                          list:(RuleManager::ExceptionsList)list {
  SEL selector = @selector(exceptionListDidChange:list:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer exceptionListDidChange:group list:list];
  }
  [self getAllExceptionsList];
}

- (void)knownSourceDidAdd:(RuleGroup)group
                      key:(uint32_t)key {
  SEL selector = @selector(knownSourceDidAdd:key:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer knownSourceDidAdd:group key:key];
  }
  [self refreshSourceList:group];
}

- (void)knownSourceDidRemove:(RuleGroup)group
                         key:(uint32_t)key {
  SEL selector = @selector(knownSourceDidRemove:key:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer knownSourceDidRemove:group
                                    key:key];
  }
  [self refreshSourceList:group];
}

- (void)knownSourceDidEnable:(RuleGroup)group
                         key:(uint32_t)key {
  SEL selector = @selector(knownSourceDidEnable:key:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer knownSourceDidEnable:group
                                    key:key];
  }
}

- (void)knownSourceDidDisable:(RuleGroup)group
                          key:(uint32_t)key {
  SEL selector = @selector(knownSourceDidDisable:key:);
  if ([self.consumer respondsToSelector:selector]) {
    [self.consumer knownSourceDidDisable:group
                                     key:key];
  }
}

@end
