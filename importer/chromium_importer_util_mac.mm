// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved


#include <stack>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/importer_type.h"
#include "importer/chromium_profile_importer.h"
#include "importer/viv_importer_utils.h"

using base::PathService;

base::FilePath GetProfileDir(importer::ImporterType importerType){
  base::FilePath profile_path;
  base::FilePath app_data_path;
  if (!PathService::Get(base::DIR_APP_DATA, &app_data_path)) {
    return app_data_path.Append("not-supported");
  }

  switch (importerType) {
    case importer::TYPE_CHROME:
      profile_path = app_data_path.Append("Google").Append("Chrome");
      break;
    case importer::TYPE_VIVALDI:
      profile_path = app_data_path.Append("Vivaldi");
      break;
    case importer::TYPE_YANDEX:
      profile_path = app_data_path.Append("Yandex").Append("YandexBrowser");
      break;
    case importer::TYPE_OPERA_OPIUM:
      profile_path = app_data_path.Append("com.operasoftware.Opera");
      break;
    case importer::TYPE_BRAVE:
      profile_path = app_data_path.Append("BraveSoftware").Append("Brave-Browser");
      break;
    case importer::TYPE_EDGE_CHROMIUM:
      profile_path = app_data_path.Append("Microsoft Edge");
      break;
    case importer::TYPE_ARC:
      profile_path = app_data_path.Append("Arc").Append("User Data");
      break;
    case importer::TYPE_OPERA_GX:
      profile_path = app_data_path.Append("com.operasoftware.OperaGX");
      break;
    default:
      profile_path = app_data_path.Append("not-supported");
      break;
  }
  return profile_path;
}
