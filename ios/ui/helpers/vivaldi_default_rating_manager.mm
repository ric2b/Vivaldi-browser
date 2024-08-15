// Copyright 2024 Vivaldi Technologies. All rights reserved.

#import "ios/ui/helpers/vivaldi_default_rating_manager.h"

#import <Foundation/Foundation.h>

namespace {
  static NSString* activeDaysKey = @"activeDays";
  static NSString* reviewPromptCountKey = @"reviewPromptCount";
  static NSString* lastPromptDateKey = @"lastPromptDate";
  static NSString* utcTimeZoneID = @"UTC";
  static const NSInteger maxPromptsPerYear = 3;
  static const NSInteger requiredActiveDays = 4;
  static const NSInteger inLastTotalDays = 7;
}

@interface VivaldiDefaultRatingManager ()

@property (strong, nonatomic) NSMutableArray<NSDate *> *activeDays;
@property (nonatomic) NSInteger reviewPromptCount;
@property (strong, nonatomic) NSDate *lastPromptDate;
@property (strong, nonatomic, readonly) NSCalendar *utcCalendar;

@end

/// find discussion about when to show the default review prompt
/// here: https://bugs.vivaldi.com/browse/VIB-654
@implementation VivaldiDefaultRatingManager

- (NSCalendar *)utcCalendar {
  NSCalendar *calendar = [NSCalendar currentCalendar];
  calendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:utcTimeZoneID];
  return calendar;
}

- (NSMutableArray<NSDate *> *)activeDays {
  return [[[NSUserDefaults standardUserDefaults]
            arrayForKey:activeDaysKey] mutableCopy] ?: [NSMutableArray array];
}

- (void)setActiveDays:(NSMutableArray<NSDate *> *)activeDays {
  [[NSUserDefaults standardUserDefaults]
    setObject:activeDays forKey:activeDaysKey];
}

- (NSInteger)reviewPromptCount {
  return [[NSUserDefaults standardUserDefaults]
           integerForKey:reviewPromptCountKey];
}

- (void)setReviewPromptCount:(NSInteger)reviewPromptCount {
  [[NSUserDefaults standardUserDefaults]
    setInteger:reviewPromptCount forKey:reviewPromptCountKey];
}

- (NSDate *)lastPromptDate {
  return [[NSUserDefaults standardUserDefaults]
                            objectForKey:lastPromptDateKey];
}

- (void)setLastPromptDate:(NSDate *)lastPromptDate {
  [[NSUserDefaults standardUserDefaults]
                     setObject:lastPromptDate forKey:lastPromptDateKey];
}

- (void)recordActiveDay {
  NSDate *today = [NSDate date];
  NSDate *utcToday = [self.utcCalendar startOfDayForDate:today];
  NSMutableArray<NSDate *> *days = [self activeDays];

  // Check if today is already recorded
  NSPredicate *todayPredicate =
    [NSPredicate predicateWithBlock:
      ^BOOL(NSDate *evaluatedObject,
            NSDictionary<NSString *,id> * _Nullable bindings) {
    return [self.utcCalendar
             isDate:evaluatedObject
             equalToDate:utcToday
             toUnitGranularity:NSCalendarUnitDay];
  }];

  NSArray<NSDate *> *todayEntries =
    [days filteredArrayUsingPredicate:todayPredicate];
  if (todayEntries.count == 0) {
    [days addObject:utcToday];  // Add today only if it's not already added
  }

  // Filter to keep only the last 7 days including today
  NSDate *sevenDaysAgo =
      [self.utcCalendar dateByAddingUnit:NSCalendarUnitDay value:-inLastTotalDays
                                  toDate:utcToday
                                 options:0];
  NSPredicate *lastWeekPredicate =
    [NSPredicate predicateWithBlock:^BOOL(NSDate *evaluatedObject,
      NSDictionary<NSString *,id> * _Nullable bindings) {
    return [evaluatedObject compare:sevenDaysAgo] == NSOrderedDescending;
  }];

  [self setActiveDays:[[days
    filteredArrayUsingPredicate:lastWeekPredicate] mutableCopy]];
}


- (BOOL)hasMetCriteria {
    return self.activeDays.count >= requiredActiveDays;
}

- (BOOL)canShowReviewPrompt {
  NSDate *currentDate = [NSDate date];
  NSInteger currentYear = [self.utcCalendar
                            component:NSCalendarUnitYear
                             fromDate:currentDate];
  NSInteger currentMonth = [self.utcCalendar
                            component:NSCalendarUnitMonth
                             fromDate:currentDate];

  // Handle the nil case for lastPromptDate
  if (self.lastPromptDate == nil) {
    self.lastPromptDate = [NSDate distantPast];
  }

  NSInteger lastPromptYear = [self.utcCalendar
                              component:NSCalendarUnitYear
                               fromDate:self.lastPromptDate];
  NSInteger lastPromptMonth = [self.utcCalendar
                                component:NSCalendarUnitMonth
                                 fromDate:self.lastPromptDate];

  if (lastPromptYear != currentYear) {
    // This will automatically update UserDefaults via setter
    self.reviewPromptCount = 0;
  }

  BOOL isNewMonth = lastPromptMonth != currentMonth;
  return [self hasMetCriteria] &&
            (isNewMonth || self.reviewPromptCount < maxPromptsPerYear);
}

- (BOOL)shouldShowReviewPrompt {
  if ([self canShowReviewPrompt]) {
    self.lastPromptDate = [NSDate date];
    // This will automatically update UserDefaults via setter
    self.reviewPromptCount += 1;
    return YES;
  }
  return NO;
}

@end
