// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_empty_state_view.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/vivaldi_tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/vivaldi_tab_grid_empty_state_view_delegate.h"
#import "ios/ui/helpers/vivaldi_colors_helper.h"
#import "ios/ui/helpers/vivaldi_global_helpers.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::IsVivaldiRunning;
using l10n_util::GetNSString;
// End Vivaldi

namespace {
const CGFloat kVerticalMargin = 16.0;
const CGFloat kImageHeight = 150.0;
const CGFloat kImageWidth = 150.0;

// Returns the image to display for the given grid `page`.
UIImage* ImageForPage(TabGridPage page) {

  if (IsVivaldiRunning()) {
    switch (page) {
      case TabGridPageIncognitoTabs:
        return [UIImage imageNamed:vTabGridEmptyStatePrivateTabsImage];
      case TabGridPageRegularTabs:
        return [UIImage imageNamed:vTabGridEmptyStateRegularTabsImage];
      case TabGridPageRemoteTabs:
        return [UIImage imageNamed:vTabGridEmptyStateSyncedTabsImage];
      case TabGridPageClosedTabs:
        return [UIImage imageNamed:vTabGridEmptyStateClosedTabsImage];
      case TabGridPageTabGroups:
        return [[UIImage alloc] init];
    }
  } // End Vivaldi

  switch (page) {
    case TabGridPageIncognitoTabs:
      return [UIImage imageNamed:@"tab_grid_incognito_tabs_empty"];
    case TabGridPageRegularTabs:
      return [UIImage imageNamed:@"tab_grid_regular_tabs_empty"];
    case TabGridPageRemoteTabs:
      // No-op. Empty page.
      break;
    case TabGridPageTabGroups:
      return [UIImage imageNamed:@"tab_grid_tab_groups_empty"];

    // Vivaldi
    case TabGridPageClosedTabs:
      break;
    // End Vivaldi

  }
  return nil;
}

// Returns the title to display for the given grid `page` and `mode`.
NSString* TitleForPageAndMode(TabGridPage page, TabGridMode mode) {
  if (mode == TabGridMode::kSearch) {
    return l10n_util::GetNSString(IDS_IOS_TAB_GRID_SEARCH_RESULTS_EMPTY_TITLE);
  }

  if (IsVivaldiRunning()) {
    switch (page) {
      case TabGridPageIncognitoTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_GRID_INCOGNITO_TABS_EMPTY_TITLE);
      case TabGridPageRegularTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_GRID_REGULAR_TABS_EMPTY_TITLE);
      case TabGridPageRemoteTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_EMPTY_TITLE);
      case TabGridPageClosedTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_SWITCHER_RECENTLY_CLOSED_TABS_EMPTY_TITLE);
      case TabGridPageTabGroups:
        return @"";
    }
  } // End Vivaldi

  switch (page) {
    case TabGridPageIncognitoTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_INCOGNITO_TABS_EMPTY_TITLE);
    case TabGridPageRegularTabs:
      return l10n_util::GetNSString(IDS_IOS_TAB_GRID_REGULAR_TABS_EMPTY_TITLE);
    case TabGridPageRemoteTabs:
      // No-op. Empty page.
      break;
    case TabGridPageTabGroups:
      return l10n_util::GetNSString(IDS_IOS_TAB_GRID_TAB_GROUPS_EMPTY_TITLE);


    // Vivaldi
    // The enum case has to be covered within switch statement
    case TabGridPageClosedTabs:
      break;
    // End Vivaldi

  }

  return nil;
}

