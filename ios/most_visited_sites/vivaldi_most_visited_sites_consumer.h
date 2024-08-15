// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_MOST_VISITED_SITES_VIVALDI_MOST_VISITED_SITES_CONSUMER_H_
#define IOS_MOST_VISITED_SITES_VIVALDI_MOST_VISITED_SITES_CONSUMER_H_

@class MostVisitedTilesConfig;

@protocol VivaldiMostVisitedSitesConsumer
// Indicates to the consumer the current Most Visited tiles to show with
// `config`.
- (void)setMostVisitedTilesConfig:(MostVisitedTilesConfig*)config;

@end

#endif  // IOS_MOST_VISITED_SITES_VIVALDI_MOST_VISITED_SITES_CONSUMER_H_
