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

use anyhow::anyhow;
use clap::Parser as _;
use cmd_runner::{license_checker::LicenseSubcommand, run_cmd, run_cmd_shell, YellowStderr};
use env_logger::Env;
use license::LICENSE_CHECKER;
use log::info;
use std::{env, ffi::OsString, path};

mod crypto_ffi;
mod ffi;
mod fuzzers;
mod jni;
mod license;

fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_env(Env::default().default_filter_or("info")).init();
    let cli: Cli = Cli::parse();

    let build_script_dir = path::PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("Must be run via Cargo to establish root directory"),
    );
    let root_dir =
        build_script_dir.parent().expect("build_scripts crate directory should have a parent");

    match cli.subcommand {
        Subcommand::RunDefaultChecks(ref check_options) => {
            run_default_checks(root_dir, check_options)?;
            print!(concat!(
                "Congratulations, the default checks passed. Since you like quality, here are\n",
                "some more checks you may like:\n",
                "    cargo run -- run-rust-fuzzers\n",
                "    cargo run -- check-stack-usage\n",
            ));
        }
        Subcommand::VerifyCi { ref check_options } => verify_ci(root_dir, check_options)?,
        Subcommand::RunAllChecks { ref check_options } => run_all_checks(root_dir, check_options)?,
        Subcommand::CleanEverything => clean_everything(root_dir)?,
        Subcommand::CheckFormat(ref options) => check_format(root_dir, options)?,
        Subcommand::CheckWorkspace(ref options) => check_workspace(root_dir, options)?,
        Subcommand::CheckAllFfi(ref options) => ffi::check_all_ffi(root_dir, options, false)?,
        Subcommand::BazelBuild => bazel_build(root_dir)?,
        Subcommand::BuildBoringssl => crypto_ffi::build_boringssl(root_dir)?,
        Subcommand::CheckBoringssl(ref options) => crypto_ffi::check_boringssl(root_dir, options)?,
        Subcommand::CheckBoringsslAtLatest(ref options) => {
            crypto_ffi::check_boringssl_at_head(root_dir, options)?
        }
        Subcommand::RunRustFuzzers => fuzzers::run_rust_fuzzers(root_dir)?,
        Subcommand::CheckFuzztest => fuzzers::build_fuzztest_unit_tests(root_dir)?,
        Subcommand::License(license_subcommand) => {
            license_subcommand.run(&LICENSE_CHECKER, root_dir)?
        }
        Subcommand::CheckUkey2Ffi(ref options) => ffi::check_ukey2_cmake(root_dir, options, false)?,
        Subcommand::RunUkey2JniTests => jni::run_ukey2_jni_tests(root_dir)?,
        Subcommand::RunNpJavaFfiTests => jni::run_np_java_ffi_tests(root_dir)?,
        Subcommand::CheckLdtCmake(ref options) => ffi::check_ldt_cmake(root_dir, options, false)?,
        Subcommand::CheckNpFfiCmake(ref options) => {
            ffi::check_np_ffi_cmake(root_dir, options, false)?
        }
        Subcommand::RunLdtKotlinTests => jni::run_ldt_kotlin_tests(root_dir)?,
    }

    Ok(())
}

fn bazel_build(root: &path::Path) -> anyhow::Result<()> {
    let beto_rust_root =
        root.parent().ok_or_else(|| anyhow!("project root dir has no parent dir"))?;
    run_cmd_shell(beto_rust_root, "bazel build ldt_np_adv_ffi")?;
    run_cmd_shell(beto_rust_root, "bazel build np_cpp_sample")?;
    Ok(())
}

