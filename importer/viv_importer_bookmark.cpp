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
  OperaBookmarkReader() = default;
  ~OperaBookmarkReader() override = default;
  OperaBookmarkReader(const OperaBookmarkReader&) = delete;
  OperaBookmarkReader& operator=(const OperaBookmarkReader&) = delete;

  void AddBookmark(const std::vector<std::u16string>& current_folder,
                   const base::DictionaryValue& entries,
                   bool is_folder,
                   std::u16string* item_name = NULL);

  const std::vector<ImportedBookmarkEntry>& Bookmarks() const {
    return bookmarks;
  }

 protected:
  void HandleEntry(const std::string& category,
                   const base::DictionaryValue& entries) override;

 private:
  std::vector<std::u16string> current_folder;
  std::vector<ImportedBookmarkEntry> bookmarks;
};

void OperaBookmarkReader::HandleEntry(const std::string& category,
                                      const base::DictionaryValue& entries) {
  if (base::LowerCaseEqualsASCII(category, "folder")) {
    std::u16string foldername;
    AddBookmark(current_folder, entries, true, &foldername);
    current_folder.push_back(foldername);
  } else if (base::LowerCaseEqualsASCII(category, "url")) {
    AddBookmark(current_folder, entries, false);
  } else if (category == "-") {
    current_folder.pop_back();
  }
}

void OperaBookmarkReader::AddBookmark(
    const std::vector<std::u16string>& current_folder,
    const base::DictionaryValue& entries,
    bool is_folder,
    std::u16string* item_name) {
  std::string temp;
  std::u16string name;
  std::u16string url;
  std::string nickname;
  std::string description;

  double created_time = 0;

  if (!is_folder && !entries.GetString("url", &url))
    url = std::u16string();

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

  ImportedBookmarkEntry entry;
  entry.in_toolbar = false;  // on_personal_bar;
  entry.is_folder = is_folder;
  entry.title = name;
  entry.nickname = nickname;
  entry.description = description;
  entry.path = current_folder;
  entry.url = GURL(url);
  entry.creation_time = base::Time::FromTimeT(created_time);

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
    const std::u16string& first_folder_name =
        bridge_->GetLocalizedString(IDS_BOOKMARK_GROUP_FROM_OPERA);
    bridge_->AddBookmarks(reader.Bookmarks(), first_folder_name);
  }
  return true;
}
