// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_profile_importer.h"

#include <memory>
#include <string>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
//#include "chrome/grit/generated_resources.h"
#include "importer/chrome_importer_utils.h"
#include "ui/base/l10n/l10n_util.h"

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
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_OPERA_OPIUM));
  chromeProfiles.push_back(
      GetChromeProfile(ImporterType::TYPE_OPERA_OPIUM_BETA));
  chromeProfiles.push_back(
      GetChromeProfile(ImporterType::TYPE_OPERA_OPIUM_DEV));
#if !defined(OS_MACOSX)
  chromeProfiles.push_back(GetChromeProfile(ImporterType::TYPE_VIVALDI));
#endif
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
              ImporterType::TYPE_OPERA_OPIUM_DEV) {
        ChromeProfileInfo operaprof;
        operaprof.profileDisplayName = base::UTF8ToUTF16("Default");
        operaprof.profileName = "";
        prof.push_back(operaprof);

      } else {
        ReadProfiles(&prof, profileDirectory);
      }
      chrome.user_profile_names = prof;

      chrome.services_supported =
          importer::FAVORITES | importer::PASSWORDS | importer::HISTORY;
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

  base::JSONReader reader;
  std::unique_ptr<base::Value> root(reader.ReadToValue(input));

  base::DictionaryValue* dict = NULL;
  if (!root->GetAsDictionary(&dict)) {
    return;
  }

  const base::Value* roots;
  if (!dict->Get("profile", &roots)) {
    return;
  }

  const base::DictionaryValue* roots_d_value =
      static_cast<const base::DictionaryValue*>(roots);

  const base::Value* vInfoCache;
  const base::DictionaryValue* vDictValue;
  if (roots_d_value->Get("info_cache", &vInfoCache)) {
    vInfoCache->GetAsDictionary(&vDictValue);
    for (base::DictionaryValue::Iterator it(*vDictValue); !it.IsAtEnd();
         it.Advance()) {
      std::string profileName = it.key();

      const base::DictionaryValue* roots_sd_value =
          static_cast<const base::DictionaryValue*>(&it.value());

      ChromeProfileInfo prof;
      const base::Value* namVal;
      if (roots_sd_value->Get("name", &namVal)) {
        base::string16 displayName;
        namVal->GetAsString(&displayName);
        prof.profileDisplayName = displayName;
      } else {
        prof.profileDisplayName = base::UTF8ToUTF16(profileName);
      }
      prof.profileName = profileName;
      cp->push_back(prof);
    }
  }
}
