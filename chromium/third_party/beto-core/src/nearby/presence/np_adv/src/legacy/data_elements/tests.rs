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

use alloc::boxed::Box;

use crate::header::VERSION_HEADER_V0_UNENCRYPTED;
use crate::legacy::data_elements::actions::{ActionBits, ActionsDataElement};
use crate::legacy::serialize::{encode_de_header, AdvBuilder, UnencryptedEncoder};
use crate::legacy::Plaintext;

use super::*;

#[test]
fn dynamic_de_works() {
    let mut builder = AdvBuilder::new(UnencryptedEncoder);

    let actions: Box<dyn SerializeDataElement<Plaintext>> =
        Box::new(ActionsDataElement::<Plaintext>::from(ActionBits::default()));
    builder.add_data_element(DynamicSerializeDataElement::from(actions.as_ref())).unwrap();

    assert_eq!(
        &[
            VERSION_HEADER_V0_UNENCRYPTED,
            encode_de_header(
                ActionsDataElement::<Plaintext>::DE_TYPE_CODE,
                DeEncodedLength::from(1),
            ),
            0
        ],
        builder.into_advertisement().unwrap().as_slice()
    );
}

pub(in crate::legacy) mod macros {
    use alloc::vec::Vec;

    use crate::legacy::data_elements::de_type::DataElementType;
    use crate::legacy::deserialize::{DeIterator, DeserializedDataElement};
    use crate::legacy::serialize::SerializedDataElement;
    use crate::legacy::PacketFlavor;

    /// Test method body that creates an array, deserializes it into a DE, serializes it,
    /// and asserts that the same bytes are produced.
    ///
    /// Evaluates to (the deserialized DE, the serialized form of the DE).
    macro_rules! de_roundtrip_test {
        ($de_type:ty, $type_variant:ident, $de_variant:ident, $flavor:ty,  $bytes:expr) => {{
            let parsed_de_enum =
                crate::legacy::data_elements::tests::macros::construct_and_parse_de::<
                    $flavor,
                >(crate::legacy::data_elements::de_type::DataElementType::$type_variant, &$bytes);
            if let crate::legacy::deserialize::DeserializedDataElement::$de_variant(de) =
                parsed_de_enum
            {
                // skip DE header byte
                let expected = <$de_type as crate::legacy::data_elements::DeserializeDataElement>
                                    ::deserialize::<$flavor>(&$bytes.as_slice()[1..]).unwrap();
                assert_eq!(expected, de);

                let serialized = crate::legacy::serialize::tests::serialize::<$flavor, _>(&de);
                assert_eq!($bytes.as_slice(), serialized.as_slice());

                (de, serialized)
            } else {
                panic!("Unexpected variant: {:?}", parsed_de_enum);
            }
        }};
    }

    pub(in crate::legacy) use de_roundtrip_test;

    /// Construct the serialized DE and parse it
    pub(in crate::legacy) fn construct_and_parse_de<F>(
        de_type: DataElementType,
        contents: &SerializedDataElement,
    ) -> DeserializedDataElement<F>
    where
        F: PacketFlavor,
    {
        let mut plain_des =
            DeIterator::new(contents.as_slice()).collect::<Result<Vec<_>, _>>().unwrap();
        assert_eq!(1, plain_des.len());
        let de = plain_des.swap_remove(0);
        assert_eq!(
            de_type,
            match de {
                DeserializedDataElement::Actions(_) => DataElementType::Actions,
                DeserializedDataElement::TxPower(_) => DataElementType::TxPower,
            }
        );
        de
    }
}

mod coverage_gaming {
    use alloc::format;

    use nom::error;
    use nom::error::ParseError;

    use crate::legacy::data_elements::de_type::DeTypeCode;
    use crate::legacy::data_elements::{DataElementDeserializeError, DataElementSerializeError};

    #[test]
    fn data_element_serialize_error_debug_eq_clone() {
        let _ = format!("{:?}", DataElementSerializeError::InsufficientSpace.clone());
        assert_eq!(
            DataElementSerializeError::InsufficientSpace,
            DataElementSerializeError::InsufficientSpace
        );
    }

    #[test]
    fn data_element_deserialize_error_debug_clone() {
        let _ = format!("{:?}", DataElementDeserializeError::InvalidStructure.clone());
    }

    #[test]
    fn data_element_deserialize_error_append() {
        assert_eq!(
            DataElementDeserializeError::InvalidStructure,
            DataElementDeserializeError::append(
                &[0_u8],
                error::ErrorKind::CrLf,
                DataElementDeserializeError::InvalidDeType {
                    de_type: DeTypeCode::try_from(10).unwrap()
                },
            )
        );
    }
}

pub(in crate::legacy) mod test_des {
    use alloc::vec;

    use rand::distributions;
    use strum_macros::EnumIter;

    use crate::legacy::data_elements::de_type::{
        DeActualLength, DeEncodedLength, DeTypeCode, MAX_DE_ENCODED_LEN,
    };
    use crate::legacy::data_elements::{
        DataElementDeserializeError, DataElementSerializationBuffer, DataElementSerializeError,
        DeserializeDataElement, LengthMapper, SerializeDataElement,
    };
    use crate::legacy::deserialize::{
        DataElementDeserializer, Deserialized, LengthError, RawDataElement,
    };
    use crate::legacy::serialize::tests::helpers::{LongDataElement, ShortDataElement};
    use crate::legacy::{PacketFlavor, Plaintext, NP_MAX_DE_CONTENT_LEN};
    use crate::private::Sealed;

