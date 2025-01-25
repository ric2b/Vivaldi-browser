// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_cell.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_content_injector.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_credential.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/favicon_size.h"
#import "url/gurl.h"

NSString* const kMaskedPasswordTitle = @"••••••••";

namespace {

constexpr CGFloat kFaviconContainterViewSize = 30;

// Returns the size that the favicon should have.
CGFloat GetFaviconSize() {
  return IsKeyboardAccessoryUpgradeEnabled() ? kFaviconContainterViewSize
                                             : gfx::kFaviconSize;
}

}  // namespace

@interface ManualFillCredentialItem ()

// The credential for this item.
@property(nonatomic, strong, readonly) ManualFillCredential* credential;

// The cell won't show a title (site name) label if it is connected to the
// previous password item.
// TODO(crbug.com/326398845): Remove once the Keyboard Accessory Upgrade feature
// has launched both on iPhone and iPad.
@property(nonatomic, assign) BOOL isConnectedToPreviousItem;

// The separator line won't show if it is connected to the next password item.
// TODO(crbug.com/326398845): Remove once the Keyboard Accessory Upgrade feature
// has launched both on iPhone and iPad.
@property(nonatomic, assign) BOOL isConnectedToNextItem;

// The delegate for this item.
@property(nonatomic, weak, readonly) id<ManualFillContentInjector>
    contentInjector;

// The UIActions that should be available from the cell's overflow menu button.
@property(nonatomic, strong) NSArray<UIAction*>* menuActions;

// The part of the cell's accessibility label that is used to indicate the index
// at which the password represented by this item is positioned in the list of
// passwords to show.
@property(nonatomic, strong) NSString* cellIndexAccessibilityLabel;

@end

@implementation ManualFillCredentialItem {
  // If `YES`, autofill button is shown for the item.
  BOOL _showAutofillFormButton;
}

- (instancetype)initWithCredential:(ManualFillCredential*)credential
         isConnectedToPreviousItem:(BOOL)isConnectedToPreviousItem
             isConnectedToNextItem:(BOOL)isConnectedToNextItem
                   contentInjector:
                       (id<ManualFillContentInjector>)contentInjector
                       menuActions:(NSArray<UIAction*>*)menuActions
       cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
            showAutofillFormButton:(BOOL)showAutofillFormButton {
  self = [super initWithType:kItemTypeEnumZero];
  if (self) {
    _credential = credential;
    _isConnectedToPreviousItem = isConnectedToPreviousItem;
    _isConnectedToNextItem = isConnectedToNextItem;
    _contentInjector = contentInjector;
    _menuActions = menuActions;
    _cellIndexAccessibilityLabel = cellIndexAccessibilityLabel;
    _showAutofillFormButton = showAutofillFormButton;
    self.cellClass = [ManualFillPasswordCell class];
  }
  return self;
}

- (void)configureCell:(ManualFillPasswordCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  [cell setUpWithCredential:self.credential
        isConnectedToPreviousCell:self.isConnectedToPreviousItem
            isConnectedToNextCell:self.isConnectedToNextItem
                  contentInjector:self.contentInjector
                      menuActions:self.menuActions
      cellIndexAccessibilityLabel:_cellIndexAccessibilityLabel
           showAutofillFormButton:_showAutofillFormButton];
}

- (const GURL&)faviconURL {
  return self.credential.URL;
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.credential.URL.spec());
}

@end

namespace {

// The offset to apply to a cell's top margin when connected to the previous
// cell.
static const CGFloat kOffsetForConnectedCell = 16;

}  // namespace

@interface ManualFillPasswordCell ()

// The credential this cell is showing.
@property(nonatomic, strong) ManualFillCredential* credential;

// The dynamic constraints for all the lines (i.e. not set in createView).
@property(nonatomic, strong)
    NSMutableArray<NSLayoutConstraint*>* dynamicConstraints;

// The constraints for the visible favicon.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* faviconContraints;

// The view displayed at the top the cell containing the favicon, the site name
// and an overflow button.
@property(nonatomic, strong) UIView* headerView;

