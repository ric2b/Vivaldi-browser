// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"

namespace safe_browsing {
void RendererURLLoaderThrottle::OnURLLoaderThrottleProviderDestroyed() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_web_request_reporter_ = nullptr;
#endif
}

void RendererURLLoaderThrottle::SetVivaldiGuard(
    base::WeakPtr<vivaldi::ThrottleGuard> guard) {
  vivaldi_throttle_guard_ = guard;
}

}  // namespace safe_browsing
