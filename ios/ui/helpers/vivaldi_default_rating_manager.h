// Copyright 2024 Vivaldi Technologies. All rights reserved.

#ifndef IOS_UI_HELPERS_VIVALDI_DEFAULT_RATING_MANAGER_H_
#define IOS_UI_HELPERS_VIVALDI_DEFAULT_RATING_MANAGER_H_

#import <Foundation/Foundation.h>

@interface VivaldiDefaultRatingManager : NSObject

/// Record the days the user has been active
- (void)recordActiveDay;
/// checking for the review prompt, if the conditions are met
- (BOOL)shouldShowReviewPrompt;

@end

#endif // IOS_UI_HELPERS_VIVALDI_DEFAULT_RATING_MANAGER_H_
