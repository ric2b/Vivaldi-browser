// Copyright (c) 2025 Vivaldi. All rights reserved.

#include "chrome/renderer/net/net_error_helper.h"
#include "chrome/renderer/net/net_error_page_controller.h"

void NetErrorHelper::OpenVivaldia() {
  GetRemoteNetErrorPageSupport()->OpenVivaldia();
}

void NetErrorPageController::OpenVivaldia() {
  if (delegate_)
    delegate_->OpenVivaldia();
}
