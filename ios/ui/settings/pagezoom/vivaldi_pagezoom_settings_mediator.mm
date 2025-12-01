// Copyright 2024-25 Vivaldi Technologies. All rights reserved.

#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_mediator.h"

#import "base/containers/flat_map.h"
#import "base/values.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/font_size/font_size_tab_helper.h"
#import "ios/public/provider/chrome/browser/text_zoom/text_zoom_api.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/settings/pagezoom/vivaldi_pagezoom_settings_prefs.h"
#import "prefs/vivaldi_pref_names.h"

namespace {
// Recursively processes a dictionary to extract zoom levels for domains.
// For nested dictionaries, it builds dot-separated keys
// (e.g. "example.com.subdomain")
// and stores the associated zoom level values in the result map.
// Parameters:
//   dict - The dictionary to process
//   prefix - Current prefix for nested keys (empty for root level)
//   result - Map to store domain->zoom_level mappings
void ProcessDictionary(const base::Value::Dict& dict,
                       const std::string& prefix,
                       base::flat_map<std::string, double>& result) {
  // Iterate through all key-value pairs in the dictionary
  for (auto it = dict.begin(); it != dict.end(); ++it) {
    // Extract the key and value from the current dictionary entry
    const std::string& key = it->first;
    const base::Value& value = it->second;

    // Build the domain key by concatenating the current key with any existing prefix
    // For example: "example.com" or "example.com.subdomain"
    std::string new_prefix = prefix.empty() ? key : prefix + "." + key;

    if (value.is_dict()) {
      // If the value is another dictionary, recursively process it
      // This handles nested domain structures
      ProcessDictionary(value.GetDict(), new_prefix, result);
    } else if (value.is_double()) {
      // If the value is a number (zoom level), store it in the result map
      // with the full domain path as the key
      result[new_prefix] = value.GetDouble();
    }
  }
}
}  // namespace

@implementation VivaldiPageZoomSettingsMediator {
  PrefService* _prefService;
}

- (instancetype)initWithOriginalPrefService:(PrefService*)originalPrefService {
  self = [super init];
  if (self) {
    _prefService = originalPrefService;
    [self.consumer setPreferenceForPageZoomLevel:self.pageZoomLevel];
  }
  return self;
}

- (void)disconnect {
  _consumer = nil;
}

#pragma mark - Properties

- (void)setConsumer:(id<VivaldiPageZoomSettingsConsumer>)consumer {
  _consumer = consumer;
  [self.consumer setPreferenceForPageZoomLevel:self.pageZoomLevel];
  [self.consumer setPreferenceForDomainSettings:self.domainSettings];
}

- (void)resetUsersDomainZoomPref {
  if (_prefService) {
    // Clear the domain settings
    _prefService->ClearPref(prefs::kIosUserZoomMultipliers);
    // Update the consumer for updating theview
    [self.consumer setPreferenceForDomainSettings:@{}];
    // Update the page zoom levels for all web states
    [self.consumer setPreferenceForPageZoomLevel:self.pageZoomLevel];
  }
}

#pragma mark - VivaldiPageZoomSettingsConsumer
- (void)setPreferenceForPageZoomLevel:(NSInteger)level {
  [VivaldiPageZoomSettingPrefs
      setPageZoomLevelWithPrefService:level inPrefServices: _prefService];
  // Applying the zoom level for all web states
  if (_browser) {
    WebStateList* webStateList = _browser->GetWebStateList();
    for (int i = 0; i < webStateList->count(); i++) {
      web::WebState* webState = webStateList->GetWebStateAt(i);
      if (webState) {
        FontSizeTabHelper* fontSizeTabHelper =
          FontSizeTabHelper::FromWebState(webState);
        fontSizeTabHelper->
          SetPageZoomSize(fontSizeTabHelper->GetFontZoomSize());
      }
    }
  }
}

- (void)setPreferenceForDomainSettings:
    (NSDictionary<NSString *, NSNumber *> *)dict {
  // no op
}

#pragma mark - Private Helpers

- (double)pageZoomLevel {
  return [VivaldiPageZoomSettingPrefs
          getPageZoomLevelWithPrefService: _prefService];
}

- (NSDictionary<NSString*, NSNumber*>*)domainSettings {
  // Return empty dictionary if preference service is not initialized
  if (!_prefService) {
    return @{};
  }

  // Get the dictionary of zoom multipliers from preferences
  const base::Value::Dict& pref =
      _prefService->GetDict(prefs::kIosUserZoomMultipliers);

  // Create a map to store domain->zoom level mappings
  base::flat_map<std::string, double> result;
  // Process the preference dictionary and populate the result map
  ProcessDictionary(pref, "", result);

  // Convert the C++ map to an Objective-C dictionary
  NSMutableDictionary<NSString*, NSNumber*>* nsDict =
    [[NSMutableDictionary alloc] init];
  for (const auto& [domain, zoom] : result) {
    // Convert domain string and zoom value to Objective-C types
    NSString* nsDomain = [NSString stringWithUTF8String:domain.c_str()];
    NSNumber* nsZoom = [NSNumber numberWithDouble:zoom];
    [nsDict setObject:nsZoom forKey:nsDomain];
  }
  return nsDict;
}

@end
