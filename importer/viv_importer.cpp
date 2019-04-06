// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "importer/viv_importer.h"

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/ini_parser.h"
#include "importer/imported_speeddial_entry.h"

static const char OPERA_PREFS_NAME[] = "operaprefs.ini";
static const char OPERA_SPEEDDIAL_NAME[] = "speeddial.ini";

bool ReadOperaIniFile(const base::FilePath &profile_dir,
                      DictionaryValueINIParser* target) {
  // profile_dir is likely not a directory but the prefs file, so check before
  // appending and breaking import.
  base::FilePath file;
  if (base::DirectoryExists(profile_dir)) {
    file = profile_dir.AppendASCII(OPERA_PREFS_NAME);
  } else if (base::PathExists(profile_dir)) {
    file = profile_dir;
  }
  std::string inifile_data;
  if (!base::ReadFileToString(file, &inifile_data))
    return false;

  target->Parse(inifile_data);
  return true;
}

OperaImporter::OperaImporter()
    : wand_version_(0), master_password_required_(false) {}

OperaImporter::~OperaImporter() {}

void OperaImporter::StartImport(const importer::SourceProfile& source_profile,
                                uint16_t items,
                                ImporterBridge* bridge) {
  bridge_ = bridge;
  profile_dir_ = source_profile.source_path;
  master_password_ = base::UTF8ToUTF16(source_profile.master_password);

  base::FilePath file = profile_dir_;

  if (source_profile.importer_type == importer::TYPE_OPERA_BOOKMARK_FILE) {
    bookmarkfilename_ = file.value();
  } else {
    if (base::LowerCaseEqualsASCII(file.BaseName().MaybeAsASCII(),
                                   OPERA_PREFS_NAME)) {
      profile_dir_ = profile_dir_.DirName();
      file = profile_dir_;
    }
    // Read Inifile
    DictionaryValueINIParser inifile_parser;
    if (ReadOperaIniFile(profile_dir_, &inifile_parser)) {
      const base::DictionaryValue& inifile = inifile_parser.root();

      inifile.GetString("User Prefs.Hot List File Ver2", &bookmarkfilename_);
      inifile.GetString("MailBox.NotesFile", &notesfilename_);
      std::string temp_val;
      master_password_required_ =
          (inifile.GetString("Security Prefs.Use Paranoid Mailpassword",
                             &temp_val) &&
           !temp_val.empty() && atoi(temp_val.c_str()) != 0);

      inifile.GetString("User Prefs.WandStorageFile", &wandfilename_);
      masterpassword_filename_ =
          profile_dir_.AppendASCII("opcert6.dat").value();
    } else {
      master_password_required_ = false;
    }
    // Fallbacks if the ini file was not found or didn't have paths.
    if (bookmarkfilename_.empty()) {
      bookmarkfilename_ = profile_dir_.AppendASCII("bookmarks.adr").value();
    }
    if (notesfilename_.empty()) {
      notesfilename_ = profile_dir_.AppendASCII("notes.adr").value();
    }
    if (wandfilename_.empty()) {
      wandfilename_ = profile_dir_.AppendASCII("wand.dat").value();
    }
  }
  bridge_->NotifyStarted();

  std::string error;
  bool success;
  if ((items & importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::FAVORITES);
    success = ImportBookMarks(&error);
    if (!success) {
      bridge_->NotifyItemFailed(importer::FAVORITES, error);
    }
    bridge_->NotifyItemEnded(importer::FAVORITES);
  }
  if ((items & importer::NOTES) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::NOTES);
    success = ImportNotes(&error);
    if (!success) {
      bridge_->NotifyItemFailed(importer::NOTES, error);
    }
    bridge_->NotifyItemEnded(importer::NOTES);
  }
  if ((items & importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::PASSWORDS);
    success = ImportWand(&error);
    if (!success) {
      bridge_->NotifyItemFailed(importer::PASSWORDS, error);
    }
    bridge_->NotifyItemEnded(importer::PASSWORDS);
  }
  if ((items & importer::SPEED_DIAL) && !cancelled()) {
    bridge_->NotifyItemStarted(importer::SPEED_DIAL);
    success = ImportSpeedDial(&error);
    if (!success) {
      bridge_->NotifyItemFailed(importer::SPEED_DIAL, error);
    }
    bridge_->NotifyItemEnded(importer::SPEED_DIAL);
  }
  bridge_->NotifyEnded();
}

bool OperaImporter::ImportSpeedDial(std::string* error) {
  std::vector<ImportedSpeedDialEntry> entries;
  DictionaryValueINIParser inifile_parser;
  base::FilePath ini_file(profile_dir_);
  ini_file = ini_file.AppendASCII(OPERA_SPEEDDIAL_NAME);

  if (ReadOperaIniFile(ini_file, &inifile_parser)) {
    for (base::DictionaryValue::Iterator itr(inifile_parser.root());
         !itr.IsAtEnd(); itr.Advance()) {
      const std::string& key = itr.key();

      if (key.find("Speed Dial ") != std::string::npos) {
        const base::DictionaryValue* dict = nullptr;
        std::unique_ptr<ImportedSpeedDialEntry> entry(
            new ImportedSpeedDialEntry);
        if (itr.value().GetAsDictionary(&dict)) {
          for (base::DictionaryValue::Iterator section(*dict);
               !section.IsAtEnd(); section.Advance()) {
            const std::string& section_key = section.key();
            const base::Value& section_value = section.value();
            if (section_key == "Title") {
              section_value.GetAsString(&entry->title);
            } else if (section_key == "Url") {
              std::string url;
              section_value.GetAsString(&url);
              entry->url = GURL(url);
            }
          }
          entries.push_back(*entry.release());
        }
      }
    }
  } else {
    *error = "Could not read speeddial.ini.";
    return false;
  }
  if (!entries.empty() && !cancelled()) {
    bridge_->AddSpeedDial(entries);
  }
  return true;
}
