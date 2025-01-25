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

//! V1 data elements.
//!
//! Commonly used DEs have dedicated types (e.g. [TxPowerDataElement], etc), but if another DE is
//! needed, [GenericDataElement] will allow constructing any type of DE.

use crate::{
    extended::{
        de_type::DeType,
        deserialize::data_element::DataElement,
        serialize::{DeHeader, SingleTypeDataElement, WriteDataElement},
        MAX_DE_LEN,
    },
    shared_data::*,
};
use array_view::ArrayView;
use sink::Sink;

mod actions;
pub use actions::ActionId;
pub use actions::ActionsDataElement;
pub use actions::DeserializedActionsDE;

#[cfg(test)]
mod tests;

/// A general purpose data element for use cases that don't fit into an existing DE type.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct GenericDataElement {
    de_type: DeType,
    data: ArrayView<u8, MAX_DE_LEN>,
}

impl GenericDataElement {
    /// Construct a `GenericDataElement` from the provided input.
    ///
    /// `de_type`: the DE type
    /// `data`: the DE contents, length <= 127
    pub fn try_from(de_type: DeType, data: &[u8]) -> Result<Self, GenericDataElementError> {
        ArrayView::try_from_slice(data)
            .ok_or(GenericDataElementError::DataTooLong)
            .map(|data| Self { de_type, data })
    }
}

/// Errors that can occur constructing a [GenericDataElement]
#[derive(Debug, PartialEq, Eq)]
pub enum GenericDataElementError {
    /// The DE data is too long
    DataTooLong,
}

impl WriteDataElement for GenericDataElement {
    fn de_header(&self) -> DeHeader {
        DeHeader::new(self.de_type, self.data.len().try_into().expect("length <= max DE len"))
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        sink.try_extend_from_slice(self.data.as_slice())
    }
}

/// Convert a deserialized DE into one you can serialize
impl<'a> From<&'a DataElement<'a>> for GenericDataElement {
    fn from(de: &'a DataElement<'a>) -> Self {
        Self::try_from(de.de_type(), de.contents())
            .expect("Deserialized DE must have a valid length")
    }
}

/// Advertising power
#[derive(Clone)]
pub struct TxPowerDataElement {
    tx_power: TxPower,
}

impl From<TxPower> for TxPowerDataElement {
    fn from(tx_power: TxPower) -> Self {
        Self { tx_power }
    }
}

impl SingleTypeDataElement for TxPowerDataElement {
    const DE_TYPE: DeType = DeType::const_from(0x05);
}

impl WriteDataElement for TxPowerDataElement {
    fn de_header(&self) -> DeHeader {
        DeHeader::new(Self::DE_TYPE, 1_u8.try_into().expect("1 is a valid length"))
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        sink.try_push(self.tx_power.as_i8() as u8)
    }
}

/// Context sync sequence number
pub struct ContextSyncSeqNumDataElement {
    num: ContextSyncSeqNum,
}

impl From<ContextSyncSeqNum> for ContextSyncSeqNumDataElement {
    fn from(num: ContextSyncSeqNum) -> Self {
        Self { num }
    }
}

impl SingleTypeDataElement for ContextSyncSeqNumDataElement {
    const DE_TYPE: DeType = DeType::const_from(0x13);
}

impl WriteDataElement for ContextSyncSeqNumDataElement {
    fn de_header(&self) -> DeHeader {
        DeHeader::new(Self::DE_TYPE, 1_u8.try_into().expect("1 is a valid length"))
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        sink.try_push(self.num.as_u8())
    }
}

/// Connectivity info
pub struct ConnectivityInfoDataElement {
    // TODO len
    info: ArrayView<u8, 24>,
}

impl ConnectivityInfoDataElement {
    /// Construct connectivity info for bluetooth.
    pub fn bluetooth(svc_id: [u8; 4], mac: [u8; 6]) -> Self {
        let mut array = [0; 24];
        array[0] = 0x1; // bluetooth
        array[1..5].copy_from_slice(&svc_id);
        array[5..11].copy_from_slice(&mac);

        Self { info: ArrayView::try_from_array(array, 11).expect("length is fixed") }
    }

    /// Construct connectivity info for mDNS.
    // TODO port type, bssid
    pub fn mdns(ip: [u8; 4], port: u8) -> Self {
        let mut array = [0; 24];
        array[0] = 0x2; // mdns
        array[1..5].copy_from_slice(&ip);
        array[5] = port;

        Self { info: ArrayView::try_from_array(array, 6).expect("length is fixed") }
    }

    /// Construct connectivity info for WiFi Direct.
    pub fn wifi_direct(ssid: [u8; 10], password: [u8; 10], freq: [u8; 2], port: u8) -> Self {
        let mut array = [0; 24];
        array[0] = 0x3; // wifi direct
        array[1..11].copy_from_slice(&ssid);
        array[11..21].copy_from_slice(&password);
        array[21..23].copy_from_slice(&freq);
        array[23] = port;

        Self { info: ArrayView::try_from_array(array, 24).expect("length is fixed") }
    }
}

impl SingleTypeDataElement for ConnectivityInfoDataElement {
    const DE_TYPE: DeType = DeType::const_from(0x11);
}

impl WriteDataElement for ConnectivityInfoDataElement {
    fn de_header(&self) -> DeHeader {
        DeHeader::new(
            Self::DE_TYPE,
            self.info.len().try_into().expect("conn info is a valid length"),
        )
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        sink.try_extend_from_slice(self.info.as_slice())
    }
}

/// Connectivity capabilities
pub struct ConnectivityCapabilityDataElement {
    // TODO len
    capability: ArrayView<u8, 7>,
}

impl ConnectivityCapabilityDataElement {
    /// Construct connectivity capabilities for WiFi Direct.
    pub fn wifi_direct(supported_freqs: [u8; 3], connected_freqs: [u8; 3]) -> Self {
        let mut array = [0; 7];
        array[0] = 0x2; // wifi direct
        array[1..4].copy_from_slice(&supported_freqs);
        array[4..7].copy_from_slice(&connected_freqs);

        Self { capability: ArrayView::try_from_array(array, 7).expect("length is fixed") }
    }
}

impl SingleTypeDataElement for ConnectivityCapabilityDataElement {
    const DE_TYPE: DeType = DeType::const_from(0x12);
}

impl WriteDataElement for ConnectivityCapabilityDataElement {
    fn de_header(&self) -> DeHeader {
        DeHeader::new(
            Self::DE_TYPE,
            self.capability.len().try_into().expect("capability is a valid length"),
        )
    }

    fn write_de_contents<S: Sink<u8>>(&self, sink: &mut S) -> Option<()> {
        sink.try_extend_from_slice(self.capability.as_slice())
    }
}
