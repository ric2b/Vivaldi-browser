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

#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/protos/confidentialcompute/aggregation_client_payload.pb.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace fcp {
namespace confidential_compute {
using ::fcp::confidentialcompute::AggregationClientPayloadHeader;
using ::fcp::confidentialcompute::CompressionType;

inline constexpr absl::string_view kConfidentialAggPayloadV1MagicBytes = "CAv1";

std::string EncodeClientPayload(ClientPayloadHeader header,
                                absl::string_view ciphertext) {
  AggregationClientPayloadHeader payload_header;
  payload_header.set_encrypted_symmetric_key(
      std::move(header.encrypted_symmetric_key));
  payload_header.set_encapsulated_public_key(
      std::move(header.encapsulated_public_key));
  payload_header.set_blob_header(std::move(header.serialized_blob_header));
  payload_header.set_compression_type(
      header.is_gzip_compressed ? CompressionType::COMPRESSION_TYPE_GZIP
                                : CompressionType::COMPRESSION_TYPE_NONE);

  std::string serialized_payload = payload_header.SerializeAsString();

  // Encode a varint to indicate the length of the payload header.
  std::string header_len_varint;
  {
    google::protobuf::io::StringOutputStream out(&header_len_varint);
    google::protobuf::io::CodedOutputStream coded_out(&out);
    coded_out.WriteVarint32(static_cast<uint32_t>(serialized_payload.size()));
  }
  return absl::StrCat(
      // Prepend a constant byte pattern to each encoded payload. This could be
      // useful in the future if we want to switch to a non protobuf-based
      // header format, for whatever reason, by allowing us to distinguish
      // between this current format and whatever future format we may come up
      // with.
      kConfidentialAggPayloadV1MagicBytes, header_len_varint,
      serialized_payload, ciphertext);
}

absl::StatusOr<ClientPayloadHeader> DecodeAndConsumeClientPayloadHeader(
    absl::string_view& encoded_data) {
  if (encoded_data.size() > std::numeric_limits<int>::max()) {
    return absl::InvalidArgumentError("Unexpectedly large encoded data");
  }

  google::protobuf::io::ArrayInputStream stream(
      reinterpret_cast<const uint8_t*>(encoded_data.data()),
      static_cast<int>(encoded_data.size()));
  absl::StatusOr<ClientPayloadHeader> result =
      DecodeAndConsumeClientPayloadHeader(stream);
  if (result.ok()) {
    // We return a view into the `encoded_data` string, which allows us to avoid
    // copying the data.
    encoded_data = encoded_data.substr(stream.ByteCount());
  }
  return result;
}

absl::StatusOr<ClientPayloadHeader> DecodeAndConsumeClientPayloadHeader(
    google::protobuf::io::ZeroCopyInputStream& stream) {
  google::protobuf::io::CodedInputStream coded_in(&stream);
  // Verify that the stream starts with expected byte pattern.
  if (std::string magic_bytes;
      !coded_in.ReadString(&magic_bytes,
                           kConfidentialAggPayloadV1MagicBytes.size()) ||
      magic_bytes != kConfidentialAggPayloadV1MagicBytes) {
    return absl::InvalidArgumentError("Could not detect magic bytes prefix");
  }

  uint32_t serialized_payload_size;
  if (!coded_in.ReadVarint32(&serialized_payload_size)) {
    return absl::InvalidArgumentError("Could not read 'size' varint64");
  }

  auto limit = coded_in.PushLimit(serialized_payload_size);
  AggregationClientPayloadHeader payload_header;
  if (!payload_header.ParseFromCodedStream(&coded_in)) {
    return absl::InvalidArgumentError("Could not parse serialized payload");
  }
  coded_in.PopLimit(limit);

  return ClientPayloadHeader{
      .encrypted_symmetric_key =
          std::move(*payload_header.mutable_encrypted_symmetric_key()),
      .encapsulated_public_key =
          std::move(*payload_header.mutable_encapsulated_public_key()),
      .serialized_blob_header =
          std::move(*payload_header.mutable_blob_header()),
      .is_gzip_compressed = payload_header.compression_type() ==
                            CompressionType::COMPRESSION_TYPE_GZIP,
  };
}

}  // namespace confidential_compute
}  // namespace fcp
