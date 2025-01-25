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

#![allow(unsafe_code, clippy::unwrap_used, clippy::expect_used, clippy::panic)]

use jni::{
    descriptors::Desc,
    objects::{JObject, JString},
    signature::{Primitive, ReturnType},
    sys::jint,
    JNIEnv, JavaVM,
};
use std::{
    error::Error,
    sync::atomic::{AtomicBool, Ordering},
};

mod common;
use common::foo_class::*;

static NATIVE_METHOD_CALLED: AtomicBool = AtomicBool::new(false);
static TEST_PANIC: AtomicBool = AtomicBool::new(false);

#[pourover::jni_method(
    package = "com.example",
    class = "Foo",
    panic_returns = -1
)]
extern "system" fn nativeReturnsInt<'local>(
    mut env: JNIEnv<'local>,
    this: JObject<'local>,
    n: jint,
) -> jint {
    NATIVE_METHOD_CALLED.store(true, Ordering::SeqCst);

    let foo_value = env
        .get_field_unchecked(&this, &FIELD, ReturnType::Primitive(Primitive::Int))
        .unwrap()
        .i()
        .unwrap();

    n * foo_value
}

#[pourover::jni_method(
    package = "com.example",
    class = "Foo",
    panic_returns = JObject::null().into(),
)]
extern "system" fn nativeReturnsObject<'local>(
    env: JNIEnv<'local>,
    _this: JObject<'local>,
    n: jint,
) -> JString<'local> {
    if TEST_PANIC.load(Ordering::SeqCst) {
        panic!("testing panic case");
    }
    env.new_string(n.to_string()).unwrap()
}

#[test]
fn can_call_native_method() -> Result<(), Box<dyn Error>> {
    // Create the environment
    let vm = JavaVM::new(
        jni::InitArgsBuilder::new()
            .version(jni::JNIVersion::V8)
            .option("-Xcheck:jni")
            .build()?,
    )?;
    let mut env = vm.attach_current_thread()?;

    // Load `Foo.class`
    {
        let foo_class = compile_foo()?;
        let loaded_foo = env.define_class(CLASS_DESC, &JObject::null(), &foo_class)?;
        env.delete_local_ref(loaded_foo)?;
    }

    let obj_foo = {
        let method_id = CONSTRUCTOR.lookup(&mut env)?;
        let args = &[jni::sys::jvalue { i: 123 }];
        // Safety: `args` must match the constructor arg count and types.
        unsafe { env.new_object_unchecked(CONSTRUCTOR.cls(), method_id, args) }?
    };
    let obj_foo = env.auto_local(obj_foo);

    NATIVE_METHOD_CALLED.store(false, Ordering::SeqCst);

    // TODO: It would be better if the JVM was able to find the method on its own,
    // but, since we are using the invocation API to create the JVM, I haven't found a way to make
    // it visible to the JVM. Normally the methods are loaded dynamically with
    // `System.loadLibrary`, but in this case the symbols already exist in the executable, and I'm
    // unsure why the JVM cannot find them. I have tried compiling with
    // `-Zexport-executable-symbols` but that wasn't working for me.
    env.register_native_methods(
        &FOO,
        &[
            jni::NativeMethod {
                name: "nativeReturnsInt".into(),
                sig: "(I)I".into(),
                fn_ptr: crate::nativeReturnsInt as *mut std::ffi::c_void,
            },
            jni::NativeMethod {
                name: "nativeReturnsObject".into(),
                sig: "(I)Ljava/lang/String;".into(),
                fn_ptr: crate::nativeReturnsObject as *mut std::ffi::c_void,
            },
        ],
    )?;

    let method_value = {
        let method_id = NATIVE_METHOD.lookup(&mut env)?;
        let args = &[jni::sys::jvalue { i: 3 }];
        // Safety: `args` must match the method arg count and types.
        unsafe {
            env.call_method_unchecked(
                &obj_foo,
                method_id,
                ReturnType::Primitive(Primitive::Int),
                args,
            )
        }?
        .i()?
    };

    assert_eq!(369, method_value);
    assert!(NATIVE_METHOD_CALLED.load(Ordering::SeqCst));

    TEST_PANIC.store(false, Ordering::SeqCst);
    let string_value: JString = {
        let method_id = NATIVE_OBJECT_METHOD.lookup(&mut env)?;
        let args = &[jni::sys::jvalue { i: 1234 }];
        // Safety: `args` must match the method arg count and types.
        unsafe { env.call_method_unchecked(&obj_foo, method_id, ReturnType::Object, args) }?
            .l()?
            .into()
    };
    assert_eq!("1234", env.get_string(&string_value)?.to_str()?);
    env.delete_local_ref(string_value)?;

    TEST_PANIC.store(true, Ordering::SeqCst);
    let string_value: JString = {
        let method_id = NATIVE_OBJECT_METHOD.lookup(&mut env)?;
        let args = &[jni::sys::jvalue { i: 1234 }];
        // Safety: `args` must match the method arg count and types.
        unsafe { env.call_method_unchecked(&obj_foo, method_id, ReturnType::Object, args) }?
            .l()?
            .into()
    };
    assert!(string_value.is_null());

    Ok(())
}
