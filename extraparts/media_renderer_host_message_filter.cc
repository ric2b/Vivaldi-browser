// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "extraparts/media_renderer_host_message_filter.h"

#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/browser/web_contents/web_contents_impl.h" // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/schema/pip_private.h"
#include "extensions/tools/vivaldi_tools.h"

namespace vivaldi {

MediaRendererHostMessageFilter::MediaRendererHostMessageFilter(
    int render_process_id, Profile* profile)
    : content::BrowserMessageFilter(VivaldiMsgStart),
      render_process_id_(render_process_id), profile_(profile) {}

MediaRendererHostMessageFilter::~MediaRendererHostMessageFilter() {}

bool MediaRendererHostMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(MediaRendererHostMessageFilter, message)
    IPC_MESSAGE_HANDLER(VivaldiMsg_MediaElementAddedEvent, OnMediaElementAdded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MediaRendererHostMessageFilter::OnMediaElementAdded() {
  content::RenderProcessHost* process_host =
      content::RenderProcessHost::FromID(render_process_id_);
  SessionID::id_type tab_id = 0;

  for (content::WebContents* contents :
       content::WebContentsImpl::GetAllWebContents()) {
    for (content::RenderFrameHost* frame : contents->GetAllFrames()) {
      if (frame->GetProcess() == process_host) {
        sessions::SessionTabHelper* helper =
            sessions::SessionTabHelper::FromWebContents(contents);
        if (helper) {
          const SessionID& session_id(helper->session_id());
          tab_id = session_id.id();
          break;
        }
      }
    }
  }
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::pip_private::OnVideoElementCreated::kEventName,
      extensions::vivaldi::pip_private::OnVideoElementCreated::Create(tab_id),
      profile_);
}

void MediaRendererHostMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    content::BrowserThread::ID* thread) {
  if (message.type() == VivaldiMsg_MediaElementAddedEvent::ID) {
    *thread = content::BrowserThread::UI;
  }
}

}  // namespace vivaldi