// The favicon for the credential. Of type FaviconView when the Keyboard
// Accessory Upgrade is disabled, and FaviconContainerView when enabled.
@property(nonatomic, strong) UIView* faviconView;

// The label with the site name and host.
@property(nonatomic, strong) UILabel* siteNameLabel;

// The menu button displayed in the cell's header.
@property(nonatomic, strong) UIButton* overflowMenuButton;

// A button showing the username, or "No Username".
@property(nonatomic, strong) UIButton* usernameButton;

// A button showing "••••••••" to resemble a password.
@property(nonatomic, strong) UIButton* passwordButton;

// Separator line. When the Keyboard Accessory Upgrade feature is enbaled, used
// to delimit the header from the rest of the cell. When disabled, used when
// needed to delimit cells.
@property(nonatomic, strong) UIView* grayLine;

// The delegate in charge of processing the user actions in this cell.
@property(nonatomic, weak) id<ManualFillContentInjector> contentInjector;

// Layout guide for the cell's content.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

// Button to autofill the current form with the credential's data.
@property(nonatomic, strong) UIButton* autofillFormButton;

@end

@implementation ManualFillPasswordCell {
  // If `YES`, autofill button is shown for the cell.
  BOOL _showAutofillFormButton;
}

#pragma mark - Public

- (void)prepareForReuse {
  [super prepareForReuse];
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;

  [NSLayoutConstraint deactivateConstraints:self.dynamicConstraints];
  [self.dynamicConstraints removeAllObjects];

  self.siteNameLabel.text = @"";
  [self configureFaviconWithAttributes:nil];

  [self.usernameButton setTitle:@"" forState:UIControlStateNormal];
  self.usernameButton.enabled = YES;
  [self.usernameButton setTitleColor:[UIColor colorNamed:kTextPrimaryColor]
                            forState:UIControlStateNormal];

  [self.passwordButton setTitle:@"" forState:UIControlStateNormal];
  self.passwordButton.accessibilityLabel = nil;
  self.passwordButton.hidden = NO;

  self.credential = nil;

  self.grayLine.hidden = NO;
}

