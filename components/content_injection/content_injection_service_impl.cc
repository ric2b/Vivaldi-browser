// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/content_injection/content_injection_service_impl.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/pickle.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"

namespace content_injection {
ServiceImpl::ServiceImpl(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}
ServiceImpl::~ServiceImpl() = default;

std::optional<int> ServiceImpl::RegisterWorldForJSInjection(
    mojom::JavascriptWorldInfoPtr world_info) {
  static int g_next_world_id = ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION;
  static base::NoDestructor<std::map<std::string, int>> g_assigned_world_ids;

  if (g_next_world_id == ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION_END)
    return std::nullopt;

  const auto existing_id = g_assigned_world_ids->find(world_info->stable_id);

  const int world_id = (existing_id == g_assigned_world_ids->end())
                           ? g_next_world_id++
                           : existing_id->second;

  auto registration =
      mojom::JavascriptWorldRegistration::New(world_id, std::move(world_info));
  std::vector<mojom::JavascriptWorldRegistrationPtr> registrations;
  registrations.push_back(registration.Clone());

  for (auto& manager : managers_) {
    manager->RegisterJavascriptWorldInfos(std::move(registrations));
  }

  g_assigned_world_ids->emplace(registration->world_info->stable_id,
                                registration->world_id);

  world_registrations_.push_back(std::move(registration));
  return world_id;
}

bool ServiceImpl::AddProvider(Provider* provider) {
  if (!providers_.insert(provider).second)
    return false;
  OnStaticContentChanged();
  return true;
}

bool ServiceImpl::RemoveProvider(Provider* provider) {
  if (providers_.erase(provider) == 0)
    return false;
  OnStaticContentChanged();
  return true;
}

void ServiceImpl::OnRenderProcessHostCreated(
    content::RenderProcessHost* process_host) {
  mojo::Remote<mojom::Manager> manager;

  if (GetBrowserContextRedirectedInIncognito(
          process_host->GetBrowserContext()) != browser_context_)
    return;

  process_host->BindReceiver(manager.BindNewPipeAndPassReceiver());
  if (last_static_content_buffer_.is_valid())
    manager->OnStaticContentUpdated(last_static_content_buffer_->Clone(
        mojo::SharedBufferHandle::AccessMode::READ_ONLY));

  manager->RegisterJavascriptWorldInfos(mojo::Clone(world_registrations_));
  managers_.Add(std::move(manager));
}

void ServiceImpl::OnStaticContentChanged() {
  static_script_injections_up_to_date_ = false;

  base::Pickle serialized_static_content;
  serialized_static_content.WriteInt(
      base::checked_cast<int>(providers_.size()));
  for (auto* provider : providers_) {
    const std::map<std::string, StaticInjectionItem>& static_content =
        provider->GetStaticContent();
    serialized_static_content.WriteInt(
        base::checked_cast<int>(static_content.size()));

    for (const auto& injection_item : static_content) {
      serialized_static_content.WriteString(injection_item.first);
      serialized_static_content.WriteInt(
          static_cast<int>(injection_item.second.metadata.type));
      serialized_static_content.WriteInt(
          static_cast<int>(injection_item.second.metadata.run_time));

      switch (injection_item.second.metadata.type) {
        case mojom::ItemType::kJS:
          if (injection_item.second.metadata.javascript_world_id !=
              content::ISOLATED_WORLD_ID_GLOBAL) {
            CHECK_GE(injection_item.second.metadata.javascript_world_id,
                     ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION);
            CHECK_LT(injection_item.second.metadata.javascript_world_id,
                     ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION_END);
          }
          serialized_static_content.WriteInt(static_cast<int>(
              injection_item.second.metadata.javascript_world_id));
          break;
        case mojom::ItemType::kCSS:
          serialized_static_content.WriteInt(static_cast<int>(
              injection_item.second.metadata.stylesheet_origin));
          break;
      }

      serialized_static_content.WriteString(injection_item.second.content);
    }
  }

  base::MappedReadOnlyRegion shared_memory =
      base::ReadOnlySharedMemoryRegion::Create(
          serialized_static_content.size());
  if (!shared_memory.IsValid())
    return;

  // Copy the pickle to shared memory.
  memcpy(shared_memory.mapping.memory(), serialized_static_content.data(),
         serialized_static_content.size());

  mojo::ScopedSharedBufferHandle static_content_buffer =
      mojo::WrapReadOnlySharedMemoryRegion(std::move(shared_memory.region));
  if (!static_content_buffer->is_valid())
    return;

  for (auto& manager : managers_) {
    manager->OnStaticContentUpdated(static_content_buffer->Clone(
        mojo::SharedBufferHandle::AccessMode::READ_ONLY));
  }

  last_static_content_buffer_ = std::move(static_content_buffer);
  static_script_injections_up_to_date_ = true;
}

mojom::InjectionsForFramePtr ServiceImpl::GetInjectionsForFrame(
    const GURL& url,
    content::RenderFrameHost* frame) {
  mojom::InjectionsForFramePtr injections = mojom::InjectionsForFrame::New();

  for (Provider* provider : providers_) {
    mojom::InjectionsForFramePtr provider_injections =
        provider->GetInjectionsForFrame(url, frame);

#if DCHECK_IS_ON
    for (const auto& static_injection :
         provider_injections->static_injections) {
      DCHECK_LT(static_injection->placeholder_replacements, 10U.size())
    }
#endif

    injections->dynamic_injections.insert(
        injections->dynamic_injections.end(),
        std::make_move_iterator(
            provider_injections->dynamic_injections.begin()),
        std::make_move_iterator(provider_injections->dynamic_injections.end()));
    injections->static_injections.insert(
        injections->static_injections.end(),
        std::make_move_iterator(provider_injections->static_injections.begin()),
        std::make_move_iterator(provider_injections->static_injections.end()));
  }

  return injections;
}

}  // namespace content_injection