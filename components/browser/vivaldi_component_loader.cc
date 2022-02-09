// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/extensions/component_loader.h"

#include "app/vivaldi_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

namespace extensions {

void ComponentLoader::AddVivaldiApp(const base::FilePath* path) {
  Add(VIVALDI_MANIFEST_JS,
      path ? *path : base::FilePath(FILE_PATH_LITERAL("vivaldi")));

  // Make sure that Vivaldi can access the extension preferences. See
  // <URL://https://developer.chrome.com/extensions/types#ChromeSetting>.
  ExtensionPrefs::Get(profile_)->RegisterAndLoadExtPrefsForVivaldi();
}

void ComponentLoader::AddVivaldiPIP() {
  PrefService* pref_service = profile_->GetPrefs();

  if (pref_service->GetBoolean(
          vivaldiprefs::kWebpagesPictureInPictureButtonEnabled) == true) {
    Add(VIVALDI_PIP_MANIFEST, base::FilePath(FILE_PATH_LITERAL(
                                  "vivaldi/components/picture-in-picture")));
  }
}

void ComponentLoader::AddVivaldiThemeStore() {
  Add(VIVALDI_THEMESTORE_MANIFEST,
      base::FilePath(FILE_PATH_LITERAL("vivaldi/components/theme-store")));
}

}  // namespace extensions