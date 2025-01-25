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

//! Internal crate for use by [`derive_fuzztest`](../derive_fuzztest/index.html). See the
//! documentation there for usage information.

use proc_macro::TokenStream;
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::{parse::Nothing, spanned::Spanned, ItemFn, Pat, PatType, Type};

/// Define a fuzz test.
///
/// All input parameters of the given function must implement `arbitrary::Arbitrary`.
///
/// This macro derives new items based on the given function.
/// 1. A `fuzz_target!` is generated that can be used with `cargo fuzz`.
/// 2. Property tests (`quickcheck` or `proptest`, based on which features are enabled) are
///    generated that can be tested using `cargo test`.
///
/// See the crate documentation [`derive_fuzztest`](../derive_fuzztest/index.html) for details.
#[proc_macro_attribute]
pub fn fuzztest(attr: TokenStream, item: TokenStream) -> TokenStream {
    fuzztest_impl(attr.into(), item.into())
        .unwrap_or_else(|e| e.into_compile_error())
        .into()
}

fn fuzztest_impl(attr: TokenStream2, item: TokenStream2) -> syn::Result<TokenStream2> {
    syn::parse2::<Nothing>(attr)?;
    let func = syn::parse2::<ItemFn>(item)?;
    let fn_def = FunctionDefinition::parse(func)?;
    let original_fn = &fn_def.func;
    let fuzz_target = derive_fuzz_target(&fn_def);
    let proptest_target = proptest::derive_proptest(&fn_def);
    let quickcheck_target = quickcheck::derive_quickcheck(&fn_def);

    Ok(quote! {
        #[allow(unused)]
        #original_fn
        #fuzz_target
        #proptest_target
        #quickcheck_target
    })
}

/// Define a fuzz target only without corresponding test.
///
/// All input parameters of the given function must implement `arbitrary::Arbitrary`.
///
/// This macro derives a `fuzz_target!` that can be used with `cargo fuzz`. If you wish to generate
/// property tests that can be used with `cargo test` as well, use [`fuzztest`][macro@fuzztest].
///
/// See the crate documentation [`derive_fuzztest`](../derive_fuzztest/index.html) for details.
#[proc_macro_attribute]
pub fn fuzz(attr: TokenStream, item: TokenStream) -> TokenStream {
    fuzz_impl(attr.into(), item.into())
        .unwrap_or_else(|e| e.into_compile_error())
        .into()
}

fn fuzz_impl(attr: TokenStream2, item: TokenStream2) -> syn::Result<TokenStream2> {
    syn::parse2::<Nothing>(attr)?;
    let func = syn::parse2::<ItemFn>(item)?;
    let fn_def = FunctionDefinition::parse(func)?;
    let original_fn = &fn_def.func;
    let fuzz_target = derive_fuzz_target(&fn_def);

    Ok(quote! {
        #[allow(unused)]
        #original_fn
        #fuzz_target
    })
}

/// Define a property test.
///
/// This is similar to using `quickcheck!` or `proptest::proptest!` directly.
///
/// All input parameters of the given function must implement `arbitrary::Arbitrary`.
///
/// Unlike [`fuzztest`][macro@fuzztest], this macro does not have to be placed in a `[[bin]]` target
/// and a single file can contain multiple of these tests. The generated tests can be run with
/// `cargo test` as usual.
#[proc_macro_attribute]
pub fn proptest(attr: TokenStream, item: TokenStream) -> TokenStream {
    proptest_impl(attr.into(), item.into())
        .unwrap_or_else(|e| e.into_compile_error())
        .into()
}

fn proptest_impl(attr: TokenStream2, item: TokenStream2) -> syn::Result<TokenStream2> {
    syn::parse2::<Nothing>(attr)?;
    let func = syn::parse2::<ItemFn>(item)?;
    let fn_def = FunctionDefinition::parse(func)?;
    let original_fn = &fn_def.func;
    let proptest_target = proptest::derive_proptest(&fn_def);

    Ok(quote! {
        #[allow(unused)]
        #original_fn
        #proptest_target
    })
}

