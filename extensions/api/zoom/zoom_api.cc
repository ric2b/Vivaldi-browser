// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/zoom/zoom_api.h"
#include "extensions/schema/zoom.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "third_party/blink/public/common/page/page_zoom.h"

#include "extensions/tools/vivaldi_tools.h"
#include "ui/vivaldi_browser_window.h"

class Browser;

namespace extensions {

using content::WebContents;

namespace {

// Helper
void SetUIZoomByWebContent(double zoom_level,
                           WebContents* web_contents,
                           const Extension* extension) {
  DCHECK(web_contents);
  zoom::ZoomController* zoom_controller =
      web_contents ? zoom::ZoomController::FromWebContents(web_contents)
                   : nullptr;
  DCHECK(zoom_controller);
  if (!zoom_controller)
    return;
  scoped_refptr<ExtensionZoomRequestClient> client(
      new ExtensionZoomRequestClient(extension));
  zoom_controller->SetZoomLevelByClient(zoom_level, client);
}

void DefaultZoomChanged(content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  double zoom_level = profile->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  double zoom_factor = blink::ZoomLevelToZoomFactor(zoom_level);
  ::vivaldi::BroadcastEvent(
      vivaldi::zoom::OnDefaultZoomChanged::kEventName,
      vivaldi::zoom::OnDefaultZoomChanged::Create(zoom_factor),
      browser_context);
}

}  // namespace

// static
BrowserContextKeyedAPIFactory<ZoomAPI>* ZoomAPI::GetFactoryInstance() {
  static base::LazyInstance<
      BrowserContextKeyedAPIFactory<ZoomAPI>>::DestructorAtExit factory =
      LAZY_INSTANCE_INITIALIZER;

  return factory.Pointer();
}

ZoomAPI::ZoomAPI(content::BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router)
    event_router->RegisterObserver(
        this, vivaldi::zoom::OnDefaultZoomChanged::kEventName);
}

ZoomAPI::~ZoomAPI() {}

void ZoomAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(!default_zoom_level_subscription_);
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->UnregisterObserver(this);
  }

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  default_zoom_level_subscription_ =
      profile->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
          base::BindRepeating(&DefaultZoomChanged, browser_context_));
}

void ZoomAPI::Shutdown() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->UnregisterObserver(this);
  }
}

// static
void ZoomAPI::AddZoomObserver(Browser* browser) {
  WebContents* web_contents =
      static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
  if (web_contents) {
    auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
    if (zoom_controller) {
      ZoomAPI* zoom_api = GetFactoryInstance()->Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
      DCHECK(zoom_api);
      zoom_controller->AddObserver(zoom_api);
    }
  }
}

// static
void ZoomAPI::RemoveZoomObserver(Browser* browser) {
  WebContents* web_contents =
      static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
  if (web_contents) {
    auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
    if (zoom_controller) {
      ZoomAPI* zoom_api = GetFactoryInstance()->Get(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
      DCHECK(zoom_api);
      if (zoom_api) {
        zoom_controller->RemoveObserver(zoom_api);
      }
    }
  }
}

void ZoomAPI::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  DCHECK(zoom_controller);
  zoom_controller->RemoveObserver(this);
}

void ZoomAPI::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  double zoom_factor = blink::ZoomLevelToZoomFactor(data.new_zoom_level);
  ::vivaldi::BroadcastEvent(vivaldi::zoom::OnUIZoomChanged::kEventName,
                            vivaldi::zoom::OnUIZoomChanged::Create(zoom_factor),
                            data.web_contents->GetBrowserContext());
}

ExtensionFunction::ResponseAction ZoomSetVivaldiUIZoomFunction::Run() {
  using vivaldi::zoom::SetVivaldiUIZoom::Params;
  namespace Results = vivaldi::zoom::SetVivaldiUIZoom::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  double zoom_level = blink::ZoomFactorToZoomLevel(params->zoom_factor);
  for (Browser* browser : *BrowserList::GetInstance()) {
    // Avoid crash if we have a non-Vivaldi window open (such as devtools for
    // our UI).
    if (browser->is_vivaldi()) {
      WebContents* web_contents =
          static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
      SetUIZoomByWebContent(zoom_level, web_contents, extension());
    }
  }

  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction ZoomGetVivaldiUIZoomFunction::Run() {
  namespace Results = vivaldi::zoom::GetVivaldiUIZoom::Results;

  // We rely on Chromium content::HostZoomMap that stores the zoom per host. So
  // this value is shared between all Vivaldi windows and we can use
  // GetSenderWebContents() to query for zoom even if that points to the hidden
  // portal page.
  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(Error("No sender WebContents"));
  }

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  DCHECK(zoom_controller);

  double zoom_Level = zoom_controller->GetZoomLevel();
  double zoom_factor = blink::ZoomLevelToZoomFactor(zoom_Level);

  return RespondNow(ArgumentList(Results::Create(zoom_factor)));
}

ExtensionFunction::ResponseAction ZoomSetDefaultZoomFunction::Run() {
  using vivaldi::zoom::SetDefaultZoom::Params;
  namespace Results = vivaldi::zoom::SetDefaultZoom::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord()) {
    profile = profile->GetOriginalProfile();
  }
  double zoom_factor = blink::ZoomFactorToZoomLevel(params->zoom_factor);

  content::StoragePartition* partition = profile->GetDefaultStoragePartition();

  ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<ChromeZoomLevelPrefs*>(partition->GetZoomLevelDelegate());
  zoom_prefs->SetDefaultZoomLevelPref(zoom_factor);

  return RespondNow(ArgumentList(Results::Create()));
}

ExtensionFunction::ResponseAction ZoomGetDefaultZoomFunction::Run() {
  namespace Results = vivaldi::zoom::GetDefaultZoom::Results;

  Profile* profile = Profile::FromBrowserContext(browser_context());
  if (profile->IsOffTheRecord()) {
    profile = profile->GetOriginalProfile();
  }
  double zoom_level = profile->GetDefaultZoomLevelForProfile();
  double zoom_factor = blink::ZoomLevelToZoomFactor(zoom_level);

  return RespondNow(ArgumentList(Results::Create(zoom_factor)));
}

}  // namespace extensions
