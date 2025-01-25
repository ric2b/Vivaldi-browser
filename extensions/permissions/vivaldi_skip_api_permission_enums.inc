// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_
#define EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_

using extensions::mojom::APIPermissionID;

namespace extensions {

void SkipVivaldiAPIPermissions(extensions::APIPermissionSet& skip) {
  // Skip vivaldi permissions in

  skip.insert(APIPermissionID::kAccessKeys);
  skip.insert(APIPermissionID::kAutoUpdate);
  skip.insert(APIPermissionID::kBookmarkContextMenu);
  skip.insert(APIPermissionID::kBookmarksPrivate);
  skip.insert(APIPermissionID::kCalendar);
  skip.insert(APIPermissionID::kContacts);
  skip.insert(APIPermissionID::kContentBlocking);
  skip.insert(APIPermissionID::kContextMenu);
  skip.insert(APIPermissionID::kDevtoolsPrivate);
  skip.insert(APIPermissionID::kEditCommand);
  skip.insert(APIPermissionID::kExtensionActionUtils);
  skip.insert(APIPermissionID::kHistoryPrivate);
  skip.insert(APIPermissionID::kImportData);
  skip.insert(APIPermissionID::kInfobars);
  skip.insert(APIPermissionID::kMailPrivate);
  skip.insert(APIPermissionID::kMenuContent);
  skip.insert(APIPermissionID::kMenubar);
  skip.insert(APIPermissionID::kMenubarMenu);
  skip.insert(APIPermissionID::kNotes);
  skip.insert(APIPermissionID::kPageActions);
  skip.insert(APIPermissionID::kPipPrivate);
  skip.insert(APIPermissionID::kPrefs);
  skip.insert(APIPermissionID::kReadingListPrivate);
  skip.insert(APIPermissionID::kRuntimePrivate);
  skip.insert(APIPermissionID::kSavedPasswords);
  skip.insert(APIPermissionID::kSearchEngines);
  skip.insert(APIPermissionID::kSessionsPrivate);
  skip.insert(APIPermissionID::kSettings);
  skip.insert(APIPermissionID::kSync);
  skip.insert(APIPermissionID::kTabsPrivate);
  skip.insert(APIPermissionID::kThemePrivate);
  skip.insert(APIPermissionID::kTranslateHistory);
  skip.insert(APIPermissionID::kThumbnails);
  skip.insert(APIPermissionID::kUtilities);
  skip.insert(APIPermissionID::kVivaldiAccount);
  skip.insert(APIPermissionID::kWebViewPrivate);
  skip.insert(APIPermissionID::kWindowPrivate);
  skip.insert(APIPermissionID::kZoom);
  skip.insert(APIPermissionID::kOmniboxPrivate);
}

}  // namespace extensions

#endif  // EXTENSIONS_PERMISSIONS_VIVALDI_SKIP_API_PERMISSION_ENUMS_INC_
