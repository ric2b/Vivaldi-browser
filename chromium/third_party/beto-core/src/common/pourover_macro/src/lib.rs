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

//! Proc macros for `pourover`. These macros are reexported by the `pourover` crate, so this crate
//! is an implementation detail.

#[cfg(proc_macro)]
use proc_macro::TokenStream;

mod call_method;
mod jni_method;
mod type_parser;

/// Export a function as a JNI native method. This will attach a `#[export_name = "..."]` attribute
/// that is formatted with the given parameters. The provided `package`, `class`, and `method_name`
/// will be combined and formatted in according to the [JNI method name resolution rules][JNI
/// naming].
///
/// [JNI naming]:
///     https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/design.html#resolving_native_method_names
///
/// # Parameters
/// - `package` (LitStr): the Java package for the class being implemented
/// - `class` (LitStr): the Java class being implemented. Use `Foo.Inner` syntax for inner classes.
/// - `method_name` (*optional* LitStr): the method's name in Java. The Rust function name will be
///   used if this parameter is not set.
/// - `panic_returns` (*optional* Expr): the value to return when a panic is encountered. This can
///   not access local variables. This may only be used with `panic=unwind` and will produce a
///   compile error otherwise.
///
/// When using `panic_returns` function arguments must be [`std::panic::UnwindSafe`]. See
/// [`std::panic::catch_unwind`] for details. In practice this will not cause issues as JNI
/// arguments and return values are passed by pointer or value and not by Rust reference.
///
/// # Example
/// ```
/// # use pourover_macro::jni_method;
/// # use jni::{sys::jint, objects::{JObject, JString}, JNIEnv};
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
#[cfg(proc_macro)]
#[proc_macro_attribute]
pub fn jni_method(meta: TokenStream, item: TokenStream) -> TokenStream {
    use quote::ToTokens;
    match jni_method::jni_method(meta.into(), item.into()) {
        Ok(item_fn) => item_fn.into_token_stream(),
        Err(err) => err.into_compile_error(),
    }
    .into()
}

