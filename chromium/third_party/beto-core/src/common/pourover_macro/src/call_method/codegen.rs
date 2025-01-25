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

//! Code generation for the `call_method!` series of proc-macros. This module is used by creating a
//! [`MethodCall`] instance and calling [`MethodCall::generate`]. The generated code will vary
//! for each macro based on the contained [`Receiver`] value. If there is a detected issue with the
//! [`MethodCall`] provided, then `generate` will return a [`CodegenError`] with information on
//! what is wrong. This error can be converted into a [`syn::Error`] for reporting any failures to
//! the user.

use proc_macro2::{Span, TokenStream};
use quote::{quote, quote_spanned, TokenStreamExt};
use syn::{parse_quote, spanned::Spanned, Expr, Ident, ItemStatic, LitStr, Type};

use crate::type_parser::{JavaType, MethodSig, NonArray, Primitive, ReturnType};

/// The errors that can be encountered during codegen. Used in [`CodegenError`].
#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum ErrorKind {
    InvalidArgsLength { expected: usize, found: usize },
    ConstructorRetValShouldBeVoid,
    InvalidTypeSignature,
}

/// An error encountered during codegen with span information. Can be converted to [`syn::Error`].
/// using [`From`].
#[derive(Clone, Debug)]
pub struct CodegenError(pub Span, pub ErrorKind);

impl From<CodegenError> for syn::Error {
    fn from(CodegenError(span, kind): CodegenError) -> syn::Error {
        use ErrorKind::*;
        match kind {
            InvalidArgsLength { expected, found } => {
                syn::Error::new(span, format!("The number of args does not match the type signature provided: expected={expected}, found={found}"))
            }
            ConstructorRetValShouldBeVoid => {
                syn::Error::new(span, "Return type should be `void` (`V`) for constructor methods")
            }
            InvalidTypeSignature => syn::Error::new(span, "Failed to parse type signature"),
        }
    }
}

/// Codegen can fail with [`CodegenError`].
pub type CodegenResult<T> = Result<T, CodegenError>;

/// Describes a method that will be generated. Create one with [`MethodCall::new`] and generate
/// code with [`MethodCall::generate`]. This should be given AST nodes from the macro input so that
/// errors are associated properly to the input span.
pub struct MethodCall {
    env: Expr,
    method: MethodInfo,
    receiver: Receiver,
    arg_exprs: Vec<Expr>,
}

impl MethodCall {
    /// Create a new MethodCall instance
    pub fn new(env: Expr, method: MethodInfo, receiver: Receiver, arg_exprs: Vec<Expr>) -> Self {
        Self {
            env,
            method,
            receiver,
            arg_exprs,
        }
    }

    /// Generate code to call the described method.
    pub fn generate(&self) -> CodegenResult<TokenStream> {
        // Needs to be threaded manually to other methods since self-referential structs can't
        // exist.
        let sig = self.method.sig()?;

        let args = self.generate_args(&sig)?;

        let method_call = self
            .receiver
            .generate_call(&self.env, &self.method, &sig, &args)?;

        // Wrap the generated code in a closure so that we can access the outer scope but any
        // variables we define aren't accessible by the outer scope. There is small hygiene issue
        // where the arg exprs have our `env` variable in scope. If this becomes an issue we can
        // refactor these exprs to be passed as closure parameters instead.
        Ok(quote! {
            (|| {
                #method_call
            })()
        })
    }

