/*
 * Copyright 2019 Google LLC
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

#include "fcp/base/random_token.h"

#include <string.h>

#include <string>

#include "absl/strings/escaping.h"
#include "fcp/base/monitoring.h"
#include "openssl/rand.h"

namespace fcp {

RandomToken RandomToken::Generate() {
  uint64_t words[2];
  static_assert(sizeof(words) == kRandomTokenSizeInBytes,
                "Should match the token size");
  int r = RAND_bytes(reinterpret_cast<unsigned char*>(words),
                     kRandomTokenSizeInBytes);
  FCP_CHECK(r == 1);
  return RandomToken(words[0], words[1]);
}

RandomToken RandomToken::FromBytes(absl::Span<char const> bytes) {
  FCP_CHECK(bytes.size() == kRandomTokenSizeInBytes);

  uint64_t words[2];
  static_assert(sizeof(words) == kRandomTokenSizeInBytes,
                "Should match the token size");
  memcpy(reinterpret_cast<char*>(words), bytes.data(), kRandomTokenSizeInBytes);
  return RandomToken(words[0], words[1]);
}

std::array<char, kRandomTokenSizeInBytes> RandomToken::ToBytes() const {
  std::array<char, kRandomTokenSizeInBytes> bytes;
  memcpy(bytes.data(), reinterpret_cast<char const*>(words_),
         kRandomTokenSizeInBytes);
  return bytes;
}

std::string RandomToken::ToString() const {
  return std::string(reinterpret_cast<char const*>(words_),
                     kRandomTokenSizeInBytes);
}

std::string RandomToken::ToPrintableString() const {
  return absl::BytesToHexString(absl::string_view(
      reinterpret_cast<char const*>(words_), kRandomTokenSizeInBytes));
}

}  // namespace fcp
