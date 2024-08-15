// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on a "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/escaping.h"
#include "gtest/gtest.h"
#include "nearby_protocol.h"
#include "np_cpp_test.h"
#include "shared_test_util.h"

// NOLINTBEGIN(readability-magic-numbers)
TEST_F(NpCppTest, ByteBufferMaxLength) {
  // Each hex byte takes up 2 characters so length 510 string = 255 bytes of hex
  auto str_bytes = generate_hex_string(510);
  auto bytes = absl::HexStringToBytes(str_bytes);
  auto buffer = nearby_protocol::ByteBuffer<
      nearby_protocol::MAX_ADV_PAYLOAD_SIZE>::TryFromString(bytes);
  ASSERT_TRUE(buffer.ok());
  auto string = buffer.value().ToString();
  ASSERT_EQ(bytes, string);
}

TEST_F(NpCppTest, ByteBufferArrayConstructor) {
  const std::array<uint8_t, 3> data{1, 2, 3};
  const nearby_protocol::ByteBuffer<nearby_protocol::MAX_ADV_PAYLOAD_SIZE>
      buffer(data);
  const std::vector<uint8_t> expected{1, 2, 3};
  ASSERT_EQ(expected, buffer.ToVector());
}

TEST_F(NpCppTest, ByteBufferTryFromSpan) {
  std::vector<uint8_t> data{1, 2, 3};
  auto buffer = nearby_protocol::ByteBuffer<3>::TryFromSpan(data);
  assert(buffer.ok());
  ASSERT_EQ(data, buffer->ToVector());
}

TEST_F(NpCppTest, ByteBufferTryFromSpanBufferNotFull) {
  std::vector<uint8_t> data{1, 2, 3};
  auto buffer = nearby_protocol::ByteBuffer<100>::TryFromSpan(data);
  assert(buffer.ok());
  ASSERT_EQ(data, buffer->ToVector());
}

TEST_F(NpCppTest, ByteBufferTryFromSpanInvalidLength) {
  std::vector<uint8_t> data{1, 2, 3};
  auto buffer = nearby_protocol::ByteBuffer<2>::TryFromSpan(data);
  ASSERT_FALSE(buffer.ok());
}

TEST_F(NpCppTest, ByteBufferTryFromSpanArray) {
  const std::array<uint8_t, 3> data{1, 2, 3};
  auto buffer = nearby_protocol::ByteBuffer<
      nearby_protocol::MAX_ADV_PAYLOAD_SIZE>::TryFromSpan(data);
  const std::vector<uint8_t> expected{1, 2, 3};
  ASSERT_EQ(expected, buffer->ToVector());
}

TEST_F(NpCppTest, ByteBufferTryFromSpanArrayInvalid) {
  std::array<uint8_t, 3> data{1, 2, 3};
  auto buffer = nearby_protocol::ByteBuffer<2>::TryFromSpan(data);
  ASSERT_FALSE(buffer.ok());
}

TEST_F(NpCppTest, ByteBufferInvalidLength) {
  // 256 bytes should fail
  auto str_bytes = generate_hex_string(512);
  auto bytes = absl::HexStringToBytes(str_bytes);
  auto buffer = nearby_protocol::ByteBuffer<
      nearby_protocol::MAX_ADV_PAYLOAD_SIZE>::TryFromString(bytes);
  ASSERT_FALSE(buffer.ok());
}

TEST_F(NpCppTest, ByteBufferRoundTrip) {
  auto bytes = absl::HexStringToBytes("2003031503");
  auto buffer = nearby_protocol::ByteBuffer<
      nearby_protocol::MAX_ADV_PAYLOAD_SIZE>::TryFromString(bytes);
  auto string = buffer.value().ToString();
  ASSERT_EQ(bytes, string);
}

TEST_F(NpCppTest, ByteBufferPayloadWrongSize) {
  auto bytes = absl::HexStringToBytes("1111111111111111111111");
  auto buffer = nearby_protocol::ByteBuffer<10>::TryFromString(bytes);
  ASSERT_FALSE(buffer.ok());
}

TEST_F(NpCppTest, ByteBufferEmptyString) {
  auto bytes = absl::HexStringToBytes("");
  auto buffer = nearby_protocol::ByteBuffer<10>::TryFromString(bytes);
  ASSERT_TRUE(buffer.ok());
}

TEST_F(NpCppTest, ByteBufferToVector) {
  auto bytes = absl::HexStringToBytes("1234567890");
  auto buffer = nearby_protocol::ByteBuffer<100>::TryFromString(bytes);
  auto vec = buffer.value().ToVector();
  const std::vector<uint8_t> expected{0x12, 0x34, 0x56, 0x78, 0x90};
  ASSERT_EQ(vec, expected);
}

TEST_F(NpCppTest, ByteBufferEndToEndPayloadAsString) {
  const std::string bytes = absl::HexStringToBytes("2003031503");
  auto buffer = nearby_protocol::ByteBuffer<
      nearby_protocol::MAX_ADV_PAYLOAD_SIZE>::TryFromString(bytes);
  ASSERT_TRUE(buffer.ok());

  const nearby_protocol::RawAdvertisementPayload adv(buffer.value());

  auto credential_slab = nearby_protocol::CredentialSlab::TryCreate().value();
  auto credential_book =
      nearby_protocol::CredentialBook::TryCreateFromSlab(credential_slab)
          .value();
  auto str = nearby_protocol::Deserializer::DeserializeAdvertisement(
                 adv, credential_book)
                 .IntoV1()
                 .TryGetSection(0)
                 .value()
                 .TryGetDataElement(0)
                 .value()
                 .GetPayload()
                 .ToString();
  ASSERT_EQ(str, absl::HexStringToBytes("03"));
}
// NOLINTEND(readability-magic-numbers)