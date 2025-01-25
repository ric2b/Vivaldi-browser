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

//! Syntax trees for the arguments to the `call_method!` series of proc macros.

use syn::{
    parse::{Parse, ParseStream},
    punctuated::Punctuated,
    Expr, LitStr, Token,
};

/// The trailing method arguments
type Args = Punctuated<Expr, Token![,]>;

/// See [`crate::call_constructor!`].
pub struct ConstructorArgs {
    pub env: Expr,
    pub cls: Expr,
    pub sig: LitStr,
    pub args: Args,
}

impl Parse for ConstructorArgs {
    fn parse(stream: ParseStream<'_>) -> syn::Result<Self> {
        let env = CommaSep::parse(stream)?.node;
        let cls = CommaSep::parse(stream)?.node;
        let sig = stream.parse()?;
        let args = parse_args(stream)?;

        Ok(Self {
            env,
            cls,
            sig,
            args,
        })
    }
}

/// See [`crate::call_static_method!`].
pub struct StaticArgs {
    pub env: Expr,
    pub cls: Expr,
    pub name: Expr,
    pub sig: LitStr,
    pub args: Args,
}

impl Parse for StaticArgs {
    fn parse(stream: ParseStream<'_>) -> syn::Result<Self> {
        let env = CommaSep::parse(stream)?.node;
        let cls = CommaSep::parse(stream)?.node;
        let name = CommaSep::parse(stream)?.node;
        let sig = stream.parse()?;
        let args = parse_args(stream)?;

        Ok(Self {
            env,
            cls,
            name,
            sig,
            args,
        })
    }
}

/// See [`crate::call_method!`].
pub struct InstanceArgs {
    pub env: Expr,
    pub cls: Expr,
    pub name: Expr,
    pub sig: LitStr,
    pub this: Expr,
    pub args: Args,
}

impl Parse for InstanceArgs {
    fn parse(stream: ParseStream<'_>) -> syn::Result<Self> {
        let env = CommaSep::parse(stream)?.node;
        let cls = CommaSep::parse(stream)?.node;
        let name = CommaSep::parse(stream)?.node;
        let sig = CommaSep::parse(stream)?.node;
        let this = stream.parse()?;
        let args = parse_args(stream)?;

        Ok(Self {
            env,
            cls,
            name,
            sig,
            this,
            args,
        })
    }
}

/// Parses the variable number of arguments to the method. A leading comma is required when there
/// are arguments being passed.
fn parse_args(stream: ParseStream<'_>) -> syn::Result<Args> {
    Ok(if stream.is_empty() {
        Punctuated::new()
    } else {
        let _ = stream.parse::<Token![,]>()?;
        Punctuated::parse_terminated(stream)?
    })
}

/// A syntax node followed by a comma.
#[allow(dead_code)]
pub struct CommaSep<T> {
    pub node: T,
    pub comma: Token![,],
}

impl<T: Parse> Parse for CommaSep<T> {
    fn parse(stream: ParseStream<'_>) -> syn::Result<Self> {
        Ok(Self {
            node: stream.parse()?,
            comma: stream.parse()?,
        })
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used, clippy::panic)]
mod test {
    use super::*;
    use quote::quote;
    use syn::parse::Parser;

    #[test]
    fn comma_sep_correct() {
        let ret: CommaSep<syn::Ident> = syn::parse2(quote![abc,]).unwrap();

        assert_eq!(ret.node, "abc");
    }

    #[test]
    fn comma_sep_incorrect() {
        assert!(syn::parse2::<CommaSep<syn::Ident>>(quote![abc]).is_err());
        assert!(syn::parse2::<CommaSep<syn::Ident>>(quote![,abc]).is_err());
        assert!(syn::parse2::<CommaSep<syn::Ident>>(quote![,]).is_err());
    }

    #[test]
    fn parse_args_no_args() {
        let args = parse_args.parse2(quote![]).unwrap();

        assert_eq!(0, args.len());
    }

    #[test]
    fn parse_args_no_args_extra_comma() {
        let args = parse_args.parse2(quote![,]).unwrap();

        assert_eq!(0, args.len());
    }

    #[test]
    fn parse_args_single_no_trailing() {
        let args = parse_args.parse2(quote![, foo]).unwrap();

        assert_eq!(1, args.len());
    }

    #[test]
    fn parse_args_single_trailing() {
        let args = parse_args.parse2(quote![, foo,]).unwrap();

        assert_eq!(1, args.len());
    }

    #[test]
    fn parse_args_multi_no_trailing() {
        let args = parse_args.parse2(quote![, one, two, three]).unwrap();

        assert_eq!(3, args.len());
    }

    #[test]
    fn parse_args_multi_trailing() {
        let args = parse_args.parse2(quote![, one, two, three,]).unwrap();

        assert_eq!(3, args.len());
    }

    #[test]
    fn parse_args_error_two_commas() {
        let res = parse_args.parse2(quote![, ,]);

        assert!(res.is_err());
    }

    #[test]
    fn parse_constructor_args() {
        let tests = [
            quote![&mut env, &CLS, "()V"],
            quote![&mut env, &CLS, "()V",],
            quote![&mut env, &CLS, "(I)V", 123],
            quote![&mut env, &CLS, "(I)V", 123,],
            quote![&mut env, &CLS, "(ILjava/lang/String;)V", 123, &some_str],
            quote![&mut env, &CLS, "(ILjava/lang/String;)V", 123, &some_str,],
        ];

        for valid in tests {
            assert!(
                syn::parse2::<ConstructorArgs>(valid.clone()).is_ok(),
                "test: {valid}"
            );
        }
    }

    #[test]
    fn parse_static_method_args() {
        let tests = [
            quote![&mut env, &CLS, "methodName", "()V"],
            quote![&mut env, &CLS, "methodName", "()V",],
            quote![&mut env, &CLS, "methodName", "(I)V", 123],
            quote![&mut env, &CLS, "methodName", "(I)V", 123,],
            quote![
                &mut env,
                &CLS,
                "methodName",
                "(ILjava/lang/String;)V",
                123,
                &some_str
            ],
            quote![
                &mut env,
                &CLS,
                "methodName",
                "(ILjava/lang/String;)V",
                123,
                &some_str,
            ],
        ];

        for valid in tests {
            assert!(
                syn::parse2::<StaticArgs>(valid.clone()).is_ok(),
                "test: {valid}"
            );
        }
    }

    #[test]
    fn parse_method_args() {
        let tests = [
            quote![&mut env, &CLS, "methodName", "()V", &this_obj],
            quote![&mut env, &CLS, "methodName", "()V", &this_obj,],
            quote![&mut env, &CLS, "methodName", "(I)V", &this_obj, 123],
            quote![&mut env, &CLS, "methodName", "(I)V", &this_obj, 123,],
            quote![
                &mut env,
                &CLS,
                "methodName",
                "(ILjava/lang/String;)V",
                &this_obj,
                123,
                &some_str
            ],
            quote![
                &mut env,
                &CLS,
                "methodName",
                "(ILjava/lang/String;)V",
                &this_obj,
                123,
                &some_str,
            ],
        ];

        for valid in tests {
            assert!(
                syn::parse2::<InstanceArgs>(valid.clone()).is_ok(),
                "test: {valid}"
            );
        }
    }
}
