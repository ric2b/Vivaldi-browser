// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_APP_OBSERVER_H_
#define VIVALDI_APP_OBSERVER_H_

#import <Cocoa/Cocoa.h>

#include "chrome/browser/ui/browser.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace vivaldi {

class VivaldiAppObserver : public extensions::BrowserContextKeyedAPI,
                           public extensions::AppWindowRegistry::Observer {
 public:
  VivaldiAppObserver(content::BrowserContext* context);
  ~VivaldiAppObserver() override;

  static extensions::BrowserContextKeyedAPIFactory<VivaldiAppObserver>*
  GetFactoryInstance();

  // Convenience method to get the AlarmManager for a content::BrowserContext.
  static VivaldiAppObserver* Get(content::BrowserContext* browser_context);

  void SetCommand(NSInteger tag, Browser* browser);

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<VivaldiAppObserver>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VivaldiAppObserver"; }
  static const bool kServiceIsNULLWhileTesting = true;

  // AppWindowRegistry::Observer
  void OnAppWindowShown(extensions::AppWindow* app_window,
                        bool was_hidden) override;
  NSInteger tag_ = 0;
  Browser* browser_ = nullptr;
  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(VivaldiAppObserver);
};

}  // namespace vivaldi

#endif  // VIVALDI_APP_OBSERVER_H_
