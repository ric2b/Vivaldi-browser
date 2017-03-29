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

#include "importer/viv_importer_utils.h"
#include "importer/viv_importer.h"
#include "base/strings/utf_string_conversions.h"

bool ReadOperaIniFile(const base::FilePath &profile_dir, DictionaryValueINIParser& target);

namespace viv_importer {

void DetectOperaProfiles(std::vector<importer::SourceProfile*>* profiles) {
  importer::SourceProfile* opera = new importer::SourceProfile;
  opera->importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_OPERA);
  opera->importer_type = importer::TYPE_OPERA;
  opera->source_path = GetProfileDir();
#if defined(OS_WIN)
  opera->app_path = GetOperaInstallPathFromRegistry();
#endif
  opera->services_supported =
      importer::FAVORITES | importer::NOTES | importer::PASSWORDS /*|
          importer::HISTORY |
          importer::COOKIES |
          importer::SEARCH_ENGINES*/;

  // Check if this profile need the master password
  DictionaryValueINIParser inifile_parser;
  if (ReadOperaIniFile(opera->source_path, inifile_parser)) {
    const base::DictionaryValue& inifile = inifile_parser.root();
    int val;
    inifile.GetInteger("Security Prefs.Use Paranoid Mailpassword", &val);
    if (val) {
      opera->services_supported |= importer::MASTER_PASSWORD;
    }
  }
  profiles->push_back(opera);
}
}

bool ReadOperaIniFile(const base::FilePath &profile_dir,
                      DictionaryValueINIParser &target) {
  // profile_dir is likely not a directory but the prefs file, so check before
  // appending
  // and breaking import.
  base::FilePath file;
  if (base::DirectoryExists(profile_dir)) {
    file = profile_dir.AppendASCII("operaprefs.ini");
  } else if (base::PathExists(profile_dir)) {
    file = profile_dir;
  }
  std::string inifile_data;
  if (!base::ReadFileToString(file, &inifile_data))
    return false;

  target.Parse(inifile_data);
  return true;
}

OperaImporter::OperaImporter(const importer::ImportConfig &import_config)
    : wand_version_(0), master_password_required(false) {
  if (import_config.arguments.size() >= 1) {
    master_password = import_config.arguments[0];
  }
}

OperaImporter::~OperaImporter() { master_password.clear(); }

void OperaImporter::StartImport(const importer::SourceProfile &source_profile,
                                uint16 items, ImporterBridge *bridge) {
  bridge_ = bridge;
  profile_dir_ = source_profile.source_path;

  base::FilePath file = profile_dir_;

#if defined(OS_WIN)
  if (base::EndsWith(file.value(), L"operaprefs.ini", false)) {
#else
  if (base::EndsWith(file.value(), "operaprefs.ini", false)) {
#endif
    profile_dir_ = profile_dir_.DirName();
    file = profile_dir_;
  }
  // Read Inifile
  DictionaryValueINIParser inifile_parser;
  if (!ReadOperaIniFile(profile_dir_, inifile_parser)) {
    bridge_->NotifyEnded();
    return;
  }

  const base::DictionaryValue &inifile = inifile_parser.root();

  inifile.GetString("User Prefs.Hot List File Ver2", &bookmarkfilename_);
  // Fallback to a bookmkarks.adr file in the profile directory
  if (bookmarkfilename_.empty()) {
    bookmarkfilename_ = profile_dir_.AppendASCII("bookmarks.adr").value();
  }

  inifile.GetString("MailBox.NotesFile", &notesfilename_);
  if (notesfilename_.empty()) {
    notesfilename_ = profile_dir_.AppendASCII("notes.adr").value();
  }

  std::string temp_val;
  master_password_required =
      (inifile.GetString("Security Prefs.Use Paranoid Mailpassword",
                          &temp_val) &&
        !temp_val.empty() && atoi(temp_val.c_str()) != 0);

  if (!inifile.GetString("User Prefs.WandStorageFile", &wandfilename_))
    wandfilename_ = profile_dir_.AppendASCII("wand.dat").value();
  masterpassword_filename_ = profile_dir_.AppendASCII("opcert6.dat").value();

  bridge_->NotifyStarted();

  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    ImportBookMarks();
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  if ((items & importer::NOTES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::NOTES);
    ImportNotes();
    bridge_->NotifyItemEnded(importer::NOTES);
  }
  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    ImportWand();
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
  bridge_->NotifyEnded();
}
