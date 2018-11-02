// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include <stack>
#include <string>
#include <vector>

#include "app/vivaldi_resources.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/ini_parser.h"
#include "importer/viv_importer.h"
#include "importer/viv_importer_utils.h"
#include "importer/viv_opera_reader.h"
#include "ui/base/l10n/l10n_util.h"

class OperaBookmarkReader : public OperaAdrFileReader {
 public:
  OperaBookmarkReader() {}
  ~OperaBookmarkReader() override{};

  void AddBookmark(const std::vector<base::string16>& current_folder,
                   const base::DictionaryValue& entries,
                   bool is_folder,
                   base::string16* item_name = NULL);

  const std::vector<ImportedBookmarkEntry>& Bookmarks() const {
    return bookmarks;
  }

 protected:
  void HandleEntry(const std::string& category,
                   const base::DictionaryValue& entries) override;

 private:
  std::vector<base::string16> current_folder;
  std::vector<ImportedBookmarkEntry> bookmarks;

  DISALLOW_COPY_AND_ASSIGN(OperaBookmarkReader);
};

void OperaBookmarkReader::HandleEntry(const std::string& category,
                                      const base::DictionaryValue& entries) {
  if (base::LowerCaseEqualsASCII(category, "folder")) {
    base::string16 foldername;
    AddBookmark(current_folder, entries, true, &foldername);
    current_folder.push_back(foldername);
  } else if (base::LowerCaseEqualsASCII(category, "url")) {
    AddBookmark(current_folder, entries, false);
  } else if (category == "-") {
    current_folder.pop_back();
  }
}

void OperaBookmarkReader::AddBookmark(
    const std::vector<base::string16>& current_folder,
    const base::DictionaryValue& entries,
    bool is_folder,
    base::string16* item_name) {
  std::string temp;
  base::string16 name;
  base::string16 url;
  base::string16 nickname;
  base::string16 description;
  base::string16 in_panel_s;

  double created_time = 0;
  double visited_time = 0;

  if (!is_folder && !entries.GetString("url", &url))
    url = base::string16();

  if (!entries.GetString("name", &name))
    name = url;

  if (item_name)
    *item_name = name;

  if (!entries.GetString("short name", &nickname)) {
    nickname.clear();
  }
  if (!entries.GetString("description", &description)) {
    description.clear();
  }
  if (!entries.GetString("created", &temp) ||
      !base::StringToDouble(temp, &created_time))
    created_time = 0;

  if (!entries.GetString("visited", &temp) ||
      !base::StringToDouble(temp, &visited_time))
    visited_time = 0;

  ImportedBookmarkEntry entry;
  entry.in_toolbar = false;  // on_personal_bar;
  entry.is_folder = is_folder;
  entry.title = name;
  entry.nickname = nickname;
  entry.description = description;
  entry.path = current_folder;
  entry.url = GURL(url);
  entry.creation_time = base::Time::FromTimeT(created_time);
  entry.visited_time = base::Time::FromTimeT(visited_time);

  bookmarks.push_back(entry);
}

bool OperaImporter::ImportBookMarks(std::string* error) {
  if (bookmarkfilename_.empty()) {
    *error = "No bookmark filename provided.";
    return false;
  }
  base::FilePath file(bookmarkfilename_);
  OperaBookmarkReader reader;

  if (!reader.LoadFile(file)) {
    *error = "Bookmark file does not exist.";
    return false;
  }
  if (!reader.Bookmarks().empty() && !cancelled()) {
    const base::string16& first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_OPERA);
    bridge_->AddBookmarks(reader.Bookmarks(), first_folder_name);
  }
  return true;
}
