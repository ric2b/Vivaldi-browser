// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_INJECTION_RENDERER_CONTENT_INJECTION_FRAME_STATE_H_
#define COMPONENTS_CONTENT_INJECTION_RENDERER_CONTENT_INJECTION_FRAME_STATE_H_

#include "base/memory/weak_ptr.h"
#include "components/content_injection/content_injection_types.h"
#include "components/content_injection/mojom/content_injection.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace blink {
class WebLocalFrame;
class WebDocumentLoader;
}  // namespace blink

namespace content_injection {

class FrameHandler : public content::RenderFrameObserver,
                     public content::RenderFrameObserverTracker<FrameHandler>,
                     public mojom::FrameHandler {
 public:
  FrameHandler(content::RenderFrame* render_frame,
               service_manager::BinderRegistry* registry);
  ~FrameHandler() override;
  FrameHandler(const FrameHandler&) = delete;
  FrameHandler& operator=(const FrameHandler&) = delete;

  void InjectScriptsForRunTime(mojom::ItemRunTime run_time);

  // Implementing mojom::FrameHandler
  void EnableStaticInjection(mojom::EnabledStaticInjectionPtr injection,
                             EnableStaticInjectionCallback callback) override;
  void DisableStaticInjection(const std::string& key,
                              DisableStaticInjectionCallback callback) override;

 private:
  void BindFrameHandlerReceiver(
      mojo::PendingReceiver<mojom::FrameHandler> receiver);

  void OnDestruct() override;
  void DidCreateNewDocument() override;

  void OnInjectionsReceived(mojom::InjectionsForFramePtr injections);

  bool AddStaticInjection(mojom::EnabledStaticInjectionPtr injection);
  bool RemoveStaticInjection(const std::string& key);

  void InjectPendingScripts();
  bool InjectScript(const std::string& key,
                    const std::string& content,
                    const mojom::InjectionItemMetadata& metadata);
  void InjectJS(const std::string& key, const std::string& content, int world_id);
  void InjectCSS(const std::string& key,
                 const std::string& content,
                 const mojom::StylesheetOrigin origin);
  void RemoveInjectedCSS(const std::string& key,
                         const mojom::StylesheetOrigin origin);

  mojo::Remote<mojom::FrameInjectionHelper> injection_helper_;
  mojom::InjectionsForFramePtr pending_injections_;
  std::set<std::string> injected_static_scripts_;
  std::optional<mojom::ItemRunTime> last_run_time_;
  mojo::ReceiverSet<mojom::FrameHandler> receivers_;
};

}  // namespace content_injection

#endif  // COMPONENTS_CONTENT_INJECTION_RENDERER_CONTENT_INJECTION_FRAME_STATE_H_
