// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_TOP_TOOLBAR_VIVALDI_NTP_TOP_TOOLBAR_VIEW_CONSUMER_H_
#define IOS_UI_NTP_TOP_TOOLBAR_VIVALDI_NTP_TOP_TOOLBAR_VIEW_CONSUMER_H_

@class VivaldiNTPTopToolbarItem;

@protocol VivaldiNTPTopToolbarViewConsumer
- (void)didSelectItemWithIndex:(NSInteger)index;
- (void)didTriggerRenameToolbarItem:(VivaldiNTPTopToolbarItem*)toolbarItem;
- (void)didTriggerRemoveToolbarItem:(VivaldiNTPTopToolbarItem*)toolbarItem;
@end

#endif  // IOS_UI_NTP_TOP_TOOLBAR_VIVALDI_NTP_TOP_TOOLBAR_VIEW_CONSUMER_H_
