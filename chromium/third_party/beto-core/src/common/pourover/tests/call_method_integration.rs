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

use jni::{objects::JObject, JavaVM};
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
        env.delete_local_ref(loaded_foo)?;
    }

    let foo_obj = pourover::call_constructor!(&mut env, &FOO, "(I)V", 123)?;
    let inner_int = pourover::call_method!(&mut env, &FOO, "getFoo", "()I", &foo_obj)?;
    assert_eq!(123, inner_int);
    env.delete_local_ref(foo_obj)?;

    let static_int = pourover::call_static_method!(&mut env, &FOO, "smfoo", "()I")?;
    assert_eq!(3, static_int);

    Ok(())
}
