// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/stub_model_type_sync_bridge.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/sync/model/metadata_change_list.h"

namespace syncer {

StubModelTypeSyncBridge::StubModelTypeSyncBridge(
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

StubModelTypeSyncBridge::~StubModelTypeSyncBridge() = default;

std::unique_ptr<MetadataChangeList>
StubModelTypeSyncBridge::CreateMetadataChangeList() {
  return nullptr;
}

std::optional<ModelError> StubModelTypeSyncBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  return {};
}

std::optional<ModelError> StubModelTypeSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  return {};
}

std::unique_ptr<DataBatch> StubModelTypeSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  return nullptr;
}

std::unique_ptr<DataBatch> StubModelTypeSyncBridge::GetAllDataForDebugging() {
  return nullptr;
}

std::string StubModelTypeSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  return std::string();
}

std::string StubModelTypeSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  return std::string();
}

}  // namespace syncer