fn check_format(root: &path::Path, options: &FormatterOptions) -> anyhow::Result<()> {
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
    info!("Running cargo checks on workspace");

    // ensure formatting is correct (Check for it first because it is fast compared to running tests)
    check_format(root, &options.formatter_options)?;

    for cargo_cmd in [
        // make sure everything compiles
        "cargo check --workspace --all-targets --quiet",
        // run all the tests
        &options.cargo_options.test("check_workspace", "--workspace --quiet"),
        // Test ukey2 builds with different crypto providers
        &options.cargo_options.test(
            "check_workspace_ukey2",
            "-p ukey2_connections -p ukey2_rs --no-default-features --features test_rustcrypto",
        ),
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

pub fn verify_ci(root: &path::Path, check_options: &crate::CheckOptions) -> anyhow::Result<()> {
    if cfg!(target_os = "linux") {
        run_all_checks(root, check_options)?;
    } else if cfg!(target_os = "macos") {
        license::LICENSE_CHECKER.check(root)?;
        check_workspace(root, check_options)?;
        ffi::check_all_ffi(root, &check_options.cargo_options, false)?;
        jni::run_np_java_ffi_tests(root)?;
        jni::run_ldt_kotlin_tests(root)?;
        jni::run_ukey2_jni_tests(root)?;
    } else if cfg!(windows) {
        license::LICENSE_CHECKER.check(root)?;
        check_workspace(root, check_options)?;
        ffi::check_all_ffi(root, &check_options.cargo_options, false)?;
    } else {
        panic!("Unsupported OS for running CI!")
    }
    Ok(())
}

/// Runs checks to ensure lints are passing and all targets are building
pub fn run_all_checks(root: &path::Path, check_options: &CheckOptions) -> anyhow::Result<()> {
    run_default_checks(root, check_options)?;
    Ok(())
}

/// Runs default checks that are suiable for verifying a local change.
pub fn run_default_checks(root: &path::Path, check_options: &CheckOptions) -> anyhow::Result<()> {
    license::LICENSE_CHECKER.check(root)?;
    check_workspace(root, check_options)?;
    // ffi build and run C++ tests against rustcrypto
    ffi::check_all_ffi(root, &check_options.cargo_options, false)?;

    // ffi build and run tests against boringssl
    crypto_ffi::check_boringssl(root, &check_options.cargo_options)?;
    ffi::check_all_ffi(root, &check_options.cargo_options, true)?;

    if !cfg!(target_os = "windows") {
        fuzzers::build_fuzztest_unit_tests(root)?;
    }
    if !cfg!(target_os = "windows") {
        // Test below requires Java SE 9, but on Windows we only have Java SE 8 installed
        jni::run_np_java_ffi_tests(root)?;
    }
    jni::run_ldt_kotlin_tests(root)?;
    jni::run_ukey2_jni_tests(root)?;
    Ok(())
}

pub fn clean_everything(root: &path::Path) -> anyhow::Result<()> {
    run_cmd_shell(root, "cargo clean")?;
    run_cmd_shell(&root.join("presence/np_c_ffi"), "cargo clean")?;
    run_cmd_shell(&root.join("crypto/crypto_provider_boringssl"), "cargo clean")?;
    run_cmd_shell(&root.join("connections/ukey2/ukey2_c_ffi"), "cargo clean")?;
    run_cmd_shell(&root.join("presence/np_java_ffi"), "./gradlew :clean")?;
    Ok(())
}

#[derive(clap::Parser)]
struct Cli {
    #[clap(subcommand)]
    subcommand: Subcommand,
}

#[derive(clap::Subcommand, Debug, Clone)]
enum Subcommand {
    /// Runs all the checks that CI runs
    VerifyCi {
        #[command(flatten)]
        check_options: CheckOptions,
    },
    /// Runs all available checks, including ones that are not normally run on CI
    RunAllChecks {
        #[command(flatten)]
        check_options: CheckOptions,
    },
    /// Builds all the bazel targets
    BazelBuild,
    /// Runs the default set of checks suitable for verifying local changes.
    RunDefaultChecks(CheckOptions),
    /// Cleans the main workspace and all sub projects - useful if upgrading rust compiler version
    /// and need dependencies to be compiled with the same version
    CleanEverything,
    /// Checks code formatting
    CheckFormat(FormatterOptions),
    /// Checks everything included in the top level workspace
    CheckWorkspace(CheckOptions),
    /// Clones boringssl and uses bindgen to generate the rust crate
    BuildBoringssl,
    /// Run crypto provider tests using boringssl backend
    CheckBoringssl(CargoOptions),
    /// Checks out latest boringssl commit and runs our tests against it
    CheckBoringsslAtLatest(CargoOptions),
    /// Build and run pure Rust fuzzers for 10000 runs
    RunRustFuzzers,
    /// Builds and runs fuzztest property based unit tests
    CheckFuzztest,
    /// Builds and runs tests for all C/C++ projects. This is a combination of CheckNpFfi,
    /// CheckLdtFfi, and CheckCmakeBuildAndTests
    CheckAllFfi(CargoOptions),
    /// Checks the CMake build and runs all of the C/C++ tests
    CheckLdtCmake(CargoOptions),
    /// Checks the CMake build and runs all of the C/C++ tests
    CheckNpFfiCmake(CargoOptions),
    #[command(flatten)]
    License(LicenseSubcommand),
    /// Builds and runs tests for the UKEY2 FFI
    CheckUkey2Ffi(CargoOptions),
    /// Runs the kotlin tests of the LDT Jni API
    RunLdtKotlinTests,
    /// Checks the build of the ukey2_jni wrapper and runs tests
    RunUkey2JniTests,
    /// Checks the build of the np_java_ffi wrapper and runs tests
    RunNpJavaFfiTests,
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct FormatterOptions {
    #[arg(long, help = "reformat files files in the workspace with the code formatter")]
    reformat: bool,
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct CheckOptions {
    #[command(flatten)]
    formatter_options: FormatterOptions,
    #[command(flatten)]
    cargo_options: CargoOptions,
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct CargoOptions {
    #[arg(long, help = "whether to run cargo with --locked")]
    locked: bool,
    #[arg(long, help = "gather coverage metrics")]
    coverage: bool,
}

impl CargoOptions {
    /// Run `cargo test` or `cargo llvm-cov` depending on the configured options.
    pub fn test(&self, tag: &str, args: impl AsRef<str>) -> String {
        format!(
            "cargo {subcommand} {locked} {args} {cov_args} -- --color=always",
            subcommand = if self.coverage { "llvm-cov" } else { "test" },
            locked = if self.locked { "--locked" } else { "" },
            args = args.as_ref(),
            cov_args = if self.coverage {
                format!("--lcov --output-path \"target/{tag}.info\"")
            } else {
                String::default()
            },
        )
    }
}
