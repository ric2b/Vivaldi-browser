// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_prefs.h"
#import "prefs/vivaldi_pref_names.h"
#import "vivaldi/prefs/vivaldi_gen_prefs.h"

@interface VivaldiAddressBarSettingsMediator () <BooleanObserver> {
  PrefService* _prefService;

  PrefBackedBoolean* _showFullAddressEnabled;
  PrefBackedBoolean* _showXForSuggestionsEnabled;
  PrefBackedBoolean* _showTypedHistoryOnFocusEnabled;

  PrefBackedBoolean* _bookmarksEnabled;
  PrefBackedBoolean* _bookmarksBoostedEnabled;
  PrefBackedBoolean* _bookmarkNicknamesEnabled;
  PrefBackedBoolean* _searchSuggestionsEnabled;
  PrefBackedBoolean* _searchHistoryEnabled;
  PrefBackedBoolean* _historyEnabled;
  PrefBackedBoolean* _directMatchEnabled;
  PrefBackedBoolean* _directMatchPrioritizationEnabled;
}
@end

@implementation VivaldiAddressBarSettingsMediator

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _prefService = originalPrefService;

    PrefService* localPrefs = GetApplicationContext()->GetLocalState();
    _showFullAddressEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:localPrefs
                 prefName:vivaldiprefs::kVivaldiShowFullAddressEnabled];
    [_showFullAddressEnabled setObserver:self];
    [self booleanDidChange:_showFullAddressEnabled];

    _showXForSuggestionsEnabled =
        [[PrefBackedBoolean alloc]
           initWithPrefService:localPrefs
              prefName:vivaldiprefs::kVivaldiShowXForSuggestionEnabled];
    [_showXForSuggestionsEnabled setObserver:self];
    [self booleanDidChange:_showXForSuggestionsEnabled];

    _searchSuggestionsEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:prefs::kSearchSuggestEnabled];
    [_searchSuggestionsEnabled setObserver:self];
    [self booleanDidChange:_searchSuggestionsEnabled];

    _showTypedHistoryOnFocusEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                prefName:vivaldiprefs::kAddressBarOmniboxShowTypedHistory];
    [_showTypedHistoryOnFocusEnabled setObserver:self];
    [self booleanDidChange:_showTypedHistoryOnFocusEnabled];

    _historyEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                 prefName:vivaldiprefs::kAddressBarOmniboxShowBrowserHistory];
    [_historyEnabled setObserver:self];
    [self booleanDidChange:_historyEnabled];

    _searchHistoryEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                 prefName:vivaldiprefs::kAddressBarOmniboxSearchHistoryEnable];
    [_searchHistoryEnabled setObserver:self];
    [self booleanDidChange:_searchHistoryEnabled];

    _bookmarksEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                 prefName:vivaldiprefs::kAddressBarOmniboxBookmarks];
    [_bookmarksEnabled setObserver:self];
    [self booleanDidChange:_bookmarksEnabled];

    _bookmarksBoostedEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                 prefName:vivaldiprefs::kAddressBarOmniboxBookmarksBoosted];
    [_bookmarksBoostedEnabled setObserver:self];
    [self booleanDidChange:_bookmarksBoostedEnabled];

    _bookmarkNicknamesEnabled =
        [[PrefBackedBoolean alloc]
           initWithPrefService:originalPrefService
              prefName:vivaldiprefs::kAddressBarOmniboxShowNicknames];
    [_bookmarkNicknamesEnabled setObserver:self];
    [self booleanDidChange:_bookmarkNicknamesEnabled];

    _directMatchEnabled =
        [[PrefBackedBoolean alloc]
           initWithPrefService:originalPrefService
              prefName:vivaldiprefs::kAddressBarSearchDirectMatchEnabled];
    [_directMatchEnabled setObserver:self];
    [self booleanDidChange:_directMatchEnabled];

    _directMatchPrioritizationEnabled =
        [[PrefBackedBoolean alloc]
           initWithPrefService:originalPrefService
              prefName:vivaldiprefs::kAddressBarSearchDirectMatchBoosted];
    [_directMatchPrioritizationEnabled setObserver:self];
    [self booleanDidChange:_directMatchPrioritizationEnabled];
  }
  return self;
}

