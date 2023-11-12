// Copyright 2022 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_DELEGATE_H_
#define IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_DELEGATE_H_

#import <UIKit/UIKit.h>

#import "ios/ui/ntp/vivaldi_speed_dial_item.h"

@protocol VivaldiSpeedDialContainerDelegate

/// Triggers when an item is selected with single tap, provides the item itself and the parent if any.
- (void)didSelectItem:(VivaldiSpeedDialItem*)item
               parent:(VivaldiSpeedDialItem*)parent;

/// Triggers when an item is instructed to be edited, provides the item itself and the parent if any.
- (void)didSelectEditItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent;

/// Triggers when an item about to 'Move out of folder' by the context menu action,
/// provides the item itself and the parent if any.
- (void)didSelectMoveItem:(VivaldiSpeedDialItem*)item
                   parent:(VivaldiSpeedDialItem*)parent;

/// Triggers when an item is moved by dragging the item from one place and dropping to another position in
/// the speed dial page. Provides the item itself and the parent if any also the destination position.
- (void)didMoveItemByDragging:(VivaldiSpeedDialItem*)item
                       parent:(VivaldiSpeedDialItem*)parent
                   toPosition:(NSInteger)position;

/// Triggers when an item about to 'Delete' by the context menu action,
/// provides the item itself and the parent if any.
- (void)didSelectDeleteItem:(VivaldiSpeedDialItem*)item
                     parent:(VivaldiSpeedDialItem*)parent;

/// Triggers when an speed dial item(Folder or URL) is about to be created by the tap action of either
/// the 'New Speed Dial' tile tap or any context menu action that tile contains.
/// Provides a boolean whether the 'New' item will be a folder, and parent of the 'New'item.
- (void)didSelectAddNewSpeedDial:(BOOL)isFolder
                          parent:(VivaldiSpeedDialItem*)parent;
@end

#endif  // IOS_UI_NTP_VIVALDI_SPEED_DIAL_CONTAINER_DELEGATE_H_
