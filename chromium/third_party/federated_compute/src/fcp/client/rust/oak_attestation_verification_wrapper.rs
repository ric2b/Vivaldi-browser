// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#![no_std]

extern crate alloc;

use alloc::{boxed::Box, format, sync::Arc, vec};
use oak_attestation_verification::{
    results::{get_hybrid_encryption_public_key, get_signing_public_key},
    AmdSevSnpDiceAttestationVerifier, AmdSevSnpPolicy, ContainerPolicy, FirmwarePolicy,
    InsecureAttestationVerifier, KernelPolicy, SystemPolicy,
};
use oak_attestation_verification_types::verifier::AttestationVerifier;
use oak_proto_rust::oak::attestation::v1::{
    attestation_results::Status, reference_values, AmdSevReferenceValues, AttestationResults,
    Endorsements, Evidence, ExtractedEvidence, OakContainersReferenceValues, ReferenceValues,
    RootLayerReferenceValues,
};
use oak_time::{clock::FixedClock, Instant};

/// Verifies Oak attestation evidence and endorsements given reference values.
pub fn verify_attestation(
    now_utc_millis: i64,
    evidence: &Evidence,
    endorsements: &Endorsements,
    reference_values: &ReferenceValues,
    use_policy_api: bool,
) -> AttestationResults {
    let clock = Arc::new(FixedClock::at_instant(Instant::from_unix_millis(now_utc_millis)));
    let verifier: Box<dyn AttestationVerifier> = match &reference_values.r#type {
        // Oak Containers (insecure)
        Some(reference_values::Type::OakContainers(OakContainersReferenceValues {
            root_layer: Some(RootLayerReferenceValues { insecure: Some(_), .. }),
            kernel_layer: Some(kernel_ref_vals),
            system_layer: Some(system_ref_vals),
            container_layer: Some(container_ref_vals),
        })) if use_policy_api => Box::new(InsecureAttestationVerifier::new(
            clock,
            vec![
                Box::new(KernelPolicy::new(kernel_ref_vals)),
                Box::new(SystemPolicy::new(system_ref_vals)),
                Box::new(ContainerPolicy::new(container_ref_vals)),
            ],
        )),

        // Oak Containers (AMD SEV-SNP)
        Some(reference_values::Type::OakContainers(OakContainersReferenceValues {
            root_layer:
                Some(RootLayerReferenceValues {
                    amd_sev:
                        Some(
                            amd_sev_ref_vals @ AmdSevReferenceValues {
                                stage0: Some(stage0_ref_vals),
                                ..
                            },
                        ),
                    insecure: None,
                    ..
                }),
            kernel_layer: Some(kernel_ref_vals),
            system_layer: Some(system_ref_vals),
            container_layer: Some(container_ref_vals),
        })) if use_policy_api => Box::new(AmdSevSnpDiceAttestationVerifier::new(
            AmdSevSnpPolicy::new(amd_sev_ref_vals),
            Box::new(FirmwarePolicy::new(stage0_ref_vals)),
            vec![
                Box::new(KernelPolicy::new(kernel_ref_vals)),
                Box::new(SystemPolicy::new(system_ref_vals)),
                Box::new(ContainerPolicy::new(container_ref_vals)),
            ],
            clock,
        )),

        // Use the legacy verification API for the Restricted Kernel since the Restricted Kernel
        // does not currently include application keys in the attestation events.
        _ => {
            return oak_attestation_verification::verifier::to_attestation_results(
                &oak_attestation_verification::verifier::verify(
                    now_utc_millis,
                    &evidence,
                    &endorsements,
                    &reference_values,
                ),
            );
        }
    };

    let mut results =
        verifier.verify(evidence, endorsements).unwrap_or_else(|err| AttestationResults {
            status: Status::GenericFailure.into(),
            reason: format!("{:#?}", err),
            ..Default::default()
        });
    if results.status != Status::Success as i32 {
        return results;
    }

    // Callers expect the extracted evidence to be populated.
    if results.extracted_evidence.is_none() {
        results.extracted_evidence = Some(ExtractedEvidence {
            signing_public_key: get_signing_public_key(&results).cloned().unwrap_or_default(),
            encryption_public_key: get_hybrid_encryption_public_key(&results)
                .cloned()
                .unwrap_or_default(),
            ..Default::default()
        });
    }
    results
}
