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

#include "nearby_protocol.h"
#include "shared_test_util.h"

// needed for running directly against the C API (ie: bypassing the C++ wrapper)
#include "benchmark/benchmark.h"
#include "np_cpp_ffi_functions.h"
#include "np_cpp_ffi_types.h"

nearby_protocol::CredentialBook CreateEmptyCredBook() {
  nearby_protocol::CredentialSlab cred_slab;
  nearby_protocol::CredentialBook cred_book(cred_slab);
  return std::move(cred_book);
}

void V0Plaintext(benchmark::State &state) {
  auto cred_book = CreateEmptyCredBook();
  auto num_advs = state.range(0);
  for ([[maybe_unused]] auto _ : state) {
    for (int i = 0; i < num_advs; i++) {
      auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
          V0AdvPlaintextMultiDe, cred_book);
      assert(result.GetKind() ==
             nearby_protocol::DeserializeAdvertisementResultKind::V0);
    }
  }
}
BENCHMARK(V0Plaintext)
    ->RangeMultiplier(10)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

void V1Plaintext(benchmark::State &state) {
  auto cred_book = CreateEmptyCredBook();
  auto num_advs = state.range(0);
  for ([[maybe_unused]] auto _ : state) {
    for (int i = 0; i < num_advs; i++) {
      auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
          V1AdvPlaintext, cred_book);
      assert(result.GetKind() ==
             nearby_protocol::DeserializeAdvertisementResultKind::V1);
    }
  }
}
BENCHMARK(V1Plaintext)
    ->RangeMultiplier(10)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

class V0Encrypted : public benchmark::Fixture {
 public:
  std::optional<nearby_protocol::CredentialBook> cred_book_;
  void SetUp(const ::benchmark::State &state) override {
    // populate credential book
    auto num_creds = state.range(0);

    nearby_protocol::CredentialSlab slab;
    for (int i = 1; i < num_creds; i++) {
      auto credential = GenerateRandomCredentialV0();
      slab.AddV0Credential(credential);
    }

    // now at the end of the list add the matching credential
    nearby_protocol::MatchedCredentialData match_data(123,
                                                      V0AdvEncryptedMetadata);
    std::array<uint8_t, 32> key_seed = {};
    std::fill_n(key_seed.begin(), 32, 0x11);
    nearby_protocol::V0MatchableCredential v0_cred(
        key_seed, V0AdvLegacyIdentityTokenHmac, match_data);
    slab.AddV0Credential(v0_cred);

    nearby_protocol::CredentialBook cred_book(slab);
    cred_book_ = std::move(cred_book);
  }
  void TearDown(const ::benchmark::State &state) override {}
};

BENCHMARK_DEFINE_F(V0Encrypted, SingleMatchingCredential)
(benchmark::State &state) {
  for ([[maybe_unused]] auto _ : state) {
    // now that the credentials have been  loaded attempt to deserialize
    auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
        V0AdvEncryptedPayload, cred_book_.value());
    benchmark::DoNotOptimize(result);

    // make sure this succeeded and that we could correctly decrypt the adv
    assert(result.GetKind() ==
           nearby_protocol::DeserializeAdvertisementResultKind::V0);
    assert(result.IntoV0().IntoLegible().GetIdentityKind() ==
           nearby_protocol::DeserializedV0IdentityKind::Decrypted);
  }
}

BENCHMARK_REGISTER_F(V0Encrypted, SingleMatchingCredential)
    ->RangeMultiplier(10)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

