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

//! Data Elementes for v0 advertisements. See `class V0DataElement`.

use crate::class::IdentityKind;
use jni::{
    objects::{JClass, JIntArray, JObject},
    signature::{Primitive, ReturnType},
    sys::{jboolean, jint, JNI_FALSE, JNI_TRUE},
    JNIEnv,
};
use np_ffi_core::{
    common::InvalidStackDataStructure, deserialize::v0::DeserializedV0IdentityKind,
    serialize::AdvertisementBuilderKind, v0,
};
use pourover::desc::{ClassDesc, FieldDesc};

static TX_POWER_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/V0DataElement$TxPower");

/// Rust representation of `class V0DataElement.TxPower`.
#[repr(transparent)]
pub struct TxPower<Obj>(pub Obj);

impl<'local> TxPower<JObject<'local>> {
    /// Create a new TxPower date element with the given `tx_power`.
    pub fn construct(env: &mut JNIEnv<'local>, tx_power: jint) -> jni::errors::Result<Self> {
        pourover::call_constructor!(env, &TX_POWER_CLASS, "(I)V", tx_power).map(Self)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> TxPower<Obj> {
    /// Cast the given Java object to `TxPower` if it is an instance of the type. Returns `None` if
    /// the object's type does not match.
    pub fn checked_cast<'other_local>(
        env: &mut JNIEnv<'other_local>,
        obj: Obj,
    ) -> jni::errors::Result<Option<Self>> {
        Ok(env.is_instance_of(obj.as_ref(), &TX_POWER_CLASS)?.then(|| Self(obj)))
    }

    /// Gets the value of the `int txPower` field.
    pub fn get_tx_power<'env_local>(
        &self,
        env: &mut JNIEnv<'env_local>,
    ) -> jni::errors::Result<jint> {
        static TX_POWER_FIELD: FieldDesc = TX_POWER_CLASS.field("txPower", "I");
        env.get_field_unchecked(
            self.0.as_ref(),
            &TX_POWER_FIELD,
            ReturnType::Primitive(Primitive::Int),
        )
        .and_then(|ret| ret.i())
    }

    /// Get the `np_ffi_core` representation of this data element.
    pub fn get_as_core<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<v0::BuildTxPowerResult> {
        let power = self.get_tx_power(env)?;
        let Ok(power) = i8::try_from(power) else {
            return Ok(v0::BuildTxPowerResult::OutOfRange);
        };
        Ok(v0::TxPower::build_from_signed_byte(power))
    }
}

static V0_ACTIONS_CLASS: ClassDesc =
    ClassDesc::new("com/google/android/nearby/presence/rust/V0DataElement$V0Actions");

/// Rust representation of `class V0DataElement.V0Actions`.
#[repr(transparent)]
pub struct V0Actions<Obj>(pub Obj);

impl<'local> V0Actions<JObject<'local>> {
    /// Create a new TxPower date element with the given identity and action bits.
    pub fn construct(
        env: &mut JNIEnv<'local>,
        identity_kind: DeserializedV0IdentityKind,
        action_bits: jint,
    ) -> jni::errors::Result<Self> {
        let identity_kind = IdentityKind::from(identity_kind).to_java(env)?;

        pourover::call_constructor!(env, &V0_ACTIONS_CLASS, "(II)V", identity_kind, action_bits)
            .map(Self)
    }
}

impl<'local, Obj: AsRef<JObject<'local>>> V0Actions<Obj> {
    /// Cast the given Java object to `V0Actions` if it is an instance of the type. Returns `None` if
    /// the object's type does not match.
    pub fn checked_cast<'other_local>(
        env: &mut JNIEnv<'other_local>,
        obj: Obj,
    ) -> jni::errors::Result<Option<Self>> {
        Ok(env.is_instance_of(obj.as_ref(), &V0_ACTIONS_CLASS)?.then(|| Self(obj)))
    }

    /// Get the `int identityKind` field from the Java object.
    pub fn get_identity_kind<'env_local>(
        &self,
        env: &mut JNIEnv<'env_local>,
    ) -> jni::errors::Result<jint> {
        static IDENTITY_KIND: FieldDesc = V0_ACTIONS_CLASS.field("identityKind", "I");

        env.get_field_unchecked(
            self.0.as_ref(),
            &IDENTITY_KIND,
            ReturnType::Primitive(Primitive::Int),
        )
        .and_then(|ret| ret.i())
    }

    /// Get the `int actionBits` field from the Java object.
    pub fn get_action_bits<'env_local>(
        &self,
        env: &mut JNIEnv<'env_local>,
    ) -> jni::errors::Result<jint> {
        static ACTION_BITS_FIELD: FieldDesc = V0_ACTIONS_CLASS.field("actionBits", "I");

        env.get_field_unchecked(
            self.0.as_ref(),
            &ACTION_BITS_FIELD,
            ReturnType::Primitive(Primitive::Int),
        )
        .and_then(|ret| ret.i())
    }

    /// Get the `np_ffi_core` representation of this data element. This returns `None` if the Rust
    /// representation would not be well formed.
    pub fn get_as_core<'env>(
        &self,
        env: &mut JNIEnv<'env>,
    ) -> jni::errors::Result<Option<v0::V0Actions>> {
        let identity_kind = self.get_identity_kind(env)?;
        let action_bits = self.get_action_bits(env)?;
        construct_actions_from_parts(env, identity_kind, action_bits)
    }
}

