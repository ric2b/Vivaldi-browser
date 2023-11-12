// Copyright 2022 Vivaldi Technologies. All rights reserved.
#import "ios/ui/bookmarks_editor/vivaldi_bookmark_parent_folder_view.h"

#import "UIKit/UIKit.h"

#import "ios/ui/bookmarks_editor/vivaldi_bookmarks_constants.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
#import "ios/ui/ntp/vivaldi_speed_dial_constants.h"
#import "ui/base/l10n/l10n_util.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {

// Returns an UIImageView for the given imgageName.
UIImageView* ImageViewForImageNamed(NSString* imageName) {
  return [[UIImageView alloc]
      initWithImage:
          [[UIImage imageNamed:imageName]
              imageWithRenderingMode:UIImageRenderingModeAlwaysOriginal]];
}

// Padding for the title label
UIEdgeInsets titleLabelPadding = UIEdgeInsetsMake(18, 0, 0, 0);
// Folder icon size
CGSize folderIconSize = CGSizeMake(24.0, 24.0);
// Padding for the folder name label
UIEdgeInsets folderNameLabelPadding = UIEdgeInsetsMake(0, 12, 0, 0);
// Right chevron size
CGSize rightChevronSize = CGSizeMake(12.0, 12.0);
// Padding for the right chevron
UIEdgeInsets rightChevronPadding = UIEdgeInsetsMake(0, 12, 0, 0);
// Padding for the speed dial selection view
UIEdgeInsets speedDialSelectionViewPadding = UIEdgeInsetsMake(8, 0, 0, 0);
// Padding for speed dial toggle
UIEdgeInsets speedDialTogglePadding = UIEdgeInsetsMake(0, 16, 8, 0);
// Padding for the speed dial selection label
UIEdgeInsets speedDialSelectionLabelPadding = UIEdgeInsetsMake(0, 0, 0, 16);
// Speed dial selection view visible height
CGFloat speedDialSelectionViewVisibleHeight = 60.f;
// Speed dial selection view hidden height
CGFloat speedDialSelectionViewHiddenHeight = 0.f;

}  // namespace

@interface VivaldiBookmarkParentFolderView()
// Label for the folder view, i.e. - "PARENT FOLDER"
@property (nonatomic,weak) UILabel* titleLabel;
// Label for the folder name.
@property (nonatomic,weak) UILabel* folderNameLabel;
// Imageview for the folder icon
@property (nonatomic,weak) UIImageView* folderIconView;
// A view that holds the speed dial selection components.
@property (nonatomic,weak) UIView* speedDialSelectionView;
// The label that describes the text for speed dial selection.
@property (nonatomic,weak) UILabel* speedDialSelectionLabel;
// Speed dial toggle.
@property (nonatomic,weak) UISwitch* setSpeedDialToggle;

// Height constaint property for speed dial selection view.
@property (nonatomic,assign) NSLayoutConstraint* speedDialSelectionViewHeight;
@end

@implementation VivaldiBookmarkParentFolderView
@synthesize titleLabel = _titleLabel;
@synthesize folderNameLabel = _folderNameLabel;
@synthesize folderIconView = _folderIconView;
@synthesize speedDialSelectionView = _speedDialSelectionView;
@synthesize speedDialSelectionLabel = _speedDialSelectionLabel;
@synthesize setSpeedDialToggle = _setSpeedDialToggle;
@synthesize speedDialSelectionViewHeight = _speedDialSelectionViewHeight;

#pragma mark - INITIALIZER
- (instancetype)initWithTitle:(NSString*)title {
  if (self = [super initWithFrame:CGRectZero]) {
    self.backgroundColor = UIColor.clearColor;
    [self setUpUI];
    self.titleLabel.text = title;
  }
  return self;
}

