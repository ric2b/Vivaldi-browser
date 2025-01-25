// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "extensions/permissions/vivaldi_api_permissions.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_info.h"

using extensions::mojom::APIPermissionID;

namespace extensions {
namespace vivaldi_api_permissions {

namespace {
// WARNING: If you are modifying a permission message in this list, be sure to
// add the corresponding permission message rule to
// ChromePermissionMessageProvider::GetCoalescedPermissionMessages as well.
constexpr APIPermissionInfo::InitInfo permissions_to_register[] = {
    // Register permissions for all extension types.
    {APIPermissionID::kAccessKeys, "accessKeys"},
    {APIPermissionID::kAutoUpdate, "autoUpdate"},
    {APIPermissionID::kBookmarkContextMenu, "bookmarkContextMenu"},
    {APIPermissionID::kBookmarksPrivate, "bookmarksPrivate"},
    {APIPermissionID::kCalendar, "calendar"},
    {APIPermissionID::kContacts, "contacts"},
    {APIPermissionID::kContentBlocking, "contentBlocking"},
    {APIPermissionID::kContextMenu, "contextMenu"},
    {APIPermissionID::kDevtoolsPrivate, "devtoolsPrivate"},
    {APIPermissionID::kEditCommand, "editcommand"},
    {APIPermissionID::kExtensionActionUtils, "extensionActionUtils"},
    {APIPermissionID::kHistoryPrivate, "historyPrivate"},
    {APIPermissionID::kImportData, "importData"},
    {APIPermissionID::kInfobars, "infobars"},
    {APIPermissionID::kMailPrivate, "mailPrivate"},
    {APIPermissionID::kMenuContent, "menuContent"},
    {APIPermissionID::kMenubar, "menubar"},
    {APIPermissionID::kMenubarMenu, "menubarMenu"},
    {APIPermissionID::kNotes, "notes"},
    {APIPermissionID::kPageActions, "pageActions"},
    {APIPermissionID::kPipPrivate, "pipPrivate"},
    {APIPermissionID::kPrefs, "prefs"},
    {APIPermissionID::kReadingListPrivate, "readingListPrivate"},
    {APIPermissionID::kRuntimePrivate, "runtimePrivate"},
    {APIPermissionID::kSearchEngines, "searchEngines"},
    {APIPermissionID::kSessionsPrivate, "sessionsPrivate"},
    {APIPermissionID::kSettings, "settings"},
    {APIPermissionID::kSavedPasswords, "savedpasswords"},
    {APIPermissionID::kSync, "sync"},
    {APIPermissionID::kTabsPrivate, "tabsPrivate"},
    {APIPermissionID::kThemePrivate, "themePrivate"},
    {APIPermissionID::kTranslateHistory, "translateHistory"},
    {APIPermissionID::kThumbnails, "thumbnails"},
    {APIPermissionID::kUtilities, "utilities"},
    {APIPermissionID::kVivaldiAccount, "vivaldiAccount"},
    {APIPermissionID::kWebViewPrivate, "webViewPrivate"},
    {APIPermissionID::kWindowPrivate, "windowPrivate"},
    {APIPermissionID::kZoom, "zoom"},
    {APIPermissionID::kOmniboxPrivate, "omniboxPrivate"},
};

}  // namespace

base::span<const APIPermissionInfo::InitInfo> GetPermissionInfos() {
  return base::make_span(permissions_to_register);
}

}  // namespace vivaldi_api_permissions
}  // namespace extensions
