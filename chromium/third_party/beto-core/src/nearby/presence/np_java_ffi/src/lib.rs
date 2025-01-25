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

//! Java bindings for np_ffi_core.

// Named lifetimes are being used to match jni crate conventions.
// See: https://docs.rs/jni/latest/jni/struct.JNIEnv.html#lifetime-names
#![allow(clippy::needless_lifetimes)]

pub mod class;
#[cfg(feature = "testing")]
pub mod testing;
