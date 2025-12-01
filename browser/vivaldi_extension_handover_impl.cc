// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include <memory>

#include "browser/vivaldi_extension_handover_impl.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"

namespace vivaldi {

static std::unique_ptr<VivaldiExtensionHandoverImpl> handover_impl_ = nullptr;

void VivaldiExtensionHandoverImpl::CreateImpl() {
  if (!handover_impl_) {
    handover_impl_.reset(new VivaldiExtensionHandoverImpl());
  }

  VivaldiExtensionHandover::SetInstance(handover_impl_.get());
}

void VivaldiExtensionHandoverImpl::ExtensionActionUtil_SendIconLoaded(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const gfx::Image& image) {
  extensions::ExtensionActionUtil::SendIconLoaded(browser_context, extension_id,
                                                  image);
}

}  // namespace vivaldi
