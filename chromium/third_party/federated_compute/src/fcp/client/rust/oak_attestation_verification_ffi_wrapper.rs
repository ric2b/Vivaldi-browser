#![no_std]

extern crate alloc;

use alloc::{boxed::Box, format};
use oak_proto_rust::oak::attestation::v1::{
    AttestationResults, EndorsementReferenceValue, Endorsements, Evidence, ReferenceValues,
    SignedEndorsement,
};
use prost::Message;

/// Represents a boxed Rust slice containing serialized data (like a proto or
/// string) held in a contiguous buffer owned by this struct, to be returned
/// over the FFI boundary.
///
/// Every instance of this struct returned over an FFI boundary should be
/// released using the
/// `fcp_rs_oak_attestation_verification_free_attestation_result`.
#[repr(C)]
pub struct SerializedResult {
    /// A pointer to the serialized data (if `size > 0`), or a null pointer (if
    /// `size == 0`).
    pub data: *mut u8,
    /// The size of the buffer.
    pub size: usize,
    /// Indicates whether the data is a serialized error or payload.
    pub is_err: bool,
}

#[no_mangle]
pub unsafe extern "C" fn fcp_rs_oak_attestation_verification_verify_attestation<'a>(
    now_utc_millis: i64,
    serialized_evidence: *const u8,
    serialized_evidence_size: usize,
    serialized_endorsements: *const u8,
    serialized_endorsements_size: usize,
    serialized_reference_values: *const u8,
    serialized_reference_values_size: usize,
    use_policy_api: bool,
) -> SerializedResult {
    let evidence = Evidence::decode(core::slice::from_raw_parts(
        serialized_evidence,
        serialized_evidence_size,
    ))
    .unwrap();
    let endorsements = Endorsements::decode(core::slice::from_raw_parts(
        serialized_endorsements,
        serialized_endorsements_size,
    ))
    .unwrap();
    let reference_values = ReferenceValues::decode(core::slice::from_raw_parts(
        serialized_reference_values,
        serialized_reference_values_size,
    ))
    .unwrap();

    let result: AttestationResults = oak_attestation_verification_wrapper::verify_attestation(
        now_utc_millis,
        &evidence,
        &endorsements,
        &reference_values,
        use_policy_api,
    );

    let serialized_result: Box<[u8]> = result.encode_to_vec().into_boxed_slice();
    // If the serialized result is an empty slice, then just return a null buffer
    // and 0 size. This ensures that we'll always be returning a valid pointer
    // pointing to a non-empty buffer below.
    if serialized_result.is_empty() {
        return SerializedResult { data: core::ptr::null_mut(), size: 0, is_err: false };
    }

    let static_serialized_result: &'static mut [u8] = Box::<[u8]>::leak(serialized_result);
    SerializedResult {
        data: static_serialized_result.as_mut_ptr(),
        size: static_serialized_result.len(),
        is_err: false,
    }
}

/// Verifies a signed endorsement given a reference value.
///
/// `now_utc_millis` is the current time in milliseconds since the Unix epoch.
/// The pointers `serialized_signed_endorsement` and
/// `serialized_reference_value` must contain serialized protos of type
/// `SignedEndorsement` and `EndorsementReferenceValue` (respectively), and must
/// remain valid for the duration of the call.
///
/// The returned result is either a serialized `EndorsementDetails` proto (on
/// success) or a string indicating the error (on failure).
#[no_mangle]
pub unsafe extern "C" fn fcp_rs_oak_attestation_verification_verify_endorsement<'a>(
    now_utc_millis: i64,
    serialized_signed_endorsement: *const u8,
    serialized_signed_endorsement_size: usize,
    serialized_reference_value: *const u8,
    serialized_reference_value_size: usize,
) -> SerializedResult {
    if serialized_signed_endorsement.is_null() || serialized_reference_value.is_null() {
        panic!("null pointer not allowed");
    }
    let signed_endorsement = SignedEndorsement::decode(core::slice::from_raw_parts(
        serialized_signed_endorsement,
        serialized_signed_endorsement_size,
    ))
    .unwrap();
    let ref_value = EndorsementReferenceValue::decode(core::slice::from_raw_parts(
        serialized_reference_value,
        serialized_reference_value_size,
    ))
    .unwrap();

    let result = oak_attestation_verification::verify_endorsement(
        now_utc_millis,
        &signed_endorsement,
        &ref_value,
    );

    if result.is_ok() {
        let serialized_result: Box<[u8]> = result.ok().unwrap().encode_to_vec().into_boxed_slice();
        let static_serialized_result: &'static mut [u8] = Box::<[u8]>::leak(serialized_result);
        SerializedResult {
            data: static_serialized_result.as_mut_ptr(),
            size: static_serialized_result.len(),
            is_err: false,
        }
    } else {
        let err = result.err().unwrap();
        let serialized_err: Box<str> = format!("{:#?}", err).into_boxed_str();
        let static_serialized_err: &'static mut str = Box::<str>::leak(serialized_err);
        SerializedResult {
            data: static_serialized_err.as_mut_ptr(),
            size: static_serialized_err.len(),
            is_err: true,
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn fcp_rs_oak_attestation_verification_free_result<'a>(
    result: SerializedResult,
) {
    // Turn the pointer & size back into a boxed slice and drop it, to ensure the
    // memory is released, but only if the slice is non-empty (since we
    // only return a valid pointer if there was any data to return in the first
    // place).
    if result.size > 0 {
        assert!(!result.data.is_null());
        drop(Box::from_raw(core::slice::from_raw_parts_mut(result.data, result.size)))
    }
}
