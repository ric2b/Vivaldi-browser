// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/zoom/zoom_api.h"
#include "extensions/schema/zoom.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension_messages.h"

class Browser;

namespace extensions {

using content::WebContents;

ZoomAPI::ZoomAPI(content::BrowserContext* context)
  : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router)
    event_router->RegisterObserver(this,
        vivaldi::zoom::OnDefaultZoomChanged::kEventName);
}

ZoomAPI::~ZoomAPI() { }

static base::LazyInstance<BrowserContextKeyedAPIFactory<ZoomAPI> >
  g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ZoomAPI>* ZoomAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

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

ZoomEventRouter::ZoomEventRouter(content::BrowserContext* context)
  : profile_(Profile::FromBrowserContext(context)) {
  base::Closure renderer_callback = base::Bind(
    &ZoomEventRouter::DefaultZoomChanged, base::Unretained(this));

  default_zoom_level_subscription_ =
    profile_->GetZoomLevelPrefs()->RegisterDefaultZoomLevelCallback(
      renderer_callback);
}

ZoomEventRouter::~ZoomEventRouter() { }

void ZoomEventRouter::DefaultZoomChanged() {
  double zoom_level = profile_->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_level);

  scoped_ptr<base::ListValue> args =
    vivaldi::zoom::OnDefaultZoomChanged::Create(zoom_factor);
  DispatchEvent(vivaldi::zoom::OnDefaultZoomChanged::kEventName, std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void ZoomEventRouter::DispatchEvent(const std::string &event_name,
  scoped_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(profile_);
  if (event_router) {
    event_router->BroadcastEvent(make_scoped_ptr(
      new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
      event_name, std::move(event_args))));
  }
}

void ZoomEventRouter::Observe(
  int type,
  const content::NotificationSource& source,
  const content::NotificationDetails& details) {
}

ZoomSetVivaldiUIZoomFunction::ZoomSetVivaldiUIZoomFunction() {
}

ZoomSetVivaldiUIZoomFunction::~ZoomSetVivaldiUIZoomFunction() {
}

bool ZoomSetVivaldiUIZoomFunction::RunAsync() {
  Browser* browser = chrome::FindLastActive();

  scoped_ptr<vivaldi::zoom::SetVivaldiUIZoom::Params> params(
    vivaldi::zoom::SetVivaldiUIZoom::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebContents* web_contents =
    dispatcher()->GetAssociatedWebContents();

  ui_zoom::ZoomController* zoom_controller =
    ui_zoom::ZoomController::FromWebContents(web_contents);
  DCHECK(zoom_controller);

  double zoom_level = content::ZoomFactorToZoomLevel(params->zoom_factor);
  scoped_refptr<ExtensionZoomRequestClient> client(
    new ExtensionZoomRequestClient(extension()));
  zoom_controller->SetZoomLevelByClient(zoom_level, client);

  SendResponse(true);
  return true;
}

ZoomGetVivaldiUIZoomFunction::ZoomGetVivaldiUIZoomFunction() {
}

ZoomGetVivaldiUIZoomFunction::~ZoomGetVivaldiUIZoomFunction() {
}

bool ZoomGetVivaldiUIZoomFunction::RunAsync() {
  Browser* browser = chrome::FindLastActive();

  WebContents* web_contents =
    dispatcher()->GetAssociatedWebContents();

  if (web_contents == NULL) {
    results_ = vivaldi::zoom::GetVivaldiUIZoom::Results::Create(-1);
    SendResponse(true);
    return true;
  }

  ui_zoom::ZoomController* zoom_controller =
    ui_zoom::ZoomController::FromWebContents(web_contents);
  DCHECK(zoom_controller);

  double zoom_Level = zoom_controller->GetZoomLevel();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_Level);

  results_ = vivaldi::zoom::GetVivaldiUIZoom::Results::Create(zoom_factor);

  SendResponse(true);
  return true;
}

ZoomSetDefaultZoomFunction::ZoomSetDefaultZoomFunction() {
}

ZoomSetDefaultZoomFunction::~ZoomSetDefaultZoomFunction() {
}

bool ZoomSetDefaultZoomFunction::RunAsync() {
  scoped_ptr<vivaldi::zoom::SetDefaultZoom::Params> params(
    vivaldi::zoom::SetDefaultZoom::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  Profile* profile = GetProfile();
  double zoom_factor = content::ZoomFactorToZoomLevel(params->zoom_factor);
  profile->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(zoom_factor);
  return true;
}

ZoomGetDefaultZoomFunction::ZoomGetDefaultZoomFunction() {
}

ZoomGetDefaultZoomFunction::~ZoomGetDefaultZoomFunction() {
}

bool ZoomGetDefaultZoomFunction::RunAsync() {
  Profile* profile = GetProfile();
  double zoom_level = profile->GetZoomLevelPrefs()->GetDefaultZoomLevelPref();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_level);

  results_ = vivaldi::zoom::GetDefaultZoom::Results::Create(zoom_factor);
  SendResponse(true);
  return true;
}

}  // namespace extensions
