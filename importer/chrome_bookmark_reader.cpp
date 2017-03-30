// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved


#include <stack>
#include <string>

#include "chrome/browser/importer/importer_list.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/common/ini_parser.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/path_service.h"
#include "chrome/common/importer/imported_bookmark_entry.h"
#include "chrome/common/importer/importer_bridge.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/browser/shell_integration.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "base/json/json_reader.h"


#include "importer/chrome_bookmark_reader.h"

const char* ChromeBookmarkFileReader::kNameKey = "name";
const char* ChromeBookmarkFileReader::kTypeKey = "type";
const char* ChromeBookmarkFileReader::kURLKey = "url";
const char* ChromeBookmarkFileReader::kTypeFolder = "folder";
const char* ChromeBookmarkFileReader::kChildrenKey = "children";
const char* ChromeBookmarkFileReader::kTypeURL = "url";

ChromeBookmarkFileReader::ChromeBookmarkFileReader() {
}

ChromeBookmarkFileReader::~ChromeBookmarkFileReader() {
}

void ChromeBookmarkFileReader::LoadFile(base::FilePath& file) {
  if (!base::PathExists(file))
    return;

  std::string input;
  ReadFileToString(file, &input);

  base::JSONReader reader;
  scoped_ptr<base::Value> root(reader.ReadToValue(input));

  base::DictionaryValue* dict = NULL;

  std::string* result = NULL;

  if (!root->GetAsDictionary(&dict)) {
    dict->GetString("checksum", result);
  }

  const base::Value* roots;
  dict->Get("roots", &roots);

  const base::DictionaryValue* roots_d_value =
      static_cast<const base::DictionaryValue*>(roots);
  const base::Value* root_folder_value;
  const base::Value* other_folder_value = nullptr;
  const base::Value* custom_roots;

  if (roots_d_value->Get("bookmark_bar", &root_folder_value) &&
      root_folder_value->IsType(base::Value::TYPE_DICTIONARY)) {
    DecodeNode(*static_cast<const base::DictionaryValue*>(root_folder_value));
  }
  if (roots_d_value->Get("other", &other_folder_value) &&
      other_folder_value->IsType(base::Value::TYPE_DICTIONARY)) {
    DecodeNode(*static_cast<const base::DictionaryValue*>(other_folder_value));
  }
  // Opera 20+ uses a custom root.
  if (roots_d_value->Get("custom_root", &custom_roots)) {
    if (custom_roots && custom_roots->IsType(base::Value::TYPE_DICTIONARY)) {
      roots_d_value = static_cast<const base::DictionaryValue*>(custom_roots);
      if (roots_d_value->Get("unsorted", &root_folder_value) &&
          root_folder_value->IsType(base::Value::TYPE_DICTIONARY)) {
        DecodeNode(
            *static_cast<const base::DictionaryValue*>(root_folder_value));
      }
      if (roots_d_value->Get("speedDial", &root_folder_value) &&
          root_folder_value->IsType(base::Value::TYPE_DICTIONARY)) {
        DecodeNode(
            *static_cast<const base::DictionaryValue*>(root_folder_value));
      }
      if (roots_d_value->Get("trash", &root_folder_value) &&
          root_folder_value->IsType(base::Value::TYPE_DICTIONARY)) {
        DecodeNode(
            *static_cast<const base::DictionaryValue*>(root_folder_value));
      }
      if (roots_d_value->Get("userRoot", &root_folder_value) &&
          root_folder_value->IsType(base::Value::TYPE_DICTIONARY)) {
        DecodeNode(
            *static_cast<const base::DictionaryValue*>(root_folder_value));
      }
    }
  }
}

bool ChromeBookmarkFileReader::DecodeNode(const base::DictionaryValue& value) {
  std::string id_string;
  int64_t id = 0;

  base::string16 title;
  value.GetString(kNameKey, &title);

  std::string date_added_string;

  std::string type_string;
  if (!value.GetString(kTypeKey, &type_string))
    return false;

  if (type_string != kTypeURL && type_string != kTypeFolder)
    return false;  // Unknown type.

  if (type_string == kTypeURL) {
    std::string url_string;
    if (!value.GetString(kURLKey, &url_string))
      return false;

    GURL url = GURL(url_string);

    HandleEntry("url", value);

  }
  else
  {
    const base::Value* child_values;
    if (!value.Get(kChildrenKey, &child_values))
      return false;

    if (child_values->GetType() != base::Value::TYPE_LIST)
      return false;

    const base::ListValue* list_values =
        static_cast<const base::ListValue*>(child_values);

    // Skip empty folders.
    if (!list_values->empty()) {
      HandleEntry("folder", value);
      DecodeChildren(*list_values);
    }
  }
  return true;
}

bool ChromeBookmarkFileReader::DecodeChildren(
  const base::ListValue& child_value_list) {

  for (size_t i = 0; i < child_value_list.GetSize(); ++i) {
    const base::Value* child_value;
    if (!child_value_list.Get(i, &child_value))
      return false;

    if (child_value->GetType() != base::Value::TYPE_DICTIONARY)
      return false;

    DecodeNode(*static_cast<const base::DictionaryValue*>(child_value));
  }

  base::DictionaryValue* dict = NULL;

  HandleEntry("-", *dict);
  return true;
}