#pragma mark - SET UP UI COMPONENTS
- (void)setUpUI {

  // Title label
  UILabel* titleLabel = [[UILabel alloc] init];
  _titleLabel = titleLabel;
  titleLabel.textColor = [UIColor colorNamed:vNTPToolbarTextColor];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  titleLabel.numberOfLines = 1;
  titleLabel.textAlignment = NSTextAlignmentLeft;

  [self addSubview:titleLabel];
  [titleLabel anchorTop:self.topAnchor
                leading:self.leadingAnchor
                 bottom:nil
               trailing:self.trailingAnchor];

  // Container to hold folder icon and title
  UIView* containerView = [UIView new];
  containerView.backgroundColor = UIColor.clearColor;
  containerView.userInteractionEnabled = YES;

  // Set up a tap gesture for folder selection
  UITapGestureRecognizer* tapGesture =
    [[UITapGestureRecognizer alloc] initWithTarget:self
                                       action:@selector(handleParentFolderTap)];
  [containerView addGestureRecognizer:tapGesture];
  tapGesture.numberOfTapsRequired = 1;

  [self addSubview:containerView];
  [containerView anchorTop:titleLabel.bottomAnchor
                   leading:self.leadingAnchor
                    bottom:nil
                  trailing:self.trailingAnchor
                   padding:titleLabelPadding];
  containerView.isAccessibilityElement = YES;
  containerView.accessibilityTraits |= UIAccessibilityTraitButton;

  // Folder Icon
  UIImageView* folderIconView =
    ImageViewForImageNamed(vBookmarksFolderIcon);
  _folderIconView = folderIconView;
  folderIconView.contentMode = UIViewContentModeScaleAspectFit;
  folderIconView.backgroundColor = UIColor.clearColor;

  [containerView addSubview:folderIconView];
  [folderIconView anchorTop:containerView.topAnchor
                    leading:containerView.leadingAnchor
                     bottom:containerView.bottomAnchor
                   trailing:nil
                       size:folderIconSize];

  // Folder Name label
  UILabel* folderNameLabel = [UILabel new];
  _folderNameLabel = folderNameLabel;
  folderNameLabel.textColor = UIColor.labelColor;
  folderNameLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  folderNameLabel.numberOfLines = 1;
  folderNameLabel.textAlignment = NSTextAlignmentLeft;

  [containerView addSubview:folderNameLabel];
  [folderNameLabel anchorTop:containerView.topAnchor
                     leading:folderIconView.trailingAnchor
                      bottom:containerView.bottomAnchor
                    trailing:nil
                     padding:folderNameLabelPadding];

  // Right Chevron for the folder selection
  UIImageView* rightChevron =
    ImageViewForImageNamed(vBookmarkFolderSelectionChevron);
  rightChevron.contentMode = UIViewContentModeScaleAspectFit;
  rightChevron.backgroundColor = UIColor.clearColor;

  [containerView addSubview:rightChevron];
  [rightChevron anchorTop:nil
                  leading:folderNameLabel.trailingAnchor
                   bottom:nil
                 trailing:containerView.trailingAnchor
                  padding:rightChevronPadding
                     size:rightChevronSize];
  [rightChevron centerYInSuperview];

  // Speed dial selection view that holds the description label and the toggle.
  UIView* speedDialSelectionView = [UIView new];
  speedDialSelectionView.backgroundColor = UIColor.clearColor;
  _speedDialSelectionView = speedDialSelectionView;

  [self addSubview:_speedDialSelectionView];
  [_speedDialSelectionView anchorTop:containerView.bottomAnchor
                             leading:self.leadingAnchor
                              bottom:self.bottomAnchor
                            trailing:self.trailingAnchor
                             padding:speedDialSelectionViewPadding];
  _speedDialSelectionViewHeight =
    [_speedDialSelectionView.heightAnchor constraintEqualToConstant:0];
  [_speedDialSelectionViewHeight setActive:YES];

  // Toggle button
  UISwitch* toggle = [UISwitch new];
  _setSpeedDialToggle = toggle;
  [speedDialSelectionView addSubview:toggle];

  [_setSpeedDialToggle anchorTop: nil
                         leading:nil
                          bottom:speedDialSelectionView.bottomAnchor
                        trailing:speedDialSelectionView.trailingAnchor
                         padding:speedDialTogglePadding];
  [_setSpeedDialToggle setHidden:YES];

  // Speed dial selection view description label
  UILabel* speedDialSelectionLabel = [[UILabel alloc] init];
  _speedDialSelectionLabel = speedDialSelectionLabel;
  NSString* titleString = GetNSString(IDS_IOS_USE_AS_SPEED_DIAL_FOLDER);
  speedDialSelectionLabel.text = titleString;
  speedDialSelectionLabel.accessibilityLabel = titleString;
  speedDialSelectionLabel.textColor =
    [UIColor colorNamed:vNTPSpeedDialDomainTextColor];
  speedDialSelectionLabel.font =
    [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  speedDialSelectionLabel.numberOfLines = 1;
  speedDialSelectionLabel.textAlignment = NSTextAlignmentLeft;

  [speedDialSelectionView addSubview:speedDialSelectionLabel];
  [_speedDialSelectionLabel anchorTop:toggle.topAnchor
                              leading:speedDialSelectionView.leadingAnchor
                               bottom:toggle.bottomAnchor
                             trailing:toggle.leadingAnchor
                              padding:speedDialSelectionLabelPadding];
  [_speedDialSelectionLabel setHidden:YES];
}

#pragma mark - PRIVATE
/// Triggers when folder view is tapped.
- (void) handleParentFolderTap {
  if (self.delegate)
    [self.delegate didTapParentFolder];
}

#pragma mark - GETTERS
- (BOOL)isUseAsSpeedDialFolder {
  return self.setSpeedDialToggle.on;
}

#pragma mark - SETTERS
- (void)setSpeedDialSelectionHidden:(BOOL)hide {
  if (hide) {
    self.speedDialSelectionViewHeight.constant =
      speedDialSelectionViewHiddenHeight;
    [self.setSpeedDialToggle setHidden:YES];
    [self.speedDialSelectionLabel setHidden:YES];
  } else {
    self.speedDialSelectionViewHeight.constant =
      speedDialSelectionViewVisibleHeight;
    [self.setSpeedDialToggle setHidden:NO];
    [self.speedDialSelectionLabel setHidden:NO];
  }
}

- (void)setParentFolderAttributesWithItem:(const VivaldiSpeedDialItem*)item {
  self.folderNameLabel.text = item.title;
  self.folderNameLabel.accessibilityLabel = item.title;
  self.folderIconView.image = item.isSpeedDial ?
    [UIImage imageNamed:vSpeedDialFolderIcon] :
    [UIImage imageNamed:vBookmarksFolderIcon];
}

- (void)setChildrenAttributesWithItem:(const VivaldiSpeedDialItem*)item {
  [self.setSpeedDialToggle setOn:item.isSpeedDial animated:NO];
}

@end
