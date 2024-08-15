// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/extensions/component_loader.h"

#include "app/vivaldi_resources.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_prefs.h"
#include "vivaldi/prefs/vivaldi_gen_pref_enums.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"

#include "base/threading/thread_restrictions.h"

namespace extensions {

void ComponentLoader::AddVivaldiApp(const base::FilePath* path) {
  base::VivaldiScopedAllowBlocking allow_blocking;

  if (path) {
    // Custom path, don't load it from the resources section but from disk
    // instead.
    base::FilePath current_dir;
    base::PathService::Get(base::DIR_CURRENT, &current_dir);
    base::FilePath path_to_use =
        path->IsAbsolute() ? *path : current_dir.Append(*path);

    // ReadFileToString called below will refuse any path containing a
    // reference to parent (..) component, for security reasons. Since the
    // path to vivapp used in development commonly contains those, we have to
    // strip them. Since the path in question can be specified directly as an
    // absolute path anyway, allowing this shouldn't cause a security issue.
    if (path_to_use.ReferencesParent()) {
      std::vector<base::FilePath::StringType> components(
          path_to_use.GetComponents());
      path_to_use.clear();

      for (const auto& component : components) {
        if (component.compare(FILE_PATH_LITERAL("..")) == 0)
          path_to_use = path_to_use.DirName();
        else
          path_to_use = path_to_use.Append(component);
      }
    }

    std::string manifest;
    base::FilePath manifest_file = path_to_use.AppendASCII("manifest.json");

    if (base::ReadFileToString(manifest_file, &manifest)) {
      DCHECK(!manifest.empty());
      Add(manifest, path_to_use, true);
    }

  } else {
    Add(VIVALDI_MANIFEST_JS, base::FilePath(FILE_PATH_LITERAL("vivaldi")));
  }

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
