// Copyright (c) 2015 Vivaldi Technologies. All rights reserved.

#ifndef CHROME_BROWSER_UI_COCOA_SPARKLE_UTIL_H_
#define CHROME_BROWSER_UI_COCOA_SPARKLE_UTIL_H_

#include <string>

class SparkleUtil {
 public:
   // Sets the feed URL
   static void SetFeedURL(const std::string& feed_url);
   // Gets the feed URL.
   static std::string GetFeedURL();
};

#endif  // CHROME_BROWSER_UI_COCOA_SPARKLE_UTIL_H_
