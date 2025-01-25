// Copyright 2023 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_HELPERS_VIVALDI_GLOBAL_HELPERS_H_
#define IOS_UI_HELPERS_VIVALDI_GLOBAL_HELPERS_H_

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>

// Enum for the iPad layout states in multi tasking.
typedef NS_ENUM(NSUInteger, IPadLayoutState) {
  LayoutStateFullScreen = 0,
  LayoutStateHalfScreen = 1,
  LayoutStateOneThirdScreen = 2,
  LayoutStateTwoThirdScreen = 3,
};

/// Contains the helper methods related to window size and device
/// state for iPad multitasking UI, also other common misc ones.
@interface VivaldiGlobalHelpers: NSObject

/// Returns whether vivaldi build is running.
+ (BOOL)isVivaldiRunning;
/// Returns the key window of the application.
+ (nullable UIWindow*)keyWindow;
/// Returns device screen size.
+ (CGRect)screenSize;
/// Returns the active window size.
+ (CGRect)windowSize;
/// Returns whether device has a valid orientation state.
+ (BOOL)isValidOrientation;
/// Returns whether current device is iPad.
+ (BOOL)isDeviceTablet;
/// Returns true if device is iPad and multitasking UI can show
/// side panel. KeyWindow is not reliable since it doesn't return
/// the active window for presented view always, therefore rely on
/// the TraitCollection/TraitEnvironment of the presented view which is
/// reliable and officially recommended API.
+ (BOOL)canShowSidePanelForTraitEnvironment:
      (id<UITraitEnvironment> _Nullable)trait;
+ (BOOL)canShowSidePanelForTrait:
      (UITraitCollection* _Nullable)trait;
/// Safe area insets for the key window.
+ (UIEdgeInsets)safeAreaInsets;
/// Returns boolean whether dark text should be used above a view using the
/// given color for background.
+ (BOOL)shouldUseDarkTextForColor:(UIColor* _Nonnull)color;
/// Returns boolean whether dark text should be used above an image using the
/// dominant color for the image.
+ (BOOL)shouldUseDarkTextForImage:(UIImage* _Nonnull)image;
/// Returns boolean whether tab strip should use default themeColor for
/// the given color. Too bright or dark(near white/black) webState themeColor could
/// conflict with tab selected color which is also white for light scheme.
+ (BOOL)shouldUseDefaultThemeColor:(UIColor* _Nonnull)color;
/// Returns the luminance for given color.
+ (CGFloat)luminanceForColor:(UIColor* _Nonnull)color;

/// Returns whether the build is final release build.
+ (BOOL)isFinalReleaseBuild;

/// Returns true if given string matches a valid domain format.
/*
 Ruturns True for below formats:
 https://www.example.com
 http://example.co.uk
 https://subdomain.example.com/path/file.html
 https://www.example.com/path/
 https://www.example.com/path?param=value
 https://www.example.com/path#fragment

 Returns False for below formats:
 https://www.example (missing top-level domain)
 https://www.example.com/path?param=<script>alert('XSS')</script>
 (potentially dangerous input)
 https://www.example.com/path/file.html#<script>alert('XSS')</script>
 (potentially dangerous input)
 */
+ (BOOL)isValidDomain:(NSString* _Nonnull)urlString;
/// Returns True for valid URL.
+ (BOOL)isValidURL:(NSString* _Nonnull)urlString;
/// Returns true if give string has http/s scheme.
+ (BOOL)urlStringHasHTTPorHTTPS:(NSString* _Nonnull)urlString;
/// Returns the host of a given string. Strip the `www.` part
/// if exists. So, `https://www.vivaldi.com` or `www.vivaldi.com`
/// will return `vivaldi.com`
+ (NSString* _Nonnull)hostOfURLString:(NSString* _Nonnull)urlString;
/// Returns alphabetically  sorted result from two provided NSString keys.
+ (NSComparisonResult)compare:(NSString* _Nonnull)first
                       second:(NSString* _Nonnull)second;
/// Returns sorted result from two provided BOOL keys
/// and whether folders or items move to the top.
/// Used for cases like sort by kind for Speed Dial,
/// Bookmarks, or Notes where two kinds of item such as folder and non-folder
/// is present.
/// Example: first, second BOOL value indicates whether isFolder is YES.
/// foldersFirst BOOL decides whether folders will move to top or bottom.
+ (NSComparisonResult)compare:(BOOL)first
                       second:(BOOL)second
                 foldersFirst:(BOOL)foldersFirst;
/// Returns True if URL A and URL B has same host.
+ (BOOL)areHostsEquivalentForURL:(NSURL* _Nonnull)aURL
                            bURL:(NSURL* _Nonnull)bURL;
/// Returns UIColor from provided HEX code for color
+ (UIColor* _Nonnull)colorWithHexString:(NSString* _Nonnull)hexString;
+ (CGFloat)colorComponentFrom:(NSString* _Nonnull)string
                        start:(NSUInteger)start
                       length:(NSUInteger)length;
+ (NSString* _Nonnull)formattedURLStringForChromeScheme:
  (NSString* _Nonnull)urlText;
/// Returns whether the given URL string is an internal
/// page.
+ (BOOL)isURLInternalPage:(NSString* _Nonnull)urlString;
/// Returns an attributed string highlighting a certain
/// part of the string wih different color.
+ (NSAttributedString* _Nonnull)
    attributedStringWithText:(NSString* _Nonnull)text
         highlight:(NSString* _Nonnull)highlight
               textColor:(UIColor* _Nonnull)textColor
                    highlightColor:(UIColor* _Nonnull)highlightColor;
@end

#endif  // IOS_UI_HELPERS_VIVALDI_GLOBAL_HELPERS_H_
