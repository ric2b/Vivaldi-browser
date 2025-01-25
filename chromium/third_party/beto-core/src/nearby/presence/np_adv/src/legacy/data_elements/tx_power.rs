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

//! Data element for TX Power.

use crate::legacy::data_elements::de_type::{DeActualLength, DeEncodedLength, DeTypeCode};
use crate::legacy::data_elements::{
    DataElementDeserializeError, DataElementSerializationBuffer, DataElementSerializeError,
    DeserializeDataElement, DirectMapPredicate, DirectMapper, LengthMapper, SerializeDataElement,
};
use crate::legacy::PacketFlavor;
use crate::private::Sealed;
use crate::shared_data::TxPower;
use sink::Sink;

/// Data element holding a [TxPower].
#[derive(Debug, PartialEq, Eq, Clone)]
pub struct TxPowerDataElement {
    /// The tx power value
    pub tx_power: TxPower,
}

impl TxPowerDataElement {
    /// Gets the underlying Tx Power value
    pub fn tx_power_value(&self) -> i8 {
        self.tx_power.as_i8()
    }
}

impl From<TxPower> for TxPowerDataElement {
    fn from(tx_power: TxPower) -> Self {
        Self { tx_power }
    }
}

impl Sealed for TxPowerDataElement {}

impl<F: PacketFlavor> SerializeDataElement<F> for TxPowerDataElement {
    fn de_type_code(&self) -> DeTypeCode {
        TxPowerDataElement::DE_TYPE_CODE
    }

    fn map_actual_len_to_encoded_len(&self, actual_len: DeActualLength) -> DeEncodedLength {
        <Self as DeserializeDataElement>::LengthMapper::map_actual_len_to_encoded_len(actual_len)
    }

    fn serialize_contents(
        &self,
        sink: &mut DataElementSerializationBuffer,
    ) -> Result<(), DataElementSerializeError> {
        sink.try_extend_from_slice(self.tx_power.as_i8().to_be_bytes().as_slice())
            .ok_or(DataElementSerializeError::InsufficientSpace)
    }
}

impl DeserializeDataElement for TxPowerDataElement {
    const DE_TYPE_CODE: DeTypeCode = match DeTypeCode::try_from(0b0101) {
        Ok(t) => t,
        Err(_) => unreachable!(),
    };

    type LengthMapper = DirectMapper<TxPowerLengthPredicate>;

    fn deserialize<F: PacketFlavor>(
        de_contents: &[u8],
    ) -> Result<Self, DataElementDeserializeError> {
        de_contents
            .try_into()
            .ok()
            .and_then(|arr: [u8; 1]| TxPower::try_from(i8::from_be_bytes(arr)).ok())
            .map(|tx_power| Self { tx_power })
            .ok_or(DataElementDeserializeError::DeserializeError { de_type: Self::DE_TYPE_CODE })
    }
}

pub(in crate::legacy) struct TxPowerLengthPredicate;

impl DirectMapPredicate for TxPowerLengthPredicate {
    fn is_valid(len: usize) -> bool {
        len == 1
    }
}

#[allow(clippy::unwrap_used)]
#[cfg(test)]
mod tests {
    use crate::legacy::data_elements::de_type::{DeActualLength, DeEncodedLength};
    use crate::legacy::data_elements::tests::macros::de_roundtrip_test;
    use crate::legacy::data_elements::tx_power::TxPowerDataElement;
    use crate::legacy::data_elements::{DeserializeDataElement, LengthMapper};
    use crate::legacy::serialize::tests::serialize;
    use crate::legacy::{Ciphertext, Plaintext};
    use crate::{shared_data, DeLengthOutOfRange};
    use std::panic;

    extern crate std;

    #[test]
    fn actual_length_must_be_1() {
        for l in [0, 2] {
            let actual = DeActualLength::try_from(l).unwrap();
            let _ = panic::catch_unwind(|| {
                <TxPowerDataElement as DeserializeDataElement>::LengthMapper::map_actual_len_to_encoded_len(actual)
            })
                .unwrap_err();
        }

        assert_eq!(
            1,
            <TxPowerDataElement as DeserializeDataElement>::LengthMapper::map_actual_len_to_encoded_len(
                DeActualLength::try_from(1).unwrap(),
            )
                .as_u8()
        )
    }

    #[test]
    fn encoded_length_must_be_1() {
        for l in [0, 2] {
            assert_eq!(
                DeLengthOutOfRange,
                <TxPowerDataElement as DeserializeDataElement>::LengthMapper::map_encoded_len_to_actual_len(
                    DeEncodedLength::try_from(l).unwrap()
                )
                    .unwrap_err()
            )
        }

        assert_eq!(
            1,
            <TxPowerDataElement as DeserializeDataElement>::LengthMapper::map_encoded_len_to_actual_len(
                DeEncodedLength::from(1)
            )
                .unwrap()
                .as_u8()
        );
    }

    #[test]
    fn tx_power_de_contents_roundtrip_unencrypted() {
        let tx = shared_data::TxPower::try_from(-10).unwrap();
        let _ = de_roundtrip_test!(
            TxPowerDataElement,
            TxPower,
            TxPower,
            Plaintext,
            serialize::<Plaintext, _>(&TxPowerDataElement::from(tx))
        );
    }

    #[test]
    fn tx_power_de_contents_roundtrip_ldt() {
        let tx = shared_data::TxPower::try_from(-10).unwrap();

        let _ = de_roundtrip_test!(
            TxPowerDataElement,
            TxPower,
            TxPower,
            Ciphertext,
            serialize::<Ciphertext, _>(&TxPowerDataElement::from(tx))
        );
    }

    mod coverage_gaming {
        use crate::legacy::data_elements::tx_power::TxPowerDataElement;
        use crate::shared_data::TxPower;
        use alloc::format;

        #[test]
        fn tx_power_de() {
            let de = TxPowerDataElement::from(TxPower::try_from(3).unwrap());
            // debug
            let _ = format!("{:?}", de);
            // trivial accessor
            assert_eq!(3, de.tx_power_value());
        }
    }
}
