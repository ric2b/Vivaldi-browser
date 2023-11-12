// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/top_menu/vivaldi_ntp_top_menu_cell.h"

#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"

namespace {
// Padding for title label. In order - Top, Left, Bottom, Right
const UIEdgeInsets titleLabelPadding =
    UIEdgeInsetsMake(4.f, 0.f, 0.f, 4.f);
// Padding for selection indicator. In order - Top, Left, Bottom, Right
const UIEdgeInsets selectionIndicatorPadding =
    UIEdgeInsetsMake(6.f, 0.f, 0.f, 16.f);
// Size for the selection indicator
// Width calculated by constraint, height is 2.f
const CGSize selectionIndicatorSize = CGSizeMake(0, 2.f);
}

@interface VivaldiNTPTopMenuCell() {}
// Title label for the menu item.
@property(nonatomic, weak) UILabel* titleLabel;
// Item selection indicator below the title label.
@property(nonatomic, weak) UIView* selectionIndicator;
@end

@implementation VivaldiNTPTopMenuCell

@synthesize titleLabel = _titleLabel;
@synthesize selectionIndicator = _selectionIndicator;

#pragma mark - INITIALIZER
- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    [self setUpUI];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.titleLabel.text = nil;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  // Container to hold the items
  UIView *container = [UIView new];
  container.backgroundColor = UIColor.clearColor;
  [self addSubview:container];
  [container fillSuperview];

  // Title label set up
  UILabel* label = [[UILabel alloc] init];
  _titleLabel = label;
  label.textColor = [UIColor colorNamed:vNTPToolbarTextColor];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.numberOfLines = 1;
  label.textAlignment = NSTextAlignmentLeft;

  [container addSubview:_titleLabel];
  [_titleLabel anchorTop:container.topAnchor
                 leading:container.leadingAnchor
                  bottom: nil
                trailing: container.trailingAnchor
                 padding: titleLabelPadding];

  // Selection indicator view
  UIView *selectionIndicatorView = [UIView new];
  _selectionIndicator = selectionIndicatorView;
  selectionIndicatorView.backgroundColor =
  [UIColor colorNamed: vNTPToolbarSelectionLineColor];

  [container addSubview:_selectionIndicator];
  [_selectionIndicator anchorTop:_titleLabel.bottomAnchor
                         leading: container.leadingAnchor
                          bottom: container.bottomAnchor
                        trailing: container.trailingAnchor
                         padding: selectionIndicatorPadding
                            size: selectionIndicatorSize];
  // Hide the view initially and toggle it only when the item is selected
  selectionIndicatorView.alpha = 0;
}

- (void)setSelected:(BOOL)selected {
    [super setSelected:selected];
  [self toggleHighlight];
}

- (void)toggleHighlight {
  [UIView animateWithDuration:0.25 animations:^{
    self.selectionIndicator.alpha = self.isSelected ? 1 : 0;
  }];

  [UIView transitionWithView: self.titleLabel
                    duration: 0.25
                     options: UIViewAnimationOptionTransitionCrossDissolve
                  animations:^{
    self.titleLabel.textColor =
      self.isSelected ? [UIColor colorNamed: vNTPToolbarTextHighlightedColor]
                      : [UIColor colorNamed: vNTPToolbarTextColor];}
                  completion: nil];
}

#pragma mark - SETTERS
- (void)configureCellWithItem:(VivaldiSpeedDialItem*)item {
  self.titleLabel.text = item.title;
}


@end