- (void)disconnect {
  [_showFullAddressEnabled stop];
  [_showFullAddressEnabled setObserver:nil];
  _showFullAddressEnabled = nil;

  [_searchSuggestionsEnabled stop];
  [_searchSuggestionsEnabled setObserver:nil];
  _searchSuggestionsEnabled = nil;

  [_showXForSuggestionsEnabled stop];
  [_showXForSuggestionsEnabled setObserver:nil];
  _showXForSuggestionsEnabled = nil;

  [_showTypedHistoryOnFocusEnabled stop];
  [_showTypedHistoryOnFocusEnabled setObserver:nil];
  _showTypedHistoryOnFocusEnabled = nil;

  [_historyEnabled stop];
  [_historyEnabled setObserver:nil];
  _historyEnabled = nil;

  [_searchHistoryEnabled stop];
  [_searchHistoryEnabled setObserver:nil];
  _searchHistoryEnabled = nil;

  [_bookmarksEnabled stop];
  [_bookmarksEnabled setObserver:nil];
  _bookmarksEnabled = nil;

  [_bookmarksBoostedEnabled stop];
  [_bookmarksBoostedEnabled setObserver:nil];
  _bookmarksBoostedEnabled = nil;

  [_bookmarkNicknamesEnabled stop];
  [_bookmarkNicknamesEnabled setObserver:nil];
  _bookmarkNicknamesEnabled = nil;

  [_directMatchEnabled stop];
  [_directMatchEnabled setObserver:nil];
  _directMatchEnabled = nil;

  [_directMatchPrioritizationEnabled stop];
  [_directMatchPrioritizationEnabled setObserver:nil];
  _directMatchPrioritizationEnabled = nil;

  _prefService = nil;
  _consumer = nil;
}

#pragma mark - Private Helpers
- (BOOL)showFullAddressEnabled {
  if (!_showFullAddressEnabled) {
    return NO;
  }
  return [_showFullAddressEnabled value];
}

- (BOOL)isShowXForSuggestionsEnabled {
  if (!_showXForSuggestionsEnabled) {
    return NO;
  }
  return [_showXForSuggestionsEnabled value];
}

- (BOOL)searchSuggestionsEnabled {
  if (!_searchSuggestionsEnabled) {
    return NO;
  }
  return [_searchSuggestionsEnabled value];
}

- (BOOL)showTypedHistoryOnFocusEnabled {
  if (!_showTypedHistoryOnFocusEnabled) {
    return NO;
  }
  return [_showTypedHistoryOnFocusEnabled value];
}

- (BOOL)isHistoryEnabled {
  if (!_historyEnabled) {
    return NO;
  }
  return [_historyEnabled value];
}

- (BOOL)isSearchHistoryEnabled {
  if (!_searchHistoryEnabled) {
    return NO;
  }
  return [_searchHistoryEnabled value];
}

- (BOOL)isBookmarksEnabled {
  if (!_bookmarksEnabled) {
    return NO;
  }
  return [_bookmarksEnabled value];
}

- (BOOL)isBookmarksBoostedEnabled {
  if (!_bookmarksBoostedEnabled) {
    return NO;
  }
  return [_bookmarksBoostedEnabled value];
}

- (BOOL)isBookmarkNicknamesEnabled {
  if (!_bookmarkNicknamesEnabled) {
    return NO;
  }
  return [_bookmarkNicknamesEnabled value];
}

- (BOOL)isDirectMatchEnabled {
  if (!_directMatchEnabled) {
    return NO;
  }
  return [_directMatchEnabled value];
}

- (BOOL)isDirectMatchPrioritizationEnabled {
  if (!_directMatchPrioritizationEnabled) {
    return NO;
  }
  return [_directMatchPrioritizationEnabled value];
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiAddressBarSettingsConsumer>)consumer {
  _consumer = consumer;
  // Options
  [self.consumer setPreferenceForShowFullAddress:[self showFullAddressEnabled]];
  [self.consumer
      setPreferenceForShowXForSugggestions:
          [self isShowXForSuggestionsEnabled]];
  [self.consumer
      setPreferenceForEnableSearchSuggestions:[self searchSuggestionsEnabled]];
  [self.consumer
      setPreferenceForShowTypedHistoryOnFocus:
          [self showTypedHistoryOnFocusEnabled]];

  // Priorities
  [self.consumer setPreferenceForEnableHistory:[self isHistoryEnabled]];
  [self.consumer
      setPreferenceForEnableSearchHistory:[self isSearchHistoryEnabled]];
  [self.consumer setPreferenceForEnableBookmarks:[self isBookmarksEnabled]];
  [self.consumer setPreferenceForEnableBookmarksBoosted:
      [self isBookmarksBoostedEnabled]];
  [self.consumer setPreferenceForEnableBookmarkNicknames:
      [self isBookmarkNicknamesEnabled]];
  [self.consumer setPreferenceForEnableDirectMatch:[self isDirectMatchEnabled]];
  [self.consumer
      setPreferenceForEnableDirectMatchPrioritization:
          [self isDirectMatchPrioritizationEnabled]];
}

