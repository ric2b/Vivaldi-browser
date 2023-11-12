// Copyright 2023 Vivaldi Technologies AS. All rights reserved.

#ifndef IOS_UI_AD_TRACKER_BLOCKER_MANGAER_VIVALDI_ATB_MANAGER_HELPER_H_
#define IOS_UI_AD_TRACKER_BLOCKER_MANGAER_VIVALDI_ATB_MANAGER_HELPER_H_

#import <Foundation/Foundation.h>

@interface ATBSourceTitleAndOrigin: NSObject

@property (nonatomic, copy, readonly) NSString* title;
@property (nonatomic, assign, readonly) NSInteger stringId;

- (instancetype)initWithTitle:(NSString*)title
                     stringId:(NSInteger)stringId;

@end

@interface VivaldiATBManagerHelper: NSObject

@property (nonatomic,strong,readonly)
    NSDictionary<NSString*, ATBSourceTitleAndOrigin*> *sourcesMap;

- (instancetype)init;
- (NSString*)titleForSourceForKey:(NSString*)key;

@end

#endif  // IOS_UI_AD_TRACKER_BLOCKER_MANGAER_VIVALDI_ATB_MANAGER_HELPER_H_
