// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/request_filter/request_filter_registry.h"
#include "components/web_cache/browser/web_cache_manager.h"

namespace vivaldi {
RequestFilterRegistry::~RequestFilterRegistry() = default;

/*static*/
void RequestFilterRegistry::ClearCacheOnNavigation() {
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}
}