fn derive_fuzz_target(fn_def: &FunctionDefinition) -> proc_macro2::TokenStream {
    let FunctionDefinition { func, args, types } = fn_def;
    let func_ident = &func.sig.ident;
    quote! {
        #[automatically_derived]
        #[cfg(fuzzing)]
        ::libfuzzer_sys::fuzz_target!(|args: ( #(#types),* )| {
            let ( #(#args),* ) = args;  // https://github.com/rust-fuzz/libfuzzer/issues/77
            #func_ident ( #(#args),* )
        });

        #[cfg(not(any(fuzzing, rust_analyzer)))]
        fn main() {
            ::std::unreachable!("Run this target with `cargo fuzz` or `cargo test` instead");
        }
    }
}

#[cfg(any(feature = "quickcheck", test))]
mod quickcheck {
    use crate::FunctionDefinition;
    use quote::quote;

    pub(crate) fn derive_quickcheck(fn_def: &FunctionDefinition) -> proc_macro2::TokenStream {
        let FunctionDefinition { func, args, types } = fn_def;
        let func_ident = &func.sig.ident;
        let adapted_types: Vec<_> = types
            .iter()
            .map(|ty| quote! { ArbitraryAdapter<#ty> })
            .collect();
        let arg_pattern: Vec<_> = args
            .iter()
            .map(|arg| quote! { ArbitraryAdapter(::core::result::Result::Ok(#arg)) })
            .collect();
        let test_name = quote::format_ident!("quickcheck_{func_ident}");
        quote! {
            #[automatically_derived]
            #[test]
            fn #test_name() {
                use ::derive_fuzztest::reexport::quickcheck::TestResult;
                use ::derive_fuzztest::arbitrary_bridge::ArbitraryAdapter;

                fn inner(args: (#(#adapted_types),*)) -> TestResult {
                    let (#(#arg_pattern),*) = args else { return TestResult::discard() };
                    match ::std::panic::catch_unwind(move || {
                        #func_ident ( #(#args),* );
                    }) {
                        ::core::result::Result::Ok(()) => TestResult::passed(),
                        ::core::result::Result::Err(e) => TestResult::error(::std::format!("{e:?}")),
                    }
                }

                ::derive_fuzztest::reexport::quickcheck::QuickCheck::new().tests(1024)
                    .quickcheck(inner as fn(_) -> TestResult);
            }
        }
    }
}

#[cfg(not(any(feature = "quickcheck", test)))]
mod quickcheck {
    use crate::FunctionDefinition;

    pub(crate) fn derive_quickcheck(_fn_def: &FunctionDefinition) -> proc_macro2::TokenStream {
        proc_macro2::TokenStream::default()
    }
}

#[cfg(any(feature = "proptest", test))]
mod proptest {
    use crate::FunctionDefinition;
    use quote::quote;
    use syn::{Ident, Signature};

    pub(crate) fn derive_proptest(fn_def: &FunctionDefinition) -> proc_macro2::TokenStream {
        let FunctionDefinition { func, args, types } = fn_def;
        let func_attrs = &func.attrs;
        let Signature {
            constness,
            asyncness,
            unsafety,
            abi,
            fn_token,
            ident,
            generics,
            paren_token: _,
            inputs: _,
            variadic: _,
            output,
        } = &func.sig;
        let proptest_ident = Ident::new(&format!("proptest_{ident}"), ident.span());
        quote! {
            #[automatically_derived]
            #[cfg(test)]
            mod #proptest_ident {
                use super::*;
                use ::derive_fuzztest::reexport::proptest;
                use ::derive_fuzztest::reexport::proptest_arbitrary_interop::arb;

                proptest::proptest! {
                    #![proptest_config(proptest::prelude::ProptestConfig {
                        cases: 1024,
                        failure_persistence: Some(Box::new(proptest::test_runner::FileFailurePersistence::WithSource("regression"))),
                        ..Default::default()
                    })]
                    #[test]
                    #(#func_attrs)*
                    #constness #asyncness #unsafety #abi #fn_token #proptest_ident #generics ( args in arb::<(#(#types),*)>() ) #output {
                        let (#(#args),*) = args;
                        #ident ( #(#args),* );
                    }
                }
            }
        }
    }
}

#[cfg(not(any(feature = "proptest", test)))]
mod proptest {
    use crate::FunctionDefinition;

    pub(crate) fn derive_proptest(_fn_def: &FunctionDefinition) -> proc_macro2::TokenStream {
        proc_macro2::TokenStream::default()
    }
}

/// Representation of a function definition annotated with one of the attribute macros in this
/// crate.
struct FunctionDefinition {
    func: ItemFn,
    args: Vec<Pat>,
    types: Vec<Type>,
}

impl FunctionDefinition {
    pub fn parse(func: ItemFn) -> syn::Result<Self> {
        let (args, types) = func
            .sig
            .inputs
            .clone()
            .into_iter()
            .map(|arg| match arg {
                syn::FnArg::Receiver(arg_receiver) => Err(syn::Error::new(
                    arg_receiver.span(),
                    "Receiver not supported",
                )),
                syn::FnArg::Typed(PatType {
                    attrs: _,
                    pat,
                    colon_token: _,
                    ty,
                }) => Ok((*pat, *ty)),
            })
            .try_fold((Vec::new(), Vec::new()), |(mut args, mut types), result| {
                result.map(|(arg, type_)| {
                    args.push(arg);
                    types.push(type_);
                    (args, types)
                })
            })?;
        Ok(Self { func, args, types })
    }
}

#[cfg(test)]
mod tests {
    use crate::{fuzz_impl, fuzztest_impl, proptest_impl};
    use quote::quote;
    use syn::parse_quote;

    /// Assert that a token stream for a `syn::File` is the same as expected.
    ///
    /// Usage is similar to `assert_eq!`:
    /// ```no_run
    /// assert_syn_file!(
    ///     macro_impl(quote! {
    ///         fn foobar() {}
    ///     }),
    ///     quote! {
    ///         fn macro_rewritten_foobar() {}
    ///     }
    /// );
    /// ```
    macro_rules! assert_syn_file {
        ($actual:expr, $expected:expr) => {
            let actual = syn::parse2::<syn::File>($actual).unwrap();
            let expected: syn::File = $expected;
            assert!(
                actual == expected,
                "{}",
                pretty_assertions::StrComparison::new(
                    &prettyplease::unparse(&expected),
                    &prettyplease::unparse(&actual),
                )
            )
        };
    }

    #[test]
    fn test_fuzztest_expansion() {
        assert_syn_file!(
            fuzztest_impl(
                quote! {},
                quote! {
                    fn foobar(input: &[u8]) {
                        panic!("I am just a test")
                    }
                }
            )
            .unwrap(),
            parse_quote! {
                #[allow(unused)]
                fn foobar(input: &[u8]) {
                    panic!("I am just a test")
                }

                #[automatically_derived]
                #[cfg(fuzzing)]
                ::libfuzzer_sys::fuzz_target!(|args: (&[u8])| {
                    let (input) = args;
                    foobar(input)
                });

                #[cfg(not(any(fuzzing, rust_analyzer)))]
                fn main() {
                    ::std::unreachable!("Run this target with `cargo fuzz` or `cargo test` instead");
                }

                #[automatically_derived]
                #[cfg(test)]
                mod proptest_foobar {
                    use super::*;
                    use ::derive_fuzztest::reexport::proptest;
                    use ::derive_fuzztest::reexport::proptest_arbitrary_interop::arb;
                    proptest::proptest! {
                        #![proptest_config(proptest::prelude::ProptestConfig {
                            cases: 1024,
                            failure_persistence: Some(Box::new(proptest::test_runner::FileFailurePersistence::WithSource("regression"))),
                            ..Default::default()
                        })]
                        #[test]
                        fn proptest_foobar(args in arb::<(&[u8])>()) {
                            let (input) = args;
                            foobar(input);
                        }
                    }
                }

                #[automatically_derived]
                #[test]
                fn quickcheck_foobar() {
                    use ::derive_fuzztest::reexport::quickcheck::TestResult;
                    use ::derive_fuzztest::arbitrary_bridge::ArbitraryAdapter;

                    fn inner(args: (ArbitraryAdapter<&[u8]>)) -> TestResult {
                        let (ArbitraryAdapter(::core::result::Result::Ok(input))) = args else {
                            return TestResult::discard()
                        };
                        match ::std::panic::catch_unwind(move || {
                            foobar(input);
                        }) {
                            ::core::result::Result::Ok(()) => TestResult::passed(),
                            ::core::result::Result::Err(e) => TestResult::error(::std::format!("{e:?}")),
                        }
                    }
                    ::derive_fuzztest::reexport::quickcheck::QuickCheck::new()
                        .tests(1024)
                        .quickcheck(inner as fn(_) -> TestResult);
                }
            }
        );
    }

    #[test]
    fn test_fuzz_expansion() {
        assert_syn_file!(
            fuzz_impl(
                quote! {},
                quote! {
                    fn foobar(input: &[u8]) {
                        panic!("I am just a test")
                    }
                }
            )
            .unwrap(),
            parse_quote! {
                #[allow(unused)]
                fn foobar(input: &[u8]) {
                    panic!("I am just a test")
                }

                #[automatically_derived]
                #[cfg(fuzzing)]
                ::libfuzzer_sys::fuzz_target!(|args: (&[u8])| {
                    let (input) = args;
                    foobar(input)
                });

                #[cfg(not(any(fuzzing, rust_analyzer)))]
                fn main() {
                    ::std::unreachable!("Run this target with `cargo fuzz` or `cargo test` instead");
                }
            }
        );
    }

    #[test]
    fn test_proptest_expansion() {
        assert_syn_file!(
            proptest_impl(
                quote! {},
                quote! {
                    fn foobar(input: &[u8]) {
                        panic!("I am just a test")
                    }
                }
            )
            .unwrap(),
            parse_quote! {
                #[allow(unused)]
                fn foobar(input: &[u8]) {
                    panic!("I am just a test")
                }

                #[automatically_derived]
                #[cfg(test)]
                mod proptest_foobar {
                    use super::*;
                    use ::derive_fuzztest::reexport::proptest;
                    use ::derive_fuzztest::reexport::proptest_arbitrary_interop::arb;
                    proptest::proptest! {
                        #![proptest_config(proptest::prelude::ProptestConfig {
                            cases: 1024,
                            failure_persistence: Some(Box::new(proptest::test_runner::FileFailurePersistence::WithSource("regression"))),
                            ..Default::default()
                        })]
                        #[test]
                        fn proptest_foobar(args in arb::<(&[u8])>()) {
                            let (input) = args;
                            foobar(input);
                        }
                    }
                }
            }
        );
    }
}
