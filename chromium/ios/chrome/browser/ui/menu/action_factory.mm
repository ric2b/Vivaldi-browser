// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/menu/action_factory.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/pasteboard_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Vivaldi
#import "app/vivaldi_apptools.h"
#import "ios/ui/context_menu/vivaldi_context_menu_constants.h"
#import "vivaldi/ios/grit/vivaldi_ios_native_strings.h"

using vivaldi::IsVivaldiRunning;
using l10n_util::GetNSString;
// End Vivaldi

@interface ActionFactory ()

// Histogram to record executed actions.
@property(nonatomic, assign) const char* histogram;

@end

@implementation ActionFactory

- (instancetype)initWithScenario:(MenuScenarioHistogram)scenario {
  if (self = [super init]) {
    _histogram = GetActionsHistogramName(scenario);
  }
  return self;
}

- (UIAction*)actionWithTitle:(NSString*)title
                       image:(UIImage*)image
                        type:(MenuActionType)type
                       block:(ProceduralBlock)block {
  // Capture only the histogram name's pointer to be copied by the block.
  const char* histogram = self.histogram;
  return [UIAction actionWithTitle:title
                             image:image
                        identifier:nil
                           handler:^(UIAction* action) {
                             base::UmaHistogramEnumeration(histogram, type);
                             if (block) {
                               block();
                             }
                           }];
}

- (UIAction*)actionToCopyURL:(const GURL)URL {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kLinkActionSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"copy_link_url"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuLink]; // End Vivaldi

  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_COPY_LINK_ACTION_TITLE)
                image:image
                 type:MenuActionType::CopyURL
                block:^{
                  StoreURLInPasteboard(URL);
                }];
}

- (UIAction*)actionToShareWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kShareSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"share"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuShare]; // End Vivaldi

  return
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL)
                      image:image
                       type:MenuActionType::Share
                      block:block];
}

- (UIAction*)actionToPinTabWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kPinSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"pin"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuPin]; // End Vivaldi

  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_PINTAB)
                image:image
                 type:MenuActionType::PinTab
                block:block];
}

- (UIAction*)actionToUnpinTabWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kPinSlashSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"unpin"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuUnpin]; // End Vivaldi

  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_UNPINTAB)
                image:image
                 type:MenuActionType::UnpinTab
                block:block];
}

- (UIAction*)actionToDeleteWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kDeleteActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"delete"];

  if (IsVivaldiRunning())
    image = [UIImage systemImageNamed:@"trash"]; // End Vivaldi

  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                      image:image
                       type:MenuActionType::Delete
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToOpenInNewTabWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kNewTabActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"open_in_new_tab"];
  ProceduralBlock completionBlock =
      [self recordMobileWebContextMenuOpenTabActionWithBlock:block];

  if (IsVivaldiRunning())
    image =
      [UIImage imageNamed:vMenuNewTab];
  // End Vivaldi

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)
                         image:image
                          type:MenuActionType::OpenInNewTab
                         block:completionBlock];
}

- (UIAction*)actionToOpenAllTabsWithBlock:(ProceduralBlock)block {
  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_OPEN_ALL_LINKS)
                         image:DefaultSymbolWithPointSize(
                                   kPlusSymbol, kSymbolActionPointSize)
                          type:MenuActionType::OpenAllInNewTabs
                         block:block];
}

- (UIAction*)actionToRemoveWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kHideActionSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"remove"];

  if (IsVivaldiRunning())
    image = [UIImage systemImageNamed:@"trash"]; // End Vivaldi

  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_REMOVE_ACTION_TITLE)
                      image:image
                       type:MenuActionType::Remove
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToEditWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kEditActionSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"edit"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuEdit]; // End Vivaldi

  return [self actionWithTitle:l10n_util::GetNSString(IDS_IOS_EDIT_ACTION_TITLE)
                         image:image
                          type:MenuActionType::Edit
                         block:block];
}

- (UIAction*)actionToHideWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kHideActionSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"remove"];

  if (IsVivaldiRunning())
    image = [UIImage systemImageNamed:@"trash"]; // End Vivaldi

  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_RECENT_TABS_HIDE_MENU_OPTION)
                      image:image
                       type:MenuActionType::Hide
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToMoveFolderWithBlock:(ProceduralBlock)block {

  if (IsVivaldiRunning()) {
    return [self
        actionWithTitle:GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                  image:[UIImage imageNamed:vMenuMove]
                   type:MenuActionType::Move
                  block:block];
  } // End Vivaldi

  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE)
                image:[UIImage imageNamed:@"move_folder"]
                 type:MenuActionType::Move
                block:block];
}

- (UIAction*)actionToMarkAsReadWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kMarkAsReadActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"mark_read"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuEyeOn]; // End Vivaldi

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_READ_ACTION)
                         image:image
                          type:MenuActionType::Read
                         block:block];
}

- (UIAction*)actionToMarkAsUnreadWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kMarkAsUnreadActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"remove"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuEyeOff]; // End Vivaldi

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION)
                         image:image
                          type:MenuActionType::Unread
                         block:block];
}

- (UIAction*)actionToOpenOfflineVersionInNewTabWithBlock:
    (ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kCheckmarkCircleSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"offline"];
  ProceduralBlock completionBlock =
      [self recordMobileWebContextMenuOpenTabActionWithBlock:block];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuOpenOfflineVersion]; // End Vivaldi

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_READING_LIST_OPEN_OFFLINE_BUTTON)
                         image:image
                          type:MenuActionType::ViewOffline
                         block:completionBlock];
}

- (UIAction*)actionToAddToReadingListWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kReadLaterActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"read_later"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuAddToReadingList]; // End Vivaldi

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)
                         image:image
                          type:MenuActionType::AddToReadingList
                         block:block];
}

