// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/extensions/component_loader.h"

#include "app/vivaldi_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"

namespace extensions {

void ComponentLoader::AddVivaldiApp(const base::FilePath* path) {
  Add(VIVALDI_MAINFEST_JS,
      path ? *path : base::FilePath(FILE_PATH_LITERAL("vivaldi")));
  // Make sure that Vivaldi can access the extension preferences. See
  // <URL://https://developer.chrome.com/extensions/types#ChromeSetting>.
  ExtensionPrefs::Get(profile_)->RegisterAndLoadExtPrefsForVivaldi();
}

}  // namespace extensions