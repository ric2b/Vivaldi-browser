// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_
#define COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace payments::facilitated {

class FacilitatedPaymentsClient;

// Implementation of `FacilitatedPaymentsDriver` for Android/Desktop. It
// is owned by `ContentFacilitatedPaymentsFactory`.
// Each `ContentFacilitatedPaymentsDriver` is associated with exactly one
// `RenderFrameHost` and communicates with exactly one
// `FacilitatedPaymentsAgent` throughout its entire lifetime.
class ContentFacilitatedPaymentsDriver : public FacilitatedPaymentsDriver {
 public:
  ContentFacilitatedPaymentsDriver(
      FacilitatedPaymentsClient* client,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      content::RenderFrameHost* render_frame_host);
  ContentFacilitatedPaymentsDriver(const ContentFacilitatedPaymentsDriver&) =
      delete;
  ContentFacilitatedPaymentsDriver& operator=(
      const ContentFacilitatedPaymentsDriver&) = delete;
  ~ContentFacilitatedPaymentsDriver() override;

  // FacilitatedPaymentsDriver:
  void TriggerPixCodeDetection(
      base::OnceCallback<void(mojom::PixCodeDetectionResult,
                              const std::string&)> callback) override;

 private:
  // Lazily binds the agent.
  const mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent>& GetAgent();

  mojo::AssociatedRemote<mojom::FacilitatedPaymentsAgent> agent_;

  // The ID of the frame to which this driver is associated.
  const content::GlobalRenderFrameHostId render_frame_host_id_;
};

}  // namespace payments::facilitated

#endif  // COMPONENTS_FACILITATED_PAYMENTS_CONTENT_BROWSER_CONTENT_FACILITATED_PAYMENTS_DRIVER_H_
