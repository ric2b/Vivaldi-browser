// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_ZOOM_ZOOM_API_H_
#define EXTENSIONS_API_ZOOM_ZOOM_API_H_

#include <memory>
#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "content/public/browser/notification_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"

namespace extensions {

class ZoomAPI;

class ZoomEventRouter : public content::NotificationObserver {
 public:
  explicit ZoomEventRouter(content::BrowserContext* context);
  ~ZoomEventRouter() override;

 private:
  void DefaultZoomChanged();
  // This method dispatches events to the extension message service.
  void DispatchEvent(const std::string& event_name,
                     std::unique_ptr<base::ListValue> event_args);
  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  std::unique_ptr<ChromeZoomLevelPrefs::DefaultZoomLevelSubscription>
      default_zoom_level_subscription_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ZoomEventRouter);
};

class ZoomAPI : public BrowserContextKeyedAPI, public EventRouter::Observer {
 public:
  explicit ZoomAPI(content::BrowserContext* context);
  ~ZoomAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ZoomAPI>* GetFactoryInstance();

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ZoomAPI>;
  content::BrowserContext* browser_context_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ZoomAPI"; }
  static const bool kServiceRedirectedInIncognito = true;

  // Created lazily upon OnListenerAdded.
  std::unique_ptr<ZoomEventRouter> zoom_event_router_;
};

class ZoomSetVivaldiUIZoomFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.setVivaldiUIZoom", ZOOM_SET_VIVALDI_UI_ZOOM)
  ZoomSetVivaldiUIZoomFunction();

 private:
  ~ZoomSetVivaldiUIZoomFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomSetVivaldiUIZoomFunction);
};

class ZoomGetVivaldiUIZoomFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.getVivaldiUIZoom", ZOOM_GET_VIVALDI_UI_ZOOM)
  ZoomGetVivaldiUIZoomFunction();

 private:
  ~ZoomGetVivaldiUIZoomFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomGetVivaldiUIZoomFunction);
};

class ZoomSetDefaultZoomFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.setDefaultZoom", ZOOM_SET_DEFAULT_ZOOM)
  ZoomSetDefaultZoomFunction();

 private:
  ~ZoomSetDefaultZoomFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomSetDefaultZoomFunction);
};

class ZoomGetDefaultZoomFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.getDefaultZoom", ZOOM_GET_DEFAULT_ZOOM)
  ZoomGetDefaultZoomFunction();

 private:
  ~ZoomGetDefaultZoomFunction() override;
  // ExtensionFunction:
  bool RunAsync() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomGetDefaultZoomFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_ZOOM_ZOOM_API_H_
