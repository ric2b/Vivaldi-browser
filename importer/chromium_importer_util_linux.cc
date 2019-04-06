// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "base/path_service.h"
#include "importer/chromium_profile_importer.h"

using base::PathService;

base::FilePath GetProfileDir(importer::ImporterType importerType) {
  base::FilePath home_path;
  if (!PathService::Get(base::DIR_HOME, &home_path)) {
    return home_path.Append("not-supported");
  }

  base::FilePath profile_path;
  switch (importerType) {
    case importer::TYPE_CHROME:
      profile_path = home_path.Append(".config").Append("google-chrome");
      break;
    case importer::TYPE_VIVALDI:
      profile_path = home_path.Append(".config").Append("vivaldi");
      break;
    case importer::TYPE_YANDEX:
      profile_path = home_path.Append(".config").Append("yandex-browser-beta");
      break;
    case importer::TYPE_OPERA_OPIUM:
      profile_path = home_path.Append(".config").Append("opera");
      break;

    default:
      profile_path = home_path.Append("not-supported");
      break;
  }
  return profile_path;
}
