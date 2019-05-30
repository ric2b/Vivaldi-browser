// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

#include "browser/vivaldi_app_observer.h"

#include "base/lazy_instance.h"
#include "chrome/browser/ui/browser_commands.h"
#include "ui/vivaldi_browser_window.h"

namespace vivaldi {

VivaldiAppObserver::VivaldiAppObserver(content::BrowserContext* context)
    : browser_context_(context) {
}

VivaldiAppObserver::~VivaldiAppObserver() {
}

static base::LazyInstance<
    extensions::BrowserContextKeyedAPIFactory<VivaldiAppObserver> >::
        DestructorAtExit g_factory = LAZY_INSTANCE_INITIALIZER;

// static
extensions::BrowserContextKeyedAPIFactory<VivaldiAppObserver>*
VivaldiAppObserver::GetFactoryInstance() {
  return g_factory.Pointer();
}

VivaldiAppObserver* VivaldiAppObserver::Get(
    content::BrowserContext* browser_context) {
  return extensions::BrowserContextKeyedAPIFactory<VivaldiAppObserver>::Get(
      browser_context);
}

void VivaldiAppObserver::SetCommand(NSInteger tag, Browser* browser) {
  browser_ = browser;
  tag_ = tag;
}

// AppWindowRegistry::Observer
void VivaldiAppObserver::OnWindowShown(VivaldiBrowserWindow* window,
                                       bool was_hidden) {
  if (browser_ && tag_ > 0) {
    chrome::ExecuteCommand(browser_, tag_);
    browser_ = nullptr;
    tag_ = 0;
  }
}

}  // namespace vivaldi
