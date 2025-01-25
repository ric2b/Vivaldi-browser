// Copyright 2022 Vivaldi Technologies. All rights reserved.

#import "ios/ui/ntp/vivaldi_private_mode_view.h"

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_url_loader_delegate.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/ui/common/vivaldi_url_constants.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kStackViewHorizontalMargin = 20.0;
const CGFloat kStackViewMaxWidth = 416.0;
const CGFloat kStackViewDefaultSpacing = 20.0;
const CGFloat kStackViewImageSpacing = 22.0;
const CGFloat kLayoutGuideVerticalMargin = 8.0;
const CGFloat kLayoutGuideMinHeight = 12.0;

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for the title of the incognito page.
UIFont* TitleFont() {
  return [[UIFontMetrics defaultMetrics]
      scaledFontForFont:[UIFont boldSystemFontOfSize:26.0]];
}

// Returns the color to use for body text.
UIColor* BodyTextColor() {
  return [UIColor colorNamed:kTextSecondaryColor];
}

// Returns a font, scaled to the current dynamic type settings, that is suitable
// for the body text of the incognito page.
UIFont* BodyFont() {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
}

GURL ConvertUserDataToGURL(NSString* urlString) {
  if (urlString) {
    return url_formatter::FixupURL(base::SysNSStringToUTF8(urlString),
                                   std::string());
  } else {
    return GURL();
  }
}

}  // namespace

@interface VivaldiPrivateModeView () <URLDropDelegate>

@end

@implementation VivaldiPrivateModeView {
  UIView* _containerView;
  UIStackView* _stackView;

  // Layout Guide whose height is the height of the bottom unsafe area.
  UILayoutGuide* _bottomUnsafeAreaGuide;
  UILayoutGuide* _bottomUnsafeAreaGuideInSuperview;

  // Height constraint for adding margins for the bottom toolbar.
  NSLayoutConstraint* _bottomToolbarMarginHeight;

  // Constraint ensuring that `containerView` is at least as high as the
  // superview of the IncognitoNTPView, i.e. the Incognito panel.
  // This ensures that if the Incognito panel is higher than a compact
  // `containerView`, the `containerView`'s `topGuide` and `bottomGuide` are
  // forced to expand, centering the views in between them.
  NSArray<NSLayoutConstraint*>* _superViewConstraints;

  // Handles drop interactions for this view.
  URLDragDropHandler* _dragDropHandler;
}

- (instancetype)initWithFrame:(CGRect)frame {
  return [self initWithFrame:frame
      showTopIncognitoImageAndTitle:YES];
}

