// Copyright (c) 2015 Vivaldi Technologies. All rights reserved.

#include "browser/mac/sparkle_util.h"

#import <AppKit/AppKit.h>
#import "third_party/sparkle_lib/Sparkle.framework/Headers/SUUpdater.h"


@interface SparkleUtils : NSObject

- (void)setFeedURL:(const std::string&)url;
- (NSString*)getFeedURL;

@end  // @interface SparkleUtils

@implementation SparkleUtils

- (void)setFeedURL:(const std::string&)url {
#ifndef VIVALDI_SPARKLE_DISABLED
  NSURL* newFeedURL =
    [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];

  [[SUUpdater sharedUpdater] setFeedURL:newFeedURL];
#endif
}

- (NSString*)getFeedURL {
#ifndef VIVALDI_SPARKLE_DISABLED
  NSURL* theFeed = [[SUUpdater sharedUpdater] feedURL];
  NSString* path = [theFeed absoluteString];
  return path;
#else
  return nullptr;
#endif
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
