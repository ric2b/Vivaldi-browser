
// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/table_view/cells/vivaldi_table_view_text_spinner_button_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"

@implementation VivaldiTableViewTextSpinnerButtonItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [VivaldiTableViewTextSpinnerButtonCell class];
    return self;
  }
  return nil;
}

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  VivaldiTableViewTextSpinnerButtonCell* cell =
      base::apple::ObjCCastStrict<VivaldiTableViewTextSpinnerButtonCell>
                                                                  (tableCell);
  [cell setUpViewWithActivityIndicator];
}

@end

@interface VivaldiTableViewTextSpinnerButtonCell ()
@property(nonatomic, weak) UIActivityIndicatorView* activityIndicator;
@end

@implementation VivaldiTableViewTextSpinnerButtonCell
#pragma mark - Public Methods
- (void)setUpViewWithActivityIndicator {
  UIActivityIndicatorView *activityIndicator =
      [[UIActivityIndicatorView alloc]
          initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  _activityIndicator = activityIndicator;
  activityIndicator.color = UIColor.whiteColor;
  activityIndicator.hidesWhenStopped = YES;
  [self.contentView addSubview:activityIndicator];
  [activityIndicator centerInSuperview];
}

- (void)setActivityIndicatorEnabled:(BOOL)enable {
  [UIView animateWithDuration:0.2
                   animations:^{
    self.button.alpha = enable ? 0 : 1;
    if (enable) {
      [self.activityIndicator startAnimating];
    } else {
      [self.activityIndicator stopAnimating];
    }
  } completion:nil];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.activityIndicator stopAnimating];
  self.button.titleLabel.alpha = 1;
}

@end
