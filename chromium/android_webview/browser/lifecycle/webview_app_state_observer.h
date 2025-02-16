// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_LIFECYCLE_WEBVIEW_APP_STATE_OBSERVER_H_
#define ANDROID_WEBVIEW_BROWSER_LIFECYCLE_WEBVIEW_APP_STATE_OBSERVER_H_

namespace android_webview {

// The interface for being notified of app state change, the implementation
// shall be added to observer list through AwContentsLifecycleNotifier.
class WebViewAppStateObserver {
 public:
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.android_webview
  // GENERATED_JAVA_CLASS_NAME_OVERRIDE: AppState
  enum class State {
    // All WebViews are detached from window.
    kUnknown,
    // At least one WebView is foreground.
    kForeground,
    // No WebView is foreground and at least one WebView is background.
    kBackground,
    // All WebViews are destroyed or no WebView has been created.
    // Observers shall use
    // AwContentsLifecycleNotifier::has_aw_contents_ever_created() to find if A
    // WebView has ever been created.
    kDestroyed,
    // Browser process initialization is not yet complete. When the browser
    // process is ready the state will change to foreground or background if
    // startup was triggered by creating a WebView, or to destroyed if no
    // WebView has yet been created.
    kStartup,
  };

  WebViewAppStateObserver();
  virtual ~WebViewAppStateObserver();

  // Invoked when app state is changed or right after this observer is added
  // into observer list.
  virtual void OnAppStateChanged(State state) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_LIFECYCLE_WEBVIEW_APP_STATE_OBSERVER_H_
