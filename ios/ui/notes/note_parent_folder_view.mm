// Copyright 2023 Vivaldi Technologies. All rights reserved.
#import "ios/ui/notes/note_parent_folder_view.h"

#import "UIKit/UIKit.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/ui/helpers/vivaldi_uiview_layout_helper.h"
#import "ios/ui/notes/note_ui_constants.h"
#import "ios/ui/ntp/vivaldi_ntp_constants.h"
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
}  // namespace

@interface NoteParentFolderView()
// Label for the folder view, i.e. - "PARENT FOLDER"
@property (nonatomic,weak) UILabel* titleLabel;
// Label for the folder name.
@property (nonatomic,weak) UILabel* folderNameLabel;
// Imageview for the folder icon
@property (nonatomic,weak) UIImageView* folderIconView;

@end

@implementation NoteParentFolderView
@synthesize titleLabel = _titleLabel;
@synthesize folderNameLabel = _folderNameLabel;
@synthesize folderIconView = _folderIconView;

#pragma mark - INITIALIZER
- (instancetype)initWithTitle:(NSString*)title {
  self = [super initWithFrame:CGRectZero];
  if (self) {
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
    ImageViewForImageNamed(vNotesFolderIcon);
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
    ImageViewForImageNamed(vNoteFolderSelectionChevron);
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

}

#pragma mark - PRIVATE
/// Triggers when folder view is tapped.
- (void) handleParentFolderTap {
  if (self.delegate)
    [self.delegate didTapParentFolder];
}

#pragma mark - SETTERS

- (void)setParentFolderAttributesWithItem:(const vivaldi::NoteNode*)item {
  if (item)
    self.folderNameLabel.text = base::SysUTF16ToNSString(item->GetTitle());
  self.folderIconView.image =
    [UIImage imageNamed:vNotesFolderIcon];
}

@end