    /// Generate the `&[jni::jvalue]` arguments slice that will be passed to the `jni` method call.
    /// This validates the argument count and types.
    fn generate_args(&self, sig: &MethodSig<'_>) -> CodegenResult<Expr> {
        // Safety: must check that arg count matches the signature
        if self.arg_exprs.len() != sig.args.len() {
            return Err(CodegenError(
                self.method.sig.span(),
                ErrorKind::InvalidArgsLength {
                    expected: sig.args.len(),
                    found: self.arg_exprs.len(),
                },
            ));
        }

        // Create each `jvalue` expression
        let type_expr_pairs = core::iter::zip(sig.args.iter().copied(), self.arg_exprs.iter());
        let jvalues = type_expr_pairs.map(|(ty, expr)| generate_jvalue(ty, expr));

        // Put the `jvalue` expressions in a slice.
        Ok(parse_quote! {
            &[#(#jvalues),*]
        })
    }
}

/// The receiver of the method call and the type of the method.
pub enum Receiver {
    /// A constructor.
    Constructor,
    /// A static method.
    Static,
    /// An instance method. The `Expr` here is the `this` object.
    Instance(Expr),
}

impl Receiver {
    /// Generate the code that performs the JNI call.
    fn generate_call(
        &self,
        env: &Expr,
        method_info: &MethodInfo,
        sig: &MethodSig<'_>,
        args: &Expr,
    ) -> CodegenResult<TokenStream> {
        // Constructors are void methods. Validate this fact.
        if matches!(*self, Receiver::Constructor) && !sig.ret.is_void() {
            return Err(CodegenError(
                method_info.sig.span(),
                ErrorKind::ConstructorRetValShouldBeVoid,
            ));
        }

        // The static item containing the `pourover::[Static]MethodDesc`.
        let method_desc = self.generate_method_desc(method_info);

        // The `jni::signature::ReturnType` that the `jni` crate uses to perform the correct native
        // call.
        let return_type = return_type_from_sig(sig.ret);

        // A conversion expression to convert from `jni::object::JValueOwned` to the actual return
        // type. We have this information from the parsed method signature whereas the `jni` crate
        // only knows this at runtime.
        let conversion = return_value_conversion_from_sig(sig.ret);

        // This preamble is used to evaluate all the client-provided expressions outside of the
        // `unsafe` block. This is the same for all receiver kinds.
        let mut method_call = quote! {
            #method_desc

            let env: &mut ::jni::JNIEnv = #env;
            let method_id = ::jni::descriptors::Desc::lookup(&METHOD_DESC, env)?;
            let args: &[::jni::sys::jvalue] = #args;
        };

        // Generate the unsafe JNI call.
        //
        // Safety: `args` contains the arguments to this method. The type signature of this
        // method is `#sig`.
        //
        // `args` must adhere to the following:
        //  - `args.len()` must match the number of arguments given in the type signature.
        //  - The union value of each arg in `args` must match the type specified in the type
        //    signature.
        //
        // These conditions are upheld by this proc macro and a compile error will be caused if
        // they are broken. No user-provided code is executed within the `unsafe` block.
        method_call.append_all(match self {
            Self::Constructor => quote! {
                unsafe {
                    env.new_object_unchecked(
                        METHOD_DESC.cls(),
                        method_id,
                        args,
                    )
                }
            },
            Self::Static => quote! {
                unsafe {
                    env.call_static_method_unchecked(
                        METHOD_DESC.cls(),
                        method_id,
                        #return_type,
                        args,
                    )
                }#conversion
            },
            Self::Instance(this) => quote! {
                let this_obj: &JObject = #this;
                unsafe {
                    env.call_method_unchecked(
                        this_obj,
                        method_id,
                        #return_type,
                        args,
                    )
                }#conversion
            },
        });

        Ok(method_call)
    }

