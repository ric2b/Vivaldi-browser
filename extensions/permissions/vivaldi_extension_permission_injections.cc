// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

/*
  This file handles per-extension injections for extensions that need special treatment
  from us (detection of vivaldi, specific features tailored for vivaldi integration, etc.)
*/

#include "chromium/extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/extension.h"

namespace {
constexpr char kProtonVpnId[] = "jplgfhpmjnbigmhklmmbgecoobifkmpa";
} // namespace

namespace extensions {

void VivaldiHandlePermissionInjections(PermissionsParser* parser, Extension* extension) {
  using extensions::mojom::APIPermissionID;

  if (extension->id() == kProtonVpnId) {
    // ProtonVPN has their own small extension that allows them to detect us and status of users.
    // Removing this would cause the extension to behave as on other chrome browsers.
    parser->AddAPIPermission(extension, APIPermissionID::kProtonVPN);
  }
}

} // namespace extensions
