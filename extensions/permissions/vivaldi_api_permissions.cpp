// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/permissions/vivaldi_api_permissions.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/api_permission.h"

namespace extensions {

std::vector<std::unique_ptr<APIPermissionInfo>>
VivaldiAPIPermissions::GetAllPermissions() const {
  // WARNING: If you are modifying a permission message in this list, be sure to
  // add the corresponding permission message rule to
  // ChromePermissionMessageProvider::GetCoalescedPermissionMessages as well.
  APIPermissionInfo::InitInfo permissions_to_register[] = {
      // Register permissions for all extension types.
      {APIPermission::kAutoUpdate, "autoUpdate"},
      {APIPermission::kBookmarksPrivate, "bookmarksPrivate"},
      {APIPermission::kCalendar, "calendar"},
      {APIPermission::kContacts, "contacts" },
      {APIPermission::kDevtoolsPrivate, "devtoolsPrivate"},
      {APIPermission::kEditCommand, "editcommand"},
      {APIPermission::kExtensionActionUtils, "extensionActionUtils"},
      {APIPermission::kHistoryPrivate, "historyPrivate"},
      {APIPermission::kImportData, "importData"},
      {APIPermission::kInfobars, "infobars" },
      {APIPermission::kMailPrivate, "mailPrivate"},
      {APIPermission::kNotes, "notes"},
      {APIPermission::kPrefs, "prefs"},
      {APIPermission::kRuntimePrivate, "runtimePrivate"},
      {APIPermission::kSessionsPrivate, "sessionsPrivate"},
      {APIPermission::kSettings, "settings"},
      {APIPermission::kSavedPasswords, "savedpasswords"},
      {APIPermission::kShowMenu, "showMenu"},
      {APIPermission::kSync, "sync"},
      {APIPermission::kTabsPrivate, "tabsPrivate"},
      {APIPermission::kThumbnails, "thumbnails"},
      {APIPermission::kUtilities, "utilities"},
      {APIPermission::kWebViewPrivate, "webViewPrivate"},
      {APIPermission::kWindowPrivate, "windowPrivate"},
      {APIPermission::kZoom, "zoom"},
  };

  std::vector<std::unique_ptr<APIPermissionInfo>> permissions;

  for (size_t i = 0; i < arraysize(permissions_to_register); ++i)
    permissions.push_back(
        base::WrapUnique(new APIPermissionInfo(permissions_to_register[i])));
  return permissions;
}

}  // namespace extensions
