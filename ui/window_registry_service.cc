// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#include "ui/window_registry_service.h"

#include "base/containers/contains.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "ui/window_registry_service_factory.h"

namespace vivaldi {

// static
WindowRegistryService* WindowRegistryService::Get(
    content::BrowserContext* context) {
  return WindowRegistryServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context));
}

WindowRegistryService::WindowRegistryService() {}

WindowRegistryService::~WindowRegistryService() {}

void WindowRegistryService::AddWindow(VivaldiBrowserWindow* window,
                                      std::string window_key) {
  if (!base::Contains(named_windows_, window_key)) {
    named_windows_[window_key] = window;
  }
}

VivaldiBrowserWindow* WindowRegistryService::RemoveWindow(
    std::string window_key) {
  NamedWindowMap::iterator it = named_windows_.find(window_key);
  if (it == named_windows_.end()) {
    return nullptr;
  }
  VivaldiBrowserWindow* exsisting_window = it->second;
  named_windows_.erase(it);
  return exsisting_window;
}

VivaldiBrowserWindow* WindowRegistryService::GetNamedWindow(
    std::string window_key) {
  NamedWindowMap::iterator it = named_windows_.find(window_key);
  if (it == named_windows_.end()) {
    return nullptr;
  }
  return it->second;
}

}  // namespace vivaldi
