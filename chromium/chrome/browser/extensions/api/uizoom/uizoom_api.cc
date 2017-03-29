// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/extensions/api/uizoom/uizoom_api.h"
#include "chrome/common/extensions/api/uizoom.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension_messages.h"

#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "extensions/browser/extension_zoom_request_client.h"

class Browser;

namespace extensions {

using content::WebContents;

UizoomSetVivaldiUIZoomFunction::UizoomSetVivaldiUIZoomFunction(){
}

UizoomSetVivaldiUIZoomFunction::~UizoomSetVivaldiUIZoomFunction(){
}

bool UizoomSetVivaldiUIZoomFunction::RunAsync(){
  Browser* browser = chrome::FindLastActiveWithHostDesktopType(
    chrome::GetActiveDesktop());

  scoped_ptr<api::uizoom::SetVivaldiUIZoom::Params> params(
    api::uizoom::SetVivaldiUIZoom::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());

  WebContents* web_contents =
    browser->tab_strip_model()->GetActiveWebContents();

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

UizoomGetVivaldiUIZoomFunction::UizoomGetVivaldiUIZoomFunction(){
}

UizoomGetVivaldiUIZoomFunction::~UizoomGetVivaldiUIZoomFunction(){
}

bool UizoomGetVivaldiUIZoomFunction::RunAsync(){
  Browser* browser = chrome::FindLastActiveWithHostDesktopType(
    chrome::GetActiveDesktop());

  WebContents* web_contents =
    browser->tab_strip_model()->GetActiveWebContents();

  if (web_contents == NULL){
    results_ = api::uizoom::GetVivaldiUIZoom::Results::Create(-1);
    SendResponse(true);
    return true;
  }

  ui_zoom::ZoomController* zoom_controller =
    ui_zoom::ZoomController::FromWebContents(web_contents);
  DCHECK(zoom_controller);

  double zoom_Level = zoom_controller->GetZoomLevel();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_Level);

  results_ = api::uizoom::GetVivaldiUIZoom::Results::Create(zoom_factor);

  SendResponse(true);
  return true;
}

}  // namespace extensions