- (UIAction*)actionToBookmarkWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"bookmark"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuAddBookmark]; // End Vivaldi

  return [self actionWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOBOOKMARKS)
                         image:image
                          type:MenuActionType::AddToBookmarks
                         block:block];
}

- (UIAction*)actionToEditBookmarkWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kEditActionSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"bookmark"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuEditBookmark]; // End Vivaldi

  return [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_BOOKMARK_CONTEXT_MENU_EDIT)
                image:image
                 type:MenuActionType::EditBookmark
                block:block];
}

- (UIAction*)actionToCloseTabWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kXMarkSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"close"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuClose]; // End Vivaldi

  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_CLOSETAB)
                image:image
                 type:MenuActionType::CloseTab
                block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionSaveImageWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kSaveImageActionSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"download"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuSave]; // End Vivaldi

  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SAVEIMAGE)
                image:image
                 type:MenuActionType::SaveImage
                block:block];
  return action;
}

- (UIAction*)actionCopyImageWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kCopyActionSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"copy"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuCopy]; // End Vivaldi

  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_COPYIMAGE)
                image:image
                 type:MenuActionType::CopyImage
                block:block];
  return action;
}

- (UIAction*)actionSearchImageWithTitle:(NSString*)title
                                  Block:(ProceduralBlock)block {
  UIImage* image =
      UseSymbols() ? CustomSymbolWithPointSize(kPhotoBadgeMagnifyingglassSymbol,
                                               kSymbolActionPointSize)
                   : [UIImage imageNamed:@"search_image"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuSearchForImage]; // End Vivaldi

  UIAction* action = [self actionWithTitle:title
                                     image:image
                                      type:MenuActionType::SearchImage
                                     block:block];
  return action;
}

- (UIAction*)actionToCloseAllTabsWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? DefaultSymbolWithPointSize(
                                      kXMarkSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"close"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuClose]; // End Vivaldi

  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(
                                IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABS)
                      image:image
                       type:MenuActionType::CloseAllTabs
                      block:block];
  action.attributes = UIMenuElementAttributesDestructive;
  return action;
}

- (UIAction*)actionToSelectTabsWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols()
                       ? DefaultSymbolWithPointSize(kCheckmarkCircleSymbol,
                                                    kSymbolActionPointSize)
                       : [UIImage imageNamed:@"select"];

  if (IsVivaldiRunning())
    image = [UIImage imageNamed:vMenuSelect]; // End Vivaldi

  UIAction* action = [self
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_SELECTTABS)
                image:image
                 type:MenuActionType::SelectTabs
                block:block];
  return action;
}

- (UIAction*)actionToSearchImageUsingLensWithBlock:(ProceduralBlock)block {
  UIImage* image = UseSymbols() ? CustomSymbolWithPointSize(
                                      kCameraLensSymbol, kSymbolActionPointSize)
                                : [UIImage imageNamed:@"lens_icon"];
  int actionTitleMessageId =
      base::FeatureList::IsEnabled(kEnableLensContextMenuAltText)
          ? IDS_IOS_CONTEXT_MENU_SEARCHIMAGEWITHGOOGLE_ALT_TEXT
          : IDS_IOS_CONTEXT_MENU_SEARCHIMAGEWITHGOOGLE;
  UIAction* action =
      [self actionWithTitle:l10n_util::GetNSString(actionTitleMessageId)
                      image:image
                       type:MenuActionType::SearchImageWithLens
                      block:block];
  return action;
}

- (ProceduralBlock)recordMobileWebContextMenuOpenTabActionWithBlock:
    (ProceduralBlock)block {
  return ^{
    base::RecordAction(base::UserMetricsAction("MobileWebContextMenuOpenTab"));
    if (block) {
      block();
    }
  };
}

#pragma mark - Vivaldi

- (UIAction*)actionToAddNoteWithBlock:(ProceduralBlock)block {
  UIImage* image = [UIImage imageNamed:@"note"];
  return [self actionWithTitle:l10n_util::GetNSString(
                         IDS_VIVALDI_CONTENT_CONTEXT_NEWNOTE)
                         image:image
                         type:MenuActionType::NewNote
                         block:block];
}

- (UIAction*)actionToAddFolderWithBlock:(ProceduralBlock)block {
  UIImage* image = [UIImage imageNamed:@"note"];
  return [self actionWithTitle:l10n_util::GetNSString(
                         IDS_VIVALDI_CONTENT_CONTEXT_NEWFOLDER)
                         image:image
                         type:MenuActionType::NewFolder
                         block:block];
}

- (UIAction*)actionToClearHistoryWithBlock:(ProceduralBlock)block {
    UIImage* image = UseSymbols()
                         ? DefaultSymbolWithPointSize(kAddBookmarkActionSymbol,
                                                      kSymbolActionPointSize)
                         : [UIImage imageNamed:@"note"];
    return [self actionWithTitle:l10n_util::GetNSStringWithFixup(
                           IDS_VIVALDI_HISTORY_OPEN_CLEAR_BROWSING_DATA_DIALOG)
                           image:image
                           type:MenuActionType::ClearHistory
                           block:block];
}


// Creates a UIAction instance whose title and icon are configured for done
// which will invoke the given edit |block| when executed.
- (UIAction*)actionDoneWithBlock:(ProceduralBlock)block {
    UIImage* image = UseSymbols()
                         ? DefaultSymbolWithPointSize(@"done",
                                                      kSymbolActionPointSize)
                         : [UIImage imageNamed:@"notes"];
    return [self actionWithTitle:l10n_util::GetNSString(
                           IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)
                           image:image
                           type:MenuActionType::NewNote
                           block:block];
  }

@end
