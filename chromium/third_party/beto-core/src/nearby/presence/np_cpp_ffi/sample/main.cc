// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <bitset>
#include <iostream>
#include <string>
#include <utility>

#include "absl/strings/escaping.h"
#include "nearby_protocol.h"
#include "np_cpp_ffi_types.h"

void SamplePanicHandler(nearby_protocol::PanicReason reason);

void HandleAdvertisementResult(
    nearby_protocol::DeserializeAdvertisementResult /*result*/);

void HandleV0Adv(nearby_protocol::DeserializedV0Advertisement /*result*/);
void HandleLegibleV0Adv(
    nearby_protocol::LegibleDeserializedV0Advertisement /*legible_adv*/);
void HandleV0IdentityKind(
    nearby_protocol::DeserializedV0IdentityKind /*identity*/);
void HandleDataElement(nearby_protocol::V0DataElement /*de*/);

void HandleV1Adv(nearby_protocol::DeserializedV1Advertisement /*adv*/);
void HandleV1Section(const nearby_protocol::DeserializedV1Section& /*section*/);
void HandleV1DataElement(nearby_protocol::V1DataElement /*de*/);

int main() {
  auto result =
      nearby_protocol::GlobalConfig::SetPanicHandler(SamplePanicHandler);
  if (result) {
    std::cout << "Successfully registered panic handler\n";
  } else {
    std::cout << "Failed register panic handler\n";
    return -1;
  }
  nearby_protocol::GlobalConfig::SetNumShards(4);

  std::cout << "\n========= Example V0 Adv ==========\n";
  std::cout << "Hex bytes: 00031503260046\n\n";

  nearby_protocol::CredentialSlab credential_slab;
  nearby_protocol::CredentialBook credential_book(credential_slab);

  const auto* v0_byte_string =
      "00"       // Adv Header
      "03"       // Public DE header
      "1503"     // Length 1 Tx Power DE with value 3
      "260046";  // Length 2 Actions

  std::string v0_bytes;
  if (!absl::HexStringToBytes(v0_byte_string, &v0_bytes)) {
    return -1;
  }
  auto v0_buffer = nearby_protocol::ByteBuffer<255>::TryFromString(v0_bytes);
  nearby_protocol::RawAdvertisementPayload const v0_payload(v0_buffer.value());

  // Try to deserialize a V0 payload
  auto deserialize_v0_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(v0_payload,
                                                              credential_book);
  HandleAdvertisementResult(std::move(deserialize_v0_result));

  std::cout << "\n========= Example V1 Adv ==========\n";
  std::cout << "Hex bytes: 20040326004603031505\n\n";

  const auto* v1_byte_string =
      "20"       // V1 Advertisement header
      "04"       // Section Header
      "03"       // Public Identity DE header
      "260046";  // Length 2 Actions DE

  std::string v1_bytes;
  if (!absl::HexStringToBytes(v1_byte_string, &v1_bytes)) {
    return -1;
  }
  auto v1_buffer = nearby_protocol::ByteBuffer<255>::TryFromString(v1_bytes);
  nearby_protocol::RawAdvertisementPayload const v1_payload(v1_buffer.value());

  // Try to deserialize a V1 payload
  auto deserialize_v1_result =
      nearby_protocol::Deserializer::DeserializeAdvertisement(v1_payload,
                                                              credential_book);
  HandleAdvertisementResult(std::move(deserialize_v1_result));

  std::cout << "\n========= User input sample ==========\n\n";
  while (true) {
    std::string user_input;
    std::cout << "Enter the hex of the advertisement you would like to parse "
                 "(see above examples): ";
    std::cin >> user_input;
    std::string bytes;
    auto hex_result = absl::HexStringToBytes(user_input, &bytes);
    if (!hex_result) {
      std::cout << "Provided string is not valid hex";
      continue;
    }
    auto buffer = nearby_protocol::ByteBuffer<255>::TryFromString(bytes);
    if (!buffer.ok()) {
      std::cout << "Too many bytes provided, must fit into a max length 255 "
                   "byte BLE advertisement\n";
      continue;
    }
    nearby_protocol::RawAdvertisementPayload const user_input_payload(
        buffer.value());

    // Try to deserialize user input
    auto user_input_result =
        nearby_protocol::Deserializer::DeserializeAdvertisement(
            user_input_payload, credential_book);
    HandleAdvertisementResult(std::move(user_input_result));

    char choice;
    do {
      std::cout << "Do you want to continue? (Y/N) ";
      std::cin >> choice;
    } while (choice != 'Y' && choice != 'N' && choice != 'n' && choice != 'y');

    if (choice == 'N' || choice == 'n') {
      return 0;
    }
  }
}

void SamplePanicHandler(nearby_protocol::PanicReason reason) {
  std::cout << "Panicking! Reason: ";
  switch (reason) {
    case nearby_protocol::PanicReason::EnumCastFailed: {
      std::cout << "EnumCastFailed \n";
      break;
    }
    case nearby_protocol::PanicReason::AssertFailed: {
      std::cout << "AssertFailed \n";
      break;
    }
    case nearby_protocol::PanicReason::InvalidStackDataStructure: {
      std::cout << "InvalidStackDataStructure \n";
      break;
    }
    case np_ffi::internal::PanicReason::ExceededMaxHandleAllocations:
      std::cout << "ExceededMaxHandleAllocations \n";
      break;
  }
  std::abort();
}

