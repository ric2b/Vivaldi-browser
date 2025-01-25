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

use proc_macro2::Span;
use syn::{
    parse::{Parse, ParseStream},
    Expr, LitStr, Token,
};

use super::meta_arg::MetaArg;
use super::substitutions::{substitute_class_chars, substitute_package_chars};

pub struct JniMethodMeta {
    /// The class descriptor in [export_name format](https://docs.oracle.com/javase/8/docs/technotes/guides/jni/spec/design.html#resolving_native_method_names)
    pub class_desc: String,
    /// The method name in Java. Use the Rust method name if not specified
    pub method_name: Option<LitStr>,
    /// The expression to run when a panic is encountered. The value of this expression will be
    /// returned by the annotated function.
    pub panic_returns: Option<Expr>,
}

impl Parse for JniMethodMeta {
    fn parse(stream: ParseStream<'_>) -> syn::Result<Self> {
        let mut package = None;
        let mut class = None;
        let mut method_name = None;
        let mut panic_returns = None;

        fn set_once<T>(opt: &mut Option<T>, value: T, span: Span, field: &str) -> syn::Result<()> {
            if let Some(_old) = opt.replace(value) {
                return Err(syn::Error::new(
                    span,
                    format!("`{field}` should not be specified twice"),
                ));
            }
            Ok(())
        }

        type Structure = syn::punctuated::Punctuated<MetaArg, Token![,]>;

        for arg in Structure::parse_terminated(stream)? {
            match arg {
                MetaArg::Package {
                    package_token,
                    value,
                    ..
                } => {
                    set_once(&mut package, value, package_token.span, "package")?;
                }
                MetaArg::Class {
                    class_token, value, ..
                } => {
                    set_once(&mut class, value, class_token.span, "class")?;
                }
                MetaArg::MethodName {
                    method_name_token,
                    value,
                    ..
                } => {
                    set_once(
                        &mut method_name,
                        value,
                        method_name_token.span,
                        "method_name",
                    )?;
                }
                MetaArg::PanicReturns {
                    panic_returns_token,
                    value,
                    ..
                } => {
                    set_once(
                        &mut panic_returns,
                        value,
                        panic_returns_token.span,
                        "panic_returns",
                    )?;
                }
            }
        }

        let Some(package) = package else {
            return Err(syn::Error::new(stream.span(), "`package` is required"));
        };

        let Some(class) = class else {
            return Err(syn::Error::new(stream.span(), "`class` is required"));
        };

        let package = {
            let package_str = package.value();
            if package_str.contains('/') {
                return Err(syn::Error::new(
                    stream.span(),
                    "`package` should use '.' as a separator",
                ));
            }
            substitute_package_chars(&package_str)
        };
        let class = substitute_class_chars(&class.value());
        let class_desc = format!("{package}_{class}");

        Ok(Self {
            class_desc,
            method_name,
            panic_returns,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use syn::parse_quote;

    #[test]
    fn can_parse() {
        let meta: JniMethodMeta = parse_quote! {
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
        };
        assert_eq!("com_example_Foo_00024Inner", &meta.class_desc);
        assert!(meta.panic_returns.is_some());
    }

    #[test]
    fn can_parse_missing_panic_returns() {
        let meta: JniMethodMeta = parse_quote! {
            package = "com.example",
            class = "Foo.Inner",
        };
        assert_eq!("com_example_Foo_00024Inner", &meta.class_desc);
        assert!(meta.panic_returns.is_none());
    }

    #[test]
    #[should_panic]
    fn double_package() {
        let _: JniMethodMeta = parse_quote! {
            package = "com.example",
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
        };
    }

    #[test]
    #[should_panic]
    fn package_uses_slashes() {
        let _: JniMethodMeta = parse_quote! {
            package = "com/example",
            class = "Foo.Inner",
            panic_returns = false,
        };
    }

    #[test]
    #[should_panic]
    fn missing_package() {
        let _: JniMethodMeta = parse_quote! {
            class = "Foo.Inner",
            panic_returns = false,
        };
    }

    #[test]
    #[should_panic]
    fn double_class() {
        let _: JniMethodMeta = parse_quote! {
            package = "com.example",
            class = "Foo.Inner",
            class = "Foo.Inner",
            panic_returns = false,
        };
    }

    #[test]
    #[should_panic]
    fn missing_class() {
        let _: JniMethodMeta = parse_quote! {
            package = "com.example",
            panic_returns = false,
        };
    }

    #[test]
    #[should_panic]
    fn double_panic_returns() {
        let _: JniMethodMeta = parse_quote! {
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
            panic_returns = false,
        };
    }
}
