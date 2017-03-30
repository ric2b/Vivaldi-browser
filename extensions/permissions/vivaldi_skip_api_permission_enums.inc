// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_
#define EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_

namespace extensions {

void SkipVivaldiAPIPermissions(extensions::APIPermissionSet &skip) {
  // Skip vivaldi permissions in

  skip.insert(APIPermission::kAutoUpdate);
  skip.insert(APIPermission::kBookmarksPrivate);
  skip.insert(APIPermission::kEditCommand);
  skip.insert(APIPermission::kExtensionActionUtils);
  skip.insert(APIPermission::kHistorySearch);
  skip.insert(APIPermission::kImportData);
  skip.insert(APIPermission::kNotes);
  skip.insert(APIPermission::kRuntimePrivate);
  skip.insert(APIPermission::kSavedPasswords);
  skip.insert(APIPermission::kSessionsPrivate);
  skip.insert(APIPermission::kSettings);
  skip.insert(APIPermission::kShowMenu);
  skip.insert(APIPermission::kTabsPrivate);
  skip.insert(APIPermission::kThumbnails);
  skip.insert(APIPermission::kZoom);
  skip.insert(APIPermission::kUtilities);
  skip.insert(APIPermission::kWebViewPrivate);
}

} // namespace extensions

#endif // EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_