void HandleAdvertisementResult(
    nearby_protocol::DeserializeAdvertisementResult result) {
  switch (result.GetKind()) {
    case nearby_protocol::DeserializeAdvertisementResultKind::Error:
      std::cout << "Error in deserializing advertisement!\n";
      break;
    case nearby_protocol::DeserializeAdvertisementResultKind::V0:
      std::cout << "Successfully deserialized a V0 advertisement!\n";
      HandleV0Adv(result.IntoV0());
      break;
    case nearby_protocol::DeserializeAdvertisementResultKind::V1:
      std::cout << "Successfully deserialized a V1 advertisement\n";
      HandleV1Adv(result.IntoV1());
      break;
  }
}

void HandleV0Adv(nearby_protocol::DeserializedV0Advertisement result) {
  switch (result.GetKind()) {
    case nearby_protocol::DeserializedV0AdvertisementKind::Legible:
      std::cout << "\tThe Advertisement is plaintext \n";
      HandleLegibleV0Adv(result.IntoLegible());
      break;
    case nearby_protocol::DeserializedV0AdvertisementKind::
        NoMatchingCredentials:
      std::cout << "\tNo matching credentials found for this adv\n";
      return;
  }
}

void HandleLegibleV0Adv(
    nearby_protocol::LegibleDeserializedV0Advertisement legible_adv) {
  HandleV0IdentityKind(legible_adv.GetIdentityKind());

  auto num_des = legible_adv.GetNumberOfDataElements();
  std::cout << "\t\tAdv contains " << static_cast<unsigned>(num_des)
            << " data elements \n";
  auto payload = legible_adv.IntoPayload();
  for (int i = 0; i < num_des; i++) {
    auto de_result = payload.TryGetDataElement(i);
    if (!de_result.ok()) {
      std::cout << "\t\tError getting DE at index: " << i << "\n";
      return;
    }
    std::cout << "\t\tSuccessfully retrieved De at index " << i << "\n";
    HandleDataElement(de_result.value());
  }
}

void HandleV0IdentityKind(
    nearby_protocol::DeserializedV0IdentityKind identity) {
  switch (identity) {
    case np_ffi::internal::DeserializedV0IdentityKind::Plaintext: {
      std::cout << "\t\tIdentity is Plaintext\n";
      break;
    }
    case np_ffi::internal::DeserializedV0IdentityKind::Decrypted: {
      std::cout << "\t\tIdentity is Encrypted\n";
      break;
    }
  }
}

void HandleDataElement(nearby_protocol::V0DataElement de) {
  switch (de.GetKind()) {
    case nearby_protocol::V0DataElementKind::TxPower: {
      std::cout << "\t\t\tDE Type is TxPower\n";
      auto tx_power = de.AsTxPower();
      std::cout << "\t\t\tpower: " << static_cast<int>(tx_power.GetAsI8())
                << "\n";
      return;
    }
    case nearby_protocol::V0DataElementKind::Actions: {
      std::cout << "\t\t\tDE Type is Actions\n";
      auto actions = de.AsActions();
      std::cout << "\t\t\tactions: " << std::bitset<32>(actions.GetAsU32())
                << "\n";
      return;
    }
  }
}

void HandleV1Adv(nearby_protocol::DeserializedV1Advertisement adv) {
  auto legible_sections = adv.GetNumLegibleSections();
  std::cout << "\tAdv has " << static_cast<unsigned>(legible_sections)
            << " legible sections \n";

  auto encrypted_sections = adv.GetNumUndecryptableSections();
  std::cout << "\tAdv has " << static_cast<unsigned>(encrypted_sections)
            << " undecryptable sections\n";

  for (auto i = 0; i < legible_sections; i++) {
    auto section_result = adv.TryGetSection(i);
    if (!section_result.ok()) {
      std::cout << "\tError getting Section at index: " << i << "\n";
      return;
    }
    std::cout << "\tSuccessfully retrieved section at index " << i << "\n";
    HandleV1Section(section_result.value());
  }
}

void HandleV1Section(const nearby_protocol::DeserializedV1Section& section) {
  switch (section.GetIdentityKind()) {
    case np_ffi::internal::DeserializedV1IdentityKind::Plaintext: {
      std::cout << "\t\tIdentity is Plaintext\n";
      break;
    }
    case np_ffi::internal::DeserializedV1IdentityKind::Decrypted: {
      std::cout << "\t\tIdentity is Encrypted\n";
      break;
    }
  }

  auto num_des = section.NumberOfDataElements();
  std::cout << "\t\tSection has " << static_cast<unsigned>(num_des)
            << " data elements \n";
  for (auto i = 0; i < num_des; i++) {
    auto de_result = section.TryGetDataElement(i);
    if (!de_result.ok()) {
      std::cout << "\t\tError getting de at index: " << i << "\n";
      return;
    }
    std::cout << "\t\tSuccessfully retrieved data element at index " << i
              << "\n";
    HandleV1DataElement(de_result.value());
  }
}

void HandleV1DataElement(nearby_protocol::V1DataElement de) {
  std::cout << "\t\t\tData Element type code: "
            << static_cast<unsigned>(de.GetDataElementTypeCode()) << "\n";
  std::cout << "\t\t\tPayload bytes as hex: "
            << absl::BytesToHexString(de.GetPayload().ToString()) << "\n";
}