- (instancetype)initWithFrame:(CGRect)frame
    showTopIncognitoImageAndTitle:(BOOL)showTopIncognitoImageAndTitle {
  self = [super initWithFrame:frame];
  if (self) {

    self.backgroundColor = UIColor.clearColor;

    _dragDropHandler = [[URLDragDropHandler alloc] init];
    _dragDropHandler.dropDelegate = self;
    [self addInteraction:[[UIDropInteraction alloc]
                             initWithDelegate:_dragDropHandler]];

    self.alwaysBounceVertical = YES;
    // The bottom safe area is taken care of with the bottomUnsafeArea guides.
    self.contentInsetAdjustmentBehavior =
        UIScrollViewContentInsetAdjustmentNever;

    // Container to hold and vertically position the stack view.
    _containerView = [[UIView alloc] initWithFrame:frame];
    _containerView.backgroundColor = UIColor.clearColor;
    [_containerView setTranslatesAutoresizingMaskIntoConstraints:NO];

    // Stackview in which all the subviews (image, labels, button) are added.
    _stackView = [[UIStackView alloc] init];
    [_stackView setTranslatesAutoresizingMaskIntoConstraints:NO];
    _stackView.axis = UILayoutConstraintAxisVertical;
    _stackView.spacing = kStackViewDefaultSpacing;
    _stackView.distribution = UIStackViewDistributionFill;
    _stackView.alignment = UIStackViewAlignmentCenter;
    [_containerView addSubview:_stackView];

    if (showTopIncognitoImageAndTitle) {
      UIImage* incognitoImage = [UIImage imageNamed:vNTPPrivateTabGhost];
      UIImageView* incognitoImageView =
          [[UIImageView alloc] initWithImage:incognitoImage];
      incognitoImageView.tintColor = [UIColor colorNamed:kTextPrimaryColor];
      [_stackView addArrangedSubview:incognitoImageView];
      [_stackView setCustomSpacing:kStackViewImageSpacing
                         afterView:incognitoImageView];
    }

    [self addTextSectionsWithTitleShown:showTopIncognitoImageAndTitle];

    // `topGuide` and `bottomGuide` exist to vertically position the stackview
    // inside the container scrollview.
    UILayoutGuide* topGuide = [[UILayoutGuide alloc] init];
    UILayoutGuide* bottomGuide = [[UILayoutGuide alloc] init];
    _bottomUnsafeAreaGuide = [[UILayoutGuide alloc] init];
    [_containerView addLayoutGuide:topGuide];
    [_containerView addLayoutGuide:bottomGuide];
    [_containerView addLayoutGuide:_bottomUnsafeAreaGuide];

    // This layout guide is used to prevent the content from being displayed
    // below the bottom toolbar.
    UILayoutGuide* bottomToolbarMarginGuide = [[UILayoutGuide alloc] init];
    [_containerView addLayoutGuide:bottomToolbarMarginGuide];

    _bottomToolbarMarginHeight =
        [bottomToolbarMarginGuide.heightAnchor constraintEqualToConstant:0];
    // Updates the constraints to the correct value.
    [self updateToolbarMargins];

    [self addSubview:_containerView];

    [NSLayoutConstraint activateConstraints:@[
      // Position the stack view's top at some margin under from the container
      // top.
      [topGuide.topAnchor constraintEqualToAnchor:_containerView.topAnchor],
      [_stackView.topAnchor constraintEqualToAnchor:topGuide.bottomAnchor
                                           constant:kLayoutGuideVerticalMargin],

      // Position the stack view's bottom guide at some margin from the
      // container bottom.
      [bottomGuide.topAnchor
          constraintEqualToAnchor:bottomToolbarMarginGuide.bottomAnchor
                         constant:kLayoutGuideVerticalMargin],
      [_containerView.bottomAnchor
          constraintEqualToAnchor:bottomGuide.bottomAnchor],

      // Position the stack view above the bottom toolbar margin guide.
      [bottomToolbarMarginGuide.topAnchor
          constraintEqualToAnchor:_stackView.bottomAnchor],

      // Center the stackview horizontally with a minimum margin.
      [_stackView.leadingAnchor
          constraintGreaterThanOrEqualToAnchor:_containerView.leadingAnchor
                                      constant:kStackViewHorizontalMargin],
      [_stackView.trailingAnchor
          constraintLessThanOrEqualToAnchor:_containerView.trailingAnchor
                                   constant:-kStackViewHorizontalMargin],
      [_stackView.centerXAnchor
          constraintEqualToAnchor:_containerView.centerXAnchor],

      // Constraint the _bottomUnsafeAreaGuide to the stack view and the
      // container view. Its height is set in the -didMoveToSuperview to take
      // into account the unsafe area.
      [_bottomUnsafeAreaGuide.topAnchor
          constraintEqualToAnchor:_stackView.bottomAnchor
                         constant:2 * kLayoutGuideMinHeight +
                                  kLayoutGuideVerticalMargin],
      [_bottomUnsafeAreaGuide.bottomAnchor
          constraintEqualToAnchor:_containerView.bottomAnchor],

      // Ensure that the stackview width is constrained.
      [_stackView.widthAnchor
          constraintLessThanOrEqualToConstant:kStackViewMaxWidth],

      // Activate the height constraints.
      _bottomToolbarMarginHeight,

      // Set a minimum top margin and make the bottom guide twice as tall as the
      // top guide.
      [topGuide.heightAnchor
          constraintGreaterThanOrEqualToConstant:kLayoutGuideMinHeight],
      [bottomGuide.heightAnchor constraintEqualToAnchor:topGuide.heightAnchor
                                             multiplier:2.0],
    ]];

    // Constraints comunicating the size of the contentView to the scrollview.
    // See UIScrollView autolayout information at
    // https://developer.apple.com/library/ios/releasenotes/General/RN-iOSSDK-6_0/index.html
    NSDictionary* viewsDictionary = @{@"containerView" : _containerView};
    NSArray* constraints = @[
      @"V:|-0-[containerView]-0-|",
      @"H:|-0-[containerView]-0-|",
    ];
    ApplyVisualConstraints(constraints, viewsDictionary);
  }
  return self;
}

