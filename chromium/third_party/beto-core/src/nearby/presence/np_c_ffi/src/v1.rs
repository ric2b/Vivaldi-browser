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

//! NP Rust C FFI functionality common to V1 ser/deser flows.

use np_ffi_core::deserialize::v1::*;

/// Converts a `V1DataElement` to a `GenericV1DataElement` which
/// only maintains information about the DE's type-code and payload.
#[no_mangle]
pub extern "C" fn np_ffi_V1DataElement_to_generic(de: V1DataElement) -> GenericV1DataElement {
    de.to_generic()
}

/// Extracts the numerical value of the given V1 DE type code as
/// an unsigned 32-bit integer.
#[no_mangle]
pub extern "C" fn np_ffi_V1DEType_to_uint32_t(de_type: V1DEType) -> u32 {
    de_type.to_u32()
}
