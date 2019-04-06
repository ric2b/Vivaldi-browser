// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

#ifndef UI_VIVALDI_NATIVE_APP_WINDOW_H_
#define UI_VIVALDI_NATIVE_APP_WINDOW_H_

#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/native_app_window.h"

class VivaldiNativeAppWindow : public extensions::NativeAppWindow,
                               public content::WebContentsObserver {
 public:
  // Update the inset. Used to update when a frameless window goes to maximized.
  virtual void UpdateEventTargeterWithInset(){};
};

#endif  // UI_VIVALDI_NATIVE_APP_WINDOW_H_
