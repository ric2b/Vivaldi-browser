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

#![allow(clippy::unwrap_used)]

use super::super::*;
use crate::legacy::data_elements::actions::{ActionBits, InstantTethering, NearbyShare};
use crate::legacy::{Ciphertext, Plaintext};
use alloc::vec::Vec;
use strum::IntoEnumIterator;

#[test]
fn new_v0_actions_invalid_length() {
    let actions = [actions::ActionType::ActiveUnlock; 8];
    let result = V0ActionsFilter::new_from_slice(&actions);
    assert!(result.is_err());
    assert_eq!(result.err().unwrap(), InvalidLength)
}

#[test]
fn new_v0_actions() {
    let actions = [actions::ActionType::ActiveUnlock; 5];
    let result = V0ActionsFilter::new_from_slice(&actions);
    assert!(result.is_ok());
}

#[test]
fn new_v0_actions_empty_slice() {
    let result = V0ActionsFilter::new_from_slice(&[]);
    assert!(result.is_ok());
}

#[test]
fn test_actions_filter_single_action_not_present() {
    // default is all 0 bits
    let action_bits = ActionBits::<Plaintext>::default();

    let filter = V0ActionsFilter::new_from_slice(&[actions::ActionType::ActiveUnlock; 1])
        .expect("1 is a valid length");

    assert_eq!(filter.match_v0_actions(&action_bits.into()), Err(NoMatch))
}

#[test]
fn test_actions_filter_all_actions_not_present() {
    // default is all 0 bits
    let action_bits = ActionBits::<Plaintext>::default();

    let filter = V0ActionsFilter::new_from_slice(&actions::ActionType::iter().collect::<Vec<_>>())
        .expect("5 is a valid length");

    assert_eq!(filter.match_v0_actions(&action_bits.into()), Err(NoMatch))
}

#[test]
fn test_actions_filter_single_action_present() {
    // default is all 0 bits
    let mut action_bits = ActionBits::<Plaintext>::default();
    action_bits.set_action(NearbyShare::from(true));

    let filter = V0ActionsFilter::new_from_slice(&actions::ActionType::iter().collect::<Vec<_>>())
        .expect("5 is a valid length");

    assert_eq!(filter.match_v0_actions(&action_bits.into()), Ok(()))
}

#[test]
fn test_actions_filter_desired_action_not_present() {
    // default is all 0 bits
    let mut action_bits = ActionBits::<Plaintext>::default();
    action_bits.set_action(NearbyShare::from(true));

    let filter = V0ActionsFilter::new_from_slice(&[
        actions::ActionType::CallTransfer,
        actions::ActionType::ActiveUnlock,
        actions::ActionType::InstantTethering,
        actions::ActionType::PhoneHub,
    ])
    .expect("4 is a valid length");

    assert_eq!(filter.match_v0_actions(&action_bits.into()), Err(NoMatch))
}

#[test]
fn test_multiple_actions_set() {
    // default is all 0 bits
    let mut action_bits = ActionBits::<Ciphertext>::default();
    action_bits.set_action(NearbyShare::from(true));
    action_bits.set_action(InstantTethering::from(true));

    let filter = V0ActionsFilter::new_from_slice(&[actions::ActionType::InstantTethering])
        .expect("1 is a valid length");

    assert_eq!(filter.match_v0_actions(&action_bits.into()), Ok(()))
}

#[test]
fn test_multiple_actions_set_both_present() {
    // default is all 0 bits
    let mut action_bits = ActionBits::<Ciphertext>::default();
    action_bits.set_action(NearbyShare::from(true));
    action_bits.set_action(InstantTethering::from(true));

    let filter = V0ActionsFilter::new_from_slice(&[
        actions::ActionType::InstantTethering,
        actions::ActionType::NearbyShare,
    ])
    .expect("7 is a valid length");

    assert_eq!(filter.match_v0_actions(&action_bits.into()), Ok(()))
}

#[test]
fn num_actions_is_correct() {
    assert_eq!(actions::ActionType::iter().count(), NUM_ACTIONS);
}
