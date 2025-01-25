// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include "components/permissions/vivaldi_permission_handler.h"
#include "components/permissions/vivaldi_permission_handler_base.h"

#include "components/permissions/permission_request_manager.h"

namespace permissions {

bool PermissionRequestManager::VivaldiHandlePermissionRequest() {
#if !BUILDFLAG(IS_ANDROID)
  // look into requests
  if (requests_.size() < 1)
    return false;

  auto request = requests_[0];
  auto isource = request_sources_map_.find(request);
  if (isource != request_sources_map_.end() &&
      vivaldi::permissions::HandlePermissionRequest(
          isource->second.requesting_frame_id, request)) {
    // Stops RestorePrompt from reaching our HandlePermissionRequest call (would lead to crash).
    current_request_prompt_disposition_.reset();
    // We set this to stop the logic in OnVisibilityChanged to try recreate the
    // view (would lead to crash).
    current_request_ui_to_use_.reset();

    return true;
  }
#endif
  return false;
}

}  // namespace permissions

namespace vivaldi {
namespace permissions {

void NotifyPermissionSet(const ::permissions::PermissionRequestID& id,
                         ContentSettingsType type,
                         ContentSetting setting) {
#if !BUILDFLAG(IS_ANDROID)
  VivaldiPermissionHandlerBase* instance = VivaldiPermissionHandlerBase::Get();
  if (instance) {
    instance->NotifyPermissionSet(id, type, setting);
  }
#endif
}

bool HandlePermissionRequest(
    const content::GlobalRenderFrameHostId& source_frame_id,
    ::permissions::PermissionRequest* request) {
#if !BUILDFLAG(IS_ANDROID)
  VivaldiPermissionHandlerBase* instance = VivaldiPermissionHandlerBase::Get();
  if (instance) {
    return instance->HandlePermissionRequest(source_frame_id, request);
  }
#endif
  return false;
}

}  // namespace permissions
}  // namespace vivaldi
