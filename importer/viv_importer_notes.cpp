// Copyright (c) 2013-2014 Vivaldi Technologies AS. All rights reserved

#include <stack>
#include <string>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
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
  OperaNotesReader() {}
  ~OperaNotesReader() override{};

  void AddNote(const std::vector<base::string16>& current_folder,
               const base::DictionaryValue& entries,
               bool is_folder,
               base::string16* item_name = NULL);

  const std::vector<ImportedNotesEntry>& Notes() const { return notes; }

 protected:
  void HandleEntry(const std::string& category,
                   const base::DictionaryValue& entries) override;

 private:
  std::vector<base::string16> current_folder;
  std::vector<ImportedNotesEntry> notes;

  DISALLOW_COPY_AND_ASSIGN(OperaNotesReader);
};

void OperaNotesReader::HandleEntry(const std::string& category,
                                   const base::DictionaryValue& entries) {
  if (base::LowerCaseEqualsASCII(category, "folder")) {
    base::string16 foldername;
    AddNote(current_folder, entries, true, &foldername);
    current_folder.push_back(foldername);
  } else if (base::LowerCaseEqualsASCII(category, "note")) {
    AddNote(current_folder, entries, false);
  } else if (category == "-") {
    current_folder.pop_back();
  }
}

void OperaNotesReader::AddNote(
    const std::vector<base::string16>& current_folder,
    const base::DictionaryValue& entries,
    bool is_folder,
    base::string16* item_name) {
  std::string temp;
  base::string16 wtemp;
  base::string16 title;
  base::string16 url;
  base::string16 nickname;
  base::string16 content;

  double created_time = 0;

  if (!is_folder && !entries.GetString("url", &url))
    url = base::string16();

  if (!entries.GetString("name", &wtemp))
    wtemp = url;

  int line_end = -1;
  for (base::string16::iterator it = wtemp.begin(); it != wtemp.end(); it++) {
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

  title = wtemp.substr(0, line_end);

  if (!is_folder)
    content = wtemp;

  if (item_name)
    *item_name = title;

  if (!entries.GetString("created", &temp) ||
      !base::StringToDouble(temp, &created_time))
    created_time = 0;

  ImportedNotesEntry entry;
  entry.is_folder = is_folder;
  entry.title = title;
  entry.content = content;
  entry.path = current_folder;
  entry.url = GURL(url);
  entry.creation_time = base::Time::FromTimeT(created_time);

  notes.push_back(entry);
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
    const base::string16& first_folder_name =
        bridge_->GetLocalizedString(IDS_NOTES_GROUP_FROM_OPERA);
    bridge_->AddNotes(reader.Notes(), first_folder_name);
  }
  return true;
}
