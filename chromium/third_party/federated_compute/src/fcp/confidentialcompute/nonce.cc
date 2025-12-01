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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "openssl/rand.h"

namespace fcp {
namespace confidential_compute {
namespace {
// The size of the session-level nonce.
constexpr size_t kNonceSize = 16;

// Generates a blob-level nonce from a session-level nonce and a blob counter.
std::string GenerateBlobNonce(std::string session_nonce, uint32_t counter) {
  // We assume that the untrusted and trusted code are running on a machine with
  // the same endianness.
  std::string blob_nonce(session_nonce.length() + sizeof(uint32_t), '\0');
  std::memcpy(blob_nonce.data(), session_nonce.data(), session_nonce.length());
  std::memcpy(blob_nonce.data() + session_nonce.length(), &counter,
              sizeof(uint32_t));
  return blob_nonce;
}

}  // namespace

NonceChecker::NonceChecker() {
  std::string nonce(kNonceSize, '\0');
  // BoringSSL documentation says that it always returns 1 so we don't check
  // the return value.
  (void)RAND_bytes(reinterpret_cast<unsigned char*>(nonce.data()),
                   nonce.size());
  session_nonce_ = std::move(nonce);
}

absl::StatusOr<NonceAndCounter> NonceGenerator::GetNextBlobNonce() {
  if (counter_ == UINT32_MAX) {
    return absl::InternalError("Counter has overflowed.");
  }
  std::string next_blob_nonce = GenerateBlobNonce(session_nonce_, counter_);

  NonceAndCounter result = {.blob_nonce = next_blob_nonce, .counter = counter_};
  counter_++;
  return result;
}

absl::Status NonceChecker::CheckBlobNonce(
    const confidentialcompute::BlobMetadata& metadata) {
  if (metadata.has_unencrypted()) {
    return absl::OkStatus();
  }

  if (metadata.hpke_plus_aead_data().counter() == UINT32_MAX) {
    return absl::InternalError("Counter has overflowed.");
  }
  if (metadata.hpke_plus_aead_data().counter() < counter_) {
    return absl::PermissionDeniedError(
        absl::StrFormat("Blob counter %d is less than the minimum expected "
                        "value %d; caller may be "
                        "attempting to reuse a previously seen nonce.",
                        metadata.hpke_plus_aead_data().counter(), counter_));
  }
  if (metadata.hpke_plus_aead_data()
          .rewrapped_symmetric_key_associated_data()
          .nonce() !=
      GenerateBlobNonce(session_nonce_,
                        metadata.hpke_plus_aead_data().counter())) {
    return absl::PermissionDeniedError(
        "RewrappedAssociatedData nonce does not match the expected value.");
  }
  counter_ = metadata.hpke_plus_aead_data().counter() + 1;
  return absl::OkStatus();
}

}  // namespace confidential_compute
}  // namespace fcp
