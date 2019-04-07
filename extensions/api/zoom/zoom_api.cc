// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/zoom/zoom_api.h"
#include "extensions/schema/zoom.h"

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "ui/vivaldi_browser_window.h"

class Browser;

namespace extensions {

using content::WebContents;

namespace {
// Helper
void SetUIZoomByWebContent(double zoom_level,
                           WebContents* web_contents,
                           const Extension* extension) {
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  DCHECK(zoom_controller);
  scoped_refptr<ExtensionZoomRequestClient> client(
      new ExtensionZoomRequestClient(extension));
  zoom_controller->SetZoomLevelByClient(zoom_level, client);
}
}  // namespace

ZoomEventRouter::ZoomEventRouter(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {
  base::Closure renderer_callback =
      base::Bind(&ZoomEventRouter::DefaultZoomChanged, base::Unretained(this));

  default_zoom_level_subscription_ =
      profile_->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
          renderer_callback);
}

ZoomEventRouter::~ZoomEventRouter() {}

void ZoomEventRouter::DefaultZoomChanged() {
  double zoom_level = profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_level);
  std::unique_ptr<base::ListValue> args =
      vivaldi::zoom::OnDefaultZoomChanged::Create(zoom_factor);
  DispatchEvent(vivaldi::zoom::OnDefaultZoomChanged::kEventName,
                std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void ZoomEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(profile_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ZoomAPI> >::DestructorAtExit g_factory_zoom =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ZoomAPI>* ZoomAPI::GetFactoryInstance() {
  return g_factory_zoom.Pointer();
}

ZoomAPI::ZoomAPI(content::BrowserContext* context) : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router)
    event_router->RegisterObserver(
        this, vivaldi::zoom::OnDefaultZoomChanged::kEventName);
}

ZoomAPI::~ZoomAPI() {}

void ZoomAPI::OnListenerAdded(const EventListenerInfo& details) {
  zoom_event_router_.reset(new ZoomEventRouter(browser_context_));
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router)
    event_router->UnregisterObserver(this);
}

void ZoomAPI::Shutdown() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router)
    event_router->UnregisterObserver(this);
}

void ZoomAPI::AddZoomObserver(Browser* browser) {
  WebContents* web_contents =
      static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
  if (web_contents) {
    auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
    if (zoom_controller) {
      zoom_controller->AddObserver(this);
    }
  }
}

void ZoomAPI::RemoveZoomObserver(Browser* browser) {
  WebContents* web_contents =
      static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
  if (web_contents) {
    auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
    if (zoom_controller) {
      zoom_controller->RemoveObserver(this);
    }
  }
}

void ZoomAPI::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  double zoom_factor = content::ZoomLevelToZoomFactor(data.new_zoom_level);
  std::unique_ptr<base::ListValue> args =
      vivaldi::zoom::OnUIZoomChanged::Create(zoom_factor);
  if (zoom_event_router_) {
    zoom_event_router_->DispatchEvent(
        vivaldi::zoom::OnUIZoomChanged::kEventName, std::move(args));
  }
}

bool ZoomSetVivaldiUIZoomFunction::RunAsync() {
  std::unique_ptr<vivaldi::zoom::SetVivaldiUIZoom::Params> params(
      vivaldi::zoom::SetVivaldiUIZoom::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  double zoom_level = content::ZoomFactorToZoomLevel(params->zoom_factor);
  for (auto* browser : *BrowserList::GetInstance()) {
    WebContents* web_contents =
        static_cast<VivaldiBrowserWindow*>(browser->window())->web_contents();
    SetUIZoomByWebContent(zoom_level, web_contents, extension());
  }

  SendResponse(true);
  return true;
}

bool ZoomGetVivaldiUIZoomFunction::RunAsync() {
  WebContents* web_contents = dispatcher()->GetAssociatedWebContents();

  if (web_contents == NULL) {
    results_ = vivaldi::zoom::GetVivaldiUIZoom::Results::Create(-1);
    SendResponse(true);
    return true;
  }

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  DCHECK(zoom_controller);

  double zoom_Level = zoom_controller->GetZoomLevel();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_Level);

  results_ = vivaldi::zoom::GetVivaldiUIZoom::Results::Create(zoom_factor);

  SendResponse(true);
  return true;
}

bool ZoomSetDefaultZoomFunction::RunAsync() {
  std::unique_ptr<vivaldi::zoom::SetDefaultZoom::Params> params(
      vivaldi::zoom::SetDefaultZoom::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  Profile* profile = GetProfile();
  if (profile->IsOffTheRecord()) {
    profile = profile->GetOriginalProfile();
  }
  double zoom_factor = content::ZoomFactorToZoomLevel(params->zoom_factor);

  content::StoragePartition* partition =
      profile->GetDefaultStoragePartition(profile);

  ChromeZoomLevelPrefs* zoom_prefs =
      static_cast<ChromeZoomLevelPrefs*>(partition->GetZoomLevelDelegate());
  zoom_prefs->SetDefaultZoomLevelPref(zoom_factor);

  SendResponse(true);
  return true;
}

bool ZoomGetDefaultZoomFunction::RunAsync() {
  Profile* profile = GetProfile();
  if (profile->IsOffTheRecord()) {
    profile = profile->GetOriginalProfile();
  }
  double zoom_level = profile->GetDefaultZoomLevelForProfile();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_level);

  results_ = vivaldi::zoom::GetDefaultZoom::Results::Create(zoom_factor);
  SendResponse(true);
  return true;
}

}  // namespace extensions