class V1SigEncryptedSingleSection : public benchmark::Fixture {
 public:
  std::optional<nearby_protocol::CredentialBook> cred_book_;
  void SetUp(const ::benchmark::State &state) override {
    // populate credential book
    auto num_creds = state.range(0);
    nearby_protocol::CredentialSlab slab;
    for (int i = 1; i < num_creds; i++) {
      auto credential = GenerateRandomCredentialV1();
      auto result = slab.AddV1Credential(credential);
      assert(result.ok());
    }
    // now at the end of the list add the matching credential
    nearby_protocol::MatchedCredentialData match_data(123,
                                                      V1AdvEncryptedMetadata);
    nearby_protocol::V1MatchableCredential v1_cred(
        V1AdvKeySeed, V1AdvExpectedMicExtendedSaltIdentityTokenHmac,
        V1AdvExpectedSignatureIdentityTokenHmac, V1AdvPublicKey, match_data);
    auto add_result = slab.AddV1Credential(v1_cred);
    assert(add_result.ok());
    nearby_protocol::CredentialBook cred_book(slab);
    cred_book_ = std::move(cred_book);
  }
  void TearDown(const ::benchmark::State &state) override {}
};

BENCHMARK_DEFINE_F(V1SigEncryptedSingleSection, SingleMatchingCredential)
(benchmark::State &state) {
  for ([[maybe_unused]] auto _ : state) {
    // now that the credentials have been  loaded attempt to deserialize
    auto result = nearby_protocol::Deserializer::DeserializeAdvertisement(
        V1AdvEncrypted, cred_book_.value());
    benchmark::DoNotOptimize(result);

    // make sure this succeeded and that we could correctly decrypt the adv
    assert(result.GetKind() ==
           nearby_protocol::DeserializeAdvertisementResultKind::V1);
    assert(result.IntoV1().GetNumLegibleSections() == 1);
  }
}

BENCHMARK_REGISTER_F(V1SigEncryptedSingleSection, SingleMatchingCredential)
    ->RangeMultiplier(10)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

struct V1CredentialData {
  uint32_t cred_id;
  std::array<uint8_t, 32> key_seed;
  std::array<uint8_t, 32> expected_unsigned_metadata_key_hmac;
  std::array<uint8_t, 32> expected_signed_metadata_key_hmac;
  std::array<uint8_t, 32> pub_key;
  std::array<uint8_t, 500> encrypted_metadata_bytes;
};

V1CredentialData GenerateData() {
  return {
      .cred_id = static_cast<uint32_t>(rand()),
      .key_seed = create_random_array<32>(),
      .expected_unsigned_metadata_key_hmac = create_random_array<32>(),
      .expected_signed_metadata_key_hmac = create_random_array<32>(),
      .pub_key = create_random_array<32>(),
      .encrypted_metadata_bytes = create_random_array<500>(),
  };
}

class LoadCredentialBook : public benchmark::Fixture {
 public:
  std::vector<V1CredentialData> creds_;
  // generate all the data in setup so the time for generation is not included
  // in the measurement
  void SetUp(const ::benchmark::State &state) override {
    // populate credential book
    auto num_creds = state.range(0);
    for (int i = 1; i < num_creds; i++) {
      auto credential = GenerateData();
      creds_.push_back(credential);
    }
  }
};

BENCHMARK_DEFINE_F(LoadCredentialBook, SingleMatchingCredential)
(benchmark::State &state) {
  for ([[maybe_unused]] auto _ : state) {
    nearby_protocol::CredentialSlab slab;
    for (auto cred : creds_) {
      nearby_protocol::MatchedCredentialData m(cred.cred_id,
                                               cred.encrypted_metadata_bytes);
      nearby_protocol::V1MatchableCredential v1_cred(
          cred.key_seed, cred.expected_unsigned_metadata_key_hmac,
          cred.expected_signed_metadata_key_hmac, cred.pub_key, m);
      auto result = slab.AddV1Credential(v1_cred);
      assert(result.ok());
    }
    nearby_protocol::CredentialBook book(slab);
    benchmark::DoNotOptimize(book);
  }
}

BENCHMARK_REGISTER_F(LoadCredentialBook, SingleMatchingCredential)
    ->RangeMultiplier(10)
    ->Range(1, 1000)
    ->Unit(benchmark::kMicrosecond);

BENCHMARK_MAIN();
