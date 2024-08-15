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
use cmd_runner::{run_cmd, run_cmd_shell, YellowStderr};
use env_logger::Env;
use std::{env, ffi::OsString, path};

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
        Subcommand::RunRustFuzzers => fuzzers::run_rust_fuzzers(&root_dir)?,
        Subcommand::CheckFuzztest => fuzzers::build_fuzztest_uts(&root_dir)?,
        Subcommand::CheckLicenseHeaders => license::check_license_headers(&root_dir)?,
        Subcommand::AddLicenseHeaders => license::add_license_headers(&root_dir)?,
        Subcommand::CheckUkey2Ffi(ref options) => ukey2::check_ukey2_ffi(&root_dir, options)?,
        Subcommand::RunUkey2JniTests => jni::run_ukey2_jni_tests(&root_dir)?,
        Subcommand::CheckLdtJni => jni::check_ldt_jni(&root_dir)?,
        Subcommand::CheckLdtCmake(ref options) => ffi::check_ldt_cmake(&root_dir, options)?,
        Subcommand::CheckNpFfiCmake(ref options) => ffi::check_np_ffi_cmake(&root_dir, options)?,
        Subcommand::RunKotlinTests => jni::run_kotlin_tests(&root_dir)?,
    }

    Ok(())
}

fn check_format(root: &path::Path, options: &CheckOptions) -> anyhow::Result<()> {
    // Rust format
    {
        let fmt_command = if options.reformat { "cargo fmt" } else { "cargo fmt --check" };
        run_cmd_shell(root, fmt_command)?;
    }

    // Java format. This uses the jar downloaded as part of the CI script or the local
    // `google-java-format` executable. The jar file path can be overridden with the
    // `GOOGLE_JAVA_FORMAT_ALL_DEPS_JAR` environment variable. See
    // <go/google-java-format#installation> for setup instructions for your dev environment if
    // needed.
    {
        let jar_path = std::env::var("GOOGLE_JAVA_FORMAT_ALL_DEPS_JAR").unwrap_or_else(|_| {
            "/opt/google-java-format/google-java-format-all-deps.jar".to_owned()
        });

        let mut fmt_command: Vec<OsString> = Vec::new();

        if path::PathBuf::from(&jar_path).exists() {
            fmt_command.extend(["java".into(), "-jar".into(), jar_path.into()]);
        } else {
            fmt_command.push("google-java-format".into());
        }

        if options.reformat {
            fmt_command.push("-i".into());
        } else {
            fmt_command.extend(["--set-exit-if-changed".into(), "--dry-run".into()]);
        }

        let root_str =
            glob::Pattern::escape(root.to_str().expect("Non-unicode paths are not supported"));
        let search = format!("{}/**/*.java", root_str);
        let java_files: Vec<_> = glob::glob(&search).unwrap().filter_map(Result::ok).collect();

        for file_set in java_files.chunks(100) {
            let mut args = fmt_command[1..].to_vec();
            args.extend(file_set.iter().map(OsString::from));

            run_cmd::<YellowStderr, _, _, _>(root, &fmt_command[0], args)?;
        }
    }

    Ok(())
}

pub fn check_workspace(root: &path::Path, options: &CheckOptions) -> anyhow::Result<()> {
    log::info!("Running cargo checks on workspace");

    // ensure formatting is correct (Check for it first because it is fast compared to running tests)
    check_format(root, options)?;

    for cargo_cmd in [
        // make sure everything compiles
        "cargo check --workspace --all-targets --quiet",
        // run all the tests
        "cargo test --workspace --quiet -- --color=always",
        // Test ukey2 builds with different crypto providers
        "cargo test -p ukey2_connections -p ukey2_rs --no-default-features --features test_rustcrypto",
        // ensure the docs are valid (cross-references to other code, etc)
        concat!(
            "RUSTDOCFLAGS='--deny warnings' ",
            "cargo doc --quiet --workspace --no-deps --document-private-items ",
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
    ffi::check_everything(root, &check_options.cargo_options)?;
    jni::check_ldt_jni(root)?;
    jni::run_kotlin_tests(root)?;
    jni::run_ukey2_jni_tests(root)?;
    ukey2::check_ukey2_ffi(root, &check_options.cargo_options)?;
    fuzzers::run_rust_fuzzers(root)?;
    fuzzers::build_fuzztest_uts(root)?;

    Ok(())
}

pub fn clean_everything(root: &path::Path) -> anyhow::Result<()> {
    run_cmd_shell(root, "cargo clean")?;
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
    /// Checks everything related to the boringssl version
    BoringsslCheckEverything(CargoOptions),
    /// Clones boringssl and uses bindgen to generate the rust crate
    BuildBoringssl,
    /// Run crypto provider tests using boringssl backend
    CheckBoringssl(CargoOptions),
    /// Build and run pure Rust fuzzers for 10000 runs
    RunRustFuzzers,
    /// Builds and runs fuzztest property based unit tests
    CheckFuzztest,
    /// Builds and runs tests for all C/C++ projects. This is a combination of CheckNpFfi,
    /// CheckLdtFfi, and CheckCmakeBuildAndTests
    FfiCheckEverything(CargoOptions),
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
    /// Checks the build of ldt_jni wrapper with non default features, ie boringssl
    CheckLdtJni,
    /// Runs the kotlin tests of the LDT Jni API
    RunKotlinTests,
    /// Checks the build of the ukey2_jni wrapper and runs tests
    RunUkey2JniTests,
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