/// Call a Java method.
///
/// # Parameters
/// `call_method!($env, $cls, $name, $sig, $this, $($args),*)`
/// - `env` (Expr: `&mut jni::JNIEnv`): The JNI environment.
/// - `cls` (Expr: `&'static ClassDesc`): The class containing the method.
/// - `name` (Expr: `&'static str`): The name of the method.
/// - `sig` (LitStr): The JNI type signature of the method. This needs to be a literal so that it
///   can be parsed by the macro to type-check args and return a correctly-typed value.
/// - `this` (Expr: `&JObject`): The Java object receiving the method call.
/// - `args` (Expr ...): A variable number of arguments to be passed to the method.
///
/// # Caching
/// Each macro callsite will generate a `static` `MethodDesc` to cache the method id. Due to this,
/// **this macro call should be wrapped in function** instead of being called multiple times.
///
/// # Type-Safety
/// The given type signature will be parsed and arguments will be type checked against it. The
/// expected types are from the `jni` crate:
/// - Primitives: `jni::sys::{jboolean, jbyte, jchar, jshort, jint, jlong, jfloat, jdouble}`
/// - Arrays: `jni::objects::{JPrimitiveArray, JObjectArray}`
/// - Objects: `jni::objects::{JObject, JString, JMap, JList}`
///
/// Similarly, the return type will be one of the types above.
///
/// # Returns
/// The macro will evaluate to `jni::errors::Result<R>` where `R` is the return type parsed from the
/// type signature.
///
/// # Example
/// Let's call `sayHello` from the following class.
/// ```java
/// package com.example;
/// class Foo {
///     int sayHello(String name) { /* ... */ }
/// }
/// ```
/// We can use `call_method!` to implement the function call.
/// ```rust
/// # use jni::{sys::jint, objects::{JObject, JString}, JNIEnv};
/// # use pourover_macro::call_method;
/// # use pourover::desc::*;
/// static MY_CLASS: ClassDesc = ClassDesc::new("com/example/Foo");
/// fn say_hello<'l>(
///     env: &mut JNIEnv<'l>,
///     my_obj: &JObject<'_>,
///     name: &JString<'_>
/// ) -> jni::errors::Result<jint> {
///     call_method!(env, &MY_CLASS, "sayHello", "(Ljava/lang/String;)I", my_obj, name)
/// }
/// ```
#[cfg(proc_macro)]
#[proc_macro]
pub fn call_method(args: TokenStream) -> TokenStream {
    call_method::call_method(args.into())
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

/// Call a static Java method.
///
/// # Parameters
/// `call_static_method!($env, $cls, $name, $sig, $($args),*)`
/// - `env` (Expr: `&mut jni::JNIEnv`): The JNI environment.
/// - `cls` (Expr: `&'static ClassDesc`): The class containing the method.
/// - `name` (Expr: `&'static str`): The name of the method.
/// - `sig` (LitStr): The JNI type signature of the method. This needs to be a literal so that it
///   can be parsed by the macro to type-check args and return a correctly-typed value.
/// - `args` (Expr ...): A variable number of arguments to be passed to the method.
///
/// # Caching
/// Each macro callsite will generate a `static` `StaticMethodDesc` to cache the method id. Due to
/// this, **this macro call should be wrapped in function** instead of being called multiple times.
///
/// # Type-Safety
/// The given type signature will be parsed and arguments will be type checked against it. The
/// expected types are from the `jni` crate:
/// - Primitives: `jni::sys::{jboolean, jbyte, jchar, jshort, jint, jlong, jfloat, jdouble}`
/// - Arrays: `jni::objects::{JPrimitiveArray, JObjectArray}`
/// - Objects: `jni::objects::{JObject, JString, JMap, JList}`
///
/// Similarly, the return type will be one of the types above.
///
/// # Returns
/// The macro will evaluate to `jni::errors::Result<R>` where `R` is the return type parsed from the
/// type signature.
///
/// # Example
/// Let's call `sayHello` from the following class.
/// ```java
/// package com.example;
/// class Foo {
///     static int sayHello(String name) { /* ... */ }
/// }
/// ```
/// We can use `call_static_method!` to implement the function call.
/// ```rust
/// # use jni::{sys::jint, objects::{JObject, JString}, JNIEnv};
/// # use pourover_macro::call_static_method;
/// # use pourover::desc::*;
/// static MY_CLASS: ClassDesc = ClassDesc::new("com/example/Foo");
/// fn say_hello<'l>(
///     env: &mut JNIEnv<'l>,
///     name: &JString<'_>
/// ) -> jni::errors::Result<jint> {
///     call_static_method!(env, &MY_CLASS, "sayHello", "(Ljava/lang/String;)I", name)
/// }
/// ```
#[cfg(proc_macro)]
#[proc_macro]
pub fn call_static_method(args: TokenStream) -> TokenStream {
    call_method::call_static_method(args.into())
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

/// Call a Java constructor.
///
/// # Parameters
/// `call_constructor!($env, $cls, $sig, $($args),*)`
/// - `env` (Expr: `&mut jni::JNIEnv`): The JNI environment.
/// - `cls` (Expr: `&'static ClassDesc`): The class to be constructed.
/// - `sig` (LitStr): The JNI type signature of the constructor. This needs to be a literal so that
///   it can be parsed by the macro to type-check args and return a correctly-typed value.
/// - `args` (Expr ...): A variable number of arguments to be passed to the constructor.
///
/// # Caching
/// Each macro callsite will generate a `static` `MethodDesc` to cache the method id. Due to this,
/// **this macro call should be wrapped in function** instead of being called multiple times.
///
/// # Type-Safety
/// The given type signature will be parsed and arguments will be type checked against it. The
/// expected types are from the `jni` crate:
/// - Primitives: `jni::sys::{jboolean, jbyte, jchar, jshort, jint, jlong, jfloat, jdouble}`
/// - Arrays: `jni::objects::{JPrimitiveArray, JObjectArray}`
/// - Objects: `jni::objects::{JObject, JString, JMap, JList}`
///
/// # Returns
/// The macro will evaluate to `jni::errors::Result<jni::objects::JObject>`.
///
/// # Example
/// Let's call the constructor from the following class.
/// ```java
/// package com.example;
/// class Foo {
///     Foo(String name) { /* ... */ }
/// }
/// ```
/// We can use `call_constructor!` to implement the function call.
/// ```rust
/// # use jni::{objects::{JObject, JString}, JNIEnv};
/// # use pourover_macro::call_constructor;
/// # use pourover::desc::*;
/// static MY_CLASS: ClassDesc = ClassDesc::new("com/example/Foo");
/// fn construct_foo<'l>(
///     env: &mut JNIEnv<'l>,
///     name: &JString<'_>
/// ) -> jni::errors::Result<JObject<'l>> {
///     call_constructor!(env, &MY_CLASS, "(Ljava/lang/String;)V", name)
/// }
/// ```
#[cfg(proc_macro)]
#[proc_macro]
pub fn call_constructor(args: TokenStream) -> TokenStream {
    call_method::call_constructor(args.into())
        .unwrap_or_else(syn::Error::into_compile_error)
        .into()
}

#[cfg(test)]
pub(crate) mod test_util {
    use proc_macro2::{TokenStream, TokenTree};

    /// Iterator that traverses TokenTree:Group structures in preorder.
    struct FlatStream {
        streams: Vec<<TokenStream as IntoIterator>::IntoIter>,
    }

    impl FlatStream {
        fn new(stream: TokenStream) -> Self {
            Self {
                streams: vec![stream.into_iter()],
            }
        }
    }

    impl Iterator for FlatStream {
        type Item = TokenTree;

        fn next(&mut self) -> Option<TokenTree> {
            let next = loop {
                let stream = self.streams.last_mut()?;
                if let Some(next) = stream.next() {
                    break next;
                }
                let _ = self.streams.pop();
            };

            if let TokenTree::Group(group) = &next {
                self.streams.push(group.stream().into_iter());
            }

            Some(next)
        }
    }

    pub fn contains_ident(stream: TokenStream, ident: &str) -> bool {
        FlatStream::new(stream)
            .filter_map(|tree| {
                let TokenTree::Ident(ident) = tree else {
                    return None;
                };
                Some(ident.to_string())
            })
            .any(|ident_str| ident_str == ident)
    }
}
