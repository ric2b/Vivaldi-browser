// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/edge_importer_utils_win.h"

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <Shlobj.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/common/importer/importer_test_registry_overrider_win.h"

namespace {

const base::char16 kEdgeSettingsMainKey[] = L"MicrosoftEdge\\Main";

const base::char16 kEdgePackageName[] =
    L"microsoft.microsoftedge_8wekyb3d8bbwe";

const base::Version kFirstVersionWhereESEIsDefault("25.10586");

// We assume at the moment that the package name never changes for Edge.
base::string16 GetEdgePackageName() {
  return kEdgePackageName;
}

base::string16 GetEdgeRegistryKey(const base::string16& key_name) {
  base::string16 registry_key =
      L"Software\\Classes\\Local Settings\\"
      L"Software\\Microsoft\\Windows\\CurrentVersion\\AppContainer\\"
      L"Storage\\";
  registry_key += GetEdgePackageName();
  registry_key += L"\\";
  registry_key += key_name;
  return registry_key;
}

base::string16 GetPotentiallyOverridenEdgeKey(
    const base::string16& desired_key_path) {
  base::string16 test_registry_override(
      ImporterTestRegistryOverrider::GetTestRegistryOverride());
  return test_registry_override.empty() ? GetEdgeRegistryKey(desired_key_path)
                                        : test_registry_override;
}

}  // namespace

namespace importer {

base::string16 GetEdgeSettingsKey() {
  return GetPotentiallyOverridenEdgeKey(kEdgeSettingsMainKey);
}

base::FilePath GetEdgeDataFilePath() {
  wchar_t buffer[MAX_PATH];
  if (::SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                        buffer) != S_OK)
    return base::FilePath();

  base::FilePath base_path(buffer);
  base::string16 rel_path = L"Packages\\";
  rel_path += GetEdgePackageName();
  rel_path += L"\\AC\\MicrosoftEdge\\User\\Default";
  return base_path.Append(rel_path);
}

bool IsEdgeFavoritesLegacyMode() {
  base::win::RegKey key(HKEY_CURRENT_USER, GetEdgeSettingsKey().c_str(),
                        KEY_READ);
  DWORD ese_enabled = 0;
  // Check whether Edge is using the new Extensible Store Engine (ESE) format
  // for its favorites.
  if (key.ReadValueDW(L"FavoritesESEEnabled", &ese_enabled) == ERROR_SUCCESS)
    return !ese_enabled;

  // Read the Edge version from its Appx manifest
  base::FilePath edge_appx_manifest;
  if (!base::PathService::Get(base::DIR_WINDOWS, &edge_appx_manifest))
    return false;
  edge_appx_manifest = edge_appx_manifest.Append(
      L"SystemApps\\" + GetEdgePackageName() + L"\\AppxManifest.xml");

  xmlDocPtr doc = xmlParseFile(edge_appx_manifest.AsUTF8Unsafe().c_str());
  if (doc == nullptr)
    return false;
  xmlXPathContextPtr context = xmlXPathNewContext(doc);
  if (context == nullptr)
    return false;
  xmlXPathRegisterNs(
      context, reinterpret_cast<const xmlChar*>("win10"),
      reinterpret_cast<const xmlChar*>(
          "http://schemas.microsoft.com/appx/manifest/foundation/windows10"));

  xmlXPathObjectPtr xpath_result = xmlXPathEvalExpression(
      reinterpret_cast<const xmlChar*>(
          "string(/win10:Package/win10:Identity/@Version)"),
      context);
  if (xpath_result == nullptr || xpath_result->type != XPATH_STRING)
    return false;

  // Check whether this version of Edge defaults to using ESE
  base::Version edge_version(reinterpret_cast<char*>(xpath_result->stringval));
  return edge_version < kFirstVersionWhereESEIsDefault;
}

bool EdgeImporterCanImport() {
  base::File::Info file_info;
  if (base::win::GetVersion() < base::win::VERSION_WIN10)
    return false;
  return base::GetFileInfo(GetEdgeDataFilePath(), &file_info) &&
         file_info.is_directory;
}

}  // namespace importer
