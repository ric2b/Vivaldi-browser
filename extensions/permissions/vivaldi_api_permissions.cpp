// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/permissions/vivaldi_api_permissions.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"

namespace extensions {
namespace vivaldi_api_permissions {

namespace {
// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetCoalescedPermissionMessages as well.
constexpr APIPermissionInfo::InitInfo permissions_to_register[] = {
    // Register permissions for all extension types.
    {APIPermission::kAccessKeys, "accessKeys"},
    {APIPermission::kAutoUpdate, "autoUpdate"},
    {APIPermission::kBookmarkContextMenu, "bookmarkContextMenu"},
    {APIPermission::kBookmarksPrivate, "bookmarksPrivate"},
    {APIPermission::kCalendar, "calendar"},
    {APIPermission::kContacts, "contacts"},
    {APIPermission::kDevtoolsPrivate, "devtoolsPrivate"},
    {APIPermission::kEditCommand, "editcommand"},
    {APIPermission::kExtensionActionUtils, "extensionActionUtils"},
    {APIPermission::kHistoryPrivate, "historyPrivate"},
    {APIPermission::kImportData, "importData"},
    {APIPermission::kInfobars, "infobars"},
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
    {APIPermission::kVivaldiAccount, "vivaldiAccount"},
    {APIPermission::kWebViewPrivate, "webViewPrivate"},
    {APIPermission::kWindowPrivate, "windowPrivate"},
    {APIPermission::kZoom, "zoom"},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

}  // namespace vivaldi_api_permissions
}  // namespace extensions
