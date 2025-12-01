// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "importer/viv_importer.h"

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/ini_parser.h"

#include "app/vivaldi_resources.h"
#include "importer/import_error_utils.h"
#include "importer/imported_speeddial_entry.h"
#include "importer/viv_import_result.h"

static const char OPERA_PREFS_NAME[] = "operaprefs.ini";
static const char OPERA_SPEEDDIAL_NAME[] = "speeddial.ini";

ImportResult ReadOperaIniFile(const base::FilePath& profile_dir,
                              DictionaryValueINIParser* target) {
  // profile_dir is likely not a directory but the prefs file, so check before
  // appending and breaking import.
  base::FilePath file;
  if (base::DirectoryExists(profile_dir)) {
    file = profile_dir.AppendASCII(OPERA_PREFS_NAME);
  } else if (base::PathExists(profile_dir)) {
    file = profile_dir;
  }

  auto result = ImportFileOperations::ReadFileToString(
      file, IDS_IMPORT_ERROR_OPERA_PREFS_FILE_NOT_FOUND,
      IDS_IMPORT_ERROR_OPERA_PREFS_READ_FAILED);
  if (!result.has_value()) {
    return import_result::Error(result.error());
  }

  target->Parse(result.value());
  return import_result::Success();
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

void OperaImporter::StartImport(
    const user_data_importer::SourceProfile& source_profile,
    uint16_t items,
    ImporterBridge* bridge) {
  bridge_ = bridge;
  profile_dir_ = source_profile.source_path;
  master_password_ = base::UTF8ToUTF16(source_profile.master_password);

  base::FilePath file = profile_dir_;

  if (source_profile.importer_type ==
      user_data_importer::TYPE_OPERA_BOOKMARK_FILE) {
    bookmarkfilename_ = file.value();
  } else {
    if (base::EqualsCaseInsensitiveASCII(file.BaseName().MaybeAsASCII(),
                                         OPERA_PREFS_NAME)) {
      profile_dir_ = profile_dir_.DirName();
      file = profile_dir_;
    }
    master_password_required_ = false;

    // Read Inifile
    DictionaryValueINIParser inifile_parser;
    auto ini_result = ReadOperaIniFile(profile_dir_, &inifile_parser);
    if (ini_result.has_value()) {
      const base::Value::Dict& inifile = inifile_parser.root();

      bookmarkfilename_ = StringToPath(
          inifile.FindStringByDottedPath("User Prefs.Hot List File Ver2"));
      notesfilename_ =
          StringToPath(inifile.FindStringByDottedPath("MailBox.NotesFile"));
      if (const std::string* s = inifile.FindStringByDottedPath(
              "Security Prefs.Use Paranoid Mailpassword")) {
        master_password_required_ = !s->empty() && atoi(s->c_str()) != 0;
      }

      wandfilename_ = StringToPath(
          inifile.FindStringByDottedPath("User Prefs.WandStorageFile"));
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
  }
  bridge_->NotifyStarted();

  if ((items & user_data_importer::FAVORITES) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::FAVORITES);
    const auto res = ImportBookMarks();
    import_result::NotifyBridge(res, bridge_.get(),
                                user_data_importer::FAVORITES);
  }
  if ((items & user_data_importer::NOTES) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::NOTES);
    const auto res = ImportNotes();
    import_result::NotifyBridge(res, bridge_.get(), user_data_importer::NOTES);
  }
  if ((items & user_data_importer::PASSWORDS) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::PASSWORDS);
    const auto res = ImportWand();
    import_result::NotifyBridge(res, bridge_.get(),
                                user_data_importer::PASSWORDS);
  }
  if ((items & user_data_importer::SPEED_DIAL) && !cancelled()) {
    bridge_->NotifyItemStarted(user_data_importer::SPEED_DIAL);
    const auto res = ImportSpeedDial();
    import_result::NotifyBridge(res, bridge_.get(),
                                user_data_importer::SPEED_DIAL);
  }
  bridge_->NotifyEnded();
}

ImportResult OperaImporter::ImportSpeedDial() {
  std::vector<ImportedSpeedDialEntry> entries;
  DictionaryValueINIParser inifile_parser;
  base::FilePath ini_file(profile_dir_);
  ini_file = ini_file.AppendASCII(OPERA_SPEEDDIAL_NAME);

  auto result = ReadOperaIniFile(ini_file, &inifile_parser);
  if (!result.has_value()) {
    // Check if it's the generic prefs error and make it more specific for
    // speeddial
    return import_result::Error(IDS_IMPORT_ERROR_OPERA_SPEEDDIAL_READ_FAILED);
  }

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
    entry.url = s ? GURL(*s) : GURL();
    entries.push_back(std::move(entry));
  }

  if (!entries.empty() && !cancelled()) {
    bridge_->AddSpeedDial(entries);
  }
  return import_result::Success();
}
