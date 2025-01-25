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

//! Java integration for the `Foo.java` test class.
//!
//! There are pourover descriptors for each member of `Foo`. Keep these in sync with changes to
//! `Foo.java`.
//!
//! Compilation is done at runtime. There isn't a way to create a build.rs for an integration test.
//! use [`compile_foo`] to get the class bytes and load the class using
//! [`jni::JNIEnv::define_class`].

use pourover::desc::*;
use std::{error::Error, process::Command};

pub const CLASS_DESC: &str = "com/example/Foo";
pub static FOO: ClassDesc = ClassDesc::new(CLASS_DESC);

pub static FIELD: FieldDesc = FOO.field("foo", "I");
pub static STATIC_FIELD: StaticFieldDesc = FOO.static_field("sfoo", "J");
pub static CONSTRUCTOR: MethodDesc = FOO.constructor("(I)V");
pub static METHOD: MethodDesc = FOO.method("mfoo", "()Z");
pub static STATIC_METHOD: StaticMethodDesc = FOO.static_method("smfoo", "()I");
pub static NATIVE_METHOD: MethodDesc = FOO.method("nativeReturnsInt", "(I)I");
pub static NATIVE_OBJECT_METHOD: MethodDesc =
    FOO.method("nativeReturnsObject", "(I)Ljava/lang/String;");

/// Quick test to ensure that Java is available on the system. The version will be logged to
/// stdout.
#[test]
fn has_java() -> Result<(), Box<dyn Error>> {
    let _ = Command::new("java").arg("--version").status()?;
    Ok(())
}

/// Compile `Foo.java` and return the class file's bytes.
#[track_caller]
pub fn compile_foo() -> Result<Vec<u8>, Box<dyn Error>> {
    // Avoid concurrent tests colliding on the file system by adding a hash of the call-site to the
    // temp file path
    let caller_hash = {
        use core::hash::{Hash, Hasher};
        let caller = core::panic::Location::caller();
        let mut hasher = std::collections::hash_map::DefaultHasher::new();
        caller.hash(&mut hasher);
        hasher.finish()
    };

    // Create a temporary dir to generate class files
    let mut tmp = std::env::temp_dir();
    tmp.push(format!("test-java-build-{caller_hash:016x}"));

    if let Err(err) = std::fs::create_dir(&tmp) {
        println!("error creating {}: {}", tmp.display(), &err);
    }

    // Compile Foo.java into the temp dir
    let _ = Command::new("javac")
        .args(["--release", "8"])
        .arg("-d")
        .arg(&tmp)
        .arg("tests/Foo.java")
        .status()?;

    // Read the class file bytes
    let class = {
        let mut class_file = tmp.clone();
        class_file.push("com/example/Foo.class");
        std::fs::read(&class_file)?
    };

    // Clean up the temp dir. If an error occurs before this we will leave the temp dir on the
    // filesystem. That should work better for debugging than cleaning up in all cases.
    std::fs::remove_dir_all(&tmp)?;

    Ok(class)
}
