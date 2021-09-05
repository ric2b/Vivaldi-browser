// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/content_injection/frame_injection_helper.h"

#include "components/content_injection/content_injection_service_factory.h"
#include "components/content_injection/content_injection_service_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace content_injection {

// static
void FrameInjectionHelper::Create(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<mojom::FrameInjectionHelper> receiver) {
  std::unique_ptr<FrameInjectionHelper> frame_helper(new FrameInjectionHelper(
      frame->GetProcess()->GetID(), frame->GetRoutingID()));
  mojo::MakeSelfOwnedReceiver(std::move(frame_helper), std::move(receiver));
}

FrameInjectionHelper::FrameInjectionHelper(int process_id, int frame_id)
    : process_id_(process_id), frame_id_(frame_id) {}

FrameInjectionHelper::~FrameInjectionHelper() = default;

void FrameInjectionHelper::GetInjections(const ::GURL& url,
                                         GetInjectionsCallback callback) {
  content::RenderFrameHost* frame =
      content::RenderFrameHost::FromID(process_id_, frame_id_);
  if (!frame) {
    std::move(callback).Run(mojom::InjectionsForFrame::New());
    return;
  }

  ServiceImpl* service = static_cast<ServiceImpl*>(
      ServiceFactory::GetInstance()->GetForBrowserContext(
          frame->GetBrowserContext()));

  std::move(callback).Run(service->GetInjectionsForFrame(url, frame));
}

}  // namespace content_injection