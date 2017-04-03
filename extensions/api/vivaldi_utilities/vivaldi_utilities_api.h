//
// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved.
//

#ifndef EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
#define EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_

#include <map>
#include <string>

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/schema/vivaldi_utilities.h"

class Browser;

namespace extensions {

class VivaldiUtilitiesEventRouter {
 public:
    explicit VivaldiUtilitiesEventRouter(Profile* profile);
    ~VivaldiUtilitiesEventRouter();

    // Helper to actually dispatch an event to extension listeners.
    void DispatchEvent(const std::string& event_name,
                       std::unique_ptr<base::ListValue> event_args);
 private:
    content::BrowserContext* browser_context_;
    DISALLOW_COPY_AND_ASSIGN(VivaldiUtilitiesEventRouter);
};

class VivaldiUtilitiesAPI : public BrowserContextKeyedAPI,
                            public extensions::AppWindowRegistry::Observer,
                            public EventRouter::Observer {
 public:
  explicit VivaldiUtilitiesAPI(content::BrowserContext* context);
  ~VivaldiUtilitiesAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>*
  GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // Returns the window id based on the given app window id.
  Browser* FindBrowserFromAppWindowId(const std::string& appwindow_id);

  // Fires an event to JS with the active device that triggers scrolling.
  // 1: Mouse, 2: Trackpad 3: Inertial"
  void ScrollType(int scrollType);

  // Maps the given app window id to a window id.
  void MapAppWindowIdToWindowId(const std::string& appwindow_id, int window_id);

  // AppWindowRegistry::Observer
  void OnAppWindowActivated(extensions::AppWindow* app_window) override;
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

 private:
  friend class BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>;

  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "UtilitiesAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  // Maps the window id of the Browser window to a AppWindow id
  typedef std::map<std::string, int> WindowIdToAppWindowId;
  WindowIdToAppWindowId appwindow_id_to_window_id_;

  std::unique_ptr<VivaldiUtilitiesEventRouter> event_router_;
};


class UtilitiesIsTabInLastSessionFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isTabInLastSession",
                             UTILITIES_ISTABINLASTSESSION)
  UtilitiesIsTabInLastSessionFunction();

 protected:
  ~UtilitiesIsTabInLastSessionFunction() override;

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsTabInLastSessionFunction);
};

class UtilitiesIsUrlValidFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.isUrlValid", UTILITIES_ISURLVALID)
  UtilitiesIsUrlValidFunction();

 protected:
  ~UtilitiesIsUrlValidFunction() override;

  // ExtensionFunction:
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesIsUrlValidFunction);
};

class UtilitiesClearAllRecentlyClosedSessionsFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.clearAllRecentlyClosedSessions",
                             UTILITIES_CLEARALLRECENTLYCLOSEDSESSIONS)
  UtilitiesClearAllRecentlyClosedSessionsFunction() = default;

 protected:
  ~UtilitiesClearAllRecentlyClosedSessionsFunction() override;

  bool RunAsync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesClearAllRecentlyClosedSessionsFunction);
};

class UtilitiesMapFocusAppWindowToWindowIdFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.mapFocusAppWindowToWindowId",
                             UTILITIES_MAPFOCUSAPPWINDOWTOWINDOWIDFUNCTION)
  UtilitiesMapFocusAppWindowToWindowIdFunction();

 protected:
  ~UtilitiesMapFocusAppWindowToWindowIdFunction() override;

  // ExtensionFunction:
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesMapFocusAppWindowToWindowIdFunction);
};

class UtilitiesBasicPrintFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.basicPrint",
                             UTILITIES_BASICPRINT)
  UtilitiesBasicPrintFunction();

  bool RunAsync() override;
 protected:
  ~UtilitiesBasicPrintFunction() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesBasicPrintFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