#pragma mark - UIView overrides

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (!self.superview) {
    return;
  }

  id<LayoutGuideProvider> safeAreaGuide = self.superview.safeAreaLayoutGuide;
  _bottomUnsafeAreaGuideInSuperview = [[UILayoutGuide alloc] init];
  [self.superview addLayoutGuide:_bottomUnsafeAreaGuideInSuperview];

  _superViewConstraints = @[
    [safeAreaGuide.bottomAnchor
        constraintEqualToAnchor:_bottomUnsafeAreaGuideInSuperview.topAnchor],
    [self.superview.bottomAnchor
        constraintEqualToAnchor:_bottomUnsafeAreaGuideInSuperview.bottomAnchor],
    [_bottomUnsafeAreaGuide.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_bottomUnsafeAreaGuideInSuperview
                                                 .heightAnchor],
    [_containerView.widthAnchor
        constraintEqualToAnchor:self.superview.widthAnchor],
    [_containerView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.superview.heightAnchor],
  ];

  [NSLayoutConstraint activateConstraints:_superViewConstraints];
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [NSLayoutConstraint deactivateConstraints:_superViewConstraints];
  [self.superview removeLayoutGuide:_bottomUnsafeAreaGuideInSuperview];
  [super willMoveToSuperview:newSuperview];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateToolbarMargins];
}

- (void)safeAreaInsetsDidChange {
  [super safeAreaInsetsDidChange];
  [self updateToolbarMargins];
}

#pragma mark - URLDropDelegate

- (BOOL)canHandleURLDropInView:(UIView*)view {
  return YES;
}

- (void)view:(UIView*)view didDropURL:(const GURL&)URL atPoint:(CGPoint)point {
  [self.URLLoaderDelegate loadURLInTab:URL];
}

#pragma mark - Private

// Updates the height of the margins for the top and bottom toolbars.
- (void)updateToolbarMargins {
  if (IsSplitToolbarMode(self)) {
    _bottomToolbarMarginHeight.constant = kSecondaryToolbarWithoutOmniboxHeight;
  } else {
    _bottomToolbarMarginHeight.constant = 0;
  }
}

// Triggers a navigation to the help page.
- (void)learnMoreButtonPressed {
  [self.URLLoaderDelegate
    loadURLInTab:ConvertUserDataToGURL(vVivaldiPrivacyAndSecurity)];
}

// Adds views containing the text of the incognito page to `_stackView`.
- (void)addTextSectionsWithTitleShown:(BOOL)showTitle {
  UIColor* bodyTextColor = BodyTextColor();
  UIColor* linkTextColor = [UIColor colorNamed:kBlueColor];

  // Title.
  if (showTitle) {
    UIColor* titleTextColor = [UIColor colorNamed:kTextPrimaryColor];
    UILabel* titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    titleLabel.font = TitleFont();
    titleLabel.textColor = titleTextColor;
    titleLabel.numberOfLines = 0;
    titleLabel.textAlignment = NSTextAlignmentCenter;
    titleLabel.text = l10n_util::GetNSString(IDS_IOS_NEW_TAB_PRIVATE_TITLE);
    titleLabel.adjustsFontForContentSizeCategory = YES;
    [_stackView addArrangedSubview:titleLabel];
  }

  // The Subtitle and Learn More link have no vertical spacing between them,
  // so they are embedded in a separate stack view.
  UILabel* subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  subtitleLabel.font = BodyFont();
  subtitleLabel.textColor = bodyTextColor;
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.text =
      l10n_util::GetNSString(IDS_IOS_NEW_TAB_PRIVATE_MESSAGE);
  subtitleLabel.adjustsFontForContentSizeCategory = YES;

  UIButton* learnMoreButton = [UIButton buttonWithType:UIButtonTypeCustom];
  learnMoreButton.accessibilityTraits = UIAccessibilityTraitLink;
  learnMoreButton.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_INCOGNITO_INTERSTITIAL_LEARN_MORE_HINT);
  [learnMoreButton
      setTitle:l10n_util::GetNSString(IDS_IOS_NEW_TAB_PRIVATE_LEARN_MORE)
      forState:UIControlStateNormal];
  [learnMoreButton setTitleColor:linkTextColor forState:UIControlStateNormal];
  learnMoreButton.titleLabel.font = BodyFont();
  learnMoreButton.titleLabel.adjustsFontForContentSizeCategory = YES;
  [learnMoreButton addTarget:self
                      action:@selector(learnMoreButtonPressed)
            forControlEvents:UIControlEventTouchUpInside];
  learnMoreButton.pointerInteractionEnabled = YES;

  UIStackView* subtitleStackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ subtitleLabel, learnMoreButton ]];
  subtitleStackView.axis = UILayoutConstraintAxisVertical;
  subtitleStackView.spacing = 0;
  subtitleStackView.distribution = UIStackViewDistributionFill;
  subtitleStackView.alignment = UIStackViewAlignmentLeading;
  [_stackView addArrangedSubview:subtitleStackView];
}

@end
