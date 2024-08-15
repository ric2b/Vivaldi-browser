// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"

#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/manifest_constants.h"

int main() {
  base::AtExitManager exit_manager;

  base::CommandLine::Init(0, nullptr);

  // Necessary to make sure the cerr is correctly allocated to the stderr file
  // It seems like Linux is not handling this correctly if only using cerr
  // This also makes sure the stamp file is created.
  std::cout.flush();

  base::i18n::InitializeICU();
  auto manifest =
      base::Value::Dict().Set(extensions::manifest_keys::kDefaultLocale, "en");
  std::string error;
  if (!extension_l10n_util::ValidateExtensionLocales(
          base::FilePath(FILE_PATH_LITERAL("resources/vivaldi")), manifest,
          &error)) {
    std::cerr << "Extension contains errors:\n\n" << error << "\n";
    return 1;
  }

  return 0;
}
