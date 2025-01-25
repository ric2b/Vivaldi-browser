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

use std::path;

use clap::Parser;
use cmd_runner::{
    cargo_workspace::{CargoOptions, CargoWorkspaceSubcommand, FormatterOptions},
    license_checker::LicenseSubcommand,
};
use license::LICENSE_CHECKER;
use xshell::Shell;

mod license;

#[derive(clap::Parser)]
struct Cli {
    #[clap(subcommand)]
    subcommand: Subcommand,
}

#[derive(clap::Subcommand, Debug, Clone)]
enum Subcommand {
    VerifyCi {
        #[command(flatten)]
        cargo_options: CargoOptions,
    },
    #[command(flatten)]
    CargoWorkspace(CargoWorkspaceSubcommand),
    #[command(flatten)]
    License(LicenseSubcommand),
}

fn main() -> anyhow::Result<()> {
    let args = Cli::parse();
    let root_dir = path::Path::new(
        &std::env::var_os("CARGO_MANIFEST_DIR")
            .expect("Must be run via Cargo to establish root directory"),
    )
    .parent()
    .expect("Workspace directory should exist")
    .to_path_buf();
    let sh = Shell::new()?;
    sh.change_dir(&root_dir);
    match args.subcommand {
        Subcommand::VerifyCi { cargo_options } => {
            cargo_options.check_workspace(&sh, "common")?;
            FormatterOptions { reformat: false }.check_format(&sh)?;
            LICENSE_CHECKER.check(&root_dir)?;
        }
        Subcommand::CargoWorkspace(workspace) => workspace.run("common", &sh)?,
        Subcommand::License(license) => license.run(&LICENSE_CHECKER, &root_dir)?,
    }
    Ok(())
}
