// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include <string>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "chrome/browser/shell_integration.h"

#include "importer/viv_importer_utils.h"

base::FilePath GetProfileDir() {
  base::FilePath ini_file;
  // The default location of the profile folder containing user data is
  // under user HOME directory in .opera folder on Linux.
  base::FilePath home = base::GetHomeDir();
  if (!home.empty()) {
    ini_file = home.Append(".opera/operaprefs.ini");
  }
  if (base::PathExists(ini_file))
    return ini_file;

  return base::FilePath();
}
