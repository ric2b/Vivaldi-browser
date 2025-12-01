// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "fcp/confidentialcompute/nonce.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace confidential_compute {
namespace {

using ::fcp::confidentialcompute::BlobMetadata;

TEST(NonceTest, GetNextBlobNonceSucceedsAndIncrementsCounter) {
  std::string session_nonce = "session_nonce";
  NonceGenerator nonce_generator(session_nonce);

  uint32_t counter_0 = 0;
  std::string expected_blob_nonce_0(session_nonce.length() + sizeof(uint32_t),
                                    '\0');
  std::memcpy(expected_blob_nonce_0.data(), session_nonce.data(),
              session_nonce.length());
  std::memcpy(expected_blob_nonce_0.data() + session_nonce.length(), &counter_0,
              sizeof(uint32_t));

  uint32_t counter_1 = 1;
  std::string expected_blob_nonce_1(session_nonce.length() + sizeof(uint32_t),
                                    '\0');
  std::memcpy(expected_blob_nonce_1.data(), session_nonce.data(),
              session_nonce.length());
  std::memcpy(expected_blob_nonce_1.data() + session_nonce.length(), &counter_1,
              sizeof(uint32_t));

  absl::StatusOr<NonceAndCounter> actual_blob_nonce_0 =
      nonce_generator.GetNextBlobNonce();
  ASSERT_OK(actual_blob_nonce_0);
  ASSERT_EQ(actual_blob_nonce_0->blob_nonce, expected_blob_nonce_0);
  ASSERT_EQ(actual_blob_nonce_0->counter, counter_0);
  absl::StatusOr<NonceAndCounter> actual_blob_nonce_1 =
      nonce_generator.GetNextBlobNonce();
  ASSERT_OK(actual_blob_nonce_1);
  ASSERT_EQ(actual_blob_nonce_1->blob_nonce, expected_blob_nonce_1);
  ASSERT_EQ(actual_blob_nonce_1->counter, counter_1);
}

TEST(NonceTest, CheckBlobNonceSucceeds) {
  NonceChecker nonce_tracker;
  std::string session_nonce = nonce_tracker.GetSessionNonce();
  uint32_t counter = 0;
  std::string blob_nonce(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce.data(), session_nonce.data(), session_nonce.length());
  std::memcpy(blob_nonce.data() + session_nonce.length(), &counter,
              sizeof(uint32_t));
  BlobMetadata metadata;
  metadata.mutable_hpke_plus_aead_data()->set_counter(counter);
  metadata.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata).ok());
}

TEST(NonceTest, CheckBlobNonceUnencryptedSucceeds) {
  NonceChecker nonce_tracker;
  std::string nonce_and_counter =
      absl::StrCat(nonce_tracker.GetSessionNonce(), 0);
  BlobMetadata metadata;
  metadata.mutable_unencrypted();

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata).ok());
}

TEST(NonceTest, CheckBlobNonceWrongCounterFails) {
  NonceChecker nonce_tracker;
  std::string session_nonce = nonce_tracker.GetSessionNonce();
  uint32_t counter = 0;
  std::string blob_nonce(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce.data(), session_nonce.data(), session_nonce.length());
  std::memcpy(blob_nonce.data() + session_nonce.length(), &counter,
              sizeof(uint32_t));
  BlobMetadata metadata;
  metadata.mutable_hpke_plus_aead_data()->set_counter(counter);
  metadata.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata).ok());
  // Reusing metadata with the same counter fails.
  absl::Status result = nonce_tracker.CheckBlobNonce(metadata);
  ASSERT_EQ(result.code(), absl::StatusCode::kPermissionDenied);
  ASSERT_EQ(result.message(),
            "Blob counter 0 is less than the minimum expected value 1; caller "
            "may be attempting to reuse a previously seen nonce.");
}

TEST(NonceTest, CheckBlobNonceWrongNonceFails) {
  NonceChecker nonce_tracker;
  std::string session_nonce = "bad nonce";
  uint32_t counter = 0;
  std::string blob_nonce(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce.data(), session_nonce.data(), session_nonce.length());
  std::memcpy(blob_nonce.data() + session_nonce.length(), &counter,
              sizeof(uint32_t));
  BlobMetadata metadata;
  metadata.mutable_hpke_plus_aead_data()->set_counter(counter);
  metadata.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce);
  absl::Status result = nonce_tracker.CheckBlobNonce(metadata);
  ASSERT_EQ(result.code(), absl::StatusCode::kPermissionDenied);
  ASSERT_EQ(result.message(),
            "RewrappedAssociatedData nonce does not match the expected value.");
}

