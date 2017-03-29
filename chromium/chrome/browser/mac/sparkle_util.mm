// Copyright (c) 2015 Vivaldi Technologies. All rights reserved.

#include "chrome/browser/mac/sparkle_util.h"

#import <AppKit/AppKit.h>
#import "third_party/Sparkle-1.9.0/Sparkle.framework/Headers/SUUpdater.h"


@interface SparkleUtils : NSObject

- (void)setFeedURL:(const std::string&)url;
- (NSString*)getFeedURL;

@end  // @interface SparkleUtils

@implementation SparkleUtils

- (void)setFeedURL:(const std::string&)url {
  NSURL* newFeedURL =
    [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];

  [[SUUpdater sharedUpdater] setFeedURL:newFeedURL];
}

- (NSString*)getFeedURL {
  NSURL* theFeed = [[SUUpdater sharedUpdater] feedURL];
  NSString* path = [theFeed absoluteString];
  return path;
}

@end // @implementation SparkleUtils

// static
void SparkleUtil::SetFeedURL(const std::string& feed_url) {
  SparkleUtils* sparkleUtils =
      [[[SparkleUtils alloc] init] autorelease];
  [sparkleUtils setFeedURL:feed_url.c_str()];
}

// static
std::string SparkleUtil::GetFeedURL() {
  SparkleUtils* sparkleUtils =
      [[[SparkleUtils alloc] init] autorelease];
  std::string feed_url([[sparkleUtils getFeedURL] UTF8String]);
  return feed_url;
}
