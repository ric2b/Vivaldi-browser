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

//! NP Rust C FFI functionality common to V0 ser/deser flows.

use crate::{panic, panic_if_invalid, unwrap, PanicReason};
use np_ffi_core::serialize::AdvertisementBuilderKind;
use np_ffi_core::utils::FfiEnum;
use np_ffi_core::v0::*;

/// Gets the tag of a `V0DataElement` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_V0DataElement_kind(de: V0DataElement) -> V0DataElementKind {
    de.kind()
}

/// Casts a `V0DataElement` to the `TxPower` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_V0DataElement_into_TX_POWER(de: V0DataElement) -> TxPower {
    unwrap(de.into_tx_power(), PanicReason::EnumCastFailed)
}

/// Upcasts a Tx power DE to a generic V0 data-element.
#[no_mangle]
pub extern "C" fn np_ffi_TxPower_into_V0DataElement(tx_power: TxPower) -> V0DataElement {
    V0DataElement::TxPower(tx_power)
}

/// Casts a `V0DataElement` to the `Actions` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_V0DataElement_into_ACTIONS(de: V0DataElement) -> V0Actions {
    unwrap(de.into_actions(), PanicReason::EnumCastFailed)
}

/// Upcasts a V0 actions DE to a generic V0 data-element.
#[no_mangle]
pub extern "C" fn np_ffi_V0Actions_into_V0DataElement(actions: V0Actions) -> V0DataElement {
    V0DataElement::Actions(actions)
}

/// Gets the tag of a `BuildTxPowerResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_BuildTxPowerResult_kind(
    result: BuildTxPowerResult,
) -> BuildTxPowerResultKind {
    result.kind()
}

/// Casts a `BuildTxPowerResult` to the `Success` variant, panicking in the
/// case where the passed value is of a different enum variant.
#[no_mangle]
pub extern "C" fn np_ffi_BuildTxPowerResult_into_SUCCESS(result: BuildTxPowerResult) -> TxPower {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Attempts to construct a new TxPower from
/// the given signed-byte value.
#[no_mangle]
pub extern "C" fn np_ffi_TxPower_build_from_signed_byte(tx_power: i8) -> BuildTxPowerResult {
    TxPower::build_from_signed_byte(tx_power)
}

/// Gets the value of the given TxPower as a signed byte.
#[no_mangle]
pub extern "C" fn np_ffi_TxPower_as_signed_byte(tx_power: TxPower) -> i8 {
    tx_power.as_i8()
}

/// Gets the discriminant of the `SetV0ActionResult` tagged-union.
#[no_mangle]
pub extern "C" fn np_ffi_SetV0ActionResult_kind(
    result: SetV0ActionResult,
) -> SetV0ActionResultKind {
    result.kind()
}

/// Attempts to cast a `SetV0ActionResult` tagged-union into the `Success` variant.
#[no_mangle]
pub extern "C" fn np_ffi_SetV0ActionResult_into_SUCCESS(result: SetV0ActionResult) -> V0Actions {
    unwrap(result.into_success(), PanicReason::EnumCastFailed)
}

/// Attempts to cast a `SetV0ActionResult` tagged-union into the `Error` variant.
#[no_mangle]
pub extern "C" fn np_ffi_SetV0ActionResult_into_ERROR(result: SetV0ActionResult) -> V0Actions {
    unwrap(result.into_error(), PanicReason::EnumCastFailed)
}

/// Constructs a new V0 actions DE with no declared boolean
/// actions and a zeroed context sync sequence number,
/// where the DE is intended for the given advertisement
/// kind (plaintext/encrypted).
#[no_mangle]
pub extern "C" fn np_ffi_build_new_zeroed_V0Actions(kind: AdvertisementBuilderKind) -> V0Actions {
    V0Actions::new_zeroed(kind)
}

/// Return whether a boolean action type is set in this data element
#[no_mangle]
pub extern "C" fn np_ffi_V0Actions_has_action(actions: V0Actions, action_type: ActionType) -> bool {
    match actions.has_action(action_type) {
        Ok(b) => b,
        Err(_) => panic(PanicReason::InvalidStackDataStructure),
    }
}

/// Attempts to set the given action bit to the given boolean value.
/// This operation may fail if the requested action bit may not be
/// set for the kind of containing advertisement (public/encrypted)
/// that this action DE is intended to belong to. In this case,
/// the original action bits will be yielded back to the caller,
/// unaltered.
#[no_mangle]
pub extern "C" fn np_ffi_V0Actions_set_action(
    actions: V0Actions,
    action_type: ActionType,
    value: bool,
) -> SetV0ActionResult {
    panic_if_invalid(actions.set_action(action_type, value))
}

/// Returns the representation of the passed `V0Actions` as an unsigned
/// integer, where the bit-positions correspond to individual actions.
#[no_mangle]
pub extern "C" fn np_ffi_V0Actions_as_u32(actions: V0Actions) -> u32 {
    actions.as_u32()
}
