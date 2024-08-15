// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_JAVA_SCRIPT_FEATURE_H_
#define IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_JAVA_SCRIPT_FEATURE_H_

#import <Foundation/Foundation.h>

#import "base/no_destructor.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace web {
class WebFrame;
class WebState;
}  // namespace web

// A class for handling force dark mode JavaScript features in a web view.
class WebsiteDarkModeJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // Singleton instance for the feature.
  static WebsiteDarkModeJavaScriptFeature* GetInstance();

  // Toggles dark mode in the given web state.
  void ToggleDarkMode(web::WebState* web_state, bool enabled);
  // Toggles dark mode in the given web state.
  void ToggleDarkMode(web::WebFrame* web_frame, bool enabled);

 private:
  friend class base::NoDestructor<WebsiteDarkModeJavaScriptFeature>;

  // Constructor and destructor.
  WebsiteDarkModeJavaScriptFeature();
  ~WebsiteDarkModeJavaScriptFeature() override;

  // Disallow copy and assign.
  WebsiteDarkModeJavaScriptFeature
      (const WebsiteDarkModeJavaScriptFeature&) = delete;
  WebsiteDarkModeJavaScriptFeature&
      operator=(const WebsiteDarkModeJavaScriptFeature&) = delete;
};


#endif  // IOS_WEBSITE_DARK_MODE_WEBSITE_DARK_MODE_JAVA_SCRIPT_FEATURE_H_
