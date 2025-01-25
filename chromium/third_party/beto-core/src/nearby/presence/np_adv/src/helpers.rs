// Copyright 2022 Google LLC
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

use nom::{bytes, combinator};

/// Nom parser for a `[u8; N]`.
pub(crate) fn parse_byte_array<const N: usize>(input: &[u8]) -> nom::IResult<&[u8], [u8; N]> {
    combinator::map_res(bytes::complete::take(N), |slice: &[u8]| slice.try_into())(input)
}

#[allow(clippy::unwrap_used)]
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_empty_array() {
        assert_eq!(([1_u8, 2, 3].as_slice(), []), parse_byte_array::<0>(&[1, 2, 3]).unwrap())
    }

    #[test]
    fn parse_nonempty_array() {
        assert_eq!(
            ([4_u8, 5, 6].as_slice(), [1, 2, 3]),
            parse_byte_array::<3>(&[1, 2, 3, 4, 5, 6]).unwrap()
        )
    }
}
