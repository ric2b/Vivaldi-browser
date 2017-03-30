// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_
#define EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_

#include <vector>

#include "base/compiler_specific.h"
#include "extensions/common/permissions/permissions_provider.h"

namespace extensions {

// Registers the permissions used in Vivaldi with the PermissionsInfo global.
class VivaldiAPIPermissions : public PermissionsProvider {
 public:
  std::vector<APIPermissionInfo*> GetAllPermissions() const override;
  std::vector<PermissionsProvider::AliasInfo> GetAllAliases() const override;
};

}  // namespace extensions

#endif // EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_