    /// A [DataElementDeserializer] that can deserialize the test stubs [ShortDataElement] and
    /// [LongDataElement].
    pub(in crate::legacy) struct TestDeDeserializer;

    impl DataElementDeserializer for TestDeDeserializer {
        type DeTypeDisambiguator = TestDataElementType;
        type Deserialized<F: PacketFlavor> = TestDataElement;

        fn map_encoded_len_to_actual_len(
            de_type: DeTypeCode,
            encoded_len: DeEncodedLength,
        ) -> Result<(Self::DeTypeDisambiguator, DeActualLength), LengthError> {
            match de_type {
                ShortDataElement::DE_TYPE_CODE => {
                    <ShortDataElement as DeserializeDataElement>::LengthMapper::map_encoded_len_to_actual_len(encoded_len)
                        .map(|l| (TestDataElementType::Short, l))
                        .map_err(|e| e.into())
                }
                LongDataElement::DE_TYPE_CODE => {
                    <LongDataElement as DeserializeDataElement>::LengthMapper::map_encoded_len_to_actual_len(encoded_len)
                        .map(|l| (TestDataElementType::Long, l))
                        .map_err(|e| e.into())
                }
                _ => Err(LengthError::InvalidType),
            }
        }

        fn deserialize_de<F: PacketFlavor>(
            raw_de: RawDataElement<Self>,
        ) -> Result<Self::Deserialized<F>, DataElementDeserializeError> {
            match raw_de.de_type {
                TestDataElementType::Short => {
                    ShortDataElement::deserialize::<F>(raw_de.contents).map(TestDataElement::Short)
                }
                TestDataElementType::Long => {
                    LongDataElement::deserialize::<F>(raw_de.contents).map(TestDataElement::Long)
                }
            }
        }
    }

    #[derive(EnumIter, Debug, Clone, Copy)]
    pub(in crate::legacy) enum TestDataElementType {
        Short,
        Long,
    }

    #[derive(Debug, PartialEq, Eq, Clone)]
    pub(in crate::legacy) enum TestDataElement {
        Short(ShortDataElement),
        Long(LongDataElement),
    }

    impl Deserialized for TestDataElement {
        fn de_type_code(&self) -> DeTypeCode {
            match self {
                TestDataElement::Short(s) => {
                    <ShortDataElement as SerializeDataElement<Plaintext>>::de_type_code(s)
                }
                TestDataElement::Long(l) => {
                    <LongDataElement as SerializeDataElement<Plaintext>>::de_type_code(l)
                }
            }
        }
    }

    impl From<ShortDataElement> for TestDataElement {
        fn from(value: ShortDataElement) -> Self {
            Self::Short(value)
        }
    }

    impl From<LongDataElement> for TestDataElement {
        fn from(value: LongDataElement) -> Self {
            Self::Long(value)
        }
    }

    impl Sealed for TestDataElement {}

    // Not representative of how the main [DeserializedDataElement] would be used, but handy
    // in tests to be able to directly serialize the deserialized representation
    impl<F: PacketFlavor> SerializeDataElement<F> for TestDataElement {
        fn de_type_code(&self) -> DeTypeCode {
            match self {
                TestDataElement::Short(s) => {
                    <ShortDataElement as SerializeDataElement<F>>::de_type_code(s)
                }
                TestDataElement::Long(l) => {
                    <LongDataElement as SerializeDataElement<F>>::de_type_code(l)
                }
            }
        }

        fn map_actual_len_to_encoded_len(&self, actual_len: DeActualLength) -> DeEncodedLength {
            match self {
                TestDataElement::Short(s) => {
                    <ShortDataElement as SerializeDataElement<F>>::map_actual_len_to_encoded_len(
                        s, actual_len,
                    )
                }
                TestDataElement::Long(l) => {
                    <LongDataElement as SerializeDataElement<F>>::map_actual_len_to_encoded_len(
                        l, actual_len,
                    )
                }
            }
        }

        fn serialize_contents(
            &self,
            sink: &mut DataElementSerializationBuffer,
        ) -> Result<(), DataElementSerializeError> {
            match self {
                TestDataElement::Short(s) => {
                    <ShortDataElement as SerializeDataElement<F>>::serialize_contents(s, sink)
                }
                TestDataElement::Long(l) => {
                    <LongDataElement as SerializeDataElement<F>>::serialize_contents(l, sink)
                }
            }
        }
    }

    impl distributions::Distribution<ShortDataElement> for distributions::Standard {
        fn sample<R: rand::Rng + ?Sized>(&self, rng: &mut R) -> ShortDataElement {
            let len = rng.gen_range(0_usize..MAX_DE_ENCODED_LEN.into());
            let mut data = vec![0; len];
            rng.fill(&mut data[..]);
            ShortDataElement::new(data)
        }
    }

    impl distributions::Distribution<LongDataElement> for distributions::Standard {
        fn sample<R: rand::Rng + ?Sized>(&self, rng: &mut R) -> LongDataElement {
            let len = rng.gen_range(LongDataElement::OFFSET..NP_MAX_DE_CONTENT_LEN);
            let mut data = vec![0; len];
            rng.fill(&mut data[..]);
            LongDataElement::new(data)
        }
    }

    /// Generate a random instance of the requested de type, or `None` if that type does not support
    /// plaintext.
    pub(crate) fn random_test_de<R>(de_type: TestDataElementType, rng: &mut R) -> TestDataElement
    where
        R: rand::Rng,
    {
        match de_type {
            TestDataElementType::Short => TestDataElement::Short(rng.gen()),
            TestDataElementType::Long => TestDataElement::Long(rng.gen()),
        }
    }
}
