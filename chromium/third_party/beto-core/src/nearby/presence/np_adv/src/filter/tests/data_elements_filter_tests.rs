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

use super::super::*;
use crate::{
    legacy::{
        data_elements::{
            actions::{ActionBits, ActiveUnlock},
            tx_power::TxPowerDataElement,
        },
        Ciphertext, Plaintext,
    },
    shared_data::TxPower,
};

#[test]
fn match_contains_tx_power() {
    let filter = V0DataElementsFilter { contains_tx_power: Some(true), actions_filter: None };

    let tx_power = TxPower::try_from(5).expect("within range");
    let result = filter.match_v0_legible_adv(|| {
        [Ok(DeserializedDataElement::<Ciphertext>::TxPower(TxPowerDataElement::from(tx_power)))]
            .into_iter()
    });
    assert_eq!(result, Ok(()))
}

#[test]
fn match_not_contains_tx_power() {
    let filter = V0DataElementsFilter { contains_tx_power: Some(true), actions_filter: None };
    let result = filter.match_v0_legible_adv::<Plaintext, _>(|| [].into_iter());
    assert_eq!(result, Err(NoMatch))
}

#[test]
fn match_not_contains_actions() {
    let filter = V0ActionsFilter::new_from_slice(&[actions::ActionType::ActiveUnlock; 1])
        .expect("1 is a valid length");
    let filter = V0DataElementsFilter { contains_tx_power: None, actions_filter: Some(filter) };
    let tx_power = TxPower::try_from(5).expect("within range");
    let result = filter.match_v0_legible_adv(|| {
        [Ok(DeserializedDataElement::<Ciphertext>::TxPower(TxPowerDataElement::from(tx_power)))]
            .into_iter()
    });
    assert_eq!(result, Err(NoMatch))
}

#[test]
fn match_contains_actions() {
    let filter = V0ActionsFilter::new_from_slice(&[actions::ActionType::ActiveUnlock; 1])
        .expect("1 is a valid length");
    let filter = V0DataElementsFilter { contains_tx_power: None, actions_filter: Some(filter) };
    let tx_power = TxPower::try_from(5).expect("within range");

    let mut action_bits = ActionBits::<Ciphertext>::default();
    action_bits.set_action(ActiveUnlock::from(true));

    let result = filter.match_v0_legible_adv(|| {
        [
            Ok(DeserializedDataElement::<Ciphertext>::TxPower(TxPowerDataElement::from(tx_power))),
            Ok(DeserializedDataElement::Actions(action_bits.into())),
        ]
        .into_iter()
    });
    assert_eq!(result, Ok(()))
}

#[test]
fn match_contains_both() {
    let filter = V0ActionsFilter::new_from_slice(&[actions::ActionType::ActiveUnlock; 1])
        .expect("1 is a valid length");
    let filter =
        V0DataElementsFilter { contains_tx_power: Some(true), actions_filter: Some(filter) };
    let tx_power = TxPower::try_from(5).expect("within range");

    let mut action_bits = ActionBits::<Ciphertext>::default();
    action_bits.set_action(ActiveUnlock::from(true));

    let result = filter.match_v0_legible_adv(|| {
        [
            Ok(DeserializedDataElement::<Ciphertext>::TxPower(TxPowerDataElement::from(tx_power))),
            Ok(DeserializedDataElement::Actions(action_bits.into())),
        ]
        .into_iter()
    });
    assert_eq!(result, Ok(()))
}

#[test]
fn match_contains_either() {
    let filter = V0ActionsFilter::new_from_slice(&[actions::ActionType::ActiveUnlock; 1])
        .expect("1 is a valid length");
    let filter =
        V0DataElementsFilter { contains_tx_power: Some(true), actions_filter: Some(filter) };
    let tx_power = TxPower::try_from(5).expect("within range");

    let result = filter.match_v0_legible_adv(|| {
        [Ok(DeserializedDataElement::<Ciphertext>::TxPower(TxPowerDataElement::from(tx_power)))]
            .into_iter()
    });
    assert_eq!(result, Err(NoMatch))
}
