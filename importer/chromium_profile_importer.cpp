// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_profile_importer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/base/l10n/l10n_util.h"

#include "app/vivaldi_resources.h"
#include "importer/chrome_importer_utils.h"
#include "importer/chromium_extension_importer.h"

using importer::ChromeProfileInfo;
using importer::ImporterType;
using importer::SourceProfile;

ChromiumProfile::ChromiumProfile()
    : importer_type(ImporterType::TYPE_UNKNOWN) {}

ChromiumProfileImporter::~ChromiumProfileImporter() {}

ChromiumProfileImporter::ChromiumProfileImporter() {
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_CHROME));
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_CHROMIUM));
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_YANDEX));
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_BRAVE));
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_EDGE_CHROMIUM));
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_OPERA_OPIUM));
  chromeProfiles.push_back(
      GetChromeProfile(ImporterType::TYPE_OPERA_OPIUM_BETA));
  chromeProfiles.push_back(
      GetChromeProfile(ImporterType::TYPE_OPERA_OPIUM_DEV));
#if !BUILDFLAG(IS_MAC)
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_VIVALDI));
#endif
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_ARC));
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_OPERA_GX));
}

ChromiumProfile ChromiumProfileImporter::GetChromeProfile(
    ImporterType importerType) {
  ChromiumProfile prof = ChromiumProfile();
  switch (importerType) {
    case ImporterType::TYPE_CHROME:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_GOOGLE_CHROME;
      return prof;

    case ImporterType::TYPE_CHROMIUM:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_CHROMIUM;
      return prof;

    case ImporterType::TYPE_YANDEX:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_YANDEX;
      return prof;

    case ImporterType::TYPE_OPERA_OPIUM:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_OPERA_OPIUM;
      return prof;

    case ImporterType::TYPE_OPERA_OPIUM_BETA:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_OPERA_OPIUM_BETA;
      return prof;

    case ImporterType::TYPE_OPERA_OPIUM_DEV:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_OPERA_OPIUM_DEV;
      return prof;

    case ImporterType::TYPE_VIVALDI:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_VIVALDI;
      return prof;

    case ImporterType::TYPE_BRAVE:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_BRAVE;
      return prof;

    case ImporterType::TYPE_EDGE_CHROMIUM:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_EDGE_CHROMIUM;
      return prof;

    case ImporterType::TYPE_ARC:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_ARC;
      return prof;

    case ImporterType::TYPE_OPERA_GX:
      prof.importer_type = importerType;
      prof.import_name_resource_idx = IDS_IMPORT_FROM_OPERA_GX;
      return prof;

    default:
      return prof;
  }
}

void ChromiumProfileImporter::DetectChromiumProfiles(
    std::vector<SourceProfile>* profiles) {
  for (size_t i = 0; i < chromeProfiles.size(); ++i) {
    base::FilePath profileDirectory =
        GetProfileDir(chromeProfiles[i].importer_type);
    bool has_profile_dir = PathExists(profileDirectory);
    if (!has_profile_dir) {
      // Vivaldi allows import from standalone, so clear the path if it doesn't
      // exist.
      profileDirectory.clear();
    }
    if (has_profile_dir ||
        chromeProfiles[i].importer_type == ImporterType::TYPE_VIVALDI) {
      SourceProfile chrome;
      chrome.importer_name =
          l10n_util::GetStringUTF16(chromeProfiles[i].import_name_resource_idx);
      chrome.importer_type = chromeProfiles[i].importer_type;
      chrome.source_path = profileDirectory;

      std::vector<ChromeProfileInfo> prof;
      if (chromeProfiles[i].importer_type == ImporterType::TYPE_OPERA_OPIUM ||
          chromeProfiles[i].importer_type ==
              ImporterType::TYPE_OPERA_OPIUM_BETA ||
          chromeProfiles[i].importer_type ==
              ImporterType::TYPE_OPERA_OPIUM_DEV ||
          chromeProfiles[i].importer_type ==
              ImporterType::TYPE_OPERA_GX) {
        ChromeProfileInfo operaprof;

        operaprof.profileDisplayName = u"Default";

        // VB-98391 - newer Opera browsers have a profile subdirectory.
        // If Default dir exists, we use it as a profile name. Older Opera
        // browsers didn't use profiles at all, having the data directly in
        // profileDirectory with no subdir.
        bool has_default = PathExists(profileDirectory.AppendASCII("Default"));
        operaprof.profileName = has_default ? "Default" : "";

        prof.push_back(operaprof);

      } else {
        ReadProfiles(&prof, profileDirectory);
      }
      chrome.user_profile_names = prof;

      chrome.services_supported = importer::FAVORITES | importer::PASSWORDS |
                                  importer::HISTORY | importer::EXTENSIONS |
                                  importer::TABS;

      profiles->push_back(chrome);
    }
  }
}

void ChromiumProfileImporter::ReadProfiles(std::vector<ChromeProfileInfo>* cp,
                                           base::FilePath profileDirectory) {
  base::FilePath profileFileName = profileDirectory.AppendASCII("Local State");
  if (!base::PathExists(profileFileName))
    return;

  std::string input;
  ReadFileToString(profileFileName, &input);

  std::optional<base::Value> root_value(base::JSONReader::Read(input));
  if (!root_value || !root_value->is_dict())
    return;

  const base::Value::Dict* profile_dict =
      root_value->GetDict().FindDict("profile");
  if (!profile_dict)
    return;

  const base::Value::Dict* info_cache = profile_dict->FindDict("info_cache");
  if (!info_cache)
    return;

  for (auto i : *info_cache) {
    const std::string& profile_name = i.first;
    const base::Value::Dict* entry = i.second.GetIfDict();
    if (!entry)
      continue;
    const std::string* display_name = entry->FindString("name");
    if (!display_name) {
      display_name = &profile_name;
    }

    ChromeProfileInfo prof;
    prof.profileDisplayName = base::UTF8ToUTF16(*display_name);
    prof.profileName = profile_name;
    cp->push_back(prof);
  }
}
