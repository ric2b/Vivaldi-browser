// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_
#define EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_

namespace extensions {

void SkipVivaldiAPIPermissions(extensions::APIPermissionSet& skip) {
  // Skip vivaldi permissions in

  skip.insert(APIPermission::kAutoUpdate);
  skip.insert(APIPermission::kBookmarksPrivate);
  skip.insert(APIPermission::kCalendar);
  skip.insert(APIPermission::kContacts);
  skip.insert(APIPermission::kDevtoolsPrivate);
  skip.insert(APIPermission::kEditCommand);
  skip.insert(APIPermission::kExtensionActionUtils);
  skip.insert(APIPermission::kHistoryPrivate);
  skip.insert(APIPermission::kImportData);
  skip.insert(APIPermission::kInfobars);
  skip.insert(APIPermission::kMailPrivate);
  skip.insert(APIPermission::kNotes);
  skip.insert(APIPermission::kPrefs);
  skip.insert(APIPermission::kRuntimePrivate);
  skip.insert(APIPermission::kSavedPasswords);
  skip.insert(APIPermission::kSessionsPrivate);
  skip.insert(APIPermission::kSettings);
  skip.insert(APIPermission::kShowMenu);
  skip.insert(APIPermission::kSync);
  skip.insert(APIPermission::kTabsPrivate);
  skip.insert(APIPermission::kThumbnails);
  skip.insert(APIPermission::kUtilities);
  skip.insert(APIPermission::kWebViewPrivate);
  skip.insert(APIPermission::kWindowPrivate);
  skip.insert(APIPermission::kZoom);
}

}  // namespace extensions

#endif  // EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_
