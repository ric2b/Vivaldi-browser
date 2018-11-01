// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_
#define EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "extensions/common/permissions/permissions_provider.h"

namespace extensions {

// Registers the permissions used in Vivaldi with the PermissionsInfo global.
class VivaldiAPIPermissions : public PermissionsProvider {
 public:
  std::vector<std::unique_ptr<APIPermissionInfo>> GetAllPermissions()
      const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_
