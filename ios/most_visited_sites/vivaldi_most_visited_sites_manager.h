// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_MOST_VISITED_SITES_VIVALDI_MOST_VISITED_SITES_MANAGER_H_
#define IOS_MOST_VISITED_SITES_VIVALDI_MOST_VISITED_SITES_MANAGER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"
#import "ios/most_visited_sites/vivaldi_most_visited_sites_consumer.h"

class ProfileIOS;

@interface VivaldiMostVisitedSitesManager : NSObject

@property(nonatomic, weak) id<VivaldiMostVisitedSitesConsumer> consumer;

- (instancetype)initWithProfile:(ProfileIOS*)profile NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)start;
- (void)stop;
- (void)refreshMostVisitedTiles;
- (void)removeMostVisited:(ContentSuggestionsMostVisitedItem*)item;

@end

#endif  // IOS_MOST_VISITED_SITES_VIVALDI_MOST_VISITED_SITES_MANAGER_H_
