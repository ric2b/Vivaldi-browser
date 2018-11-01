// Copyright (c) 2013-2016 Vivaldi Technologies AS. All rights reserved

#ifndef IMPORTER_IMPORTED_NOTES_ENTRY_H_
#define IMPORTER_IMPORTED_NOTES_ENTRY_H_

#include <vector>

#include "base/strings/string16.h"
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
  std::vector<base::string16> path;
  base::string16 title;
  base::string16 content;
  base::Time creation_time;
};

#endif  // IMPORTER_IMPORTED_NOTES_ENTRY_H_
