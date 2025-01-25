// Copyright 2023 Google LLC
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

//! Helper traits to convert byte signedness for Java interop

/// A helper trait to bitcast data to its signed representation
pub trait ToSigned {
    /// The signed version of this type.
    type Signed;

    /// Bitcast to signed
    fn to_signed(self) -> Self::Signed;
}

impl<'a> ToSigned for &'a [u8] {
    type Signed = &'a [i8];

    #[allow(unsafe_code)]
    fn to_signed(self) -> Self::Signed {
        let len = self.len();
        // Safety:
        // u8 and i8 have the same layout and the lifetime is maintained
        unsafe { core::slice::from_raw_parts(self.as_ptr() as *const i8, len) }
    }
}

/// A helper trait to bitcast data to its unsigned representation
pub trait ToUnsigned {
    /// The unsigned version of this type.
    type Unsigned;

    /// Bitcast to unsigned
    fn to_unsigned(self) -> Self::Unsigned;
}

impl<'a> ToUnsigned for &'a [i8] {
    type Unsigned = &'a [u8];

    #[allow(unsafe_code)]
    fn to_unsigned(self) -> Self::Unsigned {
        let len = self.len();
        // Safety:
        // u8 and i8 have the same layout and the lifetime is maintained
        unsafe { core::slice::from_raw_parts(self.as_ptr() as *const u8, len) }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_to_signed() {
        let input: &[u8] = &[1, 2, 3, 0, 0xff, 0xfe];
        let expected: &[i8] = &[1, 2, 3, 0, -1, -2];

        assert_eq!(expected, input.to_signed());
    }

    #[test]
    fn test_to_unsigned() {
        let input: &[i8] = &[1, 2, 3, 0, -1, -2];
        let expected: &[u8] = &[1, 2, 3, 0, 0xff, 0xfe];

        assert_eq!(expected, input.to_unsigned());
    }

    #[test]
    fn test_roundtrip_unsigned() {
        let case: &[u8] = &[1, 2, 3, 0, 0xff, 0xfe];
        assert_eq!(case, case.to_signed().to_unsigned());
    }

    #[test]
    fn test_roundtrip_signed() {
        let case: &[i8] = &[1, 2, 3, 0, -1, -2];
        assert_eq!(case, case.to_unsigned().to_signed());
    }
}
