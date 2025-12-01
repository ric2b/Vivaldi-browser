// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"

#import "base/apple/foundation_util.h"
#import "base/memory/weak_ptr.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/web/public/web_state.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/settings/vivaldi_settings_constants.h"
#import "ios/web/public/browser_state.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi

@implementation WebStateTabSwitcherItem {
  // The web state represented by this item.
  base::WeakPtr<web::WebState> _webState;
}

- (instancetype)initWithWebState:(web::WebState*)webState {
  DCHECK(webState);
  self = [super initWithIdentifier:webState->GetUniqueIdentifier()];
  if (self) {
    _webState = webState->GetWeakPtr();
  }
  return self;
}

- (web::WebState*)webState {
  return _webState.get();
}

- (GURL)URL {
  if (!_webState) {
    return GURL();
  }
  return _webState->GetVisibleURL();
}

- (NSString*)title {
  if (!_webState) {
    return nil;
  }

  if (IsVivaldiRunning() && IsUrlNtp(_webState->GetVisibleURL())) {
    NSString* pageTitle = _webState->GetBrowserState()->IsOffTheRecord() ?
        l10n_util::GetNSString(IDS_IOS_TABS_NEW_PRIVATE_TAB) :
        l10n_util::GetNSString(IDS_IOS_TABS_SPEED_DIAL);
    return pageTitle;
  } // End Vivaldi

  return tab_util::GetTabTitle(_webState.get());
}

- (BOOL)hidesTitle {

  if (IsVivaldiRunning())
    return NO; // End Vivaldi

  if (!_webState) {
    return NO;
  }
  return IsUrlNtp(_webState->GetVisibleURL());
}

- (BOOL)showsActivity {
  if (!_webState) {
    return NO;
  }
  return _webState->IsLoading();
}

#pragma mark - Favicons

- (UIImage*)NTPFavicon {

  if (IsVivaldiRunning())
    return [UIImage imageNamed:vToolbarMenu]; // End Vivaldi

  return [[UIImage alloc] init];
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[WebStateTabSwitcherItem class]]) {
    return NO;
  }
  WebStateTabSwitcherItem* otherItem =
      base::apple::ObjCCastStrict<WebStateTabSwitcherItem>(object);
  return self.identifier == otherItem.identifier;
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(self.identifier.identifier());
}

// Vivaldi
- (BOOL)isNTP {
  if (!_webState) {
    return YES;
  }
  return IsUrlNtp(_webState->GetVisibleURL());
}

- (UIColor*)themeColor {
  if (!_webState) {
    return nil;
  }
  return _webState->GetThemeColor();
}
// End Vivaldi

@end
