// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_INJECTION_RENDERER_CONTENT_INJECTION_MANAGER_H_
#define COMPONENTS_CONTENT_INJECTION_RENDERER_CONTENT_INJECTION_MANAGER_H_

#include <map>
#include <string>

#include "base/memory/shared_memory_mapping.h"
#include "base/no_destructor.h"
#include "components/content_injection/content_injection_types.h"
#include "components/content_injection/mojom/content_injection.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace content {
class RenderFrame;
}

namespace content_injection {
class Manager : public mojom::Manager {
 public:
  static Manager& GetInstance();
  static void BindReceiver(mojo::PendingReceiver<mojom::Manager> receiver);

  const StaticInjectionItem* GetInjectionItem(const std::string& key);

  static void OnFrameCreated(content::RenderFrame* render_frame,
                             service_manager::BinderRegistry* registry);
  static void RunScriptsForRunTime(content::RenderFrame* frame,
                                   mojom::ItemRunTime run_time);

  // Implementing mojom::Manager
  void OnStaticContentUpdated(
      mojo::ScopedSharedBufferHandle static_content) override;
  void RegisterJavascriptWorldInfos(
      std::vector<mojom::JavascriptWorldRegistrationPtr> registrations)
      override;

 private:
  friend class base::NoDestructor<Manager>;

  Manager();
  ~Manager() override;
  Manager(const Manager&) = delete;
  Manager& operator=(const Manager&) = delete;

  std::map<std::string, StaticInjectionItem> static_injections_;
  base::ReadOnlySharedMemoryMapping static_content_mapping_;
  mojo::Receiver<mojom::Manager> receiver_{this};
};
}  // namespace content_injection

#endif  // COMPONENTS_CONTENT_INJECTION_RENDERER_CONTENT_INJECTION_MANAGER_H_
