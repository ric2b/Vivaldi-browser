// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/content_injection/renderer/content_injection_manager.h"

#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "base/pickle.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/content_injection/content_injection_types.h"
#include "components/content_injection/renderer/content_injection_frame_handler.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/web_isolated_world_info.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"

#include "third_party/blink/public/platform/web_url.h"

namespace content_injection {

Manager::Manager() = default;
Manager::~Manager() = default;

// static
Manager& Manager::GetInstance() {
  static base::NoDestructor<Manager> instance;
  return *instance;
}

// static
void Manager::BindReceiver(mojo::PendingReceiver<mojom::Manager> receiver) {
  auto& instance = GetInstance();
  instance.receiver_.Bind(std::move(receiver));
}

void Manager::OnStaticContentUpdated(
    mojo::ScopedSharedBufferHandle static_content) {
  base::ReadOnlySharedMemoryRegion static_content_region =
      mojo::UnwrapReadOnlySharedMemoryRegion(std::move(static_content));
  static_injections_.clear();

  // Create the shared memory mapping.
  static_content_mapping_ = static_content_region.Map();
  if (!static_content_region.IsValid())
    return;

  // First get the size of the memory block.
  const base::Pickle::Header* pickle_header =
      static_content_mapping_.GetMemoryAs<base::Pickle::Header>();
  if (!pickle_header)
    return;

  // Now read in the rest of the block.
  size_t pickle_size =
      sizeof(base::Pickle::Header) + pickle_header->payload_size;
  auto memory = static_content_mapping_.GetMemoryAsSpan<uint8_t>(pickle_size);

  if (!memory.size())
    return;

  base::Pickle pickle = base::Pickle::WithUnownedBuffer(memory);
  base::PickleIterator iter(pickle);

  int provider_count;
  CHECK(iter.ReadInt(&provider_count));
  CHECK_GE(provider_count, 0);

  for (int i = 0; i < provider_count; ++i) {
    int static_item_count;
    CHECK(iter.ReadInt(&static_item_count));
    CHECK_GE(static_item_count, 0);

    for (int j = 0; j < static_item_count; ++j) {
      std::string key;
      StaticInjectionItem item;
      CHECK(iter.ReadString(&key));
      int type;
      CHECK(iter.ReadInt(&type));
      item.metadata.type = static_cast<mojom::ItemType>(type);
      CHECK_GE(item.metadata.type, mojom::ItemType::kMinValue);
      CHECK_LE(item.metadata.type, mojom::ItemType::kMaxValue);

      int run_time;
      CHECK(iter.ReadInt(&run_time));
      item.metadata.run_time = static_cast<mojom::ItemRunTime>(run_time);
      CHECK_GE(item.metadata.run_time, mojom::ItemRunTime::kMinValue);
      CHECK_LE(item.metadata.run_time, mojom::ItemRunTime::kMaxValue);

      switch (item.metadata.type) {
        case mojom::ItemType::kJS: {
          int javascript_world_id;
          CHECK(iter.ReadInt(&javascript_world_id));
          if (javascript_world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
            CHECK_GE(javascript_world_id,
                     ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION);
            CHECK_LT(javascript_world_id,
                     ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION_END);
          }
          item.metadata.javascript_world_id = javascript_world_id;
          break;
        }
        case mojom::ItemType::kCSS: {
          int stylesheet_origin;
          CHECK(iter.ReadInt(&stylesheet_origin));
          item.metadata.stylesheet_origin =
              static_cast<mojom::StylesheetOrigin>(stylesheet_origin);
          CHECK_GE(item.metadata.stylesheet_origin,
                   mojom::StylesheetOrigin::kMinValue);
          CHECK_LE(item.metadata.stylesheet_origin,
                   mojom::StylesheetOrigin::kMaxValue);
          break;
        }
      }

      CHECK(iter.ReadStringPiece(&item.content));

      static_injections_.insert({key, item});
    }
  }
}

void Manager::RegisterJavascriptWorldInfos(
    std::vector<mojom::JavascriptWorldRegistrationPtr> registrations) {
  for (auto& registration : registrations) {
    CHECK_GE(registration->world_id,
             ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION);
    CHECK_LT(registration->world_id,
             ISOLATED_WORLD_ID_VIVALDI_CONTENT_INJECTION_END);

    blink::WebIsolatedWorldInfo info;
    info.security_origin =
        blink::WebSecurityOrigin::Create(registration->world_info->host_url);
    info.human_readable_name =
        blink::WebString::FromUTF8(registration->world_info->name);
    info.stable_id =
        blink::WebString::FromUTF8(registration->world_info->stable_id);

    info.content_security_policy =
        blink::WebString::FromUTF8(registration->world_info->csp);

    blink::SetIsolatedWorldInfo(registration->world_id, info);
  }
}

/*static*/
void Manager::OnFrameCreated(content::RenderFrame* render_frame,
                             service_manager::BinderRegistry* registry) {
  new FrameHandler(render_frame, registry);
}

/*static*/
void Manager::RunScriptsForRunTime(content::RenderFrame* frame,
                                   mojom::ItemRunTime run_time) {
  if (!frame)
    return;

  FrameHandler* frame_handler = FrameHandler::Get(frame);
  if (!frame_handler)
    return;

  frame_handler->InjectScriptsForRunTime(run_time);
}

const StaticInjectionItem* Manager::GetInjectionItem(const std::string& key) {
  const auto& item = static_injections_.find(key);

  if (item == static_injections_.end())
    return nullptr;
  return &item->second;
}

}  // namespace content_injection
