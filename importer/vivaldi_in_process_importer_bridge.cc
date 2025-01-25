// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "chrome/browser/importer/in_process_importer_bridge.h"

#include "chrome/browser/importer/external_process_importer_host.h"
#include "importer/imported_notes_entry.h"
#include "importer/imported_speeddial_entry.h"
#include "importer/imported_tab_entry.h"

void InProcessImporterBridge::AddNotes(
    const std::vector<ImportedNotesEntry>& notes,
    const std::u16string& first_folder_name) {
  writer_->AddNotes(notes, first_folder_name);
}

void InProcessImporterBridge::AddSpeedDial(
    const std::vector<ImportedSpeedDialEntry>& speeddials) {
  writer_->AddSpeedDial(speeddials);
}

void InProcessImporterBridge::AddExtensions(
    const std::vector<std::string>& extensions) {
  writer_->AddExtensions(extensions, host_);
}

void InProcessImporterBridge::AddOpenTabs(
    const std::vector<ImportedTabEntry>& tabs) {
  writer_->AddOpenTabs(tabs);
}

void InProcessImporterBridge::NotifyItemFailed(importer::ImportItem item,
                                               const std::string& error) {
  host_->NotifyImportItemFailed(item, error);
}
