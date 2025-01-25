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

use std::{
    collections::HashMap,
    path::{self, PathBuf},
};

use serde::Deserialize;
use xshell::{cmd, Shell};

/// Partial structure for parsing the output of `cargo metadata --format-version=1`.
///
/// This doesn't contain all the fields in `cargo metadata`, just the ones we need.
#[derive(Deserialize)]
struct CargoMetadata {
    packages: Vec<CargoMetadataPackage>,
}

#[derive(Deserialize)]
struct CargoMetadataPackage {
    manifest_path: String,
    metadata: Option<HashMap<String, serde_json::Value>>,
    targets: Vec<CargoMetadataPackageTarget>,
}

#[derive(Deserialize)]
struct CargoMetadataPackageTarget {
    name: String,
    kind: Vec<String>,
}

/// A `bin` target defined for fuzzing. See [`iter_fuzz_binaries`].
#[derive(Debug)]
pub struct FuzzTarget {
    pub fuzz_dir: PathBuf,
    pub target_name: String,
}

impl FuzzTarget {
    /// Run the fuzz target using `cargo fuzz run`.
    pub fn run(&self, sh: &xshell::Shell) -> xshell::Result<()> {
        let FuzzTarget {
            fuzz_dir,
            target_name,
        } = self;
        cmd!(
            sh,
            "cargo +nightly fuzz run --fuzz-dir {fuzz_dir} {target_name} -- -runs=1000000 -max_total_time=30"
        )
        .run()
    }
}

/// Create an iterator over all of the fuzz targets defined in the workspace at `root`.
///
/// This iterator discovers targets using `cargo metadata`. It looks for crates with the metadata
/// `cargo-fuzz = true` in its Cargo.toml, and looks for `[[bin]]` targets in all of those crates.
pub fn iter_fuzz_binaries(root: &path::Path) -> anyhow::Result<impl Iterator<Item = FuzzTarget>> {
    let sh = Shell::new()?;
    sh.change_dir(root);
    let cargo_metadata = cmd!(sh, "cargo metadata --no-deps --format-version=1").read()?;
    let metadata_json: CargoMetadata = serde_json::from_str(&cargo_metadata)?;
    let packages = metadata_json.packages.into_iter().filter_map(|package| {
        let metadata_map = package.metadata.as_ref()?;
        metadata_map
            .get("cargo-fuzz")?
            .as_bool()?
            .then_some(package)
    });
    Ok(packages.flat_map(|package| {
        let fuzz_manifest = PathBuf::from(&package.manifest_path);
        let fuzz_dir = fuzz_manifest
            .parent()
            .expect("Fuzz manifest should have a parent directory")
            .to_owned();
        package
            .targets
            .into_iter()
            .filter(|t| t.kind.contains(&String::from("bin")))
            .map(move |target| FuzzTarget {
                fuzz_dir: fuzz_dir.clone(),
                target_name: target.name.to_string(),
            })
    }))
}

/// Runs all of the fuzz targets defined within the workspace `root`.
pub fn run_workspace_fuzz_targets(root: &path::Path) -> anyhow::Result<()> {
    log::info!("Running rust fuzzers");
    let sh = Shell::new()?;
    sh.change_dir(root);
    let fuzz_targets: Vec<_> = iter_fuzz_binaries(root)?.collect();
    log::info!(
        "Fuzzing on {} targets. This may take up to {} seconds.",
        fuzz_targets.len(),
        30 * fuzz_targets.len()
    );
    for fuzz_target in fuzz_targets {
        fuzz_target.run(&sh)?;
    }
    Ok(())
}
