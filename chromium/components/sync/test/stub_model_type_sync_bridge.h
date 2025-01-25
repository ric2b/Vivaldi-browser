// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_STUB_MODEL_TYPE_SYNC_BRIDGE_H_
#define COMPONENTS_SYNC_TEST_STUB_MODEL_TYPE_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {

// A non-functional implementation of ModelTypeSyncBridge for
// testing purposes.
class StubModelTypeSyncBridge : public ModelTypeSyncBridge {
 public:
  explicit StubModelTypeSyncBridge(
      std::unique_ptr<ModelTypeChangeProcessor> change_processor);
  ~StubModelTypeSyncBridge() override;

  std::unique_ptr<MetadataChangeList> CreateMetadataChangeList() override;
  std::optional<ModelError> MergeFullSyncData(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_data) override;
  std::optional<ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      EntityChangeList entity_changes) override;
  std::unique_ptr<DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const EntityData& entity_data) override;
  std::string GetStorageKey(const EntityData& entity_data) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_STUB_MODEL_TYPE_SYNC_BRIDGE_H_
