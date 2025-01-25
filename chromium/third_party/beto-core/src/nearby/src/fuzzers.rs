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

pub(crate) fn run_rust_fuzzers(root: &path::Path) -> anyhow::Result<()> {
    log::info!("Running rust fuzzers");
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("presence/xts_aes"),
        "cargo +nightly fuzz run xts_roundtrip -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("presence/ldt"),
        "cargo +nightly fuzz run ldt_roundtrip -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("presence/ldt_np_adv"),
        "cargo +nightly fuzz run ldt_np_decrypt -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("presence/ldt_np_adv"),
        "cargo +nightly fuzz run ldt_np_roundtrip -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("connections/ukey2/ukey2_connections"),
        "cargo +nightly fuzz run fuzz_connection -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("connections/ukey2/ukey2_connections"),
        "cargo +nightly fuzz run fuzz_from_saved_session -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("connections/ukey2/ukey2_connections"),
        "cargo +nightly fuzz run fuzz_handshake -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("crypto/crypto_provider_test"),
        "cargo +nightly fuzz run fuzz_p256 -- -runs=10000 -max_total_time=60",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &root.join("crypto/crypto_provider_test"),
        concat!(
            "cargo +nightly fuzz run fuzz_p256 --features=boringssl --no-default-features ",
            "-- -runs=10000 -max_total_time=60"
        ),
    )?;

    Ok(())
}

// Runs the fuzztest fuzzers as short lived unit tests, compatible with gtest
pub(crate) fn build_fuzztest_unit_tests(root: &path::Path) -> anyhow::Result<()> {
    log::info!("Checking fuzztest targets in unit test mode");
    run_cmd_shell(root, "cargo build -p np_c_ffi --release")?;
    run_cmd_shell(root, "cargo build -p ldt_np_adv_ffi --release")?;
    let build_dir = root.join("cmake-build");
    fs::create_dir_all(&build_dir)?;
    run_cmd_shell_with_color::<YellowStderr>(&build_dir, "cmake -G Ninja -DENABLE_FUZZ=true ..")?;

    for target in ["deserialization_fuzzer", "ldt_fuzzer"] {
        run_cmd_shell_with_color::<YellowStderr>(
            &build_dir,
            format!("cmake --build . --target {}", target),
        )?;
    }

    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir.join("presence/np_cpp_ffi/fuzz/"),
        "ctest",
    )?;
    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir.join("presence/ldt_np_adv_ffi/c/fuzz/"),
        "ctest",
    )?;
    Ok(())
}
