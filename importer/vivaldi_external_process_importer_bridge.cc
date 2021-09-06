// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "chrome/utility/importer/external_process_importer_bridge.h"

#include "importer/viv_importer.h"
#include "importer/vivaldi_profile_import_process_messages.h"

using chrome::mojom::ProfileImportObserver;

namespace {

const int kNumNotesToSend = 10;
const int kNumSpeedDialToSend = 100;

}  // namespace

void ExternalProcessImporterBridge::AddNotes(
    const std::vector<ImportedNotesEntry>& notes,
    const std::u16string& first_folder_name) {
  observer_->OnNotesImportStart(first_folder_name, notes.size());

  // |notes_left| is required for the checks below as Windows has a
  // Debug bounds-check which prevents pushing an iterator beyond its end()
  // (i.e., |it + 2 < s.end()| crashes in debug mode if |i + 1 == s.end()|).
  int notes_left = notes.end() - notes.begin();
  for (std::vector<ImportedNotesEntry>::const_iterator it = notes.begin();
       it < notes.end();) {
    std::vector<ImportedNotesEntry> notes_group;
    std::vector<ImportedNotesEntry>::const_iterator end_group =
        it + std::min(notes_left, kNumNotesToSend);
    notes_group.assign(it, end_group);

    observer_->OnNotesImportGroup(notes_group);
    notes_left -= end_group - it;
    it = end_group;
  }
  DCHECK_EQ(0, notes_left);
}

void ExternalProcessImporterBridge::AddSpeedDial(
    const std::vector<ImportedSpeedDialEntry>& speeddials) {
  observer_->OnSpeedDialImportStart(speeddials.size());

  // |sd_left| is required for the checks below as Windows has a
  // Debug bounds-check which prevents pushing an iterator beyond its end()
  // (i.e., |it + 2 < s.end()| crashes in debug mode if |i + 1 == s.end()|).
  int sd_left = speeddials.end() - speeddials.begin();
  for (std::vector<ImportedSpeedDialEntry>::const_iterator it =
           speeddials.begin();
       it < speeddials.end();) {
    std::vector<ImportedSpeedDialEntry> sd_group;
    std::vector<ImportedSpeedDialEntry>::const_iterator end_group =
        it + std::min(sd_left, kNumSpeedDialToSend);
    sd_group.assign(it, end_group);

    observer_->OnSpeedDialImportGroup(sd_group);
    sd_left -= end_group - it;
    it = end_group;
  }
  DCHECK_EQ(0, sd_left);
}

void ExternalProcessImporterBridge::NotifyItemFailed(importer::ImportItem item,
                                                     const std::string& error) {
  observer_->OnImportItemFailed(item, error);
}