- (void)setUpWithCredential:(ManualFillCredential*)credential
      isConnectedToPreviousCell:(BOOL)isConnectedToPreviousCell
          isConnectedToNextCell:(BOOL)isConnectedToNextCell
                contentInjector:(id<ManualFillContentInjector>)contentInjector
                    menuActions:(NSArray<UIAction*>*)menuActions
    cellIndexAccessibilityLabel:(NSString*)cellIndexAccessibilityLabel
         showAutofillFormButton:(BOOL)showAutofillFormButton {
  _showAutofillFormButton = showAutofillFormButton;
  if (self.contentView.subviews.count == 0) {
    [self createViewHierarchy];
  }
  self.contentInjector = contentInjector;
  self.credential = credential;

  // Holds the views whose leading anchor is constrained relative to the cell's
  // leading anchor.
  std::vector<ManualFillCellView> verticalLeadViews;

  // Header.
  if (isConnectedToPreviousCell) {
    self.siteNameLabel.hidden = YES;
    self.faviconView.hidden = YES;
  } else {
    NSAttributedString* attributedText =
        [self createSiteNameLabelAttributedText:credential];
    self.siteNameLabel.attributedText = attributedText;
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      self.siteNameLabel.numberOfLines = 0;
      self.accessibilityLabel =
          [NSString stringWithFormat:@"%@, %@", cellIndexAccessibilityLabel,
                                     attributedText.string];
    }
    self.siteNameLabel.hidden = NO;
    self.faviconView.hidden = NO;
    AddViewToVerticalLeadViews(self.headerView,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      AddViewToVerticalLeadViews(
          self.grayLine, ManualFillCellView::ElementType::kHeaderSeparator,
          verticalLeadViews);
    }
  }
  if (menuActions && menuActions.count) {
    self.overflowMenuButton.menu = [UIMenu menuWithChildren:menuActions];
    self.overflowMenuButton.hidden = NO;
  } else {
    self.overflowMenuButton.hidden = YES;
  }

  // Holds the chip buttons related to the credential that are vertical leads.
  NSMutableArray<UIView*>* credentialGroupVerticalLeadChips =
      [[NSMutableArray alloc] init];

  // Username chip button.
  if (credential.username.length) {
    NSString* username = credential.username;
    [self.usernameButton setTitle:username forState:UIControlStateNormal];
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      self.usernameButton.accessibilityLabel = l10n_util::GetNSStringF(
          IDS_IOS_MANUAL_FALLBACK_CHIP_ACCESSIBILITY_LABEL,
          base::SysNSStringToUTF16(username));
    }
  } else {
    NSString* titleString =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_USERNAME);
    [self.usernameButton setTitle:titleString forState:UIControlStateNormal];
    [self.usernameButton setTitleColor:[UIColor colorNamed:kTextSecondaryColor]
                              forState:UIControlStateNormal];
    self.usernameButton.enabled = NO;
  }
  [credentialGroupVerticalLeadChips addObject:self.usernameButton];

  // Password chip button.
  if (credential.password.length) {
    [self.passwordButton setTitle:kMaskedPasswordTitle
                         forState:UIControlStateNormal];
    self.passwordButton.accessibilityLabel = l10n_util::GetNSString(
        IsKeyboardAccessoryUpgradeEnabled()
            ? IDS_IOS_MANUAL_FALLBACK_PASSWORD_CHIP_ACCESSIBILITY_LABEL
            : IDS_IOS_SETTINGS_PASSWORD_HIDDEN_LABEL);
    [credentialGroupVerticalLeadChips addObject:self.passwordButton];
    self.passwordButton.hidden = NO;
  } else {
    self.passwordButton.hidden = YES;
  }

  AddChipGroupsToVerticalLeadViews(@[ credentialGroupVerticalLeadChips ],
                                   verticalLeadViews);

  if (isConnectedToNextCell) {
    self.grayLine.hidden = YES;
  }

  if (ShouldCreateAutofillFormButton(_showAutofillFormButton)) {
    AddViewToVerticalLeadViews(self.autofillFormButton,
                               ManualFillCellView::ElementType::kOther,
                               verticalLeadViews);
  }

  // Set and activate constraints.
  self.dynamicConstraints = [[NSMutableArray alloc] init];
  CGFloat offset = isConnectedToPreviousCell ? -kOffsetForConnectedCell : 0;
  AppendVerticalConstraintsSpacingForViews(
      self.dynamicConstraints, verticalLeadViews, self.layoutGuide, offset);
  [NSLayoutConstraint activateConstraints:self.dynamicConstraints];
}

- (NSString*)uniqueIdentifier {
  return base::SysUTF8ToNSString(self.credential.URL.spec());
}

- (void)configureWithFaviconAttributes:(FaviconAttributes*)attributes {
  if (attributes.faviconImage) {
    self.faviconView.hidden = NO;
    [NSLayoutConstraint activateConstraints:self.faviconContraints];
    [self configureFaviconWithAttributes:attributes];
    return;
  }
  [NSLayoutConstraint deactivateConstraints:self.faviconContraints];
  self.faviconView.hidden = YES;
}

#pragma mark - Private

