/*
 * Copyright 2024 Google LLC
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

#include "fcp/confidentialcompute/client_payload.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "fcp/testing/testing.h"

namespace fcp {
namespace confidential_compute {
using ::testing::FieldsAre;

TEST(ClientPayloadTest, EncodeDecodeSucceeds) {
  ClientPayloadHeader header{
      .encrypted_symmetric_key = "encrypted_symmetric_key",
      .encapsulated_public_key = "encapsulated_public_key",
      .serialized_blob_header = "blob_header",
      .is_gzip_compressed = true,
  };

  std::string encoded = EncodeClientPayload(header, "ciphertext");
  absl::string_view encoded_view(encoded);
  absl::StatusOr<ClientPayloadHeader> decoded =
      DecodeAndConsumeClientPayloadHeader(encoded_view);
  ASSERT_OK(decoded);
  EXPECT_THAT(*decoded,
              FieldsAre("encrypted_symmetric_key", "encapsulated_public_key",
                        "blob_header", true));
  EXPECT_EQ(encoded_view, "ciphertext");
}

// While not particularly useful, there's no reason why encoding and decoding an
// empty/default-initialized ClientPayload shouldn't succeed.
TEST(ClientPayloadTest, EncodeDecodeDefaultValuesSucceeds) {
  ClientPayloadHeader payload;
  std::string encoded = EncodeClientPayload(payload, "");
  absl::string_view encoded_view(encoded);
  absl::StatusOr<ClientPayloadHeader> decoded =
      DecodeAndConsumeClientPayloadHeader(encoded_view);
  ASSERT_OK(decoded);
  EXPECT_THAT(*decoded, FieldsAre("", "", "", false));
  EXPECT_EQ(encoded_view, "");
}

TEST(ClientPayloadTest, EncodeDecodeCorruptedDataFails) {
  ClientPayloadHeader header{
      .encrypted_symmetric_key = "encrypted_symmetric_key",
      .encapsulated_public_key = "encapsulated_public_key",
      .serialized_blob_header = "blob_header",
      .is_gzip_compressed = true,
  };

  std::string encoded = EncodeClientPayload(header, "ciphertext");
  // Overwrite the first byte in the encoded string, which should cause the
  // decoding step to fail.
  encoded[0] = '\1';

  absl::string_view encoded_view(encoded);
  ASSERT_THAT(DecodeAndConsumeClientPayloadHeader(encoded_view),
              IsCode(absl::StatusCode::kInvalidArgument));
}

}  // namespace confidential_compute
}  // namespace fcp
