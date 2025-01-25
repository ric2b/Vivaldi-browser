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

use std::ffi::OsStr;

use xshell::cmd;

#[derive(clap::Subcommand, Debug, Clone)]
pub enum CargoWorkspaceSubcommand {
    /// Checks test, clippy, and cargo deny in the workspace
    CheckWorkspace(CargoOptions),
    /// Checks the formatting of the workspace
    CheckFormat(FormatterOptions),
}

impl CargoWorkspaceSubcommand {
    pub fn run(&self, tag: &str, sh: &xshell::Shell) -> anyhow::Result<()> {
        match self {
            CargoWorkspaceSubcommand::CheckWorkspace(cargo_options) => {
                cargo_options.check_workspace(sh, tag)
            }
            CargoWorkspaceSubcommand::CheckFormat(formatter_options) => {
                formatter_options.check_format(sh)
            }
        }
    }
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct CargoOptions {
    #[arg(long, help = "whether to run cargo with --locked")]
    pub locked: bool,
    #[arg(long, help = "gather coverage metrics")]
    pub coverage: bool,
}

impl CargoOptions {
    /// Run `cargo test` or `cargo llvm-cov` depending on the configured options.
    pub fn test<'sh, S: AsRef<OsStr>>(
        &self,
        sh: &'sh xshell::Shell,
        tag: &str,
        args: impl IntoIterator<Item = S>,
    ) -> xshell::Cmd<'sh> {
        let locked = if self.locked { "--locked" } else { "" };
        if self.coverage {
            cmd!(
                sh,
                "cargo llvm-cov {locked} {args...} --lcov --output-path target/{tag}.info -- --color=always"
            )
        } else {
            cmd!(sh, "cargo test {locked} {args...} -- --color=always")
        }
    }

    /// Run the default set of checks on a cargo workspace
    pub fn check_workspace(&self, sh: &xshell::Shell, tag: &str) -> anyhow::Result<()> {
        self.test(sh, tag, ["--workspace"]).run()?;
        cmd!(
            sh,
            "cargo clippy --all-targets --workspace -- --deny warnings"
        )
        .run()?;
        cmd!(sh, "cargo deny --workspace check").run()?;
        // ensure the docs are valid (cross-references to other code, etc)
        cmd!(
            sh,
            "cargo doc --quiet --workspace --no-deps --document-private-items
                --target-dir target/dist_docs/{tag}"
        )
        .env("RUSTDOCFLAGS", "--deny warnings")
        .run()?;
        Ok(())
    }
}

#[derive(clap::Args, Debug, Clone, Default)]
pub struct FormatterOptions {
    #[arg(
        long,
        help = "reformat files files in the workspace with the code formatter"
    )]
    pub reformat: bool,
}

impl FormatterOptions {
    pub fn check_format(&self, sh: &xshell::Shell) -> anyhow::Result<()> {
        if self.reformat {
            cmd!(sh, "cargo fmt").run()?;
        } else {
            cmd!(sh, "cargo fmt --check").run()?;
        }
        Ok(())
    }
}
