// Copyright (c) 2013-2016 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_IMPORTED_NOTES_ENTRY_H_
#define IMPORTER_IMPORTED_NOTES_ENTRY_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

struct ImportedNotesEntry {
 public:
  ImportedNotesEntry();
  ImportedNotesEntry(const ImportedNotesEntry&);
  ~ImportedNotesEntry();
  bool operator==(const ImportedNotesEntry& other) const;

  bool is_folder = false;
  GURL url;
  std::vector<std::u16string> path;
  std::u16string title;
  std::u16string content;
  base::Time creation_time;
  base::Time last_modification_time;
};

#endif  // IMPORTER_IMPORTED_NOTES_ENTRY_H_
