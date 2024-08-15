// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include <stack>
#include <string>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/importer_bridge.h"
#include "importer/imported_notes_entry.h"
#include "importer/viv_importer.h"
#include "importer/viv_importer_utils.h"
#include "importer/viv_opera_reader.h"
#include "ui/base/l10n/l10n_util.h"

class OperaNotesReader : public OperaAdrFileReader {
 public:
  OperaNotesReader() = default;
  ~OperaNotesReader() override = default;
  OperaNotesReader(const OperaNotesReader&) = delete;
  OperaNotesReader& operator=(const OperaNotesReader&) = delete;

  void AddNote(const std::vector<std::u16string>& current_folder,
               const base::Value::Dict& entries,
               bool is_folder,
               std::u16string* item_name = NULL);

  const std::vector<ImportedNotesEntry>& Notes() const { return notes_; }

 protected:
  void HandleEntry(const std::string& category,
                   const base::Value::Dict& entries) override;

 private:
  std::vector<std::u16string> current_folder_;
  std::vector<ImportedNotesEntry> notes_;
};

void OperaNotesReader::HandleEntry(const std::string& category,
                                   const base::Value::Dict& entries) {
  if (base::EqualsCaseInsensitiveASCII(category, "folder")) {
    std::u16string foldername;
    AddNote(current_folder_, entries, true, &foldername);
    current_folder_.push_back(foldername);
  } else if (base::EqualsCaseInsensitiveASCII(category, "note")) {
    AddNote(current_folder_, entries, false);
  } else if (category == "-") {
    current_folder_.pop_back();
  }
}

void OperaNotesReader::AddNote(
    const std::vector<std::u16string>& current_folder,
    const base::Value::Dict& entries,
    bool is_folder,
    std::u16string* item_name) {

  const std::string* url = nullptr;
  if (!is_folder) {
    url = entries.FindString("url");
  }

  const std::string* name = entries.FindString("name");
  if (!name) {
    name = url;
  }

  std::u16string wtemp = name ? base::UTF8ToUTF16(*name) : std::u16string();
  int line_end = -1;
  for (std::u16string::iterator it = wtemp.begin(); it != wtemp.end(); it++) {
    // LF is coded as 0x02 char in the file, to prevent
    // linebreak. treat 2 sequential 0x02s as CRLF
    if (*it == 0x02) {
      if (it + 1 != wtemp.end() && it[1] == 0x02) {
        it = wtemp.erase(it);
      }
      *it = '\n';
      if (line_end < 0)
        line_end = it - wtemp.begin();
    }
  }

  std::u16string title = wtemp.substr(0, line_end);

  std::u16string content;
  if (!is_folder)
    content = std::move(wtemp);

  if (item_name)
    *item_name = title;

  double created_time = 0;
  if (const std::string* created = entries.FindString("created")) {
    if (!base::StringToDouble(*created, &created_time)) {
      created_time = 0;
    }
  }

  ImportedNotesEntry entry;
  entry.is_folder = is_folder;
  entry.title = std::move(title);
  entry.content = std::move(content);
  entry.path = current_folder;
  entry.url = GURL(url ? *url : std::string());
  entry.last_modification_time = entry.creation_time =
      base::Time::FromTimeT(created_time);

  notes_.push_back(std::move(entry));
}

bool OperaImporter::ImportNotes(std::string* error) {
  if (notesfilename_.empty()) {
    *error = "No notes filename provided.";
    return false;
  }
  base::FilePath file(notesfilename_);
  OperaNotesReader reader;

  if (!reader.LoadFile(file)) {
    *error = "Notes file does not exist.";
    return false;
  }
  if (!reader.Notes().empty() && !cancelled()) {
    const std::u16string& first_folder_name =
        bridge_->GetLocalizedString(IDS_NOTES_GROUP_FROM_OPERA);
    bridge_->AddNotes(reader.Notes(), first_folder_name);
  }
  return true;
}