#pragma mark - VivaldiAddressBarSettingsConsumer

- (void)setPreferenceForShowFullAddress:(BOOL)show {
  if (show != [self showFullAddressEnabled])
    [_showFullAddressEnabled setValue:show];
}

- (void)setPreferenceForShowXForSugggestions:(BOOL)show {
  if (show != [self isShowXForSuggestionsEnabled])
    [_showXForSuggestionsEnabled setValue:show];
}

- (void)setPreferenceForEnableSearchSuggestions:(BOOL)enable {
  if (enable != [self searchSuggestionsEnabled])
    [_searchSuggestionsEnabled setValue:enable];
}

- (void)setPreferenceForShowTypedHistoryOnFocus:(BOOL)show {
  if (show != [self showTypedHistoryOnFocusEnabled])
    [_showTypedHistoryOnFocusEnabled setValue:show];
}

- (void)setPreferenceForEnableHistory:(BOOL)enable {
  if (enable != [self isHistoryEnabled])
    [_historyEnabled setValue:enable];
}

- (void)setPreferenceForEnableSearchHistory:(BOOL)enable {
  if (enable != [self isSearchHistoryEnabled])
    [_searchHistoryEnabled setValue:enable];
}

- (void)setPreferenceForEnableBookmarks:(BOOL)enable {
  if (enable != [self isBookmarksEnabled])
    [_bookmarksEnabled setValue:enable];
}

- (void)setPreferenceForEnableBookmarksBoosted:(BOOL)enable {
  if (enable != [self isBookmarksBoostedEnabled])
    [_bookmarksBoostedEnabled setValue:enable];
}

- (void)setPreferenceForEnableBookmarkNicknames:(BOOL)enable {
  if (enable != [self isBookmarkNicknamesEnabled])
    [_bookmarkNicknamesEnabled setValue:enable];
}

- (void)setPreferenceForEnableDirectMatch:(BOOL)enable {
  if (enable != [self isDirectMatchEnabled])
    [_directMatchEnabled setValue:enable];
}

- (void)setPreferenceForEnableDirectMatchPrioritization:(BOOL)enable {
  if (enable != [self isDirectMatchPrioritizationEnabled])
    [_directMatchPrioritizationEnabled setValue:enable];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showFullAddressEnabled) {
    [self.consumer setPreferenceForShowFullAddress:[observableBoolean value]];
  } else if (observableBoolean == _showXForSuggestionsEnabled) {
    [self.consumer
        setPreferenceForShowXForSugggestions:[observableBoolean value]];
  } else if (observableBoolean == _searchSuggestionsEnabled) {
    [self.consumer
        setPreferenceForEnableSearchSuggestions:[observableBoolean value]];
  } else if (observableBoolean == _showTypedHistoryOnFocusEnabled) {
    [self.consumer
        setPreferenceForShowTypedHistoryOnFocus:[observableBoolean value]];
  } else if (observableBoolean == _historyEnabled) {
    [self.consumer setPreferenceForEnableHistory:[observableBoolean value]];
  } else if (observableBoolean == _searchHistoryEnabled) {
    [self.consumer
        setPreferenceForEnableSearchHistory:[observableBoolean value]];
  } else if (observableBoolean == _bookmarksEnabled) {
    [self.consumer setPreferenceForEnableBookmarks:[observableBoolean value]];
  } else if (observableBoolean == _bookmarksBoostedEnabled) {
    [self.consumer
        setPreferenceForEnableBookmarksBoosted:[observableBoolean value]];
  } else if (observableBoolean == _bookmarkNicknamesEnabled) {
    [self.consumer setPreferenceForEnableBookmarkNicknames:
        [observableBoolean value]];
  } else if (observableBoolean == _directMatchEnabled) {
    [self.consumer setPreferenceForEnableDirectMatch:[observableBoolean value]];
  } else if (observableBoolean == _directMatchPrioritizationEnabled) {
    [self.consumer
        setPreferenceForEnableDirectMatchPrioritization:
            [observableBoolean value]];
  }
}

@end
