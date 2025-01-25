// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_VIVALDI_OVERFLOW_MENU_VIVALDI_OVERFLOW_MENU_UTIL_H_
#define IOS_UI_VIVALDI_OVERFLOW_MENU_VIVALDI_OVERFLOW_MENU_UTIL_H_

#import <Foundation/Foundation.h>

extern NSString* vResetOverflowMenuDestinations;
extern NSString* vResetOverflowMenuActions;

/// While updating from older version of the app we need to
/// flush the default destinations and actions of overflow menu for two
/// reasons.
/// First, we don't want use chromium order, and don't
/// want to show few items from their list, and we have our items.
/// Second, the old list is already stored in pref from older version of
/// the app. If we don't reset old data app will not show new list,
/// new order, and worst can crash when menu or customization
/// is triggerred.
bool ShouldResetOverflowMenuActions();

void SetOverflowMenuActionsResetComplete();

#endif  // IOS_UI_VIVALDI_OVERFLOW_MENU_VIVALDI_OVERFLOW_MENU_UTIL_H_
