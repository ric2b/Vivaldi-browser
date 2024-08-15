// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_new_tab_button.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/context_menu/vivaldi_context_menu_constants.h"

using vivaldi::IsVivaldiRunning;
// End Vivaldi
namespace {

// The size of the small symbol image.
const CGFloat kSmallSymbolSize = 24;
// The size of the large symbol image.
const CGFloat kLargeSymbolSize = 37;

}  // namespace

@interface TabGridNewTabButton ()

@property(nonatomic, strong) UIImage* symbol;

@end

@implementation TabGridNewTabButton

- (instancetype)initWithLargeSize:(BOOL)largeSize {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    CGFloat symbolSize = largeSize ? kLargeSymbolSize : kSmallSymbolSize;

    if (IsVivaldiRunning()) {
      _symbol = [UIImage imageNamed:vMenuNewTab];
      self.tintColor = [UIColor labelColor];
    } else {
    _symbol = CustomSymbolWithPointSize(kPlusCircleFillSymbol, symbolSize);
    } // End Vivaldi

    [self setImage:_symbol forState:UIControlStateNormal];
    self.pointerInteractionEnabled = YES;
    self.pointerStyleProvider = CreateLiftEffectCirclePointerStyleProvider();
  }
  return self;
}

#pragma mark - Public

- (void)setPage:(TabGridPage)page {
  if (IsVivaldiRunning()) {
    [self setIconPage:page];
  } else {
  [self setSymbolPage:page];
  } // End Vivaldi
}

#pragma mark - Private

// Sets page using a symbol image.
- (void)setSymbolPage:(TabGridPage)page {
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      [self
          setImage:SymbolWithPalette(
                       self.symbol, @[ UIColor.blackColor, UIColor.whiteColor ])
          forState:UIControlStateNormal];
      break;
    case TabGridPageRegularTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      [self
          setImage:SymbolWithPalette(self.symbol,
                                     @[
                                       UIColor.blackColor,
                                       [UIColor colorNamed:kStaticBlue400Color]
                                     ])
          forState:UIControlStateNormal];
      break;
    case TabGridPageRemoteTabs:
    case TabGridPageTabGroups:

      // Vivaldi
    case TabGridPageClosedTabs:
      // End Vivaldi

      break;
  }
  _page = page;
}

#pragma mark - Vivaldi
// Sets page using icon images.
- (void)setIconPage:(TabGridPage)page {
  // self.page is inited to 0 (i.e. TabGridPageIncognito) so do not early return
  // here, otherwise when app is launched in incognito mode the image will be
  // missing.
  switch (page) {
    case TabGridPageIncognitoTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB);
      break;
    case TabGridPageRegularTabs:
      self.accessibilityLabel =
          l10n_util::GetNSString(IDS_IOS_TAB_GRID_CREATE_NEW_TAB);
      break;
    case TabGridPageRemoteTabs:
    case TabGridPageClosedTabs:
    case TabGridPageTabGroups:
      break;
  }
  _page = page;

  [self setImage:
      [_symbol imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
}

@end
