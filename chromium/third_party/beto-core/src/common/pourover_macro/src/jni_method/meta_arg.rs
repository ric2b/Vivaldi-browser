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

use syn::{
    parse::{Parse, ParseStream},
    Expr, LitStr, Token,
};

/// Custom keywords
pub mod kw {
    syn::custom_keyword!(package);
    syn::custom_keyword!(class);
    syn::custom_keyword!(method_name);
    syn::custom_keyword!(panic_returns);
}

/// Arguments to the attribute
pub enum MetaArg {
    Package {
        package_token: kw::package,
        _eq_token: Token![=],
        value: LitStr,
    },
    Class {
        class_token: kw::class,
        _eq_token: Token![=],
        value: LitStr,
    },
    MethodName {
        method_name_token: kw::method_name,
        _eq_token: Token![=],
        value: LitStr,
    },
    PanicReturns {
        panic_returns_token: kw::panic_returns,
        _eq_token: Token![=],
        value: Expr,
    },
}

impl Parse for MetaArg {
    fn parse(stream: ParseStream<'_>) -> syn::Result<Self> {
        let lookahead = stream.lookahead1();
        if lookahead.peek(kw::package) {
            Ok(MetaArg::Package {
                package_token: stream.parse::<kw::package>()?,
                _eq_token: stream.parse()?,
                value: stream.parse()?,
            })
        } else if lookahead.peek(kw::class) {
            Ok(MetaArg::Class {
                class_token: stream.parse::<kw::class>()?,
                _eq_token: stream.parse()?,
                value: stream.parse()?,
            })
        } else if lookahead.peek(kw::method_name) {
            Ok(MetaArg::MethodName {
                method_name_token: stream.parse::<kw::method_name>()?,
                _eq_token: stream.parse()?,
                value: stream.parse()?,
            })
        } else if lookahead.peek(kw::panic_returns) {
            Ok(MetaArg::PanicReturns {
                panic_returns_token: stream.parse::<kw::panic_returns>()?,
                _eq_token: stream.parse()?,
                value: stream.parse()?,
            })
        } else {
            Err(lookahead.error())
        }
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used, clippy::panic)]
mod tests {
    use super::*;
    use syn::parse_quote;

    #[test]
    fn parse_meta_arg_package() {
        let MetaArg::Package { value, .. } = parse_quote!(package = "com.example") else {
            panic!("failed to parse")
        };

        assert_eq!("com.example", value.value());
    }

    #[test]
    fn parse_meta_arg_class() {
        let MetaArg::Class { value, .. } = parse_quote!(class = "Foo") else {
            panic!("failed to parse")
        };

        assert_eq!("Foo", value.value());
    }

    #[test]
    fn parse_meta_arg_panic_returns() {
        let MetaArg::PanicReturns { value, .. } = parse_quote!(panic_returns = { foo() }) else {
            panic!("failed to parse")
        };

        let syn::Expr::Block(_) = value else {
            panic!("not a block expression")
        };
    }
}
