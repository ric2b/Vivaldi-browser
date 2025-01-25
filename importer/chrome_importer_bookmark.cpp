// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include <stack>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
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
#include "importer/chromium_importer.h"

namespace {

const char kNameKey[] = "name";
const char kTypeKey[] = "type";
const char kURLKey[] = "url";
const char kTypeFolder[] = "folder";
const char kChildrenKey[] = "children";
const char kTypeURL[] = "url";

class ChromeBookmarkReader {
 public:
  ChromeBookmarkReader() = default;
  ~ChromeBookmarkReader() = default;
  ChromeBookmarkReader(const ChromeBookmarkReader&) = delete;
  ChromeBookmarkReader& operator=(const ChromeBookmarkReader&) = delete;

  void LoadFile(const base::FilePath& file);

  const std::vector<ImportedBookmarkEntry>& Bookmarks() const {
    return bookmarks_;
  }

 private:
  void DecodeNode(const base::Value::Dict& dict);

  std::vector<std::u16string> current_folder_;
  std::vector<ImportedBookmarkEntry> bookmarks_;
};

void ChromeBookmarkReader::LoadFile(const base::FilePath& file) {
  if (!base::PathExists(file))
    return;

  std::string input;
  ReadFileToString(file, &input);

  std::optional<base::Value> root_value(base::JSONReader::Read(input));
  if (!root_value)
    return;
  base::Value::Dict* root_dict = root_value->GetIfDict();
  if (!root_dict)
    return;

  const base::Value::Dict* roots = root_dict->FindDict("roots");
  if (!roots)
    return;

  auto decode_named_folder = [&](const base::Value::Dict& parent,
                                 std::string_view folder_name) -> void {
    if (const base::Value::Dict* dict = parent.FindDict(folder_name)) {
      DecodeNode(*dict);
    }
  };

  decode_named_folder(*roots, "bookmark_bar");
  decode_named_folder(*roots, "other");

  // Opera 20+ uses a custom root.
  if (const base::Value::Dict* custom_root = roots->FindDict("custom_root")) {
    decode_named_folder(*custom_root, "unsorted");
    decode_named_folder(*custom_root, "speedDial");
    decode_named_folder(*custom_root, "trash");
    decode_named_folder(*custom_root, "userRoot");
  }
}

void ChromeBookmarkReader::DecodeNode(const base::Value::Dict& dict) {
  const std::string* type_string = dict.FindString(kTypeKey);
  if (!type_string)
    return;

  bool is_folder;
  if (*type_string == kTypeURL) {
    is_folder = false;
  } else if (*type_string == kTypeFolder) {
    is_folder = true;
  } else {
    return;
  }

  const base::Value::List* children = nullptr;
  if (is_folder) {
    children = dict.FindList(kChildrenKey);
    // Skip empty folders.
    if (!children || children->empty())
      return;
  }

#if 0 /* current unused */
  std::u16string on_personal_bar_s;
  bool on_personal_bar = false;
#endif

  const std::string* url = nullptr;
  if (!is_folder) {
    url = dict.FindString(kURLKey);
  }

  std::u16string name;
  if (const std::string* name_utf8 = dict.FindString(kNameKey)) {
    name = base::UTF8ToUTF16(*name_utf8);
  } else if (url) {
    name = base::UTF8ToUTF16(*url);
  }

  const std::string* description = dict.FindString("description");

#if 0 /* current unused */
  bool on_personal_bar = false;
  if (const std::string* s = dict.FindString("on personalbar")) {
    on_personal_bar = (base::LowerCaseEqualsASCII(s, "yes"));
  }
#endif

  double created_time = 0;
  if (const std::string* s = dict.FindString("created")) {
    if (!base::StringToDouble(*s, &created_time)) {
      created_time = 0;
    }
  }

  const std::string* nickname = nullptr;
  if (dict.FindDict("meta_info")) {
    nickname = dict.FindString("Nickname");
  }

  ImportedBookmarkEntry entry;
  entry.in_toolbar = false;  // on_personal_bar;
  entry.is_folder = is_folder;
  entry.title = name;
  entry.nickname = nickname ? *nickname : std::string();
  entry.description = description ? *description : std::string();
  entry.path = current_folder_;
  entry.url = GURL(url ? *url : std::string());
  entry.creation_time = base::Time::FromTimeT(created_time);

  bookmarks_.push_back(entry);

  if (is_folder) {
    current_folder_.push_back(name);

    for (auto& child_value : *children) {
      if (const base::Value::Dict* child = child_value.GetIfDict()) {
        DecodeNode(*child);
      }
    }
    current_folder_.pop_back();
  }
}

}  // namespace

void ChromiumImporter::ImportBookMarks() {
  if (bookmarkfilename_.empty()) {
    bridge_->NotifyEnded();
    return;
  }

  base::FilePath file(bookmarkfilename_);
  ChromeBookmarkReader reader;

  reader.LoadFile(file);

  if (!reader.Bookmarks().empty() && !cancelled()) {
    const std::u16string& first_folder_name =
        bridge_->GetLocalizedString(IDS_IMPORTED_BOOKMARKS);

    bridge_->AddBookmarks(reader.Bookmarks(), first_folder_name);
  }
}
