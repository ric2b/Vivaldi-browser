// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef COMONENTS_PERMISSIONS_VIVALDI_PERMISSION_HANDLER_H
#define COMONENTS_PERMISSIONS_VIVALDI_PERMISSION_HANDLER_H

#include "chromium/components/content_settings/core/common/content_settings.h"

namespace content {
struct GlobalRenderFrameHostId;
} // namespace content

namespace permissions {
class PermissionRequest;
class PermissionRequestID;
} // namespace permissions

namespace vivaldi {
namespace permissions {

/** Called on every notification change. Allows the permission icons to be synchronized. */
void NotifyPermissionSet(const ::permissions::PermissionRequestID& id,
                         ContentSettingsType type,
                         ContentSetting setting);

/** Tries to handle the permission request on vivaldi side.
 * @returns true if permission request will be handled by vivaldi, false otherwise */
bool HandlePermissionRequest(
    const content::GlobalRenderFrameHostId& source_frame_id,
    ::permissions::PermissionRequest* request);

} // namespace permissions
} // namespace vivaldi

#endif /* COMONENTS_PERMISSIONS_VIVALDI_PERMISSION_HANDLER_H */
