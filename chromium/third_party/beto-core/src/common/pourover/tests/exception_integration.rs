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
    sys::jint,
    JNIEnv, JavaVM,
};
use pourover::{
    desc::ClassDesc,
    exception::{null_pointer_exception, runtime_exception, JniResultExt, ThrowableJniResultExt},
};
use std::error::Error;

mod common;
use common::foo_class::*;

#[pourover::jni_method(package = "com.example", class = "Foo")]
extern "system" fn nativeReturnsInt<'local>(
    mut env: JNIEnv<'local>,
    _this: JObject<'local>,
    n: jint,
) -> jint {
    (|| match n {
        -1 => Err(runtime_exception("test runtime exception"))?,
        -2 => Err(null_pointer_exception())?,
        _ => Ok(n),
    })()
    .unwrap_or_throw(&mut env)
}

#[pourover::jni_method(
    package = "com.example",
    class = "Foo",
    panic_returns = JObject::null().into(),
)]
extern "system" fn nativeReturnsObject<'local>(
    _env: JNIEnv<'local>,
    _this: JObject<'local>,
    _n: jint,
) -> JString<'local> {
    unimplemented!()
}

pub static RUNTIME_EXCEPTION_CLASS: ClassDesc = ClassDesc::new("java/lang/RuntimeException");

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

    // Sub-test 1: Call nativeReturnsInt(99) returns the argument without exceptions
    {
        let result =
            pourover::call_method!(&mut env, FOO, "nativeReturnsInt", "(I)I", &obj_foo, 99);
        assert_eq!(99, result.unwrap());
    }

    // Sub-test 2: Call nativeReturnsInt(99) returns the argument without exceptions, so
    // extract_exception just returns Ok
    {
        let result =
            pourover::call_method!(&mut env, FOO, "nativeReturnsInt", "(I)I", &obj_foo, 99);
        let extracted_result = result.extract_exception(&mut env, "java/lang/Throwable");
        let Ok(Ok(99)) = extracted_result else {
            panic!("extracted_result should be Ok(Ok(99))")
        };
    }

    // Sub-test 3: Call nativeReturnsInt(-1) throws RuntimeException("test runtime exception")
    {
        let result =
            pourover::call_method!(&mut env, FOO, "nativeReturnsInt", "(I)I", &obj_foo, -1);
        let throwable = result
            .extract_exception(&mut env, "java/lang/RuntimeException")
            .unwrap()
            .unwrap_err();
        let message = pourover::call_method!(
            &mut env,
            RUNTIME_EXCEPTION_CLASS,
            "getMessage",
            "()Ljava/lang/String;",
            &throwable
        )
        .unwrap();
        let message = env.get_string(&message).map(String::from).unwrap();
        assert_eq!("test runtime exception", message);
    }

    // Sub-test 4: Call nativeReturnsInt(-2) throws NullPointerException()
    {
        let result =
            pourover::call_method!(&mut env, FOO, "nativeReturnsInt", "(I)I", &obj_foo, -2);
        let _ = result
            .extract_exception(&mut env, "java/lang/NullPointerException")
            .unwrap()
            .unwrap_err();
    }

    // Sub-test 5: Call nativeReturnsInt(-1) throws RuntimeException(). Calling
    // extract_exception with NullPointerException should return Err(JavaException).
    {
        let result =
            pourover::call_method!(&mut env, FOO, "nativeReturnsInt", "(I)I", &obj_foo, -1);

        let Err(jni::errors::Error::JavaException) =
            result.extract_exception(&mut env, "java/lang/NullPointerException")
        else {
            panic!(concat!(
                "Extracting NullPointerException from a result that contains RuntimeException ",
                "should return Err(JavaException)"
            ))
        };
        // The exception should still be pending on the JVM
        assert!(env.exception_check().unwrap());
    }

    Ok(())
}
