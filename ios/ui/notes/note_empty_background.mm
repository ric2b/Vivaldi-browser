// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#import "ios/ui/notes/note_empty_background.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/ui/notes/note_ui_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The spacing between the image and text label.
const CGFloat kStackViewSpacing = 24.0;

// The margin between the stack view and the edges of its superview.
const CGFloat kStackViewMargin = 24.0;

}  // namespace

@interface NoteEmptyBackground ()

// Stack view containing UI that should be centered in the page.
@property(nonatomic, strong) UIStackView* stackView;

// Text label showing the description text.
@property(nonatomic, retain) UILabel* textLabel;

@end

@implementation NoteEmptyBackground

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    // The "star" image.
    UIImage* image = [[UIImage imageNamed:@"note_empty_star"]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
    imageView.tintColor = [UIColor colorNamed:kPlaceholderImageTintColor];

    // The explanatory text label.
    self.textLabel = [[UILabel alloc] init];
    self.textLabel.backgroundColor = UIColor.clearColor;
    self.textLabel.accessibilityIdentifier =
        kNoteEmptyStateExplanatoryLabelIdentifier;
    self.textLabel.textAlignment = NSTextAlignmentCenter;
    self.textLabel.numberOfLines = 0;
    self.textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    self.textLabel.adjustsFontForContentSizeCategory = YES;
    self.textLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];

    // Vertical stack view that centers its contents.
    _stackView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ imageView, self.textLabel ]];
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.axis = UILayoutConstraintAxisVertical;
    _stackView.alignment = UIStackViewAlignmentCenter;
    _stackView.distribution = UIStackViewDistributionEqualSpacing;
    _stackView.spacing = kStackViewSpacing;
    [self addSubview:_stackView];

    // Center the stack view, with a minimum margin around the leading and
    // trailing edges.
    [NSLayoutConstraint activateConstraints:@[
      [self.centerXAnchor constraintEqualToAnchor:_stackView.centerXAnchor],
      [self.centerYAnchor constraintEqualToAnchor:_stackView.centerYAnchor],
      [self.leadingAnchor
          constraintLessThanOrEqualToAnchor:_stackView.leadingAnchor
                                   constant:-kStackViewMargin],
      [self.trailingAnchor
          constraintGreaterThanOrEqualToAnchor:_stackView.trailingAnchor
                                      constant:kStackViewMargin],

    ]];
  }
  return self;
}

- (NSString*)text {
  return self.textLabel.text;
}

- (void)setText:(NSString*)text {
  self.textLabel.text = text;
  [self setNeedsLayout];
}

@end
