// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_UI_NOTES_NOTE_HOME_CONSUMER_H_
#define IOS_UI_NOTES_NOTE_HOME_CONSUMER_H_

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"

@class NSIndexPath;
@class ShowSigninCommand;
@class SigninPromoViewConfigurator;

typedef NS_ENUM(NSInteger, NoteHomeBackgroundStyle) {
  // The default background style.
  NoteHomeBackgroundStyleDefault,

  // A background style that indicates that notes are loading.
  NoteHomeBackgroundStyleLoading,

  // A background style that indicates that no notes are present.
  NoteHomeBackgroundStyleEmpty,
};

// NoteHomeConsumer provides methods that allow mediators to update the UI.
@protocol NoteHomeConsumer<LegacyChromeTableViewConsumer>

// Refreshes the UI.
- (void)refreshContents;

// Displays the table view background for the given |style|.
- (void)updateTableViewBackgroundStyle:(NoteHomeBackgroundStyle)style;

// Displays the signin UI configured by |command|.
- (void)showSignin:(ShowSigninCommand*)command;

@end

#endif  // IOS_UI_NOTES_NOTE_HOME_CONSUMER_H_
