// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
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

base::FilePath GetMailDirectory() {
  base::FilePath mail_directory;
  // The default location of the opera mail folder containing is
  // under user HOME directory in .opera/mail folder on Linux.
  base::FilePath home = base::GetHomeDir();
  if (!home.empty()) {
    mail_directory = home.Append(".opera/mail");
  }
  if (base::PathExists(mail_directory))
    return mail_directory;

  return base::FilePath();
}

base::FilePath GetThunderbirdMailDirectory() {
  base::FilePath mail_directory;
  base::FilePath home = base::GetHomeDir();
  if (!home.empty()) {
    mail_directory = home.Append(".thunderbird");
  }

  if (base::PathExists(mail_directory)) {
    return mail_directory;
  }

  // For thunderbird installed with snap
  mail_directory = home.Append("snap/thunderbird/common/.thunderbird");
  if (base::PathExists(mail_directory)) {
     return mail_directory;
  }

  return base::FilePath();
}
