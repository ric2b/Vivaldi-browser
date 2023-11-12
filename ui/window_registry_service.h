// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef UI_WINDOW_REGISTRY_SERVICE_H_
#define UI_WINDOW_REGISTRY_SERVICE_H_

#include <map>
#include <string>
#include "base/containers/id_map.h"
#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

class VivaldiBrowserWindow;

namespace vivaldi {

class WindowRegistryService : public KeyedService {
 public:
  typedef std::map<std::string, raw_ptr<VivaldiBrowserWindow>> NamedWindowMap;

  WindowRegistryService();
  ~WindowRegistryService() override;
  WindowRegistryService(const WindowRegistryService&) = delete;
  WindowRegistryService& operator=(const WindowRegistryService&) = delete;

  void AddWindow(VivaldiBrowserWindow* window, std::string window_key);
  VivaldiBrowserWindow* RemoveWindow(std::string window_key);
  VivaldiBrowserWindow* GetNamedWindow(std::string window_key);

  static WindowRegistryService* Get(content::BrowserContext* context);

 private:
  // A list of all created windows with an id.
  NamedWindowMap named_windows_;
};

}  // namespace vivaldi

#endif  // UI_WINDOW_REGISTRY_SERVICE_H_
