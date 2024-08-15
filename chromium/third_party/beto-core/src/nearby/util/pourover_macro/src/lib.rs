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

extern crate proc_macro;

use pourover_macro_core as core;
use proc_macro::TokenStream;

/// Export a function as a JNI native method. This will attach a `#[export_name = "..."]` attribute that
/// is formatted with the given parameters.
///
/// # Parameters
/// - `package` (lit str): the Java package for the class being implemented
/// - `class` (lit str): the Java class being implemented. Use `Foo.Inner` syntax for inner
/// classes.
/// - `panic_returns` (expr): the value to return when a panic is encountered. This can not access
/// local variables. This may only be used with `panic=unwind` and will produce a compile error
/// otherwise.
///
/// When using `panic_returns` function arguments must be [`std::panic::UnwindSafe`]. See
/// [`std::panic::catch_unwind`] for details. In practice this will not cause issues as JNI
/// arguments and return values are passed by pointer or value and not by Rust reference.
///
/// # Example
/// ```
/// # use pourover_macro::jni_method;
/// # #[allow(non_camel_case_types)] type jint = i32; // avoid jni dep for test
/// # type JNIEnv<'local> = *mut ();
/// # type JObject<'local> = *mut ();
///
/// #[jni_method(package = "my.package", class = "Foo", panic_returns = -1)]
/// extern "system" fn getFoo<'local>(
///     mut env: JNIEnv<'local>,
///     this: JObject<'local>,
/// ) -> jint {
///     // ...
///     0
/// }
/// ```
///
/// This function will be exported with `#[export_name = "Java_my_package_Foo_getFoo"]`.
#[proc_macro_attribute]
pub fn jni_method(meta: TokenStream, item: TokenStream) -> TokenStream {
    core::jni_method(meta.into(), item.into()).into()
}
