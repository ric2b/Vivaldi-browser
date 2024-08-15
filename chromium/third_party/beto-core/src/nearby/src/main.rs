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

extern crate core;

use clap::Parser as _;
use cmd_runner::run_cmd_shell;
use env_logger::Env;
use std::{env, path};

mod crypto_ffi;
mod ffi;
mod fuzzers;
mod jni;
mod license;
mod ukey2;

fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_env(Env::default().default_filter_or("info")).init();
    let cli: Cli = Cli::parse();

    let root_dir = path::PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("Must be run via Cargo to establish root directory"),
    );

    match cli.subcommand {
        Subcommand::CheckEverything { ref check_options } => {
            check_everything(&root_dir, check_options)?
        }
        Subcommand::CleanEverything => clean_everything(&root_dir)?,
        Subcommand::CheckWorkspace(ref options) => check_workspace(&root_dir, options)?,
        Subcommand::FfiCheckEverything(ref options) => ffi::check_everything(&root_dir, options)?,
        Subcommand::BoringsslCheckEverything(ref options) => {
            crypto_ffi::boringssl_check_everything(&root_dir, options)?
        }
        Subcommand::BuildBoringssl => crypto_ffi::build_boringssl(&root_dir)?,
        Subcommand::CheckBoringssl(ref options) => crypto_ffi::check_boringssl(&root_dir, options)?,
        Subcommand::PrepareRustOpenssl => crypto_ffi::prepare_patched_rust_openssl(&root_dir)?,
        Subcommand::CheckOpenssl(ref options) => crypto_ffi::check_openssl(&root_dir, options)?,
        Subcommand::RunRustFuzzers => fuzzers::run_rust_fuzzers(&root_dir)?,
        Subcommand::BuildFfiFuzzers => fuzzers::build_ffi_fuzzers(&root_dir)?,
        Subcommand::CheckLicenseHeaders => license::check_license_headers(&root_dir)?,
        Subcommand::AddLicenseHeaders => license::add_license_headers(&root_dir)?,
        Subcommand::CheckLdtFfi => ffi::check_ldt_ffi_rust(&root_dir)?,
        Subcommand::CheckUkey2Ffi(ref options) => ukey2::check_ukey2_ffi(&root_dir, options)?,
        Subcommand::CheckLdtJni => jni::check_ldt_jni(&root_dir)?,
        Subcommand::CheckNpFfi(ref options) => ffi::check_np_ffi_rust(&root_dir, options)?,
        Subcommand::CheckLdtCmake(ref options) => ffi::check_ldt_cmake(&root_dir, options)?,
        Subcommand::CheckNpFfiCmake(ref options) => ffi::check_np_ffi_cmake(&root_dir, options)?,
        Subcommand::RunKotlinTests => jni::run_kotlin_tests(&root_dir)?,
    }

    Ok(())
}

pub fn check_workspace(root: &path::Path, options: &CheckOptions) -> anyhow::Result<()> {
    log::info!("Running cargo checks on workspace");

    let fmt_command = if options.reformat { "cargo fmt" } else { "cargo fmt --check" };

    for cargo_cmd in [
        // ensure formatting is correct (Check for it first because it is fast compared to running tests)
        fmt_command,
        // make sure everything compiles
        "cargo check --workspace --all-targets --quiet",
        // run all the tests
        //TODO: re-enable the openssl tests, this was potentially failing due to UB code in the
        // upstream rust-openssl crate's handling of empty slices. This repros consistently when
        // using the rust-openssl crate backed by openssl-sys on Ubuntu 20.04.
        "cargo test --workspace --quiet --exclude crypto_provider_openssl -- --color=always",
        // Test ukey2 builds with different crypto providers
        "cargo test -p ukey2_connections -p ukey2_rs --no-default-features --features test_rustcrypto",
        "cargo test -p ukey2_connections -p ukey2_rs --no-default-features --features test_openssl",
        // ensure the docs are valid (cross-references to other code, etc)
        concat!(
            "RUSTDOCFLAGS='--deny warnings -Z unstable-options --enable-index-page --generate-link-to-definition' ",
            "cargo +nightly doc --quiet --workspace --no-deps --document-private-items ",
            "--target-dir target/dist_docs",
        ),
        "cargo clippy --all-targets --workspace -- --deny warnings",
        "cargo deny --workspace check",
    ] {
        run_cmd_shell(root, cargo_cmd)?;
    }

    Ok(())
}

