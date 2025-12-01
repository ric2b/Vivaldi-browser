// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_mediator.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/ui/settings/addressbar/vivaldi_addressbar_settings_prefs.h"
#import "prefs/vivaldi_pref_names.h"
#import "vivaldi/prefs/vivaldi_gen_prefs.h"

@interface VivaldiAddressBarSettingsMediator () <BooleanObserver> {
  PrefService* _prefService;

  PrefBackedBoolean* _showFullAddressEnabled;
  PrefBackedBoolean* _bookmarksBoostedEnabled;
  PrefBackedBoolean* _bookmarkNicknamesEnabled;
  PrefBackedBoolean* _directMatchEnabled;
  PrefBackedBoolean* _directMatchPrioritizationEnabled;
  PrefBackedBoolean* _showXForSuggestionsEnabled;
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

    _showXForSuggestionsEnabled =
        [[PrefBackedBoolean alloc]
           initWithPrefService:localPrefs
              prefName:vivaldiprefs::kVivaldiShowXForSuggestionEnabled];
    [_showXForSuggestionsEnabled setObserver:self];
    [self booleanDidChange:_showXForSuggestionsEnabled];
  }
  return self;
}

- (void)disconnect {
  [_showFullAddressEnabled stop];
  [_showFullAddressEnabled setObserver:nil];
  _showFullAddressEnabled = nil;

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

  [_showXForSuggestionsEnabled stop];
  [_showXForSuggestionsEnabled setObserver:nil];
  _showXForSuggestionsEnabled = nil;

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

- (BOOL)isShowXForSuggestionsEnabled {
  if (!_showXForSuggestionsEnabled) {
    return NO;
  }
  return [_showXForSuggestionsEnabled value];
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiAddressBarSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForShowFullAddress:[self showFullAddressEnabled]];
  [self.consumer setPreferenceForEnableBookmarksBoosted:
      [self isBookmarksBoostedEnabled]];
  [self.consumer setPreferenceForEnableBookmarkNicknames:
      [self isBookmarkNicknamesEnabled]];
  [self.consumer setPreferenceForEnableDirectMatch:[self isDirectMatchEnabled]];
  [self.consumer
      setPreferenceForEnableDirectMatchPrioritization:
          [self isDirectMatchPrioritizationEnabled]];
  [self.consumer
      setPreferenceForShowXForSugggestions:
          [self isShowXForSuggestionsEnabled]];
}

#pragma mark - VivaldiAddressBarSettingsConsumer
- (void)setPreferenceForShowFullAddress:(BOOL)show {
  if (show != [self showFullAddressEnabled])
    [_showFullAddressEnabled setValue:show];
}

- (void)setPreferenceForEnableBookmarksBoosted:(BOOL)enableMatching {
  if (enableMatching != [self isBookmarksBoostedEnabled])
    [_bookmarksBoostedEnabled setValue:enableMatching];
}

- (void)setPreferenceForEnableBookmarkNicknames:(BOOL)enableMatching {
  if (enableMatching != [self isBookmarkNicknamesEnabled])
    [_bookmarkNicknamesEnabled setValue:enableMatching];
}

- (void)setPreferenceForEnableDirectMatch:(BOOL)enable {
  if (enable != [self isDirectMatchEnabled])
    [_directMatchEnabled setValue:enable];
}

- (void)setPreferenceForEnableDirectMatchPrioritization:(BOOL)enable {
  if (enable != [self isDirectMatchPrioritizationEnabled])
    [_directMatchPrioritizationEnabled setValue:enable];
}

- (void)setPreferenceForShowXForSugggestions:(BOOL)show {
  if (show != [self isShowXForSuggestionsEnabled])
    [_showXForSuggestionsEnabled setValue:show];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  if (observableBoolean == _showFullAddressEnabled) {
    [self.consumer setPreferenceForShowFullAddress:[observableBoolean value]];
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
  } else if (observableBoolean == _showXForSuggestionsEnabled) {
    [self.consumer
        setPreferenceForShowXForSugggestions:[observableBoolean value]];
  }
}

@end
