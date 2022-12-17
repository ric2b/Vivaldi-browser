// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#include "extensions/helper/vivaldi_frame_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_view_host.h"

namespace vivaldi {

WEB_CONTENTS_USER_DATA_KEY_IMPL(VivaldiFrameObserver);

VivaldiFrameObserver::VivaldiFrameObserver(content::WebContents* web_contents)
    : content::WebContentsUserData<VivaldiFrameObserver>(*web_contents),
      content::WebContentsObserver(web_contents) {
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

  Profile* profile_ =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  blink::RendererPreferences* prefs = web_contents()->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile_);
  web_contents()->SyncRendererPrefs();
}

}  // namespace vivaldi
