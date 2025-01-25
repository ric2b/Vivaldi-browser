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

use proc_macro2::TokenStream;
use syn::{punctuated::Punctuated, spanned::Spanned, ItemFn, LitStr, Token};

mod meta;
mod meta_arg;
mod substitutions;

use meta::JniMethodMeta;
use substitutions::substitute_method_chars;

pub fn jni_method(meta: TokenStream, item: TokenStream) -> syn::Result<ItemFn> {
    let meta = syn::parse2::<JniMethodMeta>(meta)?;
    let mut func = syn::parse2::<ItemFn>(item)?;

    // Check that ABI is set to `extern "system"`
    if let Some(
        ref abi @ syn::Abi {
            name: Some(ref abi_name),
            ..
        },
    ) = func.sig.abi
    {
        if abi_name.value() != "system" {
            return Err(syn::Error::new(
                abi.span(),
                "JNI methods are required to have the `extern \"system\"` ABI",
            ));
        }
    } else {
        return Err(syn::Error::new(
            func.sig.span(),
            "JNI methods are required to have the `extern \"system\"` ABI",
        ));
    }

    let export_attr = {
        // Format the name of the function as expected by the JNI layer
        let (method_name, method_name_span) = if let Some(meta_name) = &meta.method_name {
            (meta_name.value(), meta_name.span())
        } else {
            (func.sig.ident.to_string(), func.sig.ident.span())
        };
        if method_name.starts_with("Java_") {
            return Err(syn::Error::new(
                method_name_span,
                "The `jni_method` attribute will perform the JNI name formatting",
            ));
        }
        let method_name = substitute_method_chars(&method_name);

        // NOTE: doesn't handle overload suffix
        let link_name = LitStr::new(
            &format!("Java_{class}_{method_name}", class = &meta.class_desc),
            method_name_span,
        );

        syn::parse_quote! { #[export_name = #link_name] }
    };
    func.attrs.push(export_attr);

    // Allow function name to be non_snake_case if we are using it as the Java method name
    if meta.method_name.is_none() {
        let allow_attr = syn::parse_quote! { #[allow(non_snake_case)] };
        func.attrs.push(allow_attr);
    }

    // Add a panic handler if requested
    if let Some(panic_returns) = meta.panic_returns {
        let block = &func.block;
        let return_type = &func.sig.output;
        let mut lifetimes = Punctuated::new();
        for param in func.sig.generics.lifetimes() {
            lifetimes.push_value(param.clone());
            lifetimes.push_punct(<Token![,]>::default());
        }

        let panic_check = quote::quote_spanned! { panic_returns.span() =>
            #[cfg(not(panic = "unwind"))]
            ::core::compile_error!("Cannot use `panic_returns` with non-unwinding panic handler");
        };

        func.block = syn::parse_quote! {
            {
                #panic_check
                match ::std::panic::catch_unwind(move || {
                    #block
                }) {
                    Ok(ret) => ret,
                    Err(_err) => {
                        fn __on_panic<#lifetimes>() #return_type { #panic_returns }
                        __on_panic()
                    },
                }
            }
        };
    }

    // Return the modified function
    Ok(func)
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used, clippy::panic)]
mod tests {
    use super::*;
    use crate::test_util::contains_ident;
    use quote::{quote, ToTokens};

    #[test]
    fn can_parse() {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
        };

        let func = quote! {
            extern "system" fn nativeFoo<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        let out = jni_method(meta, func).unwrap();

        assert!(contains_ident(out.into_token_stream(), "catch_unwind"));
    }

    fn parse_example_output() -> syn::ItemFn {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
        };

        let func = quote! {
            extern "system" fn nativeFoo<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        jni_method(meta, func).expect("failed to generate example")
    }

    fn parse_example_output_method_name() -> syn::ItemFn {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
            method_name = "nativeBar",
            panic_returns = false,
        };

        let func = quote! {
            extern "system" fn native_bar<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        jni_method(meta, func).expect("failed to generate example")
    }

    #[test]
    fn check_output_is_itemfn() {
        let _item_fn = parse_example_output();
    }

    #[test]
    fn check_output_export_name() {
        let out = parse_example_output();

        let export_name = out
            .attrs
            .iter()
            .find_map(|attr| {
                let syn::Meta::NameValue(nv) = &attr.meta else {
                    return None;
                };
                if !nv.path.is_ident("export_name") {
                    return None;
                }
                let syn::Expr::Lit(syn::ExprLit {
                    lit: syn::Lit::Str(lit_str),
                    ..
                }) = &nv.value
                else {
                    return None;
                };
                Some(lit_str.value())
            })
            .expect("Failed to find `export_name` attribute");
        assert_eq!("Java_com_example_Foo_00024Inner_nativeFoo", export_name);
    }

    #[test]
    fn check_output_export_name_with_method_name() {
        let out = parse_example_output_method_name();

        let export_name = out
            .attrs
            .iter()
            .find_map(|attr| {
                let syn::Meta::NameValue(nv) = &attr.meta else {
                    return None;
                };
                if !nv.path.is_ident("export_name") {
                    return None;
                }
                let syn::Expr::Lit(syn::ExprLit {
                    lit: syn::Lit::Str(lit_str),
                    ..
                }) = &nv.value
                else {
                    return None;
                };
                Some(lit_str.value())
            })
            .expect("Failed to find `export_name` attribute");
        assert_eq!("Java_com_example_Foo_00024Inner_nativeBar", export_name);
    }

    #[test]
    fn check_output_allow_non_snake_case() {
        let out = parse_example_output();

        let _allow_attr = out
            .attrs
            .iter()
            .find(|attr| {
                let syn::Meta::List(ml) = &attr.meta else {
                    return false;
                };
                if !ml.path.is_ident("allow") {
                    return false;
                }
                let Ok(value) = syn::parse2::<syn::Path>(ml.tokens.clone()) else {
                    return false;
                };
                value.is_ident("non_snake_case")
            })
            .expect("Failed to find `allow(non_snake_case)` attribute");
    }

    #[test]
    fn check_output_allow_non_snake_case_not_present_with_method_name() {
        let out = parse_example_output_method_name();

        let allow_attr = out.attrs.iter().find(|attr| {
            let syn::Meta::List(ml) = &attr.meta else {
                return false;
            };
            if !ml.path.is_ident("allow") {
                return false;
            }
            let Ok(value) = syn::parse2::<syn::Path>(ml.tokens.clone()) else {
                return false;
            };
            value.is_ident("non_snake_case")
        });
        assert!(allow_attr.is_none());
    }

    #[test]
    fn no_panic_returns() {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
        };

        let func = quote! {
            extern "system" fn nativeFoo<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        let out = match jni_method(meta, func) {
            Ok(item_fn) => item_fn.into_token_stream(),
            Err(err) => err.into_compile_error(),
        };
        assert!(!contains_ident(out.clone(), "compile_error"));
        assert!(!contains_ident(out, "catch_unwind"));
    }

    #[test]
    fn missing_extern() {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
        };

        let func = quote! {
            fn nativeFoo<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        let Err(err) = jni_method(meta, func) else {
            panic!("Should fail to generate code");
        };

        assert!(err
            .to_string()
            .contains("JNI methods are required to have the `extern \"system\"` ABI"));
    }

    #[test]
    fn wrong_extern() {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
        };

        let func = quote! {
            extern "C" fn nativeFoo<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        let Err(err) = jni_method(meta, func) else {
            panic!("Should fail to generate code");
        };

        assert!(err
            .to_string()
            .contains("JNI methods are required to have the `extern \"system\"` ABI"));
    }

    #[test]
    fn already_mangled() {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
            panic_returns = false,
        };

        let func = quote! {
            extern "system" fn Java_com_example_Foo_00024Inner_nativeFoo<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        let Err(err) = jni_method(meta, func) else {
            panic!("Should fail to generate code");
        };

        assert!(err
            .to_string()
            .contains("The `jni_method` attribute will perform the JNI name formatting"));
    }

    #[test]
    fn already_mangled_method_name() {
        let meta = quote! {
            package = "com.example",
            class = "Foo.Inner",
            method_name = "Java_com_example_Foo_00024Inner_nativeFoo",
            panic_returns = false,
        };

        let func = quote! {
            extern "system" fn native_foo<'local>(
                mut env: JNIEnv<'local>,
                this: JObject<'local>
            ) -> jint {
                123
            }
        };

        let Err(err) = jni_method(meta, func) else {
            panic!("Should fail to generate code");
        };

        assert!(err
            .to_string()
            .contains("The `jni_method` attribute will perform the JNI name formatting"));
    }
}
