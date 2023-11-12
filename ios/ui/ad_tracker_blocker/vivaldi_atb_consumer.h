 // Copyright 2022 Vivaldi Technologies. All rights reserved.

 #ifndef IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSUMER_H_
 #define IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSUMER_H_

 /// VivaldiATBConsumer provides methods that allow mediator to update the
 /// ad and tracker blocker summery and settings UI.
 @protocol VivaldiATBConsumer

 /// Notifies the subscriber to refresh the ad and tracker blocker setting
 /// options
 - (void)refreshSettingOptions:(NSArray*)items;

 @end

 #endif  // IOS_UI_AD_TRACKER_BLOCKER_VIVALDI_ATB_CONSUMER_H_
