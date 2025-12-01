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

// This file hosts utility functions that return known-good values (e.g. an
// encryption config that is known to be correctly rooted in a hardware root of
// trust), for use in tests.

#ifndef FCP_CLIENT_ATTESTATION_TEST_VALUES_H_
#define FCP_CLIENT_ATTESTATION_TEST_VALUES_H_

#include "fcp/protos/federatedcompute/confidential_encryption_config.pb.h"
#include "proto/attestation/reference_value.pb.h"

namespace fcp::client::attestation::test_values {

// Returns a config that is known to be rooted in a hardware root of trust.
google::internal::federatedcompute::v1::ConfidentialEncryptionConfig
GetKnownValidEncryptionConfig();

// Returns reference values which should successfully verify the above
// encryption config.
oak::attestation::v1::ReferenceValues GetKnownValidReferenceValues();

// Returns reference values which skip all verifications.
oak::attestation::v1::ReferenceValues GetSkipAllReferenceValues();

}  // namespace fcp::client::attestation::test_values

#endif  // FCP_CLIENT_ATTESTATION_TEST_VALUES_H_
