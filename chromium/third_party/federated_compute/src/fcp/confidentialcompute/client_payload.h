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

#ifndef FCP_CONFIDENTIALCOMPUTE_CLIENT_PAYLOAD_H_
#define FCP_CONFIDENTIALCOMPUTE_CLIENT_PAYLOAD_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/io/zero_copy_stream.h"

namespace fcp {
namespace confidential_compute {

// A header describing key details about a client's uploaded ciphertext.
struct ClientPayloadHeader {
  // The symmetric key used to encrypt the payload's ciphertext, encrypted using
  // HPKE. The key is encoded as a COSE_Key struct (RFC 9052); at least the
  // following algorithms should be supported:
  //   -65538: AEAD_AES_128_GCM_SIV (fixed nonce)
  std::string encrypted_symmetric_key;
  // The ephemeral Diffie-Hellman key needed to derive the symmetric key used
  // to encrypt `encrypted_symmetric_key`.
  std::string encapsulated_public_key;
  // The serialized `fcp.confidentialcompute.BlobHeader` message associated with
  // the ciphertext. This serves as the associated data for both the encrypted
  // cipher text and the `encrypted_symmetric_key`.
  std::string serialized_blob_header;
  // Whether gzip compression was applied to the data before it was encrypted.
  bool is_gzip_compressed = false;
};

// Encodes a client's payload into a format that can be uploaded via the
// Confidential Aggregations protocol.
std::string EncodeClientPayload(ClientPayloadHeader header,
                                absl::string_view ciphertext);

// Decodes a client payload that was uploaded via the Confidential Aggregations
// protocol, consuming the header from the encoded data, and updating the
// `encoded_data` view to point to the ciphertext.
absl::StatusOr<ClientPayloadHeader> DecodeAndConsumeClientPayloadHeader(
    absl::string_view& encoded_data);

// Variant of DecodeAndConsumeClientPayloadHeader that reads from a
// ZeroCopyInputStream. On success, the stream is positioned at the start of the
// ciphertext; on failure, the position is unspecified.
absl::StatusOr<ClientPayloadHeader> DecodeAndConsumeClientPayloadHeader(
    google::protobuf::io::ZeroCopyInputStream& stream);

}  // namespace confidential_compute
}  // namespace fcp

#endif  // FCP_CONFIDENTIALCOMPUTE_CLIENT_PAYLOAD_H_
