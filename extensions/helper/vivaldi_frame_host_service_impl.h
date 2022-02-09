// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef EXTENSIONS_HELPER_VIVALDI_FRAME_HOST_SERVICE_IMPL_H_
#define EXTENSIONS_HELPER_VIVALDI_FRAME_HOST_SERVICE_IMPL_H_

#include "base/supports_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#include "renderer/mojo/vivaldi_frame_host_service.mojom.h"

namespace content {
class RenderFrameHost;
}

class VivaldiFrameHostServiceImpl
    : public vivaldi::mojom::VivaldiFrameHostService,
      public base::SupportsUserData::Data {
 public:
  VivaldiFrameHostServiceImpl(content::RenderFrameHost* frame_host);
  ~VivaldiFrameHostServiceImpl() override;

  static void BindHandler(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<vivaldi::mojom::VivaldiFrameHostService> receiver);

  // Mojo methods
  void NotifyMediaElementAdded() override;

 private:
  // Owner
  content::RenderFrameHost* const frame_host_;
  mojo::Receiver<vivaldi::mojom::VivaldiFrameHostService> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(VivaldiFrameHostServiceImpl);
};

#endif  //  EXTENSIONS_HELPER_VIVALDI_FRAME_HOST_SERVICE_IMPL_H_
