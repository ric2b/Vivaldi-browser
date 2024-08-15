// Copyright (c) 2013-2016 Vivaldi Technologies AS. All rights reserved

#include "importer/imported_notes_entry.h"

ImportedNotesEntry::ImportedNotesEntry() = default;

ImportedNotesEntry::ImportedNotesEntry(const ImportedNotesEntry&) = default;

ImportedNotesEntry::~ImportedNotesEntry() {}

bool ImportedNotesEntry::operator==(const ImportedNotesEntry& other) const {
  return (is_folder == other.is_folder && url == other.url &&
          path == other.path && content == other.content &&
          creation_time == other.creation_time &&
          last_modification_time == other.last_modification_time);
}
