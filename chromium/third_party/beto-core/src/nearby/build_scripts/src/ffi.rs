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

use crate::CargoOptions;
use cmd_runner::{run_cmd_shell, run_cmd_shell_with_color, YellowStderr};
use std::{fs, path};

// wrapper for checking all ffi related things
pub(crate) fn check_all_ffi(
    root: &path::Path,
    cargo_options: &CargoOptions,
    boringssl_enabled: bool,
) -> anyhow::Result<()> {
    check_np_ffi_cmake(root, cargo_options, boringssl_enabled)?;
    check_ldt_cmake(root, cargo_options, boringssl_enabled)?;
    check_ukey2_cmake(root, cargo_options, boringssl_enabled)?;
    Ok(())
}

pub(crate) fn check_np_ffi_cmake(
    root: &path::Path,
    cargo_options: &CargoOptions,
    boringssl_enabled: bool,
) -> anyhow::Result<()> {
    log::info!("Checking CMake build and tests for np ffi c/c++ code");
    let locked_arg = if cargo_options.locked { "--locked" } else { "" };
    let features =
        if boringssl_enabled { "--no-default-features --features boringssl" } else { "" };
    run_cmd_shell(root, format!("cargo build  -p np_c_ffi {locked_arg} {features} --release"))?;

    let build_dir = root.join("cmake-build");
    fs::create_dir_all(&build_dir)?;
    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir,
        "cmake -G Ninja -DENABLE_TESTS=true -DCMAKE_BUILD_TYPE=Release -DENABLE_FUZZ=false ..",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(&build_dir, "cmake --build . --target np_cpp_sample")?;
    run_cmd_shell_with_color::<YellowStderr>(&build_dir, "cmake --build . --target np_ffi_bench")?;

    // Force detection of updated static lib if previous version was already built
    let test_dir = build_dir.join("presence/np_cpp_ffi/tests");
    let _ = run_cmd_shell_with_color::<YellowStderr>(&test_dir, "rm np_ffi_tests");
    run_cmd_shell_with_color::<YellowStderr>(&build_dir, "cmake --build . --target np_ffi_tests")?;
    run_cmd_shell_with_color::<YellowStderr>(&test_dir, "ctest")?;
    Ok(())
}

pub(crate) fn check_ldt_cmake(
    root: &path::Path,
    cargo_options: &CargoOptions,
    boringssl_enabled: bool,
) -> anyhow::Result<()> {
    log::info!("Checking CMake build and tests for ldt c/c++ code");
    let locked_arg = if cargo_options.locked { "--locked" } else { "" };
    let features =
        if boringssl_enabled { "--no-default-features --features boringssl" } else { "" };
    run_cmd_shell(
        root,
        format!("cargo build {locked_arg} {features} -p ldt_np_adv_ffi --quiet --release"),
    )?;

    let build_dir = root.join("cmake-build");
    fs::create_dir_all(&build_dir)?;
    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir,
        "cmake -G Ninja -DENABLE_TESTS=true -DCMAKE_BUILD_TYPE=Release -DENABLE_FUZZ=false ..",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(&build_dir, "cmake --build . --target ldt_c_sample")?;
    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir,
        "cmake --build . --target ldt_benchmarks",
    )?;
    // Force detection of updated static lib if previous version was already built
    let test_dir = build_dir.join("presence/ldt_np_adv_ffi/c/tests");
    let _ = run_cmd_shell_with_color::<YellowStderr>(&test_dir, "rm ldt_ffi_tests");
    run_cmd_shell_with_color::<YellowStderr>(&build_dir, "cmake --build . --target ldt_ffi_tests")?;
    run_cmd_shell_with_color::<YellowStderr>(&test_dir, "ctest")?;
    Ok(())
}

pub(crate) fn check_ukey2_cmake(
    root: &path::Path,
    cargo_options: &CargoOptions,
    boringssl_enabled: bool,
) -> anyhow::Result<()> {
    log::info!("Checking Ukey2 ffi");

    let locked_arg = if cargo_options.locked { "--locked" } else { "" };
    let features =
        if boringssl_enabled { "--no-default-features --features boringssl" } else { "" };
    run_cmd_shell(
        root,
        format!("cargo build -p ukey2_c_ffi {locked_arg} {features} --quiet --release --lib"),
    )?;

    let build_dir = root.join("cmake-build");
    fs::create_dir_all(&build_dir)?;
    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir,
        "cmake -G Ninja -DENABLE_TESTS=true -DENABLE_FUZZ=false ..",
    )?;
    let test_dir = build_dir.join("connections/ukey2/ukey2_c_ffi/cpp");
    let _ = run_cmd_shell_with_color::<YellowStderr>(&test_dir, "rm ukey2_ffi_test");
    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir,
        "cmake --build . --target ukey2_ffi_test",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(&test_dir, "ctest")?;
    Ok(())
}
