// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "extensions/helper/vivaldi_frame_host_service_impl.h"

#include "components/sessions/content/session_tab_helper.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

#include "extensions/schema/pip_private.h"
#include "extensions/tools/vivaldi_tools.h"
#include "ui/content/vivaldi_tab_check.h"

namespace {

const char kFrameHostServiceKey[1] = "";

}  // namespace

VivaldiFrameHostServiceImpl::VivaldiFrameHostServiceImpl(
    content::RenderFrameHostImpl* frame_host)
    : frame_host_(frame_host) {}

VivaldiFrameHostServiceImpl::~VivaldiFrameHostServiceImpl() = default;

// static
void VivaldiFrameHostServiceImpl::BindHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<vivaldi::mojom::VivaldiFrameHostService> receiver) {
  auto* frame_host_impl =
      static_cast<content::RenderFrameHostImpl*>(frame_host);
  VivaldiFrameHostServiceImpl* service =
      static_cast<VivaldiFrameHostServiceImpl*>(
          frame_host_impl->GetUserData(kFrameHostServiceKey));
  if (!service) {
    auto unique_service =
        std::make_unique<VivaldiFrameHostServiceImpl>(frame_host_impl);
    service = unique_service.get();
    frame_host_impl->SetUserData(kFrameHostServiceKey,
                                 std::move(unique_service));
  }
  service->receiver_.reset();
  service->receiver_.Bind(std::move(receiver));
}

void VivaldiFrameHostServiceImpl::NotifyMediaElementAdded() {
  // TODO(igor@vivaldi.com): Consider broadcasting events originating in web
  // panels, not only tabs.
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame_host_);
  if (!web_contents)
    return;
  content::WebContents* tab_contents =
      VivaldiTabCheck::GetOuterVivaldiTab(web_contents);
  if (!tab_contents)
    return;

  sessions::SessionTabHelper* helper =
      sessions::SessionTabHelper::FromWebContents(tab_contents);
  if (!helper)
    return;

  SessionID::id_type tab_id = helper->session_id().id();
  ::vivaldi::BroadcastEvent(
      extensions::vivaldi::pip_private::OnVideoElementCreated::kEventName,
      extensions::vivaldi::pip_private::OnVideoElementCreated::Create(tab_id),
      tab_contents->GetBrowserContext());
}

void VivaldiFrameHostServiceImpl::DidChangeLoadProgressExtended(
    int64_t loaded_bytes_delta,
    int loaded_resource_delta,
    int started_resource_delta) {
  content::PageImpl& page = frame_host_->GetPage();
  page.vivaldi_update_load_counters(loaded_bytes_delta, loaded_resource_delta,
                                    started_resource_delta);
}