    fn generate_method_desc(&self, MethodInfo { cls, name, sig, .. }: &MethodInfo) -> ItemStatic {
        match self {
            Self::Constructor => parse_quote! {
                static METHOD_DESC: ::pourover::desc::MethodDesc = (#cls).constructor(#sig);
            },
            Self::Static => parse_quote! {
                static METHOD_DESC: ::pourover::desc::StaticMethodDesc = (#cls).static_method(#name, #sig);
            },
            Self::Instance(_) => parse_quote! {
                static METHOD_DESC: ::pourover::desc::MethodDesc = (#cls).method(#name, #sig);
            },
        }
    }
}

/// Information about the method being called
pub struct MethodInfo {
    cls: Expr,
    name: Expr,
    sig: LitStr,
    /// Derived from `sig.value()`. This string must be stored in the struct so that we can return
    /// a `MethodSig` instance that references it from `MethodInfo::sig()`.
    sig_str: String,
}

impl MethodInfo {
    pub fn new(cls: Expr, name: Expr, sig: LitStr) -> Self {
        let sig_str = sig.value();
        Self {
            cls,
            name,
            sig,
            sig_str,
        }
    }

    /// Parse the type signature from `sig`. Will return a [`CodegenError`] if the signature cannot
    /// be parsed.
    fn sig(&self) -> CodegenResult<MethodSig<'_>> {
        MethodSig::try_from_str(&self.sig_str)
            .ok_or_else(|| CodegenError(self.sig.span(), ErrorKind::InvalidTypeSignature))
    }
}

/// Generate a `jni::sys::jvalue` instance given a Java type and a Rust value.
///
/// Safety: The generated `jvalue` must match the given type `ty`.
fn generate_jvalue(ty: JavaType<'_>, expr: &Expr) -> TokenStream {
    // The `jvalue` field to inhabit
    let union_field: Ident;
    // The expected input type
    let type_name: Type;
    // Whether we need to call `JObject::as_raw()` on the input type
    let needs_as_raw: bool;

    // Fill the above values based the type signature.
    match ty {
        JavaType::Array { depth, ty } => {
            union_field = parse_quote![l];
            if let NonArray::Primitive(p) = ty {
                if depth.get() == 1 {
                    let prim_type = prim_to_sys_type(p);
                    type_name = parse_quote![::jni::objects::JPrimitiveArray<'_, #prim_type>]
                } else {
                    type_name = parse_quote![&::jni::objects::JObjectArray<'_>];
                }
            } else {
                type_name = parse_quote![&::jni::objects::JObjectArray<'_>];
            }
            needs_as_raw = true;
        }
        JavaType::NonArray(NonArray::Object { cls }) => {
            union_field = parse_quote![l];
            type_name = match cls {
                "java/lang/String" => parse_quote![&::jni::objects::JString<'_>],
                "java/util/List" => parse_quote![&::jni::objects::JList<'_>],
                "java/util/Map" => parse_quote![&::jni::objects::JMap<'_>],
                _ => parse_quote![&::jni::objects::JObject<'_>],
            };
            needs_as_raw = true;
        }
        JavaType::NonArray(NonArray::Primitive(p)) => {
            union_field = prim_to_union_field(p);
            type_name = prim_to_sys_type(p);
            needs_as_raw = false;
        }
    }

    // The as_raw() tokens if required.
    let as_raw = if needs_as_raw {
        quote! { .as_raw() }
    } else {
        quote![]
    };

    // Create the `jvalue` expression. This uses `identity` to produce nice type error messages.
    quote_spanned! { expr.span() =>
        ::jni::sys::jvalue {
            #union_field: ::core::convert::identity::<#type_name>(#expr) #as_raw
        }
    }
}

/// Get a `::jni::signature::ReturnType` expression from a [`crate::type_parser::ReturnType`]. This
/// value is passed to the `jni` crate so that it knows which JNI method to call.
fn return_type_from_sig(ret: ReturnType<'_>) -> Expr {
    let prim_type = |prim| parse_quote![::jni::signature::ReturnType::Primitive(::jni::signature::Primitive::#prim)];

    use crate::type_parser::{JavaType::*, NonArray::*, Primitive::*};

    match ret {
        ReturnType::Void => prim_type(quote![Void]),
        ReturnType::Returns(NonArray(Primitive(Boolean))) => prim_type(quote![Boolean]),
        ReturnType::Returns(NonArray(Primitive(Byte))) => prim_type(quote![Byte]),
        ReturnType::Returns(NonArray(Primitive(Char))) => prim_type(quote![Char]),
        ReturnType::Returns(NonArray(Primitive(Double))) => prim_type(quote![Double]),
        ReturnType::Returns(NonArray(Primitive(Float))) => prim_type(quote![Float]),
        ReturnType::Returns(NonArray(Primitive(Int))) => prim_type(quote![Int]),
        ReturnType::Returns(NonArray(Primitive(Long))) => prim_type(quote![Long]),
        ReturnType::Returns(NonArray(Primitive(Short))) => prim_type(quote![Short]),
        ReturnType::Returns(NonArray(Object { .. })) => {
            parse_quote![::jni::signature::ReturnType::Object]
        }
        ReturnType::Returns(Array { .. }) => parse_quote![::jni::signature::ReturnType::Array],
    }
}

/// A postfix call on a `jni::objects::JValueOwned` instance to convert it to the type specified by
/// `ret`. Since we have this information from the type signature we can  perform this conversion
/// in the macro.
fn return_value_conversion_from_sig(ret: ReturnType<'_>) -> TokenStream {
    use crate::type_parser::{JavaType::*, NonArray::*};

    match ret {
        ReturnType::Void => quote! { .and_then(::jni::objects::JValueOwned::v) },
        ReturnType::Returns(NonArray(Primitive(p))) => {
            let prim = prim_to_union_field(p);
            quote! { .and_then(::jni::objects::JValueOwned::#prim) }
        }
        ReturnType::Returns(NonArray(Object { cls })) => {
            let mut conversion = quote! { .and_then(::jni::objects::JValueOwned::l) };
            match cls {
                "java/lang/String" => {
                    conversion.append_all(quote! { .map(::jni::objects::JString::from) });
                }
                "java/util/List" => {
                    conversion.append_all(quote! { .map(::jni::objects::JList::from) });
                }
                "java/util/Map" => {
                    conversion.append_all(quote! { .map(::jni::objects::JMap::from) });
                }
                _ => {
                    // Already a JObject, so we are good here
                }
            }
            conversion
        }
        ReturnType::Returns(Array {
            depth,
            ty: Primitive(p),
        }) if depth.get() == 1 => {
            let sys_type = prim_to_sys_type(p);
            quote! {
                .and_then(::jni::objects::JValueOwned::l)
                .map(::jni::objects::JPrimitiveArray::<#sys_type>::from)
            }
        }
        ReturnType::Returns(Array { .. }) => quote! {
            .and_then(::jni::objects::JValueOwned::l)
            .map(::jni::objects::JObjectArray::from)
        },
    }
}

/// From a [`Primitive`], this gets the `jni::sys::jvalue` union field name for that type. This is
/// also the `jni::objects::JValueGen` getter name.
fn prim_to_union_field(p: Primitive) -> Ident {
    quote::format_ident!("{}", p.as_char().to_ascii_lowercase())
}

/// From a [`Primitive`], this gets the matching `jvalue::sys` type.
fn prim_to_sys_type(p: Primitive) -> Type {
    match p {
        Primitive::Boolean => parse_quote![::jni::sys::jboolean],
        Primitive::Byte => parse_quote![::jni::sys::jbyte],
        Primitive::Char => parse_quote![::jni::sys::jchar],
        Primitive::Double => parse_quote![::jni::sys::jdouble],
        Primitive::Float => parse_quote![::jni::sys::jfloat],
        Primitive::Int => parse_quote![::jni::sys::jint],
        Primitive::Long => parse_quote![::jni::sys::jlong],
        Primitive::Short => parse_quote![::jni::sys::jshort],
    }
}

#[cfg(test)]
#[allow(clippy::unwrap_used, clippy::expect_used, clippy::panic)]
mod tests {
    use super::*;
    use crate::test_util::contains_ident;
    use quote::ToTokens;
    use syn::parse_quote;

    fn example_method_call() -> MethodCall {
        MethodCall::new(
            parse_quote![&mut env],
            MethodInfo::new(
                parse_quote![&FOO_CLS],
                parse_quote!["example"],
                parse_quote!["(II)I"],
            ),
            Receiver::Instance(parse_quote![&foo]),
            vec![parse_quote![123], parse_quote![2 + 3]],
        )
    }

    #[test]
    fn args_are_counted() {
        let mut call = example_method_call();
        call.arg_exprs.push(parse_quote![too_many]);

        let CodegenError(_span, kind) = call.generate().unwrap_err();

        assert_eq!(
            ErrorKind::InvalidArgsLength {
                expected: 2,
                found: 3
            },
            kind
        );
    }

    #[test]
    fn constructor_return_type_is_void() {
        let mut call = example_method_call();
        call.receiver = Receiver::Constructor;

        let CodegenError(_span, kind) = call.generate().unwrap_err();

        assert_eq!(ErrorKind::ConstructorRetValShouldBeVoid, kind);
    }

    #[test]
    fn invalid_type_sig_is_error() {
        let mut call = example_method_call();
        call.method.sig = parse_quote!["L"];
        call.method.sig_str = call.method.sig.value();

        let CodegenError(_span, kind) = call.generate().unwrap_err();

        assert_eq!(ErrorKind::InvalidTypeSignature, kind);
    }

    #[test]
    fn jni_types_are_used_for_stdlib_classes_input() {
        let types = [
            ("Ljava/lang/String;", "JString"),
            ("Ljava/util/Map;", "JMap"),
            ("Ljava/util/List;", "JList"),
            ("[Ljava/lang/String;", "JObjectArray"),
            ("[[I", "JObjectArray"),
            ("[I", "JPrimitiveArray"),
            ("Lcom/example/MyObject;", "JObject"),
            ("Z", "jboolean"),
            ("C", "jchar"),
            ("B", "jbyte"),
            ("S", "jshort"),
            ("I", "jint"),
            ("J", "jlong"),
            ("F", "jfloat"),
            ("D", "jdouble"),
        ];

        for (desc, jni_type) in types {
            let jt = JavaType::try_from_str(desc).unwrap();
            let expr = parse_quote![some_value];

            let jvalue = generate_jvalue(jt, &expr);

            assert!(
                contains_ident(jvalue, jni_type),
                "desc: {desc}, jni_type: {jni_type}"
            );
        }
    }

    #[test]
    fn jni_types_are_used_for_stdlib_classes_output() {
        let types = [
            ("Ljava/lang/String;", "JString"),
            ("Ljava/util/Map;", "JMap"),
            ("Ljava/util/List;", "JList"),
            ("[Ljava/lang/String;", "JObjectArray"),
            ("[[I", "JObjectArray"),
            ("[I", "JPrimitiveArray"),
        ];

        for (desc, jni_type) in types {
            let rt = ReturnType::try_from_str(desc).unwrap();

            let conversion = return_value_conversion_from_sig(rt);

            assert!(
                contains_ident(conversion, jni_type),
                "desc: {desc}, jni_type: {jni_type}"
            );
        }
    }

    #[test]
    fn return_type_passed_to_jni_is_correct() {
        let types = [
            ("Ljava/lang/String;", "Object"),
            ("Ljava/util/Map;", "Object"),
            ("Ljava/util/List;", "Object"),
            ("[Ljava/lang/String;", "Array"),
            ("[[I", "Array"),
            ("[I", "Array"),
            ("V", "Void"),
            ("Z", "Boolean"),
            ("C", "Char"),
            ("B", "Byte"),
            ("S", "Short"),
            ("I", "Int"),
            ("J", "Long"),
            ("F", "Float"),
            ("D", "Double"),
        ];

        for (desc, return_type) in types {
            let rt = ReturnType::try_from_str(desc).unwrap();

            let expr = return_type_from_sig(rt).into_token_stream();

            assert!(
                contains_ident(expr, return_type),
                "desc: {desc}, return_type: {return_type}"
            );
        }
    }

    #[test]
    fn method_desc_is_correct() {
        let mut call = example_method_call();
        call.method.sig = parse_quote!["(II)V"];
        call.method.sig_str = call.method.sig.value();

        let tests = [
            (Receiver::Constructor, "constructor"),
            (Receiver::Static, "static_method"),
            (Receiver::Instance(parse_quote![this_value]), "method"),
        ];

        for (receiver, method_ident) in tests {
            let desc = receiver.generate_method_desc(&call.method);
            let rhs = desc.expr.into_token_stream();

            assert!(contains_ident(rhs, method_ident), "method: {method_ident}");
        }
    }

    #[test]
    fn jni_call_is_correct() {
        let mut call = example_method_call();
        call.method.sig = parse_quote!["(II)V"];
        call.method.sig_str = call.method.sig.value();
        let sig = call.method.sig().unwrap();
        let args = parse_quote![test_stub];

        let tests = [
            (Receiver::Constructor, "new_object_unchecked"),
            (Receiver::Static, "call_static_method_unchecked"),
            (
                Receiver::Instance(parse_quote![this_value]),
                "call_method_unchecked",
            ),
        ];

        for (receiver, method_ident) in tests {
            let call = receiver
                .generate_call(&call.env, &call.method, &sig, &args)
                .unwrap();

            assert!(contains_ident(call, method_ident), "method: {method_ident}");
        }
    }
}
