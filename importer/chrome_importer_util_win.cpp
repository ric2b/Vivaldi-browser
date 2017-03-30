// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved


#include <stack>
#include <string>

#include "chrome/browser/importer/importer_list.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/path_service.h"
#include "chrome/common/ini_parser.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/browser/shell_integration.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#include "importer/chromium_profile_importer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "base/json/json_reader.h"
#include "base/values.h"


#include "base/files/file_path.h"

base::FilePath GetProfileDir(importer::ImporterType importerType) {
  base::FilePath profile_path;
  base::FilePath app_data_path;

  if (importerType == importer::TYPE_OPERA_OPIUM ||
      importerType == importer::TYPE_OPERA_OPIUM_BETA ||
      importerType == importer::TYPE_OPERA_OPIUM_DEV) {
    if (!PathService::Get(base::DIR_APP_DATA, &app_data_path)) {
      return app_data_path.AppendASCII("not-supported");
    }
  } else {
    if (!PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_path)) {
      return app_data_path.AppendASCII("not-supported");
    }
  }
  switch (importerType) {
  case importer::TYPE_CHROME:
    profile_path = app_data_path.AppendASCII("Google\\Chrome\\User Data");
    break;
  case importer::TYPE_YANDEX:
    profile_path =
        app_data_path.AppendASCII("Yandex\\YandexBrowser\\User Data");
    break;
  case importer::TYPE_OPERA_OPIUM:
    profile_path = app_data_path.AppendASCII("Opera Software\\Opera Stable");
    break;
  case importer::TYPE_OPERA_OPIUM_BETA:
    profile_path = app_data_path.AppendASCII("Opera Software\\Opera Beta");
    break;
  case importer::TYPE_OPERA_OPIUM_DEV:
    profile_path = app_data_path.AppendASCII("Opera Software\\Opera Developer");
    break;
  case importer::TYPE_VIVALDI:
    profile_path = app_data_path.AppendASCII("Vivaldi\\User Data");
    break;
  default:
    profile_path = app_data_path.AppendASCII("not-supported");
    break;
  }
  return profile_path;
}
