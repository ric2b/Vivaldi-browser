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
#include "extensions/schema/vivaldi_utilities.h"

class Browser;

namespace extensions {

class VivaldiUtilitiesAPI : public BrowserContextKeyedAPI,
                            public extensions::AppWindowRegistry::Observer {
 public:
  explicit VivaldiUtilitiesAPI(content::BrowserContext* context);
  ~VivaldiUtilitiesAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<VivaldiUtilitiesAPI>*
  GetFactoryInstance();

  // Returns the window id based on the given app window id.
  Browser* FindBrowserFromAppWindowId(const std::string& appwindow_id);

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

  // Maps the window id of the Browser window to a AppWindow id
  typedef std::map<std::string, int> WindowIdToAppWindowId;
  WindowIdToAppWindowId appwindow_id_to_window_id_;
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

class UtilitiesGetAvailablePageEncodingsFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("utilities.getAvailablePageEncodings",
                             UTILITIES_GETAVAILABLEPAGEENCODINGS)
  UtilitiesGetAvailablePageEncodingsFunction() = default;

 protected:
  ~UtilitiesGetAvailablePageEncodingsFunction() override;

  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UtilitiesGetAvailablePageEncodingsFunction);
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

}  // namespace extensions

#endif  // EXTENSIONS_API_VIVALDI_UTILITIES_VIVALDI_UTILITIES_API_H_
