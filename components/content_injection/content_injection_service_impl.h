// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_SERVICE_IMPL_H_
#define COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_SERVICE_IMPL_H_

#include <set>

#include "components/content_injection/content_injection_service.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/system/buffer.h"

namespace content_injection {
class ServiceImpl : public Service,
                    public content::RenderProcessHostCreationObserver {
 public:
  explicit ServiceImpl(content::BrowserContext* browser_context);
  ~ServiceImpl() override;
  ServiceImpl(const ServiceImpl&) = delete;
  ServiceImpl& operator=(const ServiceImpl&) = delete;

  // Implementing Service
  std::optional<int> RegisterWorldForJSInjection(
      mojom::JavascriptWorldInfoPtr world_info) override;
  bool AddProvider(Provider* provider) override;
  void OnStaticContentChanged() override;
  bool RemoveProvider(Provider* provider) override;

  // Implementing content::RenderProcessHostCreationObserver
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;

  mojom::InjectionsForFramePtr GetInjectionsForFrame(
      const GURL& url,
      content::RenderFrameHost* frame);

 private:
  const raw_ptr<content::BrowserContext> browser_context_;

  std::set<Provider*> providers_;
  mojo::RemoteSet<mojom::Manager> managers_;
  std::vector<mojom::JavascriptWorldRegistrationPtr> world_registrations_;

  mojo::ScopedSharedBufferHandle last_static_content_buffer_;
  bool static_script_injections_up_to_date_ = false;
};
}  // namespace content_injection

#endif  // COMPONENTS_CONTENT_INJECTION_CONTENT_INJECTION_SERVICE_IMPL_H_