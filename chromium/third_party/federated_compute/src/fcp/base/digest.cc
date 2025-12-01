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

#include "fcp/base/digest.h"

#include <cstdint>
#include <string>

#include "absl/functional/function_ref.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "openssl/base.h"
#include "openssl/digest.h"

namespace fcp {

namespace {
static std::string ComputeSHA256(
    absl::FunctionRef<void(EVP_MD_CTX*)> digest_updater) {
  bssl::ScopedEVP_MD_CTX mdctx;
  const EVP_MD* sha256_md = EVP_sha256();
  FCP_CHECK(EVP_DigestInit_ex(mdctx.get(), sha256_md, nullptr));

  // Let the callback update the digest.
  digest_updater(mdctx.get());

  std::string result(EVP_MD_size(sha256_md), '\0');
  FCP_CHECK(EVP_DigestFinal_ex(
      mdctx.get(), reinterpret_cast<uint8_t*>(result.data()), nullptr));
  return result;
}
}  // namespace

std::string ComputeSHA256(absl::string_view data) {
  return ComputeSHA256([&data](EVP_MD_CTX* mdctx) {
    FCP_CHECK(EVP_DigestUpdate(mdctx, data.data(), data.length()));
  });
}

std::string ComputeSHA256(const absl::Cord& data) {
  return ComputeSHA256([&data](EVP_MD_CTX* mdctx) {
    for (auto chunk : data.Chunks()) {
      FCP_CHECK(EVP_DigestUpdate(mdctx, chunk.data(), chunk.length()));
    }
  });
}
}  // namespace fcp