TEST(NonceTest, CheckBlobNonceSkippingCounters) {
  NonceChecker nonce_tracker;
  std::string session_nonce = nonce_tracker.GetSessionNonce();
  uint32_t counter_0 = 0;
  std::string blob_nonce_0(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce_0.data(), session_nonce.data(),
              session_nonce.length());
  std::memcpy(blob_nonce_0.data() + session_nonce.length(), &counter_0,
              sizeof(uint32_t));
  BlobMetadata metadata_0;
  metadata_0.mutable_hpke_plus_aead_data()->set_counter(counter_0);
  metadata_0.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_0);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata_0).ok());

  // Skipping to a greater counter value should succeed.
  uint32_t counter_5 = 5;
  std::string blob_nonce_5(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce_5.data(), session_nonce.data(),
              session_nonce.length());
  std::memcpy(blob_nonce_5.data() + session_nonce.length(), &counter_5,
              sizeof(uint32_t));
  BlobMetadata metadata_5;
  metadata_5.mutable_hpke_plus_aead_data()->set_counter(counter_5);
  metadata_5.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_5);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata_5).ok());

  // Using a smaller counter value should fail, even if that specific counter
  // has never been used.
  uint32_t counter_1 = 1;
  std::string blob_nonce_1(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce_1.data(), session_nonce.data(),
              session_nonce.length());
  std::memcpy(blob_nonce_1.data() + session_nonce.length(), &counter_1,
              sizeof(uint32_t));
  BlobMetadata metadata_1;
  metadata_1.mutable_hpke_plus_aead_data()->set_counter(counter_1);
  metadata_1.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_1);
  absl::Status result = nonce_tracker.CheckBlobNonce(metadata_1);
  ASSERT_EQ(result.code(), absl::StatusCode::kPermissionDenied);
  ASSERT_EQ(
      result.message(),
      "Blob counter 1 is less than the minimum expected value 6; caller may be "
      "attempting to reuse a previously seen nonce.");
}

TEST(NonceTest, CheckBlobNonceUnencryptedDoesNotAffectCounter) {
  NonceChecker nonce_tracker;
  std::string session_nonce = nonce_tracker.GetSessionNonce();
  uint32_t counter_0 = 0;
  std::string blob_nonce_0(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce_0.data(), session_nonce.data(),
              session_nonce.length());
  std::memcpy(blob_nonce_0.data() + session_nonce.length(), &counter_0,
              sizeof(uint32_t));
  BlobMetadata metadata_0;
  metadata_0.mutable_hpke_plus_aead_data()->set_counter(counter_0);
  metadata_0.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_0);
  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata_0).ok());

  BlobMetadata unencrypted_metadata;
  unencrypted_metadata.mutable_unencrypted();
  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(unencrypted_metadata).ok());

  uint32_t counter_1 = 1;
  std::string blob_nonce_1(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce_1.data(), session_nonce.data(),
              session_nonce.length());
  std::memcpy(blob_nonce_1.data() + session_nonce.length(), &counter_1,
              sizeof(uint32_t));
  BlobMetadata metadata_1;
  metadata_1.mutable_hpke_plus_aead_data()->set_counter(counter_1);
  metadata_1.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_1);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata_1).ok());
}

TEST(NonceTest, NonceGeneratorAndCheckerWorkTogether) {
  NonceChecker nonce_tracker;
  std::string session_nonce = nonce_tracker.GetSessionNonce();
  NonceGenerator nonce_generator(session_nonce);
  absl::StatusOr<NonceAndCounter> blob_nonce_0 =
      nonce_generator.GetNextBlobNonce();
  ASSERT_OK(blob_nonce_0);
  BlobMetadata metadata_0;
  metadata_0.mutable_hpke_plus_aead_data()->set_counter(blob_nonce_0->counter);
  metadata_0.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_0->blob_nonce);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata_0).ok());

  absl::StatusOr<NonceAndCounter> blob_nonce_1 =
      nonce_generator.GetNextBlobNonce();
  ASSERT_OK(blob_nonce_1);
  BlobMetadata metadata_1;
  metadata_1.mutable_hpke_plus_aead_data()->set_counter(blob_nonce_1->counter);
  metadata_1.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_1->blob_nonce);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata_1).ok());

  absl::StatusOr<NonceAndCounter> blob_nonce_2 =
      nonce_generator.GetNextBlobNonce();
  ASSERT_OK(blob_nonce_2);
  BlobMetadata metadata_2;
  metadata_2.mutable_hpke_plus_aead_data()->set_counter(blob_nonce_2->counter);
  metadata_2.mutable_hpke_plus_aead_data()
      ->mutable_rewrapped_symmetric_key_associated_data()
      ->set_nonce(blob_nonce_2->blob_nonce);

  ASSERT_TRUE(nonce_tracker.CheckBlobNonce(metadata_2).ok());
}

}  // namespace
}  // namespace confidential_compute
}  // namespace fcp
