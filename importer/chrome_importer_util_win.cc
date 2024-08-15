// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include <stack>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/ini_parser.h"
#include "importer/chromium_profile_importer.h"
#include "ui/base/l10n/l10n_util.h"

using base::PathService;

base::FilePath GetProfileDir(importer::ImporterType importerType) {
  base::FilePath profile_path;
  base::FilePath app_data_path;

  if (importerType == importer::TYPE_OPERA_OPIUM ||
      importerType == importer::TYPE_OPERA_OPIUM_BETA ||
      importerType == importer::TYPE_OPERA_OPIUM_DEV ||
      importerType == importer::TYPE_OPERA_GX) {
    if (!PathService::Get(base::DIR_ROAMING_APP_DATA, &app_data_path)) {
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
    case importer::TYPE_CHROMIUM:
      profile_path = app_data_path.AppendASCII("Chromium\\User Data");
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
      profile_path =
          app_data_path.AppendASCII("Opera Software\\Opera Developer");
      break;
    case importer::TYPE_VIVALDI:
      profile_path = app_data_path.AppendASCII("Vivaldi\\User Data");
      break;
    case importer::TYPE_BRAVE:
      profile_path =
          app_data_path.AppendASCII("BraveSoftware\\Brave-Browser\\User Data");
      break;
    case importer::TYPE_EDGE_CHROMIUM:
      profile_path = app_data_path.AppendASCII("Microsoft\\Edge\\User Data");
      break;
    case importer::TYPE_ARC:
      profile_path = app_data_path.AppendASCII("The Browser Company\\Arc\\User Data");
      break;
    case importer::TYPE_OPERA_GX:
      profile_path = app_data_path.AppendASCII("Opera Software\\Opera GX Stable");
      break;
    default:
      profile_path = app_data_path.AppendASCII("not-supported");
      break;
  }
  return profile_path;
}
