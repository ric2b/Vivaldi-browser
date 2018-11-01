// Copyright (c) 2015 Vivaldi Technologies. All rights reserved.

#ifndef BROWSER_MAC_SPARKLE_UTIL_H_
#define BROWSER_MAC_SPARKLE_UTIL_H_

#include <string>

class SparkleUtil {
 public:
  // Sets the feed URL
  static void SetFeedURL(const std::string& feed_url);
  // Gets the feed URL.
  static std::string GetFeedURL();
};

#endif  // BROWSER_MAC_SPARKLE_UTIL_H_
