// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/pressure_service_for_worker.h"

#include "content/browser/compute_pressure/web_contents_pressure_manager_proxy.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/shared_worker_host.h"

namespace content {

template <typename WorkerHost>
bool PressureServiceForWorker<WorkerHost>::ShouldDeliverUpdate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // https://www.w3.org/TR/compute-pressure/#dfn-owning-document-set
  // https://www.w3.org/TR/compute-pressure/#dfn-may-receive-data
  if constexpr (std::is_same_v<WorkerHost, DedicatedWorkerHost>) {
    auto* rfh = RenderFrameHostImpl::FromID(
        worker_host_->GetAncestorRenderFrameHostId());
    return HasImplicitFocus(rfh);
  } else if constexpr (std::is_same_v<WorkerHost, SharedWorkerHost>) {
    if (base::ranges::any_of(
            worker_host_->GetRenderFrameIDsForWorker(), [](const auto& id) {
              return HasImplicitFocus(RenderFrameHostImpl::FromID(id));
            })) {
      return true;
    }
  }
  return false;
}

template <typename WorkerHost>
std::optional<base::UnguessableToken>
PressureServiceForWorker<WorkerHost>::GetTokenFor(
    device::mojom::PressureSource source) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if constexpr (std::is_same_v<WorkerHost, DedicatedWorkerHost>) {
    const auto* web_contents =
        WebContents::FromRenderFrameHost(RenderFrameHostImpl::FromID(
            worker_host_->GetAncestorRenderFrameHostId()));
    if (const auto* pressure_manager_proxy =
            WebContentsPressureManagerProxy::FromWebContents(web_contents)) {
      return pressure_manager_proxy->GetTokenFor(source);
    }
  }

  // Shared workers always return std::nullopt, as there is no one
  // corresponding WebContents instance to retrieve a
  // WebContentsPressureManagerProxy instance from.
  return std::nullopt;
}

template class EXPORT_TEMPLATE_DEFINE(CONTENT_EXPORT)
    PressureServiceForWorker<DedicatedWorkerHost>;
template class EXPORT_TEMPLATE_DEFINE(CONTENT_EXPORT)
    PressureServiceForWorker<SharedWorkerHost>;

}  // namespace content
