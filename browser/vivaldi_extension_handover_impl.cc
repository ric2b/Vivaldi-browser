// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include <memory>

#include "base/no_destructor.h"
#include "browser/vivaldi_extension_handover_impl.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"

namespace vivaldi {

void VivaldiExtensionHandoverImpl::CreateImpl() {
  static base::NoDestructor<VivaldiExtensionHandoverImpl> handover_impl;
  VivaldiExtensionHandover::SetInstance(handover_impl.get());
}

void VivaldiExtensionHandoverImpl::ExtensionActionUtil_SendIconLoaded(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    const gfx::Image& image) {
  extensions::ExtensionActionUtil::SendIconLoaded(browser_context, extension_id,
                                                  image);
}

}  // namespace vivaldi
