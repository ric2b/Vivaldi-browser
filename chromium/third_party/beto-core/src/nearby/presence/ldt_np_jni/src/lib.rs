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

//! JNI adapter for LDT.
//!
//! Helpful resources:
//! - <https://developer.ibm.com/articles/j-jni>
//! - <https://developer.android.com/training/articles/perf-jni>
//! - <https://www.iitk.ac.in/esc101/05Aug/tutorial/native1.1/index.html>
//! - <https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/jniTOC.html>

// We are not actually no_std because the jni crate is pulling it in, but at least this enforces
// that this lib isn't using anything from the std lib
#![no_std]
#![allow(unsafe_code)]

// Allow using Box in no_std
extern crate alloc;

use alloc::boxed::Box;

use jni::{
    objects::{JByteArray, JClass},
    sys::{jbyte, jchar, jint, jlong},
    JNIEnv,
};

use ldt::{LdtCipher, XorPadder};
use ldt_np_adv::{AuthenticatedNpLdtDecryptCipher, LdtAdvDecryptError, NpLdtEncryptCipher};
use np_hkdf::NpKeySeedHkdf;

use crypto_provider_default::CryptoProviderImpl;

/// Length limits per LDT
const MIN_DATA_LEN: usize = crypto_provider::aes::BLOCK_SIZE;
const MAX_DATA_LEN: usize = crypto_provider::aes::BLOCK_SIZE * 2 - 1;

/// Required size constraints of input parameters
const KEY_SEED_SIZE: usize = 32;
const TAG_SIZE: usize = 32;

/// Error return value for create operations
const CREATE_ERROR: jlong = 0;

/// Status code returned on successful cipher operations
const SUCCESS: jint = 0;

type LdtAdvDecrypter = AuthenticatedNpLdtDecryptCipher<CryptoProviderImpl>;
type LdtAdvEncrypter = NpLdtEncryptCipher<CryptoProviderImpl>;

/// Marker trait to ensure above types are thread safe
#[allow(dead_code)]
trait JniThreadSafe: Send + Sync {}

impl JniThreadSafe for LdtAdvDecrypter {}

impl JniThreadSafe for LdtAdvEncrypter {}

/// Create a LDT Encryption cipher.
///
/// Returns 0 on failure, or the non-zero handle as a jlong/i64 on success.
#[no_mangle]
extern "system" fn Java_com_google_android_gms_nearby_presence_hazmat_LdtNpJni_createEncryptionCipher(
    env: JNIEnv,
    _class: JClass,
    java_key_seed: JByteArray,
) -> jlong {
    create_map_to_error(|| {
        let key_seed =
            env.convert_byte_array(&java_key_seed).map_err(|_| CREATE_ERROR).and_then(|seed| {
                if seed.len() != KEY_SEED_SIZE {
                    Err(CREATE_ERROR)
                } else {
                    Ok(seed)
                }
            })?;

        let hkdf_key_seed = NpKeySeedHkdf::<CryptoProviderImpl>::new(
            #[allow(clippy::expect_used)]
            key_seed.as_slice().try_into().expect("Length is checked above"),
        );

        let cipher = LdtAdvEncrypter::new(&hkdf_key_seed.v0_ldt_key());
        box_to_handle(cipher).map_err(|_| CREATE_ERROR)
    })
}

/// Create a LDT Decryption cipher.
///
/// Returns 0 on failure, or the non-zero handle as a jlong/i64 on success.
#[no_mangle]
extern "system" fn Java_com_google_android_gms_nearby_presence_hazmat_LdtNpJni_createDecryptionCipher(
    env: JNIEnv,
    _class: JClass,
    java_key_seed: JByteArray,
    java_hmac_tag: JByteArray,
) -> jlong {
    create_map_to_error(|| {
        let key_seed =
            env.convert_byte_array(&java_key_seed).map_err(|_| CREATE_ERROR).and_then(|seed| {
                if seed.len() != KEY_SEED_SIZE {
                    Err(CREATE_ERROR)
                } else {
                    Ok(seed)
                }
            })?;
        let hmac_tag =
            env.convert_byte_array(&java_hmac_tag).map_err(|_| CREATE_ERROR).and_then(|tag| {
                if tag.len() != TAG_SIZE {
                    Err(CREATE_ERROR)
                } else {
                    Ok(tag)
                }
            })?;
        let hkdf_key_seed = NpKeySeedHkdf::<CryptoProviderImpl>::new(
            #[allow(clippy::expect_used)]
            key_seed.as_slice().try_into().expect("Length is checked above"),
        );

        #[allow(clippy::expect_used)]
        let cipher = ldt_np_adv::build_np_adv_decrypter_from_key_seed::<CryptoProviderImpl>(
            &hkdf_key_seed,
            hmac_tag.as_slice().try_into().expect("Length is checked above"),
        );
        box_to_handle(cipher).map_err(|_| CREATE_ERROR)
    })
}

