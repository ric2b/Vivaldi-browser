// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/helper/vivaldi_frame_observer.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(vivaldi::VivaldiFrameObserver);

namespace vivaldi {

VivaldiFrameObserver::VivaldiFrameObserver(content::WebContents *web_contents)
  : content::WebContentsObserver(web_contents) {
  host_zoom_map_ = content::HostZoomMap::GetForWebContents(web_contents);
}

VivaldiFrameObserver::~VivaldiFrameObserver() {}

void VivaldiFrameObserver::RenderFrameHostChanged(
  content::RenderFrameHost* old_host,
  content::RenderFrameHost* new_host) {
  content::HostZoomMap* new_host_zoom_map =
    content::HostZoomMap::GetForWebContents(web_contents());
  if (new_host_zoom_map == host_zoom_map_)
    return;

  host_zoom_map_ = new_host_zoom_map;

  Profile *profile_ = Profile::FromBrowserContext(
    web_contents()->GetBrowserContext());
  content::RendererPreferences* prefs =
    web_contents()->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(
    prefs, profile_, web_contents());
  web_contents()->GetRenderViewHost()->SyncRendererPrefs();
}

}  // namespace vivaldi
