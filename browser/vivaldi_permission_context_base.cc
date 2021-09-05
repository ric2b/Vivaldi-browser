// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "components/permissions/permission_context_base.h"

#include "app/vivaldi_apptools.h"
#include "base/callback.h"
#include "components/permissions/permission_request_id.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/guest_view/web_view/web_view_constants.h"
#endif

namespace permissions {

int PermissionContextBase::RemoveBridgeID(int bridge_id) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::map<int, int>::iterator bridge_itr =
      bridge_id_to_request_id_map_.find(bridge_id);

  if (bridge_itr == bridge_id_to_request_id_map_.end())
    return webview::kInvalidPermissionRequestID;

  int request_id = bridge_itr->second;
  bridge_id_to_request_id_map_.erase(bridge_itr);
#else
  int request_id = 0;
#endif
  return request_id;
}

void PermissionContextBase::OnPermissionRequestResponse(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    bool user_gesture,
    BrowserPermissionCallback callback,
    bool allowed,
    const std::string& user_input) {
  RemoveBridgeID(id.request_id());
  PermissionDecided(id, requesting_origin, embedding_origin,
                    std::move(callback),
                    allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

}  // namespace permissions
