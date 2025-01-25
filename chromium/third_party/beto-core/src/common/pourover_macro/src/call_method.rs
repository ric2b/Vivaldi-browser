// Copyright 2024 Google LLC
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

//! Implementation of the `call_method!` series of macros. These macros are meant to be used as
//! function-like macros and implement a statically type-safe way to call Java methods while also
//! caching the method id. The macro arguments are implemented in [`mod ast`](mod@ast) and the
//! generated code is implemented in [`mod codegen`](mod@codegen).

use proc_macro2::TokenStream;
use syn::parse_quote;

use ast::{ConstructorArgs, InstanceArgs, StaticArgs};
use codegen::{MethodCall, MethodInfo, Receiver};

mod ast;
mod codegen;

/// See [`crate::call_method!`] for usage.
pub fn call_method(args: TokenStream) -> syn::Result<TokenStream> {
    let args = syn::parse2::<InstanceArgs>(args)?;

    let method_info = MethodInfo::new(args.cls, args.name, args.sig);
    let receiver = Receiver::Instance(args.this);
    let method_call = MethodCall::new(
        args.env,
        method_info,
        receiver,
        args.args.into_iter().collect(),
    );

    method_call.generate().map_err(syn::Error::from)
}

/// See [`crate::call_static_method!`] for usage.
pub fn call_static_method(args: TokenStream) -> syn::Result<TokenStream> {
    let args = syn::parse2::<StaticArgs>(args)?;

    let method_info = MethodInfo::new(args.cls, args.name, args.sig);
    let receiver = Receiver::Static;
    let method_call = MethodCall::new(
        args.env,
        method_info,
        receiver,
        args.args.into_iter().collect(),
    );

    method_call.generate().map_err(syn::Error::from)
}

/// See [`crate::call_constructor!`] for usage.
pub fn call_constructor(args: TokenStream) -> syn::Result<TokenStream> {
    let args = syn::parse2::<ConstructorArgs>(args)?;

    let name = parse_quote!["<init>"];
    let method_info = MethodInfo::new(args.cls, name, args.sig);
    let receiver = Receiver::Constructor;
    let method_call = MethodCall::new(
        args.env,
        method_info,
        receiver,
        args.args.into_iter().collect(),
    );

    method_call.generate().map_err(syn::Error::from)
}

#[cfg(test)]
mod test {
    use super::*;
    use quote::quote;

    #[test]
    fn call_method_error() {
        let out = call_method(quote![&mut env, &CLS, "method", "INVALID", &this_obj]);
        assert!(out.is_err());
    }

    #[test]
    fn call_static_method_error() {
        let out = call_static_method(quote![&mut env, &CLS, "method", "INVALID"]);
        assert!(out.is_err());
    }

    #[test]
    fn call_constructor_error() {
        let out = call_constructor(quote![&mut env, &CLS, "INVALID"]);
        assert!(out.is_err());
    }
}