fn create_map_to_error<F: Fn() -> Result<jlong, jlong>>(f: F) -> jlong {
    f().unwrap_or_else(|e| e)
}

/// Close an LDT Encryption Cipher
#[no_mangle]
extern "system" fn Java_com_google_android_gms_nearby_presence_hazmat_LdtNpJni_closeEncryptCipher(
    _env: JNIEnv,
    _class: JClass,
    handle: jlong,
) {
    // create the box, let it be dropped
    let _ = boxed_from_handle::<LdtAdvEncrypter>(handle);
}

/// Close an LDT Decryption Cipher
#[no_mangle]
extern "system" fn Java_com_google_android_gms_nearby_presence_hazmat_LdtNpJni_closeDecryptCipher(
    _env: JNIEnv,
    _class: JClass,
    handle: jlong,
) {
    // create the box, let it be dropped
    let _ = boxed_from_handle::<LdtAdvDecrypter>(handle);
}

/// Encrypt a buffer in place.
#[no_mangle]
extern "system" fn Java_com_google_android_gms_nearby_presence_hazmat_LdtNpJni_encrypt(
    env: JNIEnv,
    _class: JClass,
    handle: jlong,
    salt: jchar,
    data: JByteArray,
) -> jint {
    map_to_error_code(|| {
        let mut buffer =
            env.convert_byte_array(&data).map_err(|_| EncryptError::JniOp).and_then(|data| {
                if !(MIN_DATA_LEN..=MAX_DATA_LEN).contains(&data.len()) {
                    Err(EncryptError::DataLen)
                } else {
                    Ok(data)
                }
            })?;

        with_handle::<LdtAdvEncrypter, _, _>(handle, |cipher| {
            cipher.encrypt(buffer.as_mut_slice(), &expand_np_salt_to_padder(salt)).map_err(
                |err| match err {
                    ldt::LdtError::InvalidLength(_) => EncryptError::DataLen,
                },
            )?;

            // Avoid a copy since transmuting from a &[u8] to a &[i8] is safe
            // Safety:
            // - u8 and jbyte/i8 are the same size have the same alignment
            let jbyte_buffer = bytes_to_jbytes(buffer.as_slice());

            env.set_byte_array_region(&data, 0, jbyte_buffer)
                .map_err(|_| EncryptError::JniOp)
                .map(|_| SUCCESS)
        })
    })
}

/// Decrypt a buffer in place.
/// Safety: We know the data pointer is safe because it is coming directly from the JVM.
#[no_mangle]
extern "system" fn Java_com_google_android_gms_nearby_presence_hazmat_LdtNpJni_decryptAndVerify(
    env: JNIEnv,
    _class: JClass,
    handle: jlong,
    salt: jchar,
    data: JByteArray,
) -> jint {
    map_to_error_code(|| {
        let mut buffer =
            env.convert_byte_array(&data).map_err(|_| DecryptError::JniOp).and_then(|data| {
                if !(MIN_DATA_LEN..=MAX_DATA_LEN).contains(&data.len()) {
                    Err(DecryptError::DataLen)
                } else {
                    Ok(data)
                }
            })?;

        with_handle::<LdtAdvDecrypter, _, _>(handle, |cipher| {
            let (identity_token, plaintext) = cipher
                .decrypt_and_verify(buffer.as_mut_slice(), &expand_np_salt_to_padder(salt))
                .map_err(|err| match err {
                    LdtAdvDecryptError::InvalidLength(_) => DecryptError::DataLen,
                    LdtAdvDecryptError::MacMismatch => DecryptError::MacMisMatch,
                })?;

            let concatenated = &[identity_token.as_slice(), plaintext.as_slice()].concat();
            let jbyte_buffer = bytes_to_jbytes(concatenated);

            env.set_byte_array_region(&data, 0, jbyte_buffer)
                .map_err(|_| DecryptError::JniOp)
                .map(|_| SUCCESS)
        })
    })
}

