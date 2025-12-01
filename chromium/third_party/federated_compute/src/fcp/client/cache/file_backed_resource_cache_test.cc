/*
 * Copyright 2022 Google LLC
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

#include "fcp/client/cache/file_backed_resource_cache.h"

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <fstream>
#include <optional>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/simulated_clock.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/selector_context.pb.h"
#include "fcp/client/test_helpers.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace client {
namespace cache {
namespace {

constexpr char kKey1[] = "1";
absl::Cord Resource1() { return absl::Cord("stream RENAISSANCE by Beyoncé"); }
constexpr char kKey2[] = "2";
absl::Cord Resource2() { return absl::Cord("stream PURE/HONEY by Beyoncé"); }
constexpr char kKey3[] = "3";
absl::Cord Resource3() {
  return absl::Cord("A third resource?? In this economy");
}
SelectorContext SampleStoredMetadata() {
  SelectorContext sample_stored_metadata;
  sample_stored_metadata.mutable_computation_properties()->set_session_name(
      "test");
  return sample_stored_metadata;
}
google::protobuf::Any Metadata() {
  google::protobuf::Any metadata;
  metadata.PackFrom(SampleStoredMetadata());
  return metadata;
}
absl::Duration kMaxAge = absl::Hours(1);
int64_t kMaxCacheSizeBytes = 10000000;

int NumFilesInDir(std::filesystem::path dir) {
  int num_files_in_dir = 0;
  for ([[maybe_unused]] auto& de : std::filesystem::directory_iterator(dir)) {
    num_files_in_dir++;
  }
  return num_files_in_dir;
}

class FileBackedResourceCacheTest : public testing::Test {
 protected:
  void SetUp() override {
    root_cache_dir_ = testing::TempDir();
    std::filesystem::path root_cache_dir(root_cache_dir_);
    cache_dir_ = root_cache_dir / "fcp" / "cache";
    root_files_dir_ = testing::TempDir();
    std::filesystem::path root_files_dir(root_files_dir_);
    manifest_path_ = root_files_dir / "fcp" / "cache_manifest.pb";
  }

  void TearDown() override {
    std::filesystem::remove_all(root_cache_dir_);
    std::filesystem::remove_all(root_files_dir_);
  }

  testing::StrictMock<MockLogManager> log_manager_;
  SimulatedClock clock_;
  std::string root_cache_dir_;
  std::string root_files_dir_;
  std::filesystem::path cache_dir_;
  std::filesystem::path manifest_path_;
};

TEST_F(FileBackedResourceCacheTest, FailToCreateParentDirectoryInBaseDir) {
  EXPECT_CALL(
      log_manager_,
      LogDiag(ProdDiagCode::RESOURCE_CACHE_FAILED_TO_CREATE_MANIFEST_DIR));
  ASSERT_THAT(
      FileBackedResourceCache::Create("/proc/0", root_cache_dir_, &log_manager_,
                                      &clock_, kMaxCacheSizeBytes),
      IsCode(INTERNAL));
}

TEST_F(FileBackedResourceCacheTest, FailToCreateParentDirectoryInCacheDir) {
  EXPECT_CALL(log_manager_,
              LogDiag(ProdDiagCode::RESOURCE_CACHE_FAILED_TO_CREATE_CACHE_DIR));
  ASSERT_THAT(
      FileBackedResourceCache::Create(root_files_dir_, "/proc/0", &log_manager_,
                                      &clock_, kMaxCacheSizeBytes),
      IsCode(INTERNAL));
}

TEST_F(FileBackedResourceCacheTest, InvalidBaseDirRelativePath) {
  EXPECT_CALL(log_manager_,
              LogDiag(ProdDiagCode::RESOURCE_CACHE_INVALID_MANIFEST_PATH));
  ASSERT_THAT(FileBackedResourceCache::Create("relative/base", root_cache_dir_,
                                              &log_manager_, &clock_,
                                              kMaxCacheSizeBytes),
              IsCode(INVALID_ARGUMENT));
}

TEST_F(FileBackedResourceCacheTest, InvalidCacheDirRelativePath) {
  EXPECT_CALL(
      log_manager_,
      LogDiag(ProdDiagCode::RESOURCE_CACHE_CACHE_ROOT_PATH_NOT_ABSOLUTE));
  ASSERT_THAT(FileBackedResourceCache::Create(root_files_dir_, "relative/cache",
                                              &log_manager_, &clock_,
                                              kMaxCacheSizeBytes),
              IsCode(INVALID_ARGUMENT));
}

TEST_F(FileBackedResourceCacheTest, SuccessfulInitialization) {
  ASSERT_OK(FileBackedResourceCache::Create(root_files_dir_, root_cache_dir_,
                                            &log_manager_, &clock_,
                                            kMaxCacheSizeBytes));
}

TEST_F(FileBackedResourceCacheTest, CacheFile) {
  auto resource_cache = FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      kMaxCacheSizeBytes);
  ASSERT_OK(resource_cache);
  ASSERT_OK(
      (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));

  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata> cached_resource =
      (*resource_cache)->Get(kKey1, std::nullopt);
  ASSERT_OK(cached_resource);
  ASSERT_EQ(Resource1(), (*cached_resource).resource);
  ASSERT_EQ(Metadata().GetTypeName(),
            (*cached_resource).metadata.GetTypeName());
  SelectorContext stored_metadata;
  (*cached_resource).metadata.UnpackTo(&stored_metadata);
  ASSERT_THAT(SampleStoredMetadata(), EqualsProto(stored_metadata));
}

TEST_F(FileBackedResourceCacheTest, CacheFileCloseReinitializeFileStillCached) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK(
        (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  }

  // Advance the clock a little bit
  clock_.AdvanceTime(absl::Minutes(1));

  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource = (*resource_cache)->Get(kKey1, std::nullopt);
    ASSERT_OK(cached_resource);
    ASSERT_EQ(Resource1(), (*cached_resource).resource);
  }
}

TEST_F(FileBackedResourceCacheTest, CacheTooBigFileReturnsResourceExhausted) {
  auto resource_cache = FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      (int64_t)(Resource1().size() / 2));
  ASSERT_OK(resource_cache);
  ASSERT_THAT(
      (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)),
      IsCode(RESOURCE_EXHAUSTED));
}

TEST_F(FileBackedResourceCacheTest,
       UnreadableManifestReturnsInternalButIsThenReadable) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK(
        (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  }

  // There should be the one file we cached.
  ASSERT_EQ(NumFilesInDir(cache_dir_), 1);

  // Write some garbage to the manifest.
  {
    std::ofstream ofs(manifest_path_, std::ofstream::trunc);
    ofs << "garbage garbage garbage";
  }

  {
    EXPECT_CALL(log_manager_,
                LogDiag(ProdDiagCode::RESOURCE_CACHE_MANIFEST_READ_FAILED));
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_THAT(resource_cache, IsCode(INTERNAL));
  }

  // Failing to read the manifest should have deleted it.
  ASSERT_EQ(std::filesystem::exists(manifest_path_), false);
  // But there will still be files in the cache dir. These files will be cleaned
  // up the next time the cache is initialized.
  ASSERT_EQ(NumFilesInDir(cache_dir_), 1);

  // We should be able to create a new FileBackedResourceCache successfully
  // since the garbage manifest was deleted.
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    // Initializing the cache should have deleted the untracked files in the
    // cache dir.
    ASSERT_EQ(NumFilesInDir(cache_dir_), 0);
    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
    ASSERT_THAT((*resource_cache)->Get(kKey1, std::nullopt), IsCode(NOT_FOUND));
  }
}

TEST_F(FileBackedResourceCacheTest,
       UnreadableManifestReturnsInternalButIsThenWritable) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK(
        (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  }

  // There should be the one file we cached.
  ASSERT_EQ(NumFilesInDir(cache_dir_), 1);

  // Write some garbage to the manifest.
  {
    std::ofstream ofs(manifest_path_, std::ofstream::trunc);
    ofs << "garbage garbage garbage";
  }

  {
    EXPECT_CALL(log_manager_,
                LogDiag(ProdDiagCode::RESOURCE_CACHE_MANIFEST_READ_FAILED));
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_THAT(resource_cache, IsCode(INTERNAL));
  }

  // Failing to read the manifest should have deleted it.
  ASSERT_EQ(std::filesystem::exists(manifest_path_), false);
  // But there will still be files in the cache dir. These files will be cleaned
  // up the next time the cache is initialized.
  ASSERT_EQ(NumFilesInDir(cache_dir_), 1);

  // We should be able to create a new FileBackedResourceCache successfully
  // since it was reset.
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    // Initializing the cache should have deleted the untracked files in the
    // cache dir.
    ASSERT_EQ(NumFilesInDir(cache_dir_), 0);
    ASSERT_OK(resource_cache);
    ASSERT_OK(
        (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
    ASSERT_EQ(NumFilesInDir(cache_dir_), 1);
  }
}

TEST_F(FileBackedResourceCacheTest, PutTwoFilesThenGetThem) {
  auto resource_cache = FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      kMaxCacheSizeBytes);
  ASSERT_OK(resource_cache);
  ASSERT_OK((*resource_cache)->Put(kKey1, Resource1(), Metadata(), kMaxAge));
  ASSERT_OK((*resource_cache)->Put(kKey2, Resource2(), Metadata(), kMaxAge));

  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
      cached_resource1 = (*resource_cache)->Get(kKey1, std::nullopt);
  ASSERT_OK(cached_resource1);
  ASSERT_EQ(Resource1(), (*cached_resource1).resource);

  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
      cached_resource2 = (*resource_cache)->Get(kKey2, std::nullopt);
  ASSERT_OK(cached_resource2);
  ASSERT_EQ(Resource2(), (*cached_resource2).resource);
}

TEST_F(FileBackedResourceCacheTest, CacheFileThenExpire) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK((*resource_cache)->Put(kKey1, Resource1(), Metadata(), kMaxAge));
  }

  // Advance the clock a little bit beyond max_age
  clock_.AdvanceTime(kMaxAge + absl::Minutes(1));

  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);

    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource = (*resource_cache)->Get(kKey1, std::nullopt);
    ASSERT_THAT(cached_resource, IsCode(NOT_FOUND));
  }
}

TEST_F(FileBackedResourceCacheTest, PutTwoFilesThenOneExpires) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK((*resource_cache)->Put(kKey1, Resource1(), Metadata(), kMaxAge));
    ASSERT_OK(
        (*resource_cache)->Put(kKey2, Resource2(), Metadata(), kMaxAge * 2));
  }

  // Advance the clock a little bit beyond the first resource's expiry.

  clock_.AdvanceTime(kMaxAge + absl::Minutes(1));
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource1 = (*resource_cache)->Get(kKey1, std::nullopt);
    ASSERT_THAT(cached_resource1, IsCode(NOT_FOUND));

    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource2 = (*resource_cache)->Get(kKey2, std::nullopt);
    ASSERT_OK(cached_resource2);
    ASSERT_EQ(Resource2(), (*cached_resource2).resource);
  }
}

TEST_F(FileBackedResourceCacheTest, CacheFileThenUpdateExpiry) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK((*resource_cache)->Put(kKey1, Resource1(), Metadata(), kMaxAge));
  }

  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);

    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
    // Pass a new max_age when we Get the resource, updating its expiry time.
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource = (*resource_cache)->Get(kKey1, 6 * kMaxAge);
    ASSERT_OK(cached_resource);
    ASSERT_EQ(Resource1(), (*cached_resource).resource);
  }

  // Advance the clock. Even though we've now passed the original expiry, the
  // resource should still be cached because we updated the expiry with the
  // Get().
  clock_.AdvanceTime(kMaxAge + absl::Minutes(5));

  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);

    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
    // Pass a new max_age when we Get the resource, updating its expiry time.
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource = (*resource_cache)->Get(kKey1, 6 * kMaxAge);
    ASSERT_OK(cached_resource);
    ASSERT_EQ(Resource1(), (*cached_resource).resource);
  }
}

TEST_F(FileBackedResourceCacheTest, CacheExceedsMaxCacheSize) {
  // Room for resource2 and resource3 but not quite enough for resource1 as
  // well.
  int64_t local_max_cache_size_bytes =
      Resource2().size() + Resource3().size() + (Resource1().size() / 2);

  auto resource_cache = FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      local_max_cache_size_bytes);
  ASSERT_OK(resource_cache);
  ASSERT_OK(
      (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  clock_.AdvanceTime(absl::Minutes(1));
  ASSERT_OK(
      (*resource_cache)->Put(kKey2, Resource2(), Metadata(), absl::Hours(1)));
  clock_.AdvanceTime(absl::Minutes(1));
  ASSERT_OK(
      (*resource_cache)->Put(kKey3, Resource3(), Metadata(), absl::Hours(1)));

  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  ASSERT_OK((*resource_cache)->Get(kKey3, std::nullopt));
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  ASSERT_OK((*resource_cache)->Get(kKey2, std::nullopt));
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
  ASSERT_THAT((*resource_cache)->Get(kKey1, std::nullopt), IsCode(NOT_FOUND));
}

TEST_F(FileBackedResourceCacheTest,
       CacheExceedsMaxCacheSizeLeastRecentlyUsedDeleted) {
  int64_t local_max_cache_size_bytes =
      Resource1().size() + (Resource2().size() / 2) + Resource3().size();

  auto resource_cache = FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      local_max_cache_size_bytes);
  ASSERT_OK(resource_cache);
  ASSERT_OK(
      (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  clock_.AdvanceTime(absl::Minutes(1));
  ASSERT_OK(
      (*resource_cache)->Put(kKey2, Resource2(), Metadata(), absl::Hours(1)));
  clock_.AdvanceTime(absl::Minutes(1));
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  // Get resource1 so we update it's least recently used time before we put in
  // resource3. This should cause resource2 to get deleted instead of resource1
  // when we add resource3.
  ASSERT_OK((*resource_cache)->Get(kKey1, std::nullopt));
  clock_.AdvanceTime(absl::Minutes(1));
  ASSERT_OK(
      (*resource_cache)->Put(kKey3, Resource3(), Metadata(), absl::Hours(1)));

  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  ASSERT_OK((*resource_cache)->Get(kKey3, std::nullopt));
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
  ASSERT_THAT((*resource_cache)->Get(kKey2, std::nullopt), IsCode(NOT_FOUND));
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  ASSERT_OK((*resource_cache)->Get(kKey1, std::nullopt));
}

TEST_F(FileBackedResourceCacheTest, FileInCacheDirButNotInManifest) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK(
        (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  }

  // Delete the manifest!
  std::filesystem::remove(manifest_path_);

  // There should be the one file we cached.
  ASSERT_EQ(NumFilesInDir(cache_dir_), 1);

  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);

    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource = (*resource_cache)->Get(kKey1, std::nullopt);
    ASSERT_THAT(cached_resource, IsCode(NOT_FOUND));
    // The cache dir should also be empty, because we reinitialized the cache
    // and there was an untracked file in it.
    ASSERT_EQ(NumFilesInDir(cache_dir_), 0);
  }
}

// Covers the case where a user manually deletes the app's cache dir.
TEST_F(FileBackedResourceCacheTest, FileInManifestButRootCacheDirDeleted) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK(
        (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  }

  // Delete the entire cache dir from the root.
  std::filesystem::remove_all(root_cache_dir_);

  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);

    // Now we should gracefully fail even though the file is in the manifest but
    // not on disk.
    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource = (*resource_cache)->Get(kKey1, std::nullopt);
    ASSERT_THAT(cached_resource, IsCode(NOT_FOUND));
  }
}

TEST_F(FileBackedResourceCacheTest, FileInManifestButNotInCacheDir) {
  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);
    ASSERT_OK(
        (*resource_cache)->Put(kKey1, Resource1(), Metadata(), absl::Hours(1)));
  }

  // Delete the file we just cached.
  std::filesystem::remove(cache_dir_ / kKey1);

  {
    auto resource_cache = FileBackedResourceCache::Create(
        root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
        kMaxCacheSizeBytes);
    ASSERT_OK(resource_cache);

    // Now we should gracefully fail even though the file is in the manifest but
    // not on disk.
    EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
    absl::StatusOr<FileBackedResourceCache::ResourceAndMetadata>
        cached_resource = (*resource_cache)->Get(kKey1, std::nullopt);
    ASSERT_THAT(cached_resource, IsCode(NOT_FOUND));
  }
}

}  // namespace
}  // namespace cache
}  // namespace client
}  // namespace fcp
