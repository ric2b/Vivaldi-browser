// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/search_engine/vivaldi_search_engine_settings_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "prefs/vivaldi_pref_names.h"

@interface VivaldiSearchEngineSettingsMediator () <BooleanObserver,
                                                  SearchEngineObserving> {
  ProfileIOS* _profile;  // weak
  TemplateURLService* _templateURLService;  // weak
  // Bridge for TemplateURLServiceObserver.
  std::unique_ptr<SearchEngineObserverBridge> _observer;

  // Boolean observer for search engine nickname
  PrefBackedBoolean* _searchEngineNicknameEnabled;
  // Boolean observer for search suggestion
  PrefBackedBoolean* _searchSuggestionsEnabled;
}
@end

@implementation VivaldiSearchEngineSettingsMediator

- (instancetype)initWithProfile:(ProfileIOS*)profile {
  self = [super init];
  if (self) {
    _profile = profile;
    _templateURLService =
        ios::TemplateURLServiceFactory::GetForProfile(profile);
    _observer =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    _templateURLService->Load();

    PrefService* localPrefs = GetApplicationContext()->GetLocalState();
    _searchEngineNicknameEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:localPrefs
                prefName:vivaldiprefs::kVivaldiEnableSearchEngineNickname];
    [_searchEngineNicknameEnabled setObserver:self];
    [self booleanDidChange:_searchEngineNicknameEnabled];

    _searchSuggestionsEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:profile->GetPrefs()
                prefName:prefs::kSearchSuggestEnabled];
    [_searchSuggestionsEnabled setObserver:self];
    [self booleanDidChange:_searchSuggestionsEnabled];

  }
  return self;
}

- (void)disconnect {
  [_searchEngineNicknameEnabled stop];
  [_searchEngineNicknameEnabled setObserver:nil];
  _searchEngineNicknameEnabled = nil;

  [_searchSuggestionsEnabled stop];
  [_searchSuggestionsEnabled setObserver:nil];
  _searchSuggestionsEnabled = nil;

  // Remove observer bridges.
  _observer.reset();

  _profile = nullptr;
  _templateURLService = nullptr;
  _consumer = nil;
}

#pragma mark - Private Helpers
- (BOOL)searchEngineNicknameEnabled {
  if (!_searchEngineNicknameEnabled) {
    return YES;
  }
  return [_searchEngineNicknameEnabled value];
}

- (BOOL)searchSuggestionsEnabled {
  if (!_searchSuggestionsEnabled) {
    return NO;
  }
  return [_searchSuggestionsEnabled value];
}

- (NSString*)searchEngineNameForType:
    (TemplateURLService::DefaultSearchType)type {
  if (!_templateURLService) {
    return @"";
  }
  const TemplateURL* const default_provider =
      _templateURLService->GetDefaultSearchProvider(type);

  if (!default_provider) {
    return @"";
  }
  return base::SysUTF16ToNSString(default_provider->short_name());
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiSearchEngineSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer
      setPreferenceForEnableSearchEngineNickname:
          [self searchEngineNicknameEnabled]];
  [self.consumer
      setPreferenceForEnableSearchSuggestions:
          [self searchSuggestionsEnabled]];
  [self.consumer
      setSearchEngineForRegularTabs:
          [self searchEngineNameForType:
                  TemplateURLService::kDefaultSearchMain]];
  [self.consumer
      setSearchEngineForPrivateTabs:
         [self searchEngineNameForType:
                  TemplateURLService::kDefaultSearchPrivate]];
}

#pragma mark - VivaldiSearchEngineSettingsViewControllerDelegate

- (void)searchEngineNicknameEnabled:(BOOL)enabled {
  if (enabled != [self searchEngineNicknameEnabled])
    [_searchEngineNicknameEnabled setValue:enabled];
}

- (void)searchSuggestionsEnabled:(BOOL)enabled {
  if (enabled != [self searchSuggestionsEnabled])
    [_searchSuggestionsEnabled setValue:enabled];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _searchEngineNicknameEnabled) {
    [self.consumer
         setPreferenceForEnableSearchEngineNickname:
            [self searchEngineNicknameEnabled]];
  } else if (observableBoolean == _searchSuggestionsEnabled) {
    [self.consumer
        setPreferenceForEnableSearchSuggestions:
            [self searchSuggestionsEnabled]];
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  [self.consumer
       setSearchEngineForRegularTabs:
            [self searchEngineNameForType:
                    TemplateURLService::kDefaultSearchMain]];
  [self.consumer
       setSearchEngineForPrivateTabs:
            [self searchEngineNameForType:
                    TemplateURLService::kDefaultSearchPrivate]];
}

@end
