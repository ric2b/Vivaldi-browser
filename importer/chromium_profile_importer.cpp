// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#include "importer/chromium_profile_importer.h"
#include "base/files/file_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "base/json/json_reader.h"
#include "importer/chrome_importer_utils.h"
#include "base/strings/utf_string_conversions.h"

#include "app/vivaldi_resources.h"

using namespace importer;
using namespace base;
using namespace std;


ChromiumProfile::ChromiumProfile()
  : importer_type(ImporterType::TYPE_UNKNOWN){
}

ChromiumProfileImporter::~ChromiumProfileImporter(){

}

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

ChromiumProfile
ChromiumProfileImporter::GetChromeProfile(ImporterType importerType) {
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
    vector<SourceProfile> *profiles) {
  for (size_t i = 0; i < chromeProfiles.size(); ++i) {
    FilePath profileDirectory =
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

      vector<ChromeProfileInfo> prof;
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
        importer::FAVORITES |
        /*    importer::NOTES |*/
        importer::PASSWORDS |
        importer::HISTORY /*|
               importer::COOKIES
               importer::SEARCH_ENGINES*/;
      profiles->push_back(chrome);
    }
  }
}

void ChromiumProfileImporter::ReadProfiles(vector<ChromeProfileInfo> *cp,
                                           FilePath profileDirectory) {
  FilePath profileFileName = profileDirectory.AppendASCII("Local State");
  if (!PathExists(profileFileName))
    return;

  string input;
  ReadFileToString(profileFileName, &input);

  JSONReader reader;
  std::unique_ptr<Value> root(reader.ReadToValue(input));

  DictionaryValue* dict = NULL;
  if (!root->GetAsDictionary(&dict)) {
    return;
  }

  const Value* roots;
  if (!dict->Get("profile", &roots)) {
    return;
  }

  const DictionaryValue* roots_d_value =
    static_cast<const DictionaryValue*>(roots);

  const Value* vInfoCache;
  const DictionaryValue* vDictValue;
  if (roots_d_value->Get("info_cache", &vInfoCache)) {

    vInfoCache->GetAsDictionary(&vDictValue);
    for (DictionaryValue::Iterator it(*vDictValue); !it.IsAtEnd(); it.Advance()){
      string profileName = it.key();

      const DictionaryValue* roots_sd_value =
        static_cast<const DictionaryValue*>(&it.value());

      ChromeProfileInfo prof;
      const Value* namVal;
      if (roots_sd_value->Get("name", &namVal)) {
        string16 displayName;
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
