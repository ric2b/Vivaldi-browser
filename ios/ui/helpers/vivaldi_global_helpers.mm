// Copyright 2023 Vivaldi Technologies. All rights reserved.

#import "ios/ui/helpers/vivaldi_global_helpers.h"

#import "app/vivaldi_apptools.h"
#import "app/vivaldi_constants.h"
#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "url/gurl.h"

using vivaldi::kVivaldiUIScheme;

@implementation VivaldiGlobalHelpers

+ (BOOL)isVivaldiRunning {
  return vivaldi::IsVivaldiRunning();
}

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

+ (BOOL)isDeviceTablet {
  return [UIDevice currentDevice].userInterfaceIdiom
      == UIUserInterfaceIdiomPad;
}

+ (BOOL)canShowSidePanelForTraitEnvironment:(id<UITraitEnvironment>)trait {
  if (!trait)
    return NO;

  return [self canShowSidePanelForTrait:trait.traitCollection];
}

+ (BOOL)canShowSidePanelForTrait:(UITraitCollection*)trait {
  if (!trait)
    return NO;

  return [self isDeviceTablet] && !IsSplitToolbarMode(trait);
}

+ (UIEdgeInsets)safeAreaInsets {
  if (self.keyWindow) {
    return self.keyWindow.safeAreaInsets;
  }
  return UIEdgeInsetsZero;
}

+ (BOOL)shouldUseDarkTextForColor:(UIColor* _Nonnull)color {
  return [self luminanceForColor:color] >= 0.6;
}

+ (BOOL)shouldUseDarkTextForImage:(UIImage* _Nonnull)image {

  CIImage *inputImage = [[CIImage alloc] initWithImage:image];
  if (!inputImage) return NO;
  CIVector *extentVector = [CIVector vectorWithX:inputImage.extent.origin.x
                                               Y:inputImage.extent.origin.y
                                               Z:inputImage.extent.size.width
                                               W:inputImage.extent.size.height];

  CIFilter *filter = [CIFilter filterWithName:@"CIAreaAverage"
                                keysAndValues:kCIInputImageKey, inputImage,
                      kCIInputExtentKey, extentVector, nil];
  if (!filter) return NO;

  CIImage *outputImage = filter.outputImage;
  if (!outputImage) return NO;

  CIContext *context =
      [CIContext
          contextWithOptions:@{kCIContextWorkingColorSpace: [NSNull null]}];
  UInt8 bitmap[4];
  [context render:outputImage
         toBitmap:&bitmap
         rowBytes:4
           bounds:CGRectMake(0, 0, 1, 1)
           format:kCIFormatRGBA8
       colorSpace:nil];

  UIColor *dominantColor = [UIColor colorWithRed:((CGFloat)bitmap[0]/255.0)
                                           green:((CGFloat)bitmap[1]/255.0)
                                            blue:((CGFloat)bitmap[2]/255.0)
                                           alpha:((CGFloat)bitmap[3]/255.0)];

  return [self shouldUseDarkTextForColor:dominantColor];
}

+ (BOOL)shouldUseDefaultThemeColor:(UIColor* _Nonnull)color {
  // Convert UIColor to CIColor
  CIColor *ciColor = [[CIColor alloc] initWithColor:color];
  if (ciColor.alpha == 0)
    return YES;

  return [self luminanceForColor:color] >= 0.95;
}

+ (CGFloat)luminanceForColor:(UIColor* _Nonnull)color {
  // Convert UIColor to CIColor
  CIColor *ciColor = [[CIColor alloc] initWithColor:color];
  // Calculate the luminance
  // Equation: Y = 0.2126 * R + 0.7152 * G + 0.0722 * B
  CGFloat luminance =
      0.2126 * ciColor.red + 0.7152 * ciColor.green + 0.0722 * ciColor.blue;
  return luminance;
}

