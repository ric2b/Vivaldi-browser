// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"

namespace extensions {

void MimeHandlerViewContainerManager::WillDetach(
    blink::DetachReason detach_reason) {
  OnDestruct();
}

}  // namespace extensions
