// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#import "ios/website_dark_mode/website_dark_mode_java_script_feature.h"

#import <WebKit/WebKit.h>

#import "base/no_destructor.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"

namespace {
const char kScriptName[] = "website_dark_mode";

NSString* kEnableDarkModeFunction =
    @"__gCrWeb.websiteDarkMode.enableDarkMode();";
NSString* kDisableDarkModeFunction =
    @"__gCrWeb.websiteDarkMode.disableDarkMode();";
}  // namespace

// static
WebsiteDarkModeJavaScriptFeature*
  WebsiteDarkModeJavaScriptFeature::GetInstance() {
  static base::NoDestructor<WebsiteDarkModeJavaScriptFeature> instance;
  return instance.get();
}

WebsiteDarkModeJavaScriptFeature::WebsiteDarkModeJavaScriptFeature()
    : web::JavaScriptFeature(
          web::ContentWorld::kPageContentWorld,
          {FeatureScript::CreateWithFilename(
              kScriptName,
              FeatureScript::InjectionTime::kDocumentStart,
              FeatureScript::TargetFrames::kAllFrames,
              FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

WebsiteDarkModeJavaScriptFeature::~WebsiteDarkModeJavaScriptFeature() = default;


void WebsiteDarkModeJavaScriptFeature::ToggleDarkMode(web::WebState* web_state,
                                                      bool enabled) {
  for (web::WebFrame* frame :
       web_state->GetPageWorldWebFramesManager()->GetAllWebFrames()) {
    ToggleDarkMode(frame, enabled);
  }

  // Change the color of the scrollview scroll bar depending on the mode.
  id<CRWWebViewProxy> webProxy = web_state->GetWebViewProxy();
  CRWWebViewScrollViewProxy* scrollProxy = webProxy.scrollViewProxy;
  UIScrollView* scrollView = scrollProxy.asUIScrollView;
  scrollView.indicatorStyle = enabled ?
      UIScrollViewIndicatorStyleWhite : UIScrollViewIndicatorStyleDefault;
}

void WebsiteDarkModeJavaScriptFeature::ToggleDarkMode(web::WebFrame* web_frame,
                                                      bool enabled) {
  NSString* script =
      enabled ? kEnableDarkModeFunction : kDisableDarkModeFunction;
  if (web_frame) {
    web_frame->ExecuteJavaScript(base::SysNSStringToUTF16(script));
  }
}
