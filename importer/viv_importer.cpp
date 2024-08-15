// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "importer/viv_importer.h"

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/ini_parser.h"

#include "importer/imported_speeddial_entry.h"

static const char OPERA_PREFS_NAME[] = "operaprefs.ini";
static const char OPERA_SPEEDDIAL_NAME[] = "speeddial.ini";

bool ReadOperaIniFile(const base::FilePath& profile_dir,
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

base::FilePath::StringType StringToPath(const std::string* str) {
  if (!str)
    return base::FilePath::StringType();

#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(*str);
#else
  return *str;
#endif
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

  if (base::EqualsCaseInsensitiveASCII(file.BaseName().MaybeAsASCII(),
                                  OPERA_PREFS_NAME)) {
    profile_dir_ = profile_dir_.DirName();
    file = profile_dir_;
  }
  master_password_required_ = false;

  // Read Inifile
  DictionaryValueINIParser inifile_parser;
  if (ReadOperaIniFile(profile_dir_, &inifile_parser)) {
    const base::Value::Dict& inifile = inifile_parser.root();

    bookmarkfilename_ =
        StringToPath(inifile.FindStringByDottedPath("User Prefs.Hot List File Ver2"));
    notesfilename_ =
        StringToPath(inifile.FindStringByDottedPath("MailBox.NotesFile"));
    if (const std::string* s = inifile.FindStringByDottedPath("Security Prefs.Use Paranoid Mailpassword")) {
      master_password_required_ = !s->empty() && atoi(s->c_str()) != 0;
    }

    wandfilename_ =
        StringToPath(inifile.FindStringByDottedPath("User Prefs.WandStorageFile"));
    masterpassword_filename_ =
        profile_dir_.AppendASCII("opcert6.dat").value();
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
    for (auto i : inifile_parser.root()) {
      const std::string& key = i.first;
      if (key.find("Speed Dial ") == std::string::npos)
        continue;
      const base::Value::Dict* dict = i.second.GetIfDict();
      if (!dict)
        continue;

      ImportedSpeedDialEntry entry;
      const std::string* s = dict->FindString("Title");
      entry.title = base::UTF8ToUTF16(s ? *s : std::string());
      s = dict->FindString("Url");
      entry.url = s ? GURL(*s): GURL();
      entries.push_back(std::move(entry));
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