// Returns the message to display for the given grid `page` and `mode`.
NSString* BodyTextForPageAndMode(TabGridPage page, TabGridMode mode) {
  if (mode == TabGridMode::kSearch) {
    return nil;
  }

  if (IsVivaldiRunning()) {
    switch (page) {
      case TabGridPageIncognitoTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_GRID_INCOGNITO_TABS_EMPTY_MESSAGE);
      case TabGridPageRegularTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_GRID_REGULAR_TABS_EMPTY_MESSAGE);
      case TabGridPageRemoteTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_EMPTY_MESSAGE);
      case TabGridPageClosedTabs:
        return l10n_util::GetNSString(
            IDS_VIVALDI_TAB_SWITCHER_RECENTLY_CLOSED_TABS_EMPTY_MESSAGE);
      case TabGridPageTabGroups:
        return @"";
    }
  } // End Vivaldi

  switch (page) {
    case TabGridPageIncognitoTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_INCOGNITO_TABS_EMPTY_MESSAGE);
    case TabGridPageRegularTabs:
      return l10n_util::GetNSString(
          IDS_IOS_TAB_GRID_REGULAR_TABS_EMPTY_MESSAGE);
    case TabGridPageRemoteTabs:
      // No-op. Empty page.
      break;
    case TabGridPageTabGroups:
      return l10n_util::GetNSString(IDS_IOS_TAB_GRID_TAB_GROUPS_EMPTY_MESSAGE);

    // Vivaldi
    case TabGridPageClosedTabs:
      break;
    // End Vivaldi

  }

  return nil;
}

// Vivaldi
UIButton* VivaldiLoginButton() {
  UIButton* button = [UIButton new];
  button.backgroundColor =
      [UIColor colorNamed:vTabGridEmptyStateLoginButtonBGColor];
  button.layer.cornerRadius = vRecentTabsEmptyStateLoginButtonCornerRadius;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button setTitle:GetNSString(IDS_VIVALDI_ACCOUNT_LOG_IN)
          forState:UIControlStateNormal];
  [button setTitleColor:UIColor.vSystemBlue
               forState:UIControlStateNormal];
  return button;
}

UIButton* VivaldiRegisterButton() {
  UIButton* button = [UIButton new];
  button.backgroundColor = UIColor.clearColor;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button setTitle:GetNSString(IDS_VIVALDI_ACCOUNT_REGISTER)
          forState:UIControlStateNormal];
  [button setTitleColor:UIColor.vSystemBlue
               forState:UIControlStateNormal];
  return button;
}

UIButton* VivaldiEnableSyncButton() {
  UIButton* button = [UIButton new];
  button.backgroundColor =
      [UIColor colorNamed:vTabGridEmptyStateLoginButtonBGColor];
  button.layer.cornerRadius = vRecentTabsEmptyStateLoginButtonCornerRadius;
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button setTitle:GetNSString(IDS_VIVALDI_ACCOUNT_ENABLE_SYNC)
          forState:UIControlStateNormal];
  [button setTitleColor:UIColor.vSystemBlue
               forState:UIControlStateNormal];
  return button;
}

UIActivityIndicatorView* VivaldiEnableSyncActivityIndicator() {
  UIActivityIndicatorView *activityIndicator =
      [[UIActivityIndicatorView alloc]
          initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleMedium];
  activityIndicator.color = UIColor.vSystemBlue;
  activityIndicator.hidesWhenStopped = YES;
  return activityIndicator;
}

const UIEdgeInsets buttonContentsInsets = UIEdgeInsetsMake(0, 20, 0, 20);
const UIEdgeInsets topLabelPadding =
    UIEdgeInsetsMake(kVerticalMargin, 12, 0, 12);
const UIEdgeInsets bottomLabelPadding =
    UIEdgeInsetsMake(kTabGridEmptyStateVerticalMargin, 12, 0, 12);
const UIEdgeInsets buttonsContainerPadding =
    UIEdgeInsetsMake(kVerticalMargin*2, 0, kVerticalMargin, 0);
const CGFloat buttonContentsPadding = 20;

// End Vivaldi

}  // namespace

@interface TabGridEmptyStateView ()
@property(nonatomic, strong) UIView* container;
@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) NSLayoutConstraint* scrollViewHeight;
@property(nonatomic, weak) UIImageView* imageView;
@property(nonatomic, weak) UILabel* topLabel;
@property(nonatomic, weak) UILabel* bottomLabel;

