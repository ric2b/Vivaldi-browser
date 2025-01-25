// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef PERMISSIONS_VIVALDI_PERMISSION_HANDLER_IMPL_H
#define PERMISSIONS_VIVALDI_PERMISSION_HANDLER_IMPL_H

#include "chromium/components/content_settings/core/common/content_settings.h"
#include "components/permissions/vivaldi_permission_handler_base.h"

namespace permissions {
class PermissionRequest;
class PermissionRequestID;
}  // namespace permissions

namespace vivaldi {
namespace permissions {

/** This is UI only (ie. no unittest) permission handler implementation, spawned
 * at vivaldi startup, handles the bridging from Permission request sources to
 * our permission prompts in JS.
 * @note In unit tests, this class is not spawned, as the chromium instance
 * itself isn't and we're dependent on that. This is handled by
 * vivaldi::NotifyPermissionSet and vivaldi::HandlePermissionRequest functions
 * (noop in no-instance situations).
 */
class VivaldiPermissionHandlerImpl : public VivaldiPermissionHandlerBase {
 public:
  VivaldiPermissionHandlerImpl() = default;

  // Mostly used for initialization.
  static VivaldiPermissionHandlerImpl* GetInstance();

  /// Called when permission changed (ALLOW/DENY, etc) from chromium, forwards
  /// the events to JS.
  void NotifyPermissionSet(const ::permissions::PermissionRequestID& id,
                           ContentSettingsType type,
                           ContentSetting setting) override;

  /** Called to handle a queued permission request via our overrides (and not by
   * builtin chromium dialog).
   * @return true if the request was handled by the override */
  bool HandlePermissionRequest(
      const content::GlobalRenderFrameHostId& source_frame_id,
      ::permissions::PermissionRequest* request) override;
};

}  // namespace permissions
}  // namespace vivaldi

#endif /* PERMISSIONS_VIVALDI_PERMISSION_HANDLER_IMPL_H */
