// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include <stack>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/ini_parser.h"
#include "ui/base/l10n/l10n_util.h"

#include "app/vivaldi_resources.h"
#include "importer/viv_importer.h"
#include "importer/viv_importer_utils.h"
#include "importer/viv_opera_reader.h"

class OperaBookmarkReader : public OperaAdrFileReader {
 public:
  OperaBookmarkReader() = default;
  ~OperaBookmarkReader() override = default;
  OperaBookmarkReader(const OperaBookmarkReader&) = delete;
  OperaBookmarkReader& operator=(const OperaBookmarkReader&) = delete;

  void AddBookmark(const std::vector<std::u16string>& current_folder,
                   const base::Value::Dict& entries,
                   bool is_folder,
                   std::u16string* item_name = NULL);

  const std::vector<ImportedBookmarkEntry>& Bookmarks() const {
    return bookmarks_;
  }

 protected:
  void HandleEntry(const std::string& category,
                   const base::Value::Dict& entries) override;

 private:
  std::vector<std::u16string> current_folder_;
  std::vector<ImportedBookmarkEntry> bookmarks_;
};

void OperaBookmarkReader::HandleEntry(const std::string& category,
                                      const base::Value::Dict& entries) {
  if (base::EqualsCaseInsensitiveASCII(category, "folder")) {
    std::u16string foldername;
    AddBookmark(current_folder_, entries, true, &foldername);
    current_folder_.push_back(foldername);
  } else if (base::EqualsCaseInsensitiveASCII(category, "url")) {
    AddBookmark(current_folder_, entries, false);
  } else if (category == "-") {
    current_folder_.pop_back();
  }
}

void OperaBookmarkReader::AddBookmark(
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

  const std::string* nickname = entries.FindString("short name");
  const std::string* description = entries.FindString("description");

  double created_time = 0;
  if (const std::string* created = entries.FindString("created")) {
    if (!base::StringToDouble(*created, &created_time)) {
      created_time = 0;
    }
  }

  ImportedBookmarkEntry entry;
  entry.in_toolbar = false;  // on_personal_bar;
  entry.is_folder = is_folder;
  entry.title = name ? base::UTF8ToUTF16(*name) : std::u16string();
  entry.nickname = nickname ? *nickname : std::string();
  entry.description = description ? *description : std::string();
  entry.path = current_folder;
  entry.url = GURL(url ? *url : std::string());
  entry.creation_time = base::Time::FromTimeT(created_time);

  if (item_name) {
    *item_name = entry.title;
  }

  bookmarks_.push_back(std::move(entry));
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
