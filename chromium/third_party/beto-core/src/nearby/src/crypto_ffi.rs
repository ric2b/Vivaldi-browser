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

use anyhow::anyhow;
use cmd_runner::{run_cmd_shell, run_cmd_shell_with_color, YellowStderr};
use semver::{Version, VersionReq};
use std::{env, fs, path::Path};

use crate::CargoOptions;

pub fn build_boringssl(root: &Path) -> anyhow::Result<()> {
    let bindgen_version_req = VersionReq::parse(">=0.69.4")?;
    let bindgen_version = get_bindgen_version()?;

    if !bindgen_version_req.matches(&bindgen_version) {
        return Err(anyhow!("Bindgen does not match expected version: {bindgen_version_req}"));
    }

    let vendor_dir = root
        .parent()
        .ok_or_else(|| anyhow!("project root dir no parent dir"))?
        .join("boringssl-build");
    fs::create_dir_all(&vendor_dir)?;

    let build_dir = root
        .parent()
        .ok_or_else(|| anyhow!("project root dir no parent dir"))?
        .join("third_party/boringssl/build");
    fs::create_dir_all(&build_dir)?;

    let target = run_cmd_shell_with_color::<YellowStderr>(&vendor_dir, "rustc -vV")?
        .stdout()
        .lines()
        .find(|l| l.starts_with("host: "))
        .and_then(|l| l.split_once(' '))
        .ok_or_else(|| anyhow!("Couldn't get rustc target"))?
        .1
        .to_string();
    let target = shell_escape::escape(target.into());
    run_cmd_shell_with_color::<YellowStderr>(
        &build_dir,
        format!(
            "cmake -G Ninja .. -DRUST_BINDINGS={} -DCMAKE_POSITION_INDEPENDENT_CODE=true",
            target
        ),
    )?;
    run_cmd_shell(&build_dir, "ninja")?;

    Ok(())
}

pub fn check_boringssl(root: &Path, cargo_options: &CargoOptions) -> anyhow::Result<()> {
    log::info!("Checking boringssl");

    build_boringssl(root)?;

    let bssl_dir = root.join("crypto/crypto_provider_boringssl");

    let locked_arg = if cargo_options.locked { "--locked" } else { "" };

    run_cmd_shell(&bssl_dir, format!("cargo check {locked_arg}"))?;
    run_cmd_shell(&bssl_dir, "cargo fmt --check")?;
    run_cmd_shell(&bssl_dir, "cargo clippy --all-targets")?;
    run_cmd_shell(&bssl_dir, cargo_options.test("check_boringssl", ""))?;
    run_cmd_shell(&bssl_dir, "cargo doc --no-deps")?;
    run_cmd_shell(
        root,
        cargo_options.test(
            "check_boringssl_ukey2",
            "-p ukey2_connections -p ukey2_rs --no-default-features --features test_boringssl",
        ),
    )?;
    Ok(())
}

/// Checks out latest boringssl commit and runs our crypto provider tests against it
pub fn check_boringssl_at_head(root: &Path, cargo_options: &CargoOptions) -> anyhow::Result<()> {
    // TODO: find a better way, a kokoro implemented auto-roller?
    build_boringssl_at_latest(root)?;

    let bssl_dir = root.join("crypto/crypto_provider_boringssl");
    run_cmd_shell(&bssl_dir, "cargo check")?;
    run_cmd_shell(&bssl_dir, cargo_options.test("check_boringssl_latest", ""))?;
    Ok(())
}

fn build_boringssl_at_latest(root: &Path) -> anyhow::Result<()> {
    // Now check boringssl against HEAD. Kokoro does not allow us to directly update the git submodule
    // so we must use manual hackery instead :/
    run_cmd_shell(root.parent().unwrap(), "rm -Rf third_party/boringssl")?;
    run_cmd_shell(
        &root.parent().unwrap().join("third_party"),
        "git clone https://boringssl.googlesource.com/boringssl",
    )?;
    run_cmd_shell(
        &root.parent().unwrap().join("third_party/boringssl"),
        "git checkout origin/master",
    )?;
    build_boringssl(root)?;
    Ok(())
}

fn get_bindgen_version() -> anyhow::Result<Version> {
    let bindgen_version_output = run_cmd_shell(&env::current_dir().unwrap(), "bindgen --version")?;

    let version = bindgen_version_output
        .stdout()
        .lines()
        .next()
        .ok_or(anyhow!("bindgen version output stream is empty"))?
        .strip_prefix("bindgen ")
        .ok_or(anyhow!("bindgen version output missing expected prefix of \"bindgen \""))?
        .parse::<Version>()?;

    Ok(version)
}
