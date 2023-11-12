// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/helpers/vivaldi_global_helpers.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/url_formatter/url_fixer.h"
#import "url/gurl.h"

@implementation VivaldiGlobalHelpers

+ (nullable UIWindow*)keyWindow {
  NSSet<UIScene*>* windowScenes =
      [UIApplication sharedApplication].connectedScenes;
  for (UIScene* scene : windowScenes) {
    if ([scene.delegate
            conformsToProtocol:@protocol(UIWindowSceneDelegate)]) {
      return [(id<UIWindowSceneDelegate>)scene.delegate window];
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
       (self.isHorizontalTraitRegular &&
        self.iPadLayoutState == LayoutStateTwoThirdScreen));
}

+ (BOOL)isFinalReleaseBuild {
  NSString* scheme =
      base::apple::ObjCCast<NSString>([base::apple::FrameworkBundle()
          objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  if (!scheme)
    return NO;
  return [scheme isEqualToString:@"vivaldi_final"];
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

+ (BOOL)areHostsEquivalentForURL:(NSURL* _Nonnull)aURL
                            bURL:(NSURL* _Nonnull)bURL {
  // Define a constant for the minimum number of components in a domain.
  // Domains should at least have a top-level domain and a second-level domain,
  // e.g. "apple.com".
  static NSInteger const kMinDomainComponents = 2;

  // Extract host strings from the input URLs
  NSString *aHost = aURL.host;
  NSString *bHost = bURL.host;

  // If either host string is nil, return NO since there's nothing to compare
  if (!aHost || !bHost) return NO;

  // Split host strings into components separated by "."
  NSArray *aHostComponents = [aHost componentsSeparatedByString:@"."];
  NSArray *bHostComponents = [bHost componentsSeparatedByString:@"."];

  // If either host string does not have the minimum number of domain components,
  // perform a simple string comparison and return the result.
  if (aHostComponents.count < kMinDomainComponents ||
      bHostComponents.count < kMinDomainComponents)
    return [aHost isEqualToString:bHost];

  // Construct domain strings from the last two components of the host strings,
  // and compare them.
  NSString *aDomain =
    [NSString stringWithFormat:@"%@.%@",
        aHostComponents[aHostComponents.count - kMinDomainComponents],
          aHostComponents.lastObject];
  NSString *bDomain =
    [NSString stringWithFormat:@"%@.%@",
        bHostComponents[bHostComponents.count - kMinDomainComponents],
          bHostComponents.lastObject];

  // Return YES if the domain strings are equal, NO otherwise.
  return [aDomain isEqualToString:bDomain];
}

@end