// Vivaldi
@property(nonatomic, weak) UIView* buttonsContainer;
@property(nonatomic, weak) UIButton* loginButton;
@property(nonatomic, weak) UIButton* registerButton;
@property(nonatomic, weak) UIButton* enableSyncButton;
@property(nonatomic, weak) UIActivityIndicatorView* activityIndicator;

// Constraint for action buttons container visible state
@property(nonatomic,strong) NSLayoutConstraint*
    buttonsContainerVisibleConstraint;
@property(nonatomic,strong) NSLayoutConstraint*
    syncButtonSyncingStateWidthConstraint;

// Constraints for image view
@property (strong, nonatomic) NSLayoutConstraint *imageHeightConstraint;
@property (strong, nonatomic) NSLayoutConstraint *imageWidthConstraint;
// End Vivaldi

@end

@implementation TabGridEmptyStateView
// GridEmptyView properties.
@synthesize scrollViewContentInsets = _scrollViewContentInsets;
@synthesize activePage = _activePage;
@synthesize tabGridMode = _tabGridMode;

- (instancetype)initWithPage:(TabGridPage)page {
  if ((self = [super initWithFrame:CGRectZero])) {
    _activePage = page;
    _tabGridMode = TabGridMode::kNormal;
  }
  return self;
}

#pragma mark - GridEmptyView

- (void)setScrollViewContentInsets:(UIEdgeInsets)scrollViewContentInsets {
  _scrollViewContentInsets = scrollViewContentInsets;
  self.scrollView.contentInset = scrollViewContentInsets;
  self.scrollViewHeight.constant =
      scrollViewContentInsets.top + scrollViewContentInsets.bottom;
}

- (void)setTabGridMode:(TabGridMode)tabGridMode {
  if (_tabGridMode == tabGridMode) {
    return;
  }

  _tabGridMode = tabGridMode;

  self.topLabel.text = TitleForPageAndMode(_activePage, _tabGridMode);
  self.bottomLabel.text = BodyTextForPageAndMode(_activePage, _tabGridMode);
}

- (void)setActivePage:(TabGridPage)activePage {
  if (_activePage == activePage) {
    return;
  }

  _activePage = activePage;

  self.imageView.image = ImageForPage(_activePage);
  self.topLabel.text = TitleForPageAndMode(_activePage, _tabGridMode);
  self.bottomLabel.text = BodyTextForPageAndMode(_activePage, _tabGridMode);
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  if (newSuperview) {
    // The first time this moves to a superview, perform the view setup.
    if (self.subviews.count == 0)
      [self setupViews];

    if (IsVivaldiRunning()) {
      [self.container
        anchorTop:nil
          leading:self.safeAreaLayoutGuide.leadingAnchor
           bottom:nil
         trailing:self.safeAreaLayoutGuide.trailingAnchor
          padding:UIEdgeInsetsMake(0, vTabGridEmptyStateViewContainerPadding,
                                   0, vTabGridEmptyStateViewContainerPadding)];
      [self.container centerXInSuperview];
    } else {
    [self.container.widthAnchor
        constraintLessThanOrEqualToAnchor:self.safeAreaLayoutGuide.widthAnchor]
        .active = YES;
    [self.container.centerXAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.centerXAnchor]
        .active = YES;
    } // End Vivaldi

  }
}

#pragma mark - Private

