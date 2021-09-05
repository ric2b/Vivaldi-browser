// Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved.

#include "extensions/helper/vivaldi_frame_observer.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/render_view_host.h"
#include "renderer/vivaldi_render_messages.h"

namespace vivaldi {

WEB_CONTENTS_USER_DATA_KEY_IMPL(VivaldiFrameObserver)

VivaldiFrameObserver::VivaldiFrameObserver(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  host_zoom_map_ = content::HostZoomMap::GetForWebContents(web_contents);
}

VivaldiFrameObserver::~VivaldiFrameObserver() {}

bool VivaldiFrameObserver::OnMessageReceived(const IPC::Message& message,
                                             content::RenderFrameHost* render_frame_host) {
  // If both the old and the new RenderFrameHosts have focus, then we are
  // getting messages in the wrong order, which causes us to lose track of the
  // actual focused element. See VB-72174.
  if (web_contents()->GetFocusedFrame() != render_frame_host) {
    content::RenderFrameHostImpl* rfhi =
        static_cast<content::RenderFrameHostImpl*>(render_frame_host);
    if (rfhi->IsFocused()) {
      return false;
    }
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(VivaldiFrameObserver, message, render_frame_host)
  IPC_MESSAGE_HANDLER(VivaldiMsg_DidUpdateFocusedElementInfo,
                      OnDidUpdateFocusedElementInfo)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void VivaldiFrameObserver::OnDidUpdateFocusedElementInfo(std::string tagname,
                                                         std::string type,
                                                         bool editable,
                                                         std::string role) {
  focused_element_tagname_ = tagname;
  focused_element_type_ = type;
  focused_element_editable_ = editable;
  focused_element_role_ = role;
}

void VivaldiFrameObserver::GetFocusedElementInfo(std::string* tagname,
                                                 std::string* type,
                                                 bool* editable,
                                                 std::string* role) {
  *tagname = focused_element_tagname_;
  *type = focused_element_type_;
  *editable = focused_element_editable_;
  *role = focused_element_role_;
}

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
  blink::RendererPreferences* prefs =
      web_contents()->GetMutableRendererPrefs();
  renderer_preferences_util::UpdateFromSystemSettings(prefs, profile_);
  web_contents()->SyncRendererPrefs();
}

}  // namespace vivaldi
