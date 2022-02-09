// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_INJECTION_FRAME_INJECTION_HELPER_H_
#define COMPONENTS_CONTENT_INJECTION_FRAME_INJECTION_HELPER_H_

#include "components/content_injection/mojom/content_injection.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {
class RenderFrameHost;
}

namespace content_injection {
class FrameInjectionHelper : public mojom::FrameInjectionHelper {
 public:
  static void Create(
      content::RenderFrameHost* frame,
      mojo::PendingReceiver<mojom::FrameInjectionHelper> receiver);
  ~FrameInjectionHelper() override;

  // Implementing mojom::FrameInjectionHelper
  void GetInjections(const ::GURL& url,
                     GetInjectionsCallback callback) override;

 private:
  FrameInjectionHelper(int process_id, int frame_id);
  FrameInjectionHelper(const FrameInjectionHelper&) = delete;
  FrameInjectionHelper& operator=(const FrameInjectionHelper&) = delete;

  int process_id_;
  int frame_id_;
};

}  // namespace content_injection

#endif  // COMPONENTS_CONTENT_INJECTION_FRAME_INJECTION_HELPER_H_