- (void)setupViews {
  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  self.container = container;

  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  self.scrollView = scrollView;

  UILabel* topLabel = [[UILabel alloc] init];
  _topLabel = topLabel;
  topLabel.translatesAutoresizingMaskIntoConstraints = NO;
  topLabel.text = TitleForPageAndMode(_activePage, _tabGridMode);
  topLabel.textColor = UIColorFromRGB(kTabGridEmptyStateTitleTextColor);
  topLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  topLabel.adjustsFontForContentSizeCategory = YES;
  topLabel.numberOfLines = 0;
  topLabel.textAlignment = NSTextAlignmentCenter;

  UILabel* bottomLabel = [[UILabel alloc] init];
  _bottomLabel = bottomLabel;
  bottomLabel.translatesAutoresizingMaskIntoConstraints = NO;
  bottomLabel.text = BodyTextForPageAndMode(_activePage, _tabGridMode);
  bottomLabel.textColor = UIColorFromRGB(kTabGridEmptyStateBodyTextColor);
  bottomLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  bottomLabel.adjustsFontForContentSizeCategory = YES;
  bottomLabel.numberOfLines = 0;
  bottomLabel.textAlignment = NSTextAlignmentCenter;

  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:ImageForPage(_activePage)];
  _imageView = imageView;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;

  if (IsVivaldiRunning()) {
    topLabel.textColor = [UIColor colorNamed:vTabGridEmptyStateTitleTextColor];
    bottomLabel.textColor =
        [UIColor colorNamed:vTabGridEmptyStateBodyTextColor];
    imageView.contentMode = UIViewContentModeScaleAspectFit;

    // Set up action buttons for recent tabs. These are not visible in any
    // other tab grid states.
    UIButton* loginButton = VivaldiLoginButton();
    _loginButton = loginButton;
    [loginButton addTarget:self
                    action:@selector(handleLoginButtonTap)
          forControlEvents:UIControlEventTouchUpInside];
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets =
        NSDirectionalEdgeInsetsMake(0, buttonContentsPadding,
                                    0, buttonContentsPadding);
    loginButton.configuration = buttonConfiguration;

    UIButton* registerButton = VivaldiRegisterButton();
    _registerButton = registerButton;
    [registerButton addTarget:self
                       action:@selector(handleRegisterButtonTap)
             forControlEvents:UIControlEventTouchUpInside];

    UIButton* enableSyncButton = VivaldiEnableSyncButton();
    _enableSyncButton = enableSyncButton;
    [enableSyncButton addTarget:self
                     action:@selector(handleEnableSyncButtonTap)
                    forControlEvents:UIControlEventTouchUpInside];
    enableSyncButton.configuration = buttonConfiguration;

    UIActivityIndicatorView* activityIndicator =
        VivaldiEnableSyncActivityIndicator();
    _activityIndicator = activityIndicator;
    [enableSyncButton addSubview:activityIndicator];
    [activityIndicator centerInSuperview];

    UIStackView* buttonStackView =
      [[UIStackView alloc] initWithArrangedSubviews:@[
        loginButton, registerButton, enableSyncButton
      ]];

    buttonStackView.distribution = UIStackViewDistributionFill;
    buttonStackView.spacing = kVerticalMargin;
    buttonStackView.axis = UILayoutConstraintAxisHorizontal;

    UIView* buttonsContainer = [[UIView alloc] init];
    buttonsContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _buttonsContainer = buttonsContainer;

    [buttonsContainer addSubview:buttonStackView];
    [buttonStackView fillSuperview];

    [container addSubview:buttonsContainer];
  } // End Vivaldi

  [container addSubview:imageView];

  [container addSubview:topLabel];
  [container addSubview:bottomLabel];
  [scrollView addSubview:container];
  [self addSubview:scrollView];

  self.scrollViewHeight = VerticalConstraintsWithInset(
      container, scrollView,
      self.scrollViewContentInsets.top + self.scrollViewContentInsets.bottom);

  if (IsVivaldiRunning()) {
    // Constraint references
    self.imageHeightConstraint =
        [imageView.heightAnchor constraintEqualToConstant:kImageHeight];
    self.imageWidthConstraint =
        [imageView.widthAnchor constraintEqualToConstant:kImageWidth];

    // Activating constraints
    [NSLayoutConstraint activateConstraints:@[
      [imageView.topAnchor constraintEqualToAnchor:container.topAnchor],
      self.imageWidthConstraint,
      self.imageHeightConstraint,
      [imageView.centerXAnchor constraintEqualToAnchor:container.centerXAnchor]
    ]];

    // Update constraint if needed.
    [self updateImageViewConstraint];

    [self setUpConstraintsForVivaldi:scrollView
                           container:container
                           imageView:imageView
                            topLabel:topLabel
                         bottomLabel:bottomLabel
                    buttonsContainer:_buttonsContainer];
  } else {

  [NSLayoutConstraint activateConstraints:@[
    [imageView.topAnchor constraintEqualToAnchor:container.topAnchor],
    [imageView.widthAnchor constraintEqualToConstant:kImageWidth],
    [imageView.heightAnchor constraintEqualToConstant:kImageHeight],
    [imageView.centerXAnchor constraintEqualToAnchor:container.centerXAnchor],
  ]];

  [NSLayoutConstraint activateConstraints:@[
    [topLabel.topAnchor constraintEqualToAnchor:imageView.bottomAnchor
                                       constant:kVerticalMargin],
    [topLabel.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [topLabel.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
    [topLabel.bottomAnchor
        constraintEqualToAnchor:bottomLabel.topAnchor
                       constant:-kTabGridEmptyStateVerticalMargin],

    [bottomLabel.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
    [bottomLabel.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [bottomLabel.bottomAnchor constraintEqualToAnchor:container.bottomAnchor
                                             constant:-kVerticalMargin],

    [container.topAnchor constraintEqualToAnchor:scrollView.topAnchor],
    [container.bottomAnchor constraintEqualToAnchor:scrollView.bottomAnchor],

    [scrollView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [scrollView.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor],
    [scrollView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ]];
  } // End Vivaldi

}

#pragma mark - VIVALDI

#pragma mark - Trait Collection
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateImageViewConstraint];
}

- (void)setUpConstraintsForVivaldi:(UIScrollView*)scrollView
                         container:(UIView*)container
                         imageView:(UIImageView*)imageView
                          topLabel:(UILabel*)topLabel
                       bottomLabel:(UILabel*)bottomLabel
                  buttonsContainer:(UIView*)buttonsContainer {

  [topLabel anchorTop:imageView.bottomAnchor
              leading:container.leadingAnchor
               bottom:nil
             trailing:container.trailingAnchor
              padding:topLabelPadding];

  [bottomLabel anchorTop:topLabel.bottomAnchor
                 leading:container.leadingAnchor
                  bottom:nil
                trailing:container.trailingAnchor
                 padding:bottomLabelPadding];

  [buttonsContainer anchorTop:bottomLabel.bottomAnchor
                      leading:nil
                       bottom:container.bottomAnchor
                     trailing:nil
                      padding:buttonsContainerPadding
                         size:CGSizeMake(0, 0)];
  [buttonsContainer centerXInSuperview];
  buttonsContainer.hidden = YES;

  _buttonsContainerVisibleConstraint =
      [buttonsContainer.heightAnchor
          constraintEqualToConstant:vRecentTabsEmptyStateLoginButtonHeight];
  _buttonsContainerVisibleConstraint.active = NO;

  _syncButtonSyncingStateWidthConstraint =
      [_enableSyncButton.widthAnchor
          constraintEqualToConstant:vRecentTabsEmptyStateLoginButtonHeight];
  _syncButtonSyncingStateWidthConstraint.active = NO;

  [container anchorTop:scrollView.topAnchor
               leading:nil
                bottom:scrollView.bottomAnchor
              trailing:nil];

  [NSLayoutConstraint activateConstraints:@[

    [buttonsContainer.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
    [buttonsContainer.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.trailingAnchor],

    [scrollView.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [scrollView.topAnchor constraintGreaterThanOrEqualToAnchor:self.topAnchor],
    [scrollView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ]];
}

- (void)setAuthButtonsContainerHidden:(BOOL)hidden {
  _buttonsContainer.hidden = hidden;
  _buttonsContainerVisibleConstraint.active = !hidden;
}

- (void)setAuthBottonsHidden:(BOOL)hidden {
  _loginButton.hidden = hidden;
  _registerButton.hidden = hidden;
}

- (void)setEnableSyncButtonHidden:(BOOL)hidden {
  _enableSyncButton.hidden = hidden;
}

- (void)setEnableSyncButtonWithSyncingState:(BOOL)isSyncing {

  _syncButtonSyncingStateWidthConstraint.active = isSyncing;

  [UIView animateWithDuration:0.5
                        delay:0
       usingSpringWithDamping:0.8
        initialSpringVelocity:0.3
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
    self.enableSyncButton.layer.cornerRadius =
        isSyncing ? vRecentTabsEmptyStateLoginButtonHeight/2 :
                    vRecentTabsEmptyStateLoginButtonCornerRadius;
    self.enableSyncButton.titleLabel.alpha = isSyncing ? 0 : 1;
    if (isSyncing) {
      [self.activityIndicator startAnimating];
    } else {
      [self.activityIndicator stopAnimating];
    }
    [self layoutIfNeeded];
  }
                   completion:nil];
}

- (void)updateEmptyViewWithUserState:(SessionsSyncUserState)state {
  switch (state) {
  case SessionsSyncUserState::USER_SIGNED_OUT: {
    [self setAuthButtonsContainerHidden:NO];
    [self setAuthBottonsHidden:NO];
    [self setEnableSyncButtonHidden:YES];

    self.topLabel.text =
        GetNSString(IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_UNAUTH_TITLE);
    self.bottomLabel.text =
        GetNSString(
            IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_UNAUTH_MESSAGE);
    break;
  }
    case SessionsSyncUserState::USER_SIGNED_IN_SYNC_IN_PROGRESS: {
      // We will show a button to enable sync in this state.
      // So, we can hide the register and login button for this state and show
      // enable sync button but with a loading spinner instead.
      [self setAuthButtonsContainerHidden:NO];
      [self setAuthBottonsHidden:YES];
      [self setEnableSyncButtonHidden:NO];
      [self setEnableSyncButtonWithSyncingState:YES];

      self.topLabel.text =
          GetNSString(IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_UNAUTH_TITLE);
      self.bottomLabel.text =
          GetNSString(
              IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_SYNC_IN_PROGRESS_MESSAGE);

      break;
    }
  case SessionsSyncUserState::USER_SIGNED_IN_SYNC_OFF: {
    // We will show a button to enable sync in this state.
    // So, we can hide the register and login button for this state and show
    // enable sync button.
    [self setAuthButtonsContainerHidden:NO];
    [self setAuthBottonsHidden:YES];
    [self setEnableSyncButtonHidden:NO];
    [self setEnableSyncButtonWithSyncingState:NO];

    self.topLabel.text =
        GetNSString(IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_UNAUTH_TITLE);
    self.bottomLabel.text =
        GetNSString(IDS_VIVALDI_TAB_SWITCHER_SYNC_TABS_ENABLE_SYNC_MESSAGE);
    break;
  }
  case SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS: {
    [self setAuthButtonsContainerHidden:YES];
    // No need to show a message here since empty view is not expected to be
    // visible in this state.
    break;
  }
  case SessionsSyncUserState::USER_SIGNED_IN_SYNC_ON_NO_SESSIONS: {
    [self setAuthButtonsContainerHidden:YES];
    self.topLabel.text = TitleForPageAndMode(_activePage, _tabGridMode);
    self.bottomLabel.text = BodyTextForPageAndMode(_activePage, _tabGridMode);
    break;
  }
  default: {
    break;
  }
  }
}

- (void)updateImageViewConstraint {
  if ([VivaldiGlobalHelpers isDeviceTablet])
    return;

  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    self.imageHeightConstraint.constant = 0;
    self.imageWidthConstraint.constant = 0;
    self.imageView.alpha = 0;
  } else {
    self.imageHeightConstraint.constant = kImageHeight;
    self.imageWidthConstraint.constant = kImageWidth;
    self.imageView.alpha = 1;
  }
  [self layoutIfNeeded];
}

#pragma mark - ACTIONS
- (void)handleLoginButtonTap {
  if (self.delegate)
    [self.delegate didSelectLoginFromEmptyStateView];
}

- (void)handleRegisterButtonTap {
  if (self.delegate)
    [self.delegate didSelectRegisterFromEmptyStateView];
}

- (void)handleEnableSyncButtonTap {
  if (self.delegate)
    [self.delegate didSelectEnableSyncFromEmptyStateView];
}
// End Vivaldi

@end
