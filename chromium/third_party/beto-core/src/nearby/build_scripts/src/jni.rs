// Copyright 2023 Google LLC
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

use cmd_runner::run_cmd_shell;
use std::path;

pub fn run_ldt_kotlin_tests(root: &path::Path) -> anyhow::Result<()> {
    run_cmd_shell(root, "cargo build -p ldt_np_jni")?;
    let kotlin_lib_path = root.to_path_buf().join("presence/ldt_np_jni/java/LdtNpJni");
    run_cmd_shell(&kotlin_lib_path, "./gradlew :test")?;
    Ok(())
}

pub fn run_ukey2_jni_tests(root: &path::Path) -> anyhow::Result<()> {
    run_cmd_shell(root, "cargo build -p ukey2_jni")?;
    let ukey2_jni_path = root.to_path_buf().join("connections/ukey2/ukey2_jni/java");
    run_cmd_shell(&ukey2_jni_path, "./gradlew :test")?;
    Ok(())
}

pub fn run_np_java_ffi_tests(root: &path::Path) -> anyhow::Result<()> {
    run_cmd_shell(
        root,
        "cargo build -p np_java_ffi -F crypto_provider_default/rustcrypto -F testing",
    )?;
    let np_java_path = root.to_path_buf().join("presence/np_java_ffi");
    run_cmd_shell(&np_java_path, "./gradlew :test --info --rerun")?;
    Ok(())
}