// Creates and sets up the view hierarchy.
- (void)createViewHierarchy {
  self.layoutGuide =
      AddLayoutGuideToContentView(self.contentView, /*cell_has_header=*/YES);

  self.selectionStyle = UITableViewCellSelectionStyleNone;

  NSMutableArray<NSLayoutConstraint*>* staticConstraints =
      [[NSMutableArray alloc] init];

  self.faviconView = IsKeyboardAccessoryUpgradeEnabled()
                         ? [[FaviconContainerView alloc] init]
                         : [[FaviconView alloc] init];
  self.faviconView.translatesAutoresizingMaskIntoConstraints = NO;
  self.faviconView.clipsToBounds = YES;
  self.faviconView.hidden = YES;
  self.faviconContraints = @[
    [self.faviconView.widthAnchor constraintEqualToConstant:GetFaviconSize()],
    [self.faviconView.heightAnchor
        constraintEqualToAnchor:self.faviconView.widthAnchor],
  ];

  self.siteNameLabel = CreateLabel();
  self.overflowMenuButton = CreateOverflowMenuButton();
  self.headerView = CreateHeaderView(self.faviconView, self.siteNameLabel,
                                     self.overflowMenuButton);
  [self.contentView addSubview:self.headerView];
  AppendHorizontalConstraintsForViews(staticConstraints, @[ self.headerView ],
                                      self.layoutGuide);

  self.grayLine = CreateGraySeparatorForContainer(self.contentView);

  self.usernameButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapUsernameButton:), self);
  [self.contentView addSubview:self.usernameButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.usernameButton ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  self.passwordButton = CreateChipWithSelectorAndTarget(
      @selector(userDidTapPasswordButton:), self);
  [self.contentView addSubview:self.passwordButton];
  AppendHorizontalConstraintsForViews(
      staticConstraints, @[ self.passwordButton ], self.layoutGuide,
      kChipsHorizontalMargin,
      AppendConstraintsHorizontalEqualOrSmallerThanGuide);

  if (ShouldCreateAutofillFormButton(_showAutofillFormButton)) {
    self.autofillFormButton = CreateAutofillFormButton();
    [self.contentView addSubview:self.autofillFormButton];
    AppendHorizontalConstraintsForViews(
        staticConstraints, @[ self.autofillFormButton ], self.layoutGuide);
  }

  [NSLayoutConstraint activateConstraints:staticConstraints];
}

- (void)userDidTapUsernameButton:(UIButton*)button {
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectUsername"));
  [self.contentInjector userDidPickContent:self.credential.username
                             passwordField:NO
                             requiresHTTPS:NO];
}

- (void)userDidTapPasswordButton:(UIButton*)button {
  if (![self.contentInjector canUserInjectInPasswordField:YES
                                            requiresHTTPS:YES]) {
    return;
  }
  base::RecordAction(
      base::UserMetricsAction("ManualFallback_Password_SelectPassword"));
  [self.contentInjector userDidPickContent:self.credential.password
                             passwordField:YES
                             requiresHTTPS:YES];
}

// Configure the favicon with the given `attributes`.
- (void)configureFaviconWithAttributes:(FaviconAttributes*)attributes {
  FaviconView* favicon;
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    FaviconContainerView* faviconContainerView =
        static_cast<FaviconContainerView*>(self.faviconView);
    favicon = faviconContainerView.faviconView;
  } else {
    favicon = static_cast<FaviconView*>(self.faviconView);
  }
  [favicon configureWithAttributes:attributes];
}

// Creates the attributed string containing the site name and potentially a host
// subtitle for the site name label.
- (NSMutableAttributedString*)createSiteNameLabelAttributedText:
    (ManualFillCredential*)credential {
  NSString* siteName = credential.siteName ? credential.siteName : @"";
  NSString* host;
  NSMutableAttributedString* attributedString;

  BOOL shouldShowHost = credential.host && credential.host.length &&
                        ![credential.host isEqualToString:credential.siteName];
  if (shouldShowHost) {
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      host = credential.host;
    }

    // If the Keyboard Accessory Upgrade feature is disabled, `host` will be
    // `nil` here, and so it won't be added to `attributedString` right away.
    attributedString = CreateHeaderAttributedString(siteName, host);

    if (!IsKeyboardAccessoryUpgradeEnabled()) {
      host = [NSString stringWithFormat:@" –– %@", credential.host];
      NSDictionary* attributes = @{
        NSForegroundColorAttributeName :
            [UIColor colorNamed:kTextSecondaryColor],
        NSFontAttributeName :
            [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
      };
      NSAttributedString* hostAttributedString =
          [[NSAttributedString alloc] initWithString:host
                                          attributes:attributes];
      [attributedString appendAttributedString:hostAttributedString];
    }
  } else {
    attributedString = CreateHeaderAttributedString(siteName, nil);
  }

  return attributedString;
}

@end
