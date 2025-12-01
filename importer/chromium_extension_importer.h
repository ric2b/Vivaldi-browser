// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_CHROMIUM_EXTENSION_IMPORTER_H_
#define IMPORTER_CHROMIUM_EXTENSION_IMPORTER_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/webstore_install_result.h"

#include <string>
#include <vector>

class Profile;

namespace extension_importer {

class ChromiumExtensionsImporter {
 public:
  explicit ChromiumExtensionsImporter(Profile* profile);
  ~ChromiumExtensionsImporter();

  void OnExtensionAdded(bool success,
                        const std::string& error,
                        extensions::webstore_install::Result result);
  void AddExtensions(const std::vector<std::string> extensions);

  static bool CanImportExtensions(const base::FilePath& profile_dir);
  static std::vector<std::string> GetImportableExtensions(
      const base::FilePath& profile_dir);

 private:
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ChromiumExtensionsImporter> weak_ptr_factory_{this};
};

}  // namespace extension_importer

#endif  // IMPORTER_CHROMIUM_EXTENSION_IMPORTER_H_
