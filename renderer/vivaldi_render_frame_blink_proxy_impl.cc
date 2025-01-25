// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "renderer/vivaldi_render_frame_blink_proxy_impl.h"

#ifndef BLINK_MOJO_IMPL
#define BLINK_MOJO_IMPL 1
#endif

#include "base/no_destructor.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"

#include "components/content_injection/renderer/content_injection_manager.h"
#include "components/request_filter/adblock_filter/mojom/adblock_cosmetic_filter.mojom-blink.h"
#include "renderer/mojo/vivaldi_frame_host_service.mojom-blink.h"
#include "renderer/vivaldi_frame_service_impl.h"

namespace {

const char kBlinkProxyKey[1] = "";

struct RenderFrameData : public base::SupportsUserData::Data {
  mojo::Remote<vivaldi::mojom::blink::VivaldiFrameHostService>
      frame_host_service;
  mojo::Remote<adblock_filter::mojom::blink::CosmeticFilter> cosmetic_filter;
};

RenderFrameData* GetRenderFrameData(blink::WebLocalFrame* web_frame,
                                    content::RenderFrame*& render_frame) {
  DCHECK(render_frame == nullptr);
  render_frame = content::RenderFrame::FromWebFrame(web_frame);
  if (!render_frame)
    return nullptr;
  return static_cast<RenderFrameData*>(
      render_frame->GetUserData(kBlinkProxyKey));
}

}  // namespace

// static
void VivaldiRenderFrameBlinkProxyImpl::InitProxy() {
  static base::NoDestructor<VivaldiRenderFrameBlinkProxyImpl> instance;
  DCHECK(GetProxy() == instance.get());
}

// static
void VivaldiRenderFrameBlinkProxyImpl::PrepareFrame(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry) {
  DCHECK(!render_frame->GetUserData(kBlinkProxyKey));
  InitProxy();
  auto data = std::make_unique<RenderFrameData>();
  render_frame->SetUserData(kBlinkProxyKey, std::move(data));
  VivaldiFrameServiceImpl::Register(render_frame);
  content_injection::Manager::GetInstance().OnFrameCreated(render_frame,
                                                           registry);
}

vivaldi::mojom::blink::VivaldiFrameHostService*
VivaldiRenderFrameBlinkProxyImpl::GetFrameHostService(
    blink::WebLocalFrame* web_frame) {
  content::RenderFrame* render_frame = nullptr;
  RenderFrameData* data = GetRenderFrameData(web_frame, render_frame);
  if (!data)
    return nullptr;
  if (!data->frame_host_service) {
    render_frame->GetBrowserInterfaceBroker().GetInterface(
        data->frame_host_service.BindNewPipeAndPassReceiver());
  }
  return data->frame_host_service.get();
}

adblock_filter::mojom::blink::CosmeticFilter*
VivaldiRenderFrameBlinkProxyImpl::GetCosmeticFilter(
    blink::WebLocalFrame* web_frame) {
  content::RenderFrame* render_frame = nullptr;
  RenderFrameData* data = GetRenderFrameData(web_frame, render_frame);
  if (!data)
    return nullptr;
  if (!data->cosmetic_filter) {
    render_frame->GetBrowserInterfaceBroker().GetInterface(
        data->cosmetic_filter.BindNewPipeAndPassReceiver());
  }
  return data->cosmetic_filter.get();
}
