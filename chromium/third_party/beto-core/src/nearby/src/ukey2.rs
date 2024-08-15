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

use cmd_runner::{run_cmd_shell, run_cmd_shell_with_color, YellowStderr};
use std::{fs, path};

use crate::CargoOptions;

pub(crate) fn check_ukey2_ffi(
    root: &path::Path,
    cargo_options: &CargoOptions,
) -> anyhow::Result<()> {
    log::info!("Checking Ukey2 ffi");
    let ffi_dir = root.join("connections/ukey2/ukey2_c_ffi");

    let locked_arg = if cargo_options.locked { "--locked" } else { "" };

    // Default build, RustCrypto
    run_cmd_shell(&ffi_dir, format!("cargo build {locked_arg} --quiet --release --lib"))?;
    // OpenSSL
    run_cmd_shell(
        &ffi_dir,
        format!("cargo build {locked_arg} --quiet --no-default-features --features=openssl"),
    )?;

    run_cmd_shell(&ffi_dir, "cargo doc --quiet --no-deps")?;
    run_cmd_shell(&ffi_dir, "cargo clippy --no-default-features --features=openssl")?;

    run_cmd_shell(&ffi_dir, "cargo deny check")?;

    let ffi_build_dir = ffi_dir.join("cpp/build");
    fs::create_dir_all(&ffi_build_dir)?;
    run_cmd_shell_with_color::<YellowStderr>(&ffi_build_dir, "cmake ..")?;
    run_cmd_shell_with_color::<YellowStderr>(&ffi_build_dir, "cmake --build .")?;
    run_cmd_shell_with_color::<YellowStderr>(&ffi_build_dir, "ctest")?;

    Ok(())
}
