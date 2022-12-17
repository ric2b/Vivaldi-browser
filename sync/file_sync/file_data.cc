// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved.

#include "sync/file_sync/file_data.h"

namespace file_sync {
SyncedFileData::SyncedFileData() = default;
SyncedFileData::~SyncedFileData() = default;
SyncedFileData::SyncedFileData(SyncedFileData&&) = default;
SyncedFileData& SyncedFileData::operator=(SyncedFileData&&) = default;


bool SyncedFileData::IsUnreferenced() const {
  bool unreferenced = true;
  for (auto& references_for_type : local_references) {
    unreferenced = unreferenced && references_for_type.second.empty();
  }
  for (auto& references_for_type : sync_references) {
    unreferenced = unreferenced && references_for_type.second.empty();
  }
  return unreferenced;
}

void SyncedFileData::RunPendingCallbacks() {
  for (auto& pending_callback : pending_callbacks) {
    std::move(pending_callback).Run(content);
  }

  pending_callbacks.clear();
}
}