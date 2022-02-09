// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

void ResourceFetcher::setServeOnlyCachedResources(bool enable) {
  if (enable == onlyLoadServeCachedResources_)
    return;

  onlyLoadServeCachedResources_ = enable;
}

}  // namespace blink
