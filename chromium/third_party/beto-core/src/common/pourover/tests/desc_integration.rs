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

#![allow(unsafe_code)]

use jni::{
    descriptors::Desc,
    objects::JObject,
    signature::{JavaType, Primitive, ReturnType},
    JavaVM,
};
use std::error::Error;

mod common;
use common::foo_class::*;

#[test]
fn jni_access() -> Result<(), Box<dyn Error>> {
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
        let loaded_foo = env.auto_local(loaded_foo);

        let found_foo = FOO.lookup(&mut env)?;
        assert!(env.is_same_object(&loaded_foo, found_foo.as_ref())?);
    }

    // Verify we can call the constructor
    let obj_foo = {
        let method_id = CONSTRUCTOR.lookup(&mut env)?;
        let args = &[jni::sys::jvalue { i: 123 }];
        // Safety: `args` must match the constructor arg count and types.
        unsafe { env.new_object_unchecked(CONSTRUCTOR.cls(), method_id, args) }?
    };
    assert!(env.is_instance_of(&obj_foo, &FOO)?);

    // Verify we can access all of the members

    let field_value = {
        env.get_field_unchecked(&obj_foo, &FIELD, ReturnType::Primitive(Primitive::Int))?
            .i()?
    };
    assert_eq!(123, field_value);

    let method_value = {
        let method_id = METHOD.lookup(&mut env)?;
        let args = &[];
        // Safety: `args` must match the method arg count and types.
        unsafe {
            env.call_method_unchecked(
                &obj_foo,
                method_id,
                ReturnType::Primitive(Primitive::Boolean),
                args,
            )
        }?
        .z()?
    };
    assert!(method_value);

    env.delete_local_ref(obj_foo)?;

    let static_field_value = {
        env.get_static_field_unchecked(&FOO, &STATIC_FIELD, JavaType::Primitive(Primitive::Long))?
            .j()?
    };
    assert_eq!(321, static_field_value);

    let static_method_value = {
        let method_id = STATIC_METHOD.lookup(&mut env)?;
        let args = &[];
        // Safety: `args` must match the method arg count and types.
        unsafe {
            env.call_static_method_unchecked(
                &FOO,
                method_id,
                ReturnType::Primitive(Primitive::Int),
                args,
            )
        }?
        .i()?
    };
    assert_eq!(3, static_method_value);

    Ok(())
}