/// A zero-copy conversion from a u8 slice to a jbyte slice
fn bytes_to_jbytes(bytes: &[u8]) -> &[jbyte] {
    // Safety:
    // - u8 and jbyte/i8 are the same size have the same alignment
    unsafe { alloc::slice::from_raw_parts(bytes.as_ptr() as *const jbyte, bytes.len()) }
}

/// Reconstruct a `Box<T>` from `handle`, and invoke `f` with the resulting `&T`.
///
/// The `Box<T>` is leaked after invoking `block` rather than dropped so that the handle can be used
/// again.
///
/// Returns the result of evaluating `f`.
fn with_handle<T, U, F: FnMut(&T) -> U>(handle: jlong, mut f: F) -> U {
    let boxed = boxed_from_handle(handle);
    let ret = f(&boxed);

    // don't consume the box -- need to keep the handle alive
    let _ = Box::leak(boxed);
    ret
}

/// Reconstruct a `Box<T>` from `handle`.
///
/// `handle` must be an aligned, non-null `jlong` representation of a pointer produced from
/// `Box::into_raw` that has not yet been deallocated.
fn boxed_from_handle<T>(handle: jlong) -> Box<T> {
    // on 32-bit systems, truncate i64 to low 32 bits (which should be the only bits that were set
    // when the jlong handle was created).
    let handle_usize = handle as usize;
    // convert pointer-sized integer to pointer
    unsafe { Box::from_raw(handle_usize as *mut _) }
}

/// Constructs a `Box<T>`, leaks a pointer to it, and converts the pointer to `jlong`.
///
/// If the pointer can't fit, `Err` is returned.
fn box_to_handle<T>(thing: T) -> Result<jlong, ()> {
    // Box::new heap allocates space for the thing
    // Box::into_raw intentionally leaks into an aligned, non-null pointer
    let pointer = Box::into_raw(Box::new(thing));
    // As a best practice, cast from pointer to usize because usize is always pointer sized, so the
    // cast is easy to reason about.
    // https://doc.rust-lang.org/reference/expressions/operator-expr.html#pointer-to-address-cast
    let ptr_usize = pointer as usize;
    // Fallible conversion into a u64 -- eventually 128 bit pointer types will fail here.
    // Assuming it fits, integer cast should be either no conversion or zero extension.
    ptr_usize
        .try_into()
        .map_err(|_| {
            // resuscitate the Box so that its drop can run, otherwise we would leak on error
            unsafe {
                let _ = Box::from_raw(pointer);
            }
        })
        // Now that we know the pointer fits in 64 bits, can cast u64 to i64/jlong.
        .map(|ptr_64: u64| ptr_64 as jlong)
}

/// Expand the NP salt to the size needed to be an LDT XorPadder.
///
/// Returns a XorPadder containing the HKDF of the salt.
fn expand_np_salt_to_padder(np_salt: jchar) -> XorPadder<{ crypto_provider::aes::BLOCK_SIZE }> {
    let salt_bytes = np_salt.to_be_bytes();
    ldt_np_adv::salt_padder::<CryptoProviderImpl>(salt_bytes.into())
}

fn map_to_error_code<E: JniError, F: Fn() -> Result<jint, E>>(f: F) -> jint {
    f().unwrap_or_else(|e| e.to_jni_error_code())
}

trait JniError {
    fn to_jni_error_code(&self) -> jint;
}

#[derive(Debug)]
enum EncryptError {
    /// Data is the wrong length
    DataLen,
    /// JNI op failed
    JniOp,
}

impl JniError for EncryptError {
    fn to_jni_error_code(&self) -> jint {
        match self {
            Self::DataLen => -1,
            Self::JniOp => -2,
        }
    }
}

#[derive(Debug)]
enum DecryptError {
    /// Data is the wrong length
    DataLen,
    /// The mac did not match the provided tag
    MacMisMatch,
    /// JNI op failed
    JniOp,
}

impl JniError for DecryptError {
    /// Returns an error code suitable for returning from Ldt encrypt/decrypt JNI calls.
    fn to_jni_error_code(&self) -> jint {
        match self {
            Self::DataLen => -1,
            Self::JniOp => -2,
            Self::MacMisMatch => -3,
        }
    }
}
