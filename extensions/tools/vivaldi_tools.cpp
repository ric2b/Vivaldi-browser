// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include <memory>
#include <string>
#include <utility>

#include "extensions/tools/vivaldi_tools.h"

#include "base/time/time.h"
#include "content/public/common/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "extensions/browser/event_router.h"

namespace vivaldi {

Browser* FindVivaldiBrowser() {
  BrowserList* browser_list_impl = BrowserList::GetInstance();
  if (browser_list_impl && browser_list_impl->size() > 0)
    return browser_list_impl->get(0);
  return NULL;
}

void BroadcastEvent(const std::string& eventname,
                    std::unique_ptr<base::ListValue> args,
                    content::BrowserContext* context) {
  std::unique_ptr<extensions::Event> event(new extensions::Event(
    extensions::events::VIVALDI_EXTENSION_EVENT, eventname, std::move(args)));
  extensions::EventRouter* event_router = extensions::EventRouter::Get(context);
  if (event_router) {
    event_router->BroadcastEvent(std::move(event));
  }
}  // namespace

// Start chromium copied code
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

base::Time GetTime(double ms_from_epoch) {
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  return (seconds_from_epoch == 0)
    ? base::Time::UnixEpoch()
    : base::Time::FromDoubleT(seconds_from_epoch);
}

blink::WebFloatPoint FromUICoordinates(content::WebContents* web_contents,
                                       blink::WebFloatPoint p) {
  // Account for the zoom factor in the UI.
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller)
    return p;
  double zoom_factor =
      content::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  return blink::WebFloatPoint(p.x * zoom_factor, p.y * zoom_factor);
}

blink::WebFloatPoint ToUICoordinates(content::WebContents* web_contents,
                                     blink::WebFloatPoint p) {
  // Account for the zoom factor in the UI.
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller)
    return p;
  double zoom_factor =
      content::ZoomLevelToZoomFactor(zoom_controller->GetZoomLevel());
  return blink::WebFloatPoint(p.x / zoom_factor, p.y / zoom_factor);
}

}  // namespace vivaldi