/// Helper to build a [`V0Actions`][v0::V0Actions] instance from raw Java fields. This will return
/// `None` if the given `identity_kind` is invalid.
fn construct_actions_from_parts(
    env: &mut JNIEnv<'_>,
    identity_kind: jint,
    action_bits: jint,
) -> jni::errors::Result<Option<v0::V0Actions>> {
    let opt_kind = IdentityKind::from_java(env, identity_kind)?;

    let wrapper = match opt_kind {
        Some(IdentityKind::Plaintext) => v0::V0Actions::Plaintext,
        Some(IdentityKind::Decrypted) => v0::V0Actions::Encrypted,
        _ => return Ok(None),
    };

    let bits = v0::V0ActionBits::from(action_bits as u32);
    Ok(Some(wrapper(bits)))
}

#[pourover::jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0DataElement.V0Actions"
)]
extern "system" fn nativeHasAction<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    identity_kind: jint,
    action_bits: jint,
    action: jint,
) -> jboolean {
    let actions = match construct_actions_from_parts(&mut env, identity_kind, action_bits) {
        Ok(Some(actions)) => actions,
        Ok(None) => {
            let _ = env.throw_new(
                "java/lang/IllegalStateException",
                "V0Actions is not validly constructed",
            );
            return JNI_FALSE;
        }
        Err(_jni_err) => {
            return JNI_FALSE;
        }
    };

    let Ok(action) = u8::try_from(action).map_err(From::from).and_then(v0::ActionType::try_from)
    else {
        return JNI_FALSE;
    };

    if actions.has_action(action).unwrap_or(false) {
        JNI_TRUE
    } else {
        JNI_FALSE
    }
}

#[pourover::jni_method(
    package = "com.google.android.nearby.presence.rust",
    class = "V0DataElement.V0Actions"
)]
extern "system" fn nativeMergeActions<'local>(
    mut env: JNIEnv<'local>,
    _cls: JClass<'local>,
    identity_kind: jint,
    actions: JIntArray<'local>,
) -> jint {
    let actions = {
        let Ok(len) = env.get_array_length(&actions) else {
            // `crate jni` should have already thrown
            return 0;
        };
        let Ok(len) = usize::try_from(len) else {
            // Should never occur
            return 0;
        };
        let mut vec = vec![0; len];
        if env.get_int_array_region(&actions, 0, &mut vec[..]).is_err() {
            // `crate jni` should have already thrown
            return 0;
        };
        vec
    };

    let mut parse_kind =
        |identity_kind: jint| -> jni::errors::Result<Option<AdvertisementBuilderKind>> {
            IdentityKind::from_java(&mut env, identity_kind).map(|opt_kind| {
                opt_kind.and_then(|kind| match kind {
                    IdentityKind::Plaintext => Some(AdvertisementBuilderKind::Public),
                    IdentityKind::Decrypted => Some(AdvertisementBuilderKind::Encrypted),
                    _ => None,
                })
            })
        };

    let adv_kind = match parse_kind(identity_kind) {
        Ok(Some(adv_kind)) => adv_kind,
        Ok(None) => {
            let _ = env.throw_new("java/lang/IllegalArgumentException", "Invalid identity kind");
            return 0;
        }
        Err(_jni_err) => return 0,
    };

    let mut v0_actions = v0::V0Actions::new_zeroed(adv_kind);
    for action in actions {
        let Ok(action_type) =
            u8::try_from(action).map_err(Into::into).and_then(v0::ActionType::try_from)
        else {
            let _ = env.throw_new(
                "java/lang/IllegalArgumentException",
                format!("V0ActionType is invalid: {action}"),
            );
            return 0;
        };

        v0_actions = match v0_actions.set_action(action_type, true) {
            Ok(v0::SetV0ActionResult::Success(v0_actions)) => v0_actions,
            Ok(v0::SetV0ActionResult::Error(_v0_actions)) => {
                let _ = env.throw_new(
                    "java/lang/IllegalStateException",
                    "V0ActionType is not valid for this IdentityKind",
                );
                return 0;
            }
            Err(InvalidStackDataStructure) => {
                let _ = env
                    .throw_new("java/lang/IllegalStateException", "Memory has somehow corrupted");
                return 0;
            }
        };
    }

    v0_actions.as_u32() as jint
}
