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
  PrefBackedBoolean* _bookmarksMatchingEnabled;
  PrefBackedBoolean* _bookmarksNicknameMatchingEnabled;
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

    _bookmarksMatchingEnabled =
        [[PrefBackedBoolean alloc]
            initWithPrefService:originalPrefService
                 prefName:vivaldiprefs::kAddressBarOmniboxShowBookmarks];
    [_bookmarksMatchingEnabled setObserver:self];
    [self booleanDidChange:_bookmarksMatchingEnabled];

    _bookmarksNicknameMatchingEnabled =
        [[PrefBackedBoolean alloc]
           initWithPrefService:originalPrefService
              prefName:vivaldiprefs::kAddressBarOmniboxShowNicknames];
    [_bookmarksNicknameMatchingEnabled setObserver:self];
    [self booleanDidChange:_bookmarksNicknameMatchingEnabled];

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

  [_bookmarksMatchingEnabled stop];
  [_bookmarksMatchingEnabled setObserver:nil];
  _bookmarksMatchingEnabled = nil;

  [_bookmarksNicknameMatchingEnabled stop];
  [_bookmarksNicknameMatchingEnabled setObserver:nil];
  _bookmarksNicknameMatchingEnabled = nil;

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

- (BOOL)isBookmarksMatchingEnabled {
  if (!_bookmarksMatchingEnabled) {
    return NO;
  }
  return [_bookmarksMatchingEnabled value];
}

- (BOOL)isBookmarksNicknameMatchingEnabled {
  if (!_bookmarksNicknameMatchingEnabled) {
    return NO;
  }
  return [_bookmarksNicknameMatchingEnabled value];
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
  [self.consumer setPreferenceForShowFullAddress:[self showFullAddressEnabled]];
  [self.consumer setPreferenceForEnableBookmarksMatching:
      [self isBookmarksMatchingEnabled]];
  [self.consumer setPreferenceForEnableBookmarksNicknameMatching:
      [self isBookmarksNicknameMatchingEnabled]];
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

- (void)setPreferenceForEnableBookmarksMatching:(BOOL)enableMatching {
  if (enableMatching != [self isBookmarksMatchingEnabled])
    [_bookmarksMatchingEnabled setValue:enableMatching];
}

- (void)setPreferenceForEnableBookmarksNicknameMatching:(BOOL)enableMatching {
  if (enableMatching != [self isBookmarksNicknameMatchingEnabled])
    [_bookmarksNicknameMatchingEnabled setValue:enableMatching];
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
  } else if (observableBoolean == _bookmarksMatchingEnabled) {
    [self.consumer
        setPreferenceForEnableBookmarksMatching:[observableBoolean value]];
  } else if (observableBoolean == _bookmarksNicknameMatchingEnabled) {
    [self.consumer setPreferenceForEnableBookmarksNicknameMatching:
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
