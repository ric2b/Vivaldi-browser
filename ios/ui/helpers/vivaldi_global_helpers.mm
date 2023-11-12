// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/helpers/vivaldi_global_helpers.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/url_formatter/url_fixer.h"
#import "url/gurl.h"

@implementation VivaldiGlobalHelpers

+ (nullable UIWindow*)keyWindow {
  // In iOS 15 and later key windows are a deprecated concept. Window state
  // should be determined at the scene rather than the application level.
  if (@available(iOS 15, *)) {
    NSSet<UIScene*>* windowScenes =
        [UIApplication sharedApplication].connectedScenes;
    for (UIScene* scene : windowScenes) {
      if ([scene.delegate
              conformsToProtocol:@protocol(UIWindowSceneDelegate)]) {
        return [(id<UIWindowSceneDelegate>)scene.delegate window];
      }
    }
  } else {
    NSArray<UIWindow*>* windows = [UIApplication sharedApplication].windows;
    // Find a key window if it exists.
    for (UIWindow* window in windows) {
      if (window.isKeyWindow)
        return window;
    }
  }
  return nil;
}

+ (CGRect)screenSize {
  return [[UIScreen mainScreen] bounds];
}

+ (CGRect)windowSize {
  if (self.keyWindow) {
    return self.keyWindow.frame;
  }
  return CGRectZero;
}


+ (BOOL)isValidOrientation {
  UIDeviceOrientation orientation= [[UIDevice currentDevice] orientation];
  return UIDeviceOrientationIsValidInterfaceOrientation(orientation);
}


+ (BOOL)isiPadOrientationPortrait {

  UIDeviceOrientation orientation= [[UIDevice currentDevice] orientation];

  // If orientation is not valid, show the landscape layout.
  if (!self.isValidOrientation) {
    return NO;
  }

  return UIDeviceOrientationIsPortrait(orientation);
}

+ (BOOL)isDeviceTablet {
  return [UIDevice currentDevice].userInterfaceIdiom
      == UIUserInterfaceIdiomPad;
}

+ (BOOL)isSplitOrSlideOver {
  if (self.keyWindow) {
    return (self.keyWindow.frame.size.width !=
            self.keyWindow.screen.bounds.size.width);
  }
  return NO;
}

+ (IPadLayoutState)iPadLayoutState {

  CGSize screenSize = [self screenSize].size;
  CGSize windowSize = [self windowSize].size;
  CGFloat screenWidth = screenSize.width;
  CGFloat windowWidth = windowSize.width;
  if (CGSizeEqualToSize(screenSize, windowSize)) {
    return LayoutStateFullScreen;
  }

  CGFloat percent = (windowWidth / screenWidth) * 100.0;
  if (percent <= 55.0 && percent >= 45.0) {
    // In between 55% and 45% -> 1/2
    return LayoutStateHalfScreen;
  } else if (percent > 55.0) {
    // More than 55% -> 2/3
    return LayoutStateTwoThirdScreen;
  } else {
    // Less than 45% -> 1/3
    return LayoutStateOneThirdScreen;
  }
}

+ (BOOL)isVerticalTraitCompact {
  UIUserInterfaceSizeClass verticalSizeClass =
      self.keyWindow.traitCollection.verticalSizeClass;
  return verticalSizeClass == UIUserInterfaceSizeClassCompact;
}

+ (BOOL)isHorizontalTraitRegular {
  UIUserInterfaceSizeClass horizontalSizeClass =
      self.keyWindow.traitCollection.horizontalSizeClass;
  return horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

+ (BOOL)isVerticalTraitRegular {
  UIUserInterfaceSizeClass verticalSizeClass =
      self.keyWindow.traitCollection.verticalSizeClass;
  return verticalSizeClass == UIUserInterfaceSizeClassRegular;
}

+ (BOOL)canShowSidePanel {
  return self.isDeviceTablet &&
      ((self.isHorizontalTraitRegular && self.isVerticalTraitRegular) ||
       self.iPadLayoutState == LayoutStateFullScreen ||
       self.iPadLayoutState == LayoutStateTwoThirdScreen);
}

+ (BOOL)isValidDomain:(NSString* _Nonnull)urlString {
  NSString *pattern =
      @"^(https?://)?([\\da-z.-]+)\\.([a-z.]{2,6})([/\\w .-]*)*/?$";
  NSError *error = nil;
  NSRegularExpression *regex =
    [NSRegularExpression regularExpressionWithPattern:pattern
          options:NSRegularExpressionCaseInsensitive
            error:&error];

  NSTextCheckingResult *match =
    [regex firstMatchInString:urlString
                      options:0
                        range:NSMakeRange(0, [urlString length])];
  return match;
}

+ (BOOL)isValidURL:(NSString* _Nonnull)urlString {
  if (![VivaldiGlobalHelpers urlStringHasHTTPorHTTPS:urlString]) {
    urlString = [NSString stringWithFormat:@"http://%@", urlString];
  }

  NSURL *url = [NSURL URLWithString:urlString];
  if (!url) {
      return NO;
  }
  if (!url.scheme || !url.host) {
      return NO;
  }
  return YES;
}

+ (BOOL)urlStringHasHTTPorHTTPS:(NSString* _Nonnull)urlString {
  NSURL *url = [NSURL URLWithString:urlString];
  NSString *scheme = [url scheme];

  if (scheme == nil) {
      return NO;
  }

  NSString *lowercaseScheme = [scheme lowercaseString];
  return [lowercaseScheme isEqualToString:@"http"] ||
         [lowercaseScheme isEqualToString:@"https"];
}

+ (NSString* _Nonnull)hostOfURLString:(NSString* _Nonnull)urlString {

  if (![VivaldiGlobalHelpers urlStringHasHTTPorHTTPS:urlString]) {
    urlString = [NSString stringWithFormat:@"http://%@", urlString];
  }

  NSURL *url = [NSURL URLWithString:urlString];
  if (!url) {
    return nil;
  }
  NSString *host = url.host;
  return host;
}

+ (NSComparisonResult)compare:(NSString*)first
                       second:(NSString*)second {
  NSComparisonResult result = NSOrderedSame;
  if ([first length] && [second length]) {
    result = [first caseInsensitiveCompare:second];
  } else if ([first length]) {
    result = NSOrderedAscending;
  } else if ([second length]) {
    result = NSOrderedDescending;
  }

  if (result == NSOrderedSame) {
    if ([first length] && [second length]) {
      result = [first caseInsensitiveCompare:second];
    } else if ([first length]) {
      result = NSOrderedAscending;
    } else if ([second length]) {
      result = NSOrderedDescending;
    }
  }
  return result;
}
@end
