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

#![allow(clippy::unwrap_used)]

extern crate std;

use super::*;
use crate::legacy::data_elements::{DeserializeDataElement, LengthMapper};
use crate::legacy::serialize::tests::helpers::{LongDataElement, ShortDataElement};
use crate::legacy::{Ciphertext, PacketFlavor};
use crate::{
    legacy::{
        data_elements::{
            actions::{ActionBits, ActionsDataElement},
            tx_power::TxPowerDataElement,
            SerializeDataElement,
        },
        Plaintext,
    },
    shared_data::TxPower,
};
use alloc::vec;
use core::panic::{RefUnwindSafe, UnwindSafe};
use std::{collections, panic};

#[test]
fn de_type_code_in_range_ok() {
    assert_eq!(3, DeTypeCode::try_from(3).unwrap().code);
}

#[test]
fn de_type_code_out_of_range_err() {
    assert_eq!(DeTypeCodeOutOfRange, DeTypeCode::try_from(30).unwrap_err());
}

#[test]
fn de_actual_length_in_range_ok() {
    assert_eq!(3, DeActualLength::try_from(3).unwrap().len);
}

#[test]
fn de_actual_length_out_of_range_err() {
    assert_eq!(
        DeLengthOutOfRange,
        DeActualLength::try_from(NP_MAX_DE_CONTENT_LEN + 1).unwrap_err()
    );
}

#[test]
fn de_encoded_length_in_range_ok() {
    assert_eq!(3, DeEncodedLength::from(3).len);
}

#[test]
fn de_encoded_length_out_of_range_err() {
    assert_eq!(DeLengthOutOfRange, DeEncodedLength::try_from(MAX_DE_ENCODED_LEN + 1).unwrap_err());
}

#[test]
fn de_length_actual_encoded_round_trip() {
    fn do_de_length_test<
        F: PacketFlavor,
        D: DeserializeDataElement + SerializeDataElement<F> + UnwindSafe + RefUnwindSafe,
    >(
        de: D,
    ) {
        // for all possible lengths, calculate actual -> encoded and the inverse
        let actual_to_encoded = (0_u8..=255)
            .filter_map(|num| DeActualLength::try_from(usize::from(num)).ok())
            .filter_map(|actual: DeActualLength| {
                panic::catch_unwind(|| de.map_actual_len_to_encoded_len(actual))
                    .ok()
                    .map(|encoded| (actual, encoded))
            })
            .collect::<collections::HashMap<_, _>>();

        let encoded_to_actual = (0_u8..=255)
            .filter_map(|num| DeEncodedLength::try_from(num).ok())
            .filter_map(|encoded: DeEncodedLength| {
                D::LengthMapper::map_encoded_len_to_actual_len(encoded)
                    .ok()
                    .map(|actual| (encoded, actual))
            })
            .collect::<collections::HashMap<_, _>>();

        // ensure the two maps are inverses of each other
        assert_eq!(
            actual_to_encoded,
            encoded_to_actual.into_iter().map(|(encoded, actual)| (actual, encoded)).collect(),
        );
    }

    do_de_length_test::<Plaintext, _>(TxPowerDataElement::from(TxPower::try_from(1).unwrap()));
    do_de_length_test(ActionsDataElement::<Plaintext>::from(ActionBits::default()));
    do_de_length_test(ActionsDataElement::<Ciphertext>::from(ActionBits::default()));

    // might as well make sure our test DEs behave as well
    do_de_length_test::<Plaintext, _>(ShortDataElement::new(vec![]));
    do_de_length_test::<Plaintext, _>(LongDataElement::new(vec![]));
}

mod coverage_gaming {
    use crate::legacy::data_elements::de_type::{
        DataElementType, DeActualLength, DeEncodedLength, DeTypeCode, DeTypeCodeOutOfRange,
    };

    extern crate std;

    use std::{collections, format};

    #[test]
    fn de_type_code_debug_hash_clone() {
        let code = DeTypeCode::try_from(3).unwrap();
        let _ = format!("{:?}", code.clone());
        let _ = collections::HashSet::from([code]);
    }

    #[test]
    fn de_type_code_out_of_range() {
        // debug, clone
        let _ = format!("{:?}", DeTypeCodeOutOfRange.clone());
    }

    #[test]
    fn de_actual_length() {
        // debug, clone
        let _ = format!("{:?}", DeActualLength::try_from(3).unwrap().clone());
    }

    #[test]
    fn de_encoded_length() {
        // debug, clone
        let _ = format!("{:?}", DeEncodedLength::from(3).clone());
    }

    #[test]
    fn de_type() {
        // debug, clone
        let _ = format!("{:?}", DataElementType::TxPower.clone());
    }
}
