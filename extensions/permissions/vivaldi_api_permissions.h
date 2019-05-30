// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_
#define EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_

#include "base/containers/span.h"

#include "extensions/common/permissions/api_permission.h"

namespace extensions {
namespace vivaldi_api_permissions {

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos();

}  // namespace vivaldi_api_permissions
}  // namespace extensions

#endif  // EXTENSIONS_PERMISSIONS_VIVALDI_API_PERMISSIONS_H_
