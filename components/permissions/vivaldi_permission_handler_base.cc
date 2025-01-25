// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "components/permissions/vivaldi_permission_handler_base.h"

namespace vivaldi {
namespace permissions {

namespace {
VivaldiPermissionHandlerBase* g_handler = nullptr;
}  // namespace

VivaldiPermissionHandlerBase::VivaldiPermissionHandlerBase() {
  g_handler = this;
}

VivaldiPermissionHandlerBase::~VivaldiPermissionHandlerBase() {
  g_handler = nullptr;
}

// static
VivaldiPermissionHandlerBase* VivaldiPermissionHandlerBase::Get() {
  DCHECK(g_handler);
  return g_handler;
}

void VivaldiPermissionHandlerBase::NotifyPermissionSet(
    const ::permissions::PermissionRequestID& id,
    ContentSettingsType type,
    ContentSetting setting) {
  // base class. empty impl.
}

bool VivaldiPermissionHandlerBase::HandlePermissionRequest(
    const content::GlobalRenderFrameHostId& source_frame_id,
    ::permissions::PermissionRequest* request) {
  return false;
}

}  // namespace permissions
}  // namespace vivaldi