/// Runs checks to ensure lints are passing and all targets are building
pub fn check_everything(root: &path::Path, check_options: &CheckOptions) -> anyhow::Result<()> {
    license::check_license_headers(root)?;
    check_workspace(root, check_options)?;
    crypto_ffi::check_boringssl(root, &check_options.cargo_options)?;
    crypto_ffi::check_openssl(root, &check_options.cargo_options)?;
    ffi::check_everything(root, &check_options.cargo_options)?;
    jni::check_ldt_jni(root)?;
    jni::run_kotlin_tests(root)?;
    ukey2::check_ukey2_ffi(root, &check_options.cargo_options)?;
    fuzzers::run_rust_fuzzers(root)?;
    fuzzers::build_ffi_fuzzers(root)?;

    Ok(())
}

pub fn clean_everything(root: &path::Path) -> anyhow::Result<()> {
    run_cmd_shell(root, "cargo clean")?;
    run_cmd_shell(&root.join("presence/ldt_np_adv_ffi"), "cargo clean")?;
    run_cmd_shell(&root.join("presence/np_c_ffi"), "cargo clean")?;
    run_cmd_shell(&root.join("crypto/crypto_provider_boringssl"), "cargo clean")?;
    run_cmd_shell(&root.join("connections/ukey2/ukey2_c_ffi"), "cargo clean")?;
    Ok(())
}

#[derive(clap::Parser)]
struct Cli {
    #[clap(subcommand)]
    subcommand: Subcommand,
}

#[derive(clap::Subcommand, Debug, Clone)]
enum Subcommand {
    /// Checks everything in beto-rust
    CheckEverything {
        #[command(flatten)]
        check_options: CheckOptions,
    },
    /// Cleans the main workspace and all sub projects - useful if upgrading rust compiler version
    /// and need dependencies to be compiled with the same version
    CleanEverything,
    /// Checks everything included in the top level workspace
    CheckWorkspace(CheckOptions),
    /// Checks everything related to the boringssl version (equivalent of running check-boringssl
    /// + check-openssl)
    BoringsslCheckEverything(CargoOptions),
    /// Clones boringssl and uses bindgen to generate the rust crate
    BuildBoringssl,
    /// Run crypto provider tests using boringssl backend
    CheckBoringssl(CargoOptions),
    /// Applies AOSP specific patches to the 3p `openssl` crate so that it can use a boringssl
    /// backend
    PrepareRustOpenssl,
    /// Run crypto provider tests using openssl crate with boringssl backend
    CheckOpenssl(CargoOptions),
    /// Build and run pure Rust fuzzers for 10000 runs
    RunRustFuzzers,
    /// Build FFI fuzzers
    BuildFfiFuzzers,
    /// Builds and runs tests for all C/C++ projects. This is a combination of CheckNpFfi,
    /// CheckLdtFfi, and CheckCmakeBuildAndTests
    FfiCheckEverything(CargoOptions),
    /// Builds the crate checks the cbindgen generation of C/C++ bindings
    CheckNpFfi(CargoOptions),
    /// Builds ldt_np_adv_ffi crate with all possible different sets of feature flags
    CheckLdtFfi,
    /// Checks the CMake build and runs all of the C/C++ tests
    CheckLdtCmake(CargoOptions),
    /// Checks the CMake build and runs all of the C/C++ tests
    CheckNpFfiCmake(CargoOptions),
    /// Checks the workspace 3rd party crates and makes sure they have a valid license
    CheckLicenseHeaders,
    /// Generate new headers for any files that are missing them
    AddLicenseHeaders,
    /// Builds and runs tests for the UKEY2 FFI
    CheckUkey2Ffi(CargoOptions),
    /// Checks the build of ldt_jni wrapper with non default features, ie rust-openssl, and boringssl
    CheckLdtJni,
    /// Runs the kotlin tests of the LDT Jni API
    RunKotlinTests,
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct CheckOptions {
    #[arg(long, help = "reformat files with cargo fmt")]
    reformat: bool,
    #[command(flatten)]
    cargo_options: CargoOptions,
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct CargoOptions {
    #[arg(long, help = "whether to run cargo with --locked")]
    locked: bool,
}
