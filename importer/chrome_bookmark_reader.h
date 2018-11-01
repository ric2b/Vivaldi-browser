// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_CHROME_BOOKMARK_READER_H_
#define IMPORTER_CHROME_BOOKMARK_READER_H_

#include <string>

#include "base/values.h"
#include "components/bookmarks/browser/bookmark_model.h"

class ChromeBookmarkFileReader {
 public:
  void LoadFile(const base::FilePath& file);

 protected:
  virtual void HandleEntry(const std::string& category,
                           const base::DictionaryValue& entries) = 0;

  ChromeBookmarkFileReader();

  virtual ~ChromeBookmarkFileReader();

 private:
  bool DecodeNode(const base::DictionaryValue& value);
  bool DecodeChildren(const base::ListValue& child_value_list);

  static const char* kNameKey;
  static const char* kTypeKey;
  static const char* kURLKey;
  static const char* kTypeFolder;
  static const char* kChildrenKey;
  static const char* kTypeURL;

  DISALLOW_COPY_AND_ASSIGN(ChromeBookmarkFileReader);
};

#endif  // IMPORTER_CHROME_BOOKMARK_READER_H_
