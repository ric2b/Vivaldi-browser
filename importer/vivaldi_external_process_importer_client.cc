// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved.

#include "chrome/browser/importer/external_process_importer_client.h"

#include "chrome/browser/importer/in_process_importer_bridge.h"
#include "importer/vivaldi_profile_import_process_messages.h"

void ExternalProcessImporterClient::OnImportItemFailed(
    importer::ImportItem import_item,
    const std::string& error_msg) {
  if (cancelled_)
    return;

  bridge_->NotifyItemFailed(import_item, error_msg);
}

void ExternalProcessImporterClient::OnNotesImportStart(
    const std::u16string& first_folder_name,
    uint32_t total_notes_count) {
  if (cancelled_)
    return;

  notes_first_folder_name_ = first_folder_name;
  total_notes_count_ = total_notes_count;
  notes_.reserve(total_notes_count);
}

void ExternalProcessImporterClient::OnNotesImportGroup(
    const std::vector<ImportedNotesEntry>& notes_group) {
  if (cancelled_)
    return;

  // Collect sets of bookmarks from importer process until we have reached
  // total_bookmarks_count_:
  notes_.insert(notes_.end(), notes_group.begin(), notes_group.end());
  if (notes_.size() == total_notes_count_)
    bridge_->AddNotes(notes_, notes_first_folder_name_);
}

void ExternalProcessImporterClient::OnSpeedDialImportStart(
    uint32_t total_count) {
  if (cancelled_)
    return;

  total_speeddial_count_ = total_count;
  speeddial_.reserve(total_count);
}

void ExternalProcessImporterClient::OnSpeedDialImportGroup(
    const std::vector<ImportedSpeedDialEntry>& group) {
  if (cancelled_)
    return;

  speeddial_.insert(speeddial_.end(), group.begin(), group.end());
  if (speeddial_.size() == total_speeddial_count_)
    bridge_->AddSpeedDial(speeddial_);
}

void ExternalProcessImporterClient::OnExtensionsImportStart(
    uint32_t total_count) {
  if (cancelled_)
    return;

  total_extensions_count_ = total_count;
  extensions_.reserve(total_count);
}

void ExternalProcessImporterClient::OnExtensionsImportGroup(
    const std::vector<std::string>& group) {
  if (cancelled_)
    return;

  extensions_.insert(extensions_.end(), group.begin(), group.end());
  if (extensions_.size() == total_extensions_count_)
    bridge_->AddExtensions(extensions_);
}

void ExternalProcessImporterClient::OnTabImportStart(
    uint32_t total_count) {
  if (cancelled_)
    return;

  total_tab_count_ = total_count;
  tabs_.reserve(total_count);
}

void ExternalProcessImporterClient::OnTabImportGroup(
    const std::vector<ImportedTabEntry>& group) {
  if (cancelled_)
    return;

  tabs_.insert(tabs_.end(), group.begin(), group.end());
  if (tabs_.size() == total_tab_count_)
    bridge_->AddOpenTabs(tabs_);
}
