// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/panel/panel_button_cell.h"

#import "ios/chrome/grit/ios_strings.h"
#import "ios/panel/panel_constants.h"
#import "ios/panel/panel_interaction_controller.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using l10n_util::GetNSString;

@interface PanelButtonCell() {}
@property(nonatomic, weak) UIImageView* imageView;
@property(nonatomic, strong)
    NSDictionary<NSNumber*, NSDictionary*> *assetConfiguration;
@end

@implementation PanelButtonCell

@synthesize imageView = _imageView;

#pragma mark - INITIALIZER
- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setUpUI];
    self.assetConfiguration = [self getAssetConfiguration];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.imageView.image = nil;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {
  // Container to hold the items
  UIView *container = [UIView new];
  [self addSubview:container];
  [container fillSuperview];

  // Icon set up
  UIImageView* imgView = [[UIImageView alloc] init];
  _imageView = imgView;
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.clipsToBounds = YES;

  [container addSubview:_imageView];
  [_imageView fillSuperview];

  self.isAccessibilityElement = YES;
  self.accessibilityTraits |= UIAccessibilityTraitButton;
}

#pragma mark - PRIVATE

/// Returns the computed asset for each item for selected and not-selected state
/// and accessibility string.
- (NSDictionary<NSNumber*, NSDictionary*>*)getAssetConfiguration {
  return @{
    @(PanelPage::BookmarksPage): @{
      @"normal": vPanelBookmarks,
      @"highlighted": vPanelBookmarksActive,
      @"accessibilityLabel": GetNSString(IDS_IOS_TOOLS_MENU_BOOKMARKS)
    },
    @(PanelPage::ReadinglistPage): @{
      @"normal": vPanelReadingList,
      @"highlighted": vPanelReadingListActive,
      @"accessibilityLabel": GetNSString(IDS_IOS_TOOLS_MENU_READING_LIST)
    },
    @(PanelPage::NotesPage): @{
      @"normal": vPanelNotes,
      @"highlighted": vPanelNotesActive,
      @"accessibilityLabel": GetNSString(IDS_VIVALDI_TOOLS_MENU_NOTES)
    },
    @(PanelPage::HistoryPage): @{
      @"normal": vPanelHistory,
      @"highlighted": vPanelHistoryActive,
      @"accessibilityLabel": GetNSString(IDS_IOS_TOOLS_MENU_HISTORY)
    },
    @(PanelPage::TranslatePage): @{
      @"normal": vPanelTranslate,
      @"highlighted": vPanelTranslateActive,
      @"accessibilityLabel": GetNSString(IDS_VIVALDI_TRANSLATE_TITLE)
    }
  };
}

#pragma mark - SETTERS
- (void)configureCellWithIndex:(NSInteger)index
                   highlighted:(BOOL)highlighted {
  NSDictionary *config = self.assetConfiguration[@(index)];
  NSString *imageName = highlighted ?
      config[@"highlighted"] : config[@"normal"];
  self.imageView.image = [UIImage imageNamed:imageName];
  self.accessibilityLabel = config[@"accessibilityLabel"];
}

@end
