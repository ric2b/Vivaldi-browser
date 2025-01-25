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
use std::{
    env,
    path::{self, PathBuf},
};

mod ctap_protocol;
mod platform;
mod remote_auth_protocol;

fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_env(Env::default().default_filter_or("info")).init();
    let cli: Cli = Cli::parse();

    let build_scripts_dir = PathBuf::from(
        env::var("CARGO_MANIFEST_DIR").expect("Must be run via Cargo to establish root directory"),
    );
    let root_dir = build_scripts_dir
        .parent()
        .expect("build_scripts directory should have a parent");

    match cli.subcommand {
        Subcommand::CheckEverything(ref options) => check_everything(root_dir, options)?,
        Subcommand::CheckWorkspace(ref options) => check_workspace(root_dir, options)?,
        Subcommand::CheckCtapProtocol => ctap_protocol::check_ctap_protocol(root_dir)?,
        Subcommand::CheckPlatform => platform::check_platform(root_dir)?,
        Subcommand::CheckRemoteAuthProtocol => {
            remote_auth_protocol::check_remote_auth_protocol(root_dir)?
        }
    }

    Ok(())
}

pub fn check_everything(root: &path::Path, check_options: &CheckOptions) -> anyhow::Result<()> {
    check_workspace(root, check_options)?;
    ctap_protocol::check_ctap_protocol(root)?;
    platform::check_platform(root)?;
    remote_auth_protocol::check_remote_auth_protocol(root)?;
    Ok(())
}

pub fn check_workspace(root: &path::Path, options: &CheckOptions) -> anyhow::Result<()> {
    log::info!("Running cargo checks on workspace");

    let fmt_command = if options.reformat {
        "cargo fmt"
    } else {
        "cargo fmt --check"
    };

    for cargo_cmd in [
        fmt_command,
        "cargo check --workspace --all-targets --quiet",
        "cargo test --workspace --quiet -- --color=always",
        "cargo doc --quiet --no-deps",
        "cargo deny --workspace check",
        "cargo clippy --all-targets --workspace -- --deny warnings",
    ] {
        run_cmd_shell(root, cargo_cmd)?;
    }

    Ok(())
}

#[derive(clap::Parser)]
struct Cli {
    #[clap(subcommand)]
    subcommand: Subcommand,
}

#[derive(clap::Subcommand, Debug, Clone)]
#[allow(clippy::enum_variant_names)]
enum Subcommand {
    /// Checks everything in remoteauth
    CheckEverything(CheckOptions),
    /// Checks everything included in the top level workspace
    CheckWorkspace(CheckOptions),
    /// Build and run tests for the CTAP Protocol
    CheckCtapProtocol,
    /// Builds and run tests for the Platform
    CheckPlatform,
    /// Builds and run tests for the Remote Auth Protocol
    CheckRemoteAuthProtocol,
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct CheckOptions {
    #[arg(long, help = "reformat files with cargo fmt")]
    reformat: bool,
}