+ (BOOL)isFinalReleaseBuild {
  NSString* scheme =
      base::apple::ObjCCast<NSString>([base::apple::FrameworkBundle()
          objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  if (!scheme)
    return NO;
  return [scheme isEqualToString:@"vivaldi-final"];
}

+ (BOOL)isValidDomain:(NSString* _Nonnull)urlString {
  NSString *domainPattern =
      @"^(https?://)?([\\da-z.-]+)\\.([a-z.]{2,6})([/\\w .-]*)*/?$";
  NSError *error = nil;

  // Check for valid domain
  NSRegularExpression *domainRegex =
      [NSRegularExpression
         regularExpressionWithPattern:domainPattern
                              options:NSRegularExpressionCaseInsensitive
                                error:&error];
  NSTextCheckingResult *domainMatch =
      [domainRegex firstMatchInString:urlString
                              options:0
                                range:NSMakeRange(0, urlString.length)];
  // If its a valid domain return early.
  if (domainMatch != nil)
    return YES;

  // Check for valid IP or Localhost
  NSString *ipOrLocalhostPattern =
      @"^(https?://)?((\\d{1,3}\\.){3}\\d{1,3}|localhost)([/\\w .-]*)*/?$";
  NSRegularExpression *ipOrLocalhostRegex =
      [NSRegularExpression
         regularExpressionWithPattern:ipOrLocalhostPattern
                              options:NSRegularExpressionCaseInsensitive
                                error:&error];
  NSTextCheckingResult *ipOrLocalhostMatch =
      [ipOrLocalhostRegex firstMatchInString:urlString
                                     options:0
                                       range:NSMakeRange(0, urlString.length)];

  return ipOrLocalhostMatch != nil;
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

  NSString* replaceRange = @"www.";
  if (([host length] >= [replaceRange length]) &&
      ([[host substringToIndex: [replaceRange length]]
        isEqualToString:replaceRange]))
    return [host substringFromIndex: [replaceRange length]];
  else
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

+ (NSComparisonResult)compare:(BOOL)first
                       second:(BOOL)second
                 foldersFirst:(BOOL)foldersFirst {
  if (first && !second) {
    return foldersFirst ? NSOrderedAscending : NSOrderedDescending;
  } else if (!first && second) {
    return foldersFirst ? NSOrderedDescending : NSOrderedAscending;
  } else {
    return NSOrderedSame;
  }
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

+ (UIColor* _Nonnull)colorWithHexString:(NSString* _Nonnull)hexString {
  NSString *colorString =
      [[hexString stringByReplacingOccurrencesOfString: @"#"
                                            withString: @""] uppercaseString];

  CGFloat alpha, red, blue, green;
  switch ([colorString length]) {
    case 3: // #RGB
      alpha = 1.0f;
      red   = [self colorComponentFrom: colorString start: 0 length: 1];
      green = [self colorComponentFrom: colorString start: 1 length: 1];
      blue  = [self colorComponentFrom: colorString start: 2 length: 1];
      break;
    case 4: // #ARGB
      alpha = [self colorComponentFrom: colorString start: 0 length: 1];
      red   = [self colorComponentFrom: colorString start: 1 length: 1];
      green = [self colorComponentFrom: colorString start: 2 length: 1];
      blue  = [self colorComponentFrom: colorString start: 3 length: 1];
      break;
    case 6: // #RRGGBB
      alpha = 1.0f;
      red   = [self colorComponentFrom: colorString start: 0 length: 2];
      green = [self colorComponentFrom: colorString start: 2 length: 2];
      blue  = [self colorComponentFrom: colorString start: 4 length: 2];
      break;
    case 8: // #AARRGGBB
      alpha = [self colorComponentFrom: colorString start: 0 length: 2];
      red   = [self colorComponentFrom: colorString start: 2 length: 2];
      green = [self colorComponentFrom: colorString start: 4 length: 2];
      blue  = [self colorComponentFrom: colorString start: 6 length: 2];
      break;
    default:
      alpha = 1.0;
      red   = 1.0;
      green = 1.0;
      blue  = 1.0;
      break;
  }
  return [UIColor colorWithRed: red green: green blue: blue alpha: alpha];
}

+ (CGFloat)colorComponentFrom:(NSString* _Nonnull)string
                        start:(NSUInteger)start
                       length:(NSUInteger)length {
  NSString *substring = [string substringWithRange: NSMakeRange(start, length)];
  NSString *fullHex = length == 2 ? substring :
      [NSString stringWithFormat: @"%@%@", substring, substring];

  unsigned hexComponent;
  [[NSScanner scannerWithString: fullHex] scanHexInt: &hexComponent];
  return hexComponent / 255.0;
}

+ (NSString* _Nonnull)formattedURLStringForChromeScheme:
  (NSString* _Nonnull)urlText {
  NSURLComponents* locationUrlComponents =
      [NSURLComponents componentsWithString:urlText];
  NSString* chromeSchemeString =
      [NSString stringWithUTF8String:kChromeUIScheme];
  NSString* vivaldiSchemeString =
      [NSString stringWithUTF8String:kVivaldiUIScheme];
  if ([[locationUrlComponents scheme] isEqualToString:chromeSchemeString]) {
    [locationUrlComponents setScheme:vivaldiSchemeString];
    return [[locationUrlComponents URL] absoluteString];
  }
  return urlText;
}

+ (BOOL)isURLInternalPage:(NSString* _Nonnull)urlString {
  NSString* prefixStringVivaldi = @"vivaldi://";
  NSString* prefixStringChrome = @"chrome://";
  NSString* url = [urlString lowercaseString];

  BOOL isInternal = NO;

  if (url.length > 0) {
    if ([url containsString:prefixStringVivaldi] ||
        [url containsString:prefixStringChrome])
      isInternal = YES;
  } else {
    return NO;
  }

  return isInternal;
}

+ (NSAttributedString*)attributedStringWithText:(NSString*)text
                                      highlight:(NSString*)highlight
                                      textColor:(UIColor*)textColor
                                 highlightColor:(UIColor*)highlightColor {
  NSMutableAttributedString *attributedString =
      [[NSMutableAttributedString alloc] initWithString:text];

  // Apply the text color to the entire text
  [attributedString addAttribute:NSForegroundColorAttributeName
                           value:textColor
                           range:NSMakeRange(0, text.length)];

  // Apply the highlight color to the highlight part
  NSRange highlightRange = [text rangeOfString:highlight];
  if (highlightRange.location != NSNotFound) {
    [attributedString addAttribute:NSForegroundColorAttributeName
                             value:highlightColor
                             range:highlightRange];
  }
  return attributedString;
}

@end
