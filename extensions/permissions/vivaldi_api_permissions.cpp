// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/permissions/vivaldi_api_permissions.h"

#include "extensions/common/permissions/api_permission.h"

namespace extensions {

std::vector<APIPermissionInfo*>
VivaldiAPIPermissions::GetAllPermissions() const {
  // WARNING: If you are modifying a permission message in this list, be sure to
  // add the corresponding permission message rule to
  // ChromePermissionMessageProvider::GetCoalescedPermissionMessages as well.
  APIPermissionInfo::InitInfo permissions_to_register[] = {
      // Register permissions for all extension types.
      {APIPermission::kAutoUpdate, "autoUpdate"},
      {APIPermission::kBookmarksPrivate, "bookmarksPrivate"},
      {APIPermission::kEditCommand, "editcommand" },
      {APIPermission::kExtensionActionUtils, "extensionActionUtils"},
      {APIPermission::kHistorySearch, "historySearch" },
      {APIPermission::kImportData, "importData"},
      {APIPermission::kNotes, "notes"},
      {APIPermission::kRuntimePrivate, "runtimePrivate" },
      {APIPermission::kSessionsPrivate, "sessionsPrivate" },
      {APIPermission::kSettings, "settings" },
      {APIPermission::kSavedPasswords, "savedpasswords"},
      {APIPermission::kShowMenu, "showMenu"},
      {APIPermission::kTabsPrivate, "tabsPrivate"},
      {APIPermission::kThumbnails, "thumbnails"},
      {APIPermission::kZoom, "zoom" },
      {APIPermission::kUtilities, "utilities"},
      {APIPermission::kWebViewPrivate, "webViewPrivate"},
  };

  std::vector<APIPermissionInfo*> permissions;

  for (size_t i = 0; i < arraysize(permissions_to_register); ++i)
    permissions.push_back(new APIPermissionInfo(permissions_to_register[i]));
  return permissions;
}

std::vector<PermissionsProvider::AliasInfo>
VivaldiAPIPermissions::GetAllAliases() const {
  // Register aliases.
  std::vector<PermissionsProvider::AliasInfo> aliases;
  return aliases;
}

}   // namespace extensions
