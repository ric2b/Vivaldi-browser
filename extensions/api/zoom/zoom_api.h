// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_API_ZOOM_ZOOM_API_H_
#define EXTENSIONS_API_ZOOM_ZOOM_API_H_

#include <memory>
#include <string>

#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "components/zoom/zoom_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class ZoomAPI : public BrowserContextKeyedAPI,
                private EventRouter::Observer,
                public zoom::ZoomObserver {
 public:
  explicit ZoomAPI(content::BrowserContext* context);
  ~ZoomAPI() override;
  // KeyedService implementation.
  void Shutdown() override;

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<ZoomAPI>* GetFactoryInstance();

  static void AddZoomObserver(Browser* browser);
  static void RemoveZoomObserver(Browser* browser);

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<ZoomAPI>;

  // ZoomObserver implementation.
  void OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "ZoomAPI"; }
  static const bool kServiceRedirectedInIncognito = true;

  content::BrowserContext* browser_context_;

  // Listener for default zoom level. We create it lazily upon OnListenerAdded.
  // We cannot initialize it in the constructor as the profile is not fully
  // ready at that point.
  std::unique_ptr<ChromeZoomLevelPrefs::DefaultZoomLevelSubscription>
      default_zoom_level_subscription_;
};

class ZoomSetVivaldiUIZoomFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.setVivaldiUIZoom", ZOOM_SET_VIVALDI_UI_ZOOM)
  ZoomSetVivaldiUIZoomFunction() = default;

 private:
  ~ZoomSetVivaldiUIZoomFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomSetVivaldiUIZoomFunction);
};

class ZoomGetVivaldiUIZoomFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.getVivaldiUIZoom", ZOOM_GET_VIVALDI_UI_ZOOM)
  ZoomGetVivaldiUIZoomFunction() = default;

 private:
  ~ZoomGetVivaldiUIZoomFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomGetVivaldiUIZoomFunction);
};

class ZoomSetDefaultZoomFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.setDefaultZoom", ZOOM_SET_DEFAULT_ZOOM)
  ZoomSetDefaultZoomFunction() = default;

 private:
  ~ZoomSetDefaultZoomFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomSetDefaultZoomFunction);
};

class ZoomGetDefaultZoomFunction : public UIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("zoom.getDefaultZoom", ZOOM_GET_DEFAULT_ZOOM)
  ZoomGetDefaultZoomFunction() = default;

 private:
  ~ZoomGetDefaultZoomFunction() override = default;
  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(ZoomGetDefaultZoomFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_API_ZOOM_ZOOM_API_H_
