/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FCP_CLIENT_OPSTATS_PDS_BACKED_OPSTATS_DB_H_
#define FCP_CLIENT_OPSTATS_PDS_BACKED_OPSTATS_DB_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/time.h"
#include "fcp/client/log_manager.h"
#include "fcp/client/opstats/opstats_db.h"
#include "fcp/protos/opstats.pb.h"
#include "protostore/file-storage.h"
#include "protostore/proto-data-store.h"

namespace fcp {
namespace client {
namespace opstats {

// An implementation of OpStatsDb based on protodatastore cpp.
class PdsBackedOpStatsDb : public OpStatsDb {
 public:
  static constexpr char kParentDir[] = "fcp/opstats";
  static constexpr char kDbFileName[] = "opstats.pb";

  // Factory method to create PdsBackedOpStatsDb. The provided path is the
  // absolute path for the base directory for storing files. OpStatsDb will
  // attempt to create subdirectories and file, so the directory must grant
  // read/write access. The ttl is the duration that an OperationalStats message
  // is kept since its last update time.
  static absl::StatusOr<std::unique_ptr<OpStatsDb>> Create(
      const std::string& base_dir, absl::Duration ttl, LogManager& log_manager,
      int64_t max_size_bytes);

  ~PdsBackedOpStatsDb() override;

  // Returns the data in the db, or an error from the read operation. If the
  // read fails, will try to reset the db to be empty. The returned data is not
  // necessarily restricted according to the ttl.
  absl::StatusOr<OpStatsSequence> Read() override ABSL_LOCKS_EXCLUDED(mutex_);

  // Modifies the data in the db based on the supplied transformation function
  // and ttl restrictions. If there is an error fetching the existing data, the
  // db is reset. No transformation is applied if the reset fails.
  absl::Status Transform(std::function<void(OpStatsSequence&)> func) override
      ABSL_LOCKS_EXCLUDED(mutex_);

 private:
  PdsBackedOpStatsDb(
      std::unique_ptr<protostore::ProtoDataStore<OpStatsSequence>> db,
      std::unique_ptr<protostore::FileStorage> file_storage, absl::Duration ttl,
      LogManager& log_manager, int64_t max_size_bytes,
      std::function<void()> lock_releaser)
      : ttl_(std::move(ttl)),
        db_(std::move(db)),
        storage_(std::move(file_storage)),
        log_manager_(log_manager),
        max_size_bytes_(max_size_bytes),
        lock_releaser_(lock_releaser) {}

  const absl::Duration ttl_;
  std::unique_ptr<protostore::ProtoDataStore<OpStatsSequence>> db_
      ABSL_GUARDED_BY(mutex_);
  std::unique_ptr<protostore::FileStorage> storage_;
  LogManager& log_manager_;
  const int64_t max_size_bytes_;
  std::function<void()> lock_releaser_;
  absl::Mutex mutex_;
};

}  // namespace opstats
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_OPSTATS_PDS_BACKED_OPSTATS_DB_H_
