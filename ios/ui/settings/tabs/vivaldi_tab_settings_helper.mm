// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/settings/tabs/vivaldi_tab_settings_helper.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "url/gurl.h"

@implementation VivaldiTabSettingsHelper

// It wil return homepage URL set by user or startpage
+ (NSString*)getHomePageURLWithPref:(PrefService*)prefService {
  NSString* url = [VivaldiTabSettingPrefs
                    getHomepageUrlWithPrefService: prefService];
  if ([VivaldiGlobalHelpers isValidURL:url])
    return url;
  else
    return base::SysUTF8ToNSString(kChromeUINewTabURL);
}

// It wil return newtab URL set by user or other internal pages
+ (NSString*)getNewTabURLWithPref:(PrefService*)prefService {
  VivaldiNTPType setting = [VivaldiTabSettingPrefs
                            getNewTabSettingWithPrefService: prefService];
  NSString *newtab = base::SysUTF8ToNSString(kChromeUINewTabURL);
  NSString *blankpage = base::SysUTF8ToNSString(url::kAboutBlankURL);
  NSString* url = nil;
  switch (setting) {
    case VivaldiNTPTypeStartpage:
      url = newtab;
      break;
    case VivaldiNTPTypeHomepage:
      url = [VivaldiTabSettingPrefs getHomepageUrlWithPrefService: prefService];
      break;
    case VivaldiNTPTypeBlankpage:
      url = blankpage;
      break;
    case VivaldiNTPTypeURL:
      url = [VivaldiTabSettingPrefs getNewTabUrlWithPrefService: prefService];
      break;
    default:
      url = newtab;
      break;
  }
  if ([VivaldiGlobalHelpers isValidURL:url] ||
       [url isEqualToString: blankpage])
    return url;
  else
    return newtab;
}

@end
