// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_NTP_NSD_VIVALDI_NSD_VIEW_DELEGATE_H_
#define IOS_UI_NTP_NSD_VIVALDI_NSD_VIEW_DELEGATE_H_

@class VivaldiNSDDirectMatchCategory;

@protocol VivaldiNSDViewDelegate
- (void)didSelectItemWithTitle:(NSString*)title
                           url:(NSString*)url
                           add:(BOOL)add;

- (void)didSelectCategory:(VivaldiNSDDirectMatchCategory*)category;
@end

#endif  // IOS_UI_NTP_NSD_VIVALDI_NSD_VIEW_DELEGATE_H_
