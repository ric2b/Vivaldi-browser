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

use chrono::Datelike;
use file_header::{check_headers_recursively, license::spdx::*};
use std::path;

#[derive(clap::Subcommand, Debug, Clone)]
pub enum LicenseSubcommand {
    /// Checks the workspace 3rd party crates and makes sure they have a valid license
    CheckLicenseHeaders,
    /// Generate new headers for any files that are missing them
    AddLicenseHeaders,
}

impl LicenseSubcommand {
    pub fn run(&self, checker: &LicenseChecker, root: &path::Path) -> anyhow::Result<()> {
        match self {
            LicenseSubcommand::CheckLicenseHeaders => checker.check(root)?,
            LicenseSubcommand::AddLicenseHeaders => checker.add_missing(root)?,
        }
        Ok(())
    }
}

pub struct LicenseChecker {
    pub ignore: &'static [&'static str],
}

impl LicenseChecker {
    pub fn check(&self, root: &path::Path) -> anyhow::Result<()> {
        log::info!("Checking license headers");
        let ignore = self.ignore_globset()?;
        let results = check_headers_recursively(
            root,
            |p| !ignore.is_match(p),
            APACHE_2_0.build_header(YearCopyrightOwnerValue::new(
                u32::try_from(chrono::Utc::now().year())?,
                "Google LLC".to_string(),
            )),
            4,
        )?;

        for path in results.no_header_files.iter() {
            eprintln!("Header not present: {path:?}");
        }

        for path in results.binary_files.iter() {
            eprintln!("Binary file: {path:?}");
        }
        if !results.binary_files.is_empty() {
            eprintln!("Consider adding binary files to the ignore list in src/licence.rs.");
        }

        if results.has_failure() {
            Err(anyhow::anyhow!("License header check failed"))
        } else {
            Ok(())
        }
    }

    pub fn add_missing(&self, root: &path::Path) -> anyhow::Result<()> {
        let ignore = self.ignore_globset()?;
        for p in file_header::add_headers_recursively(
            root,
            |p| !ignore.is_match(p),
            APACHE_2_0.build_header(YearCopyrightOwnerValue::new(
                u32::try_from(chrono::Utc::now().year())?,
                "Google LLC".to_string(),
            )),
        )? {
            println!("Added header: {:?}", p);
        }

        Ok(())
    }

    fn ignore_globset(&self) -> Result<globset::GlobSet, globset::Error> {
        let mut builder = globset::GlobSet::builder();
        for lic in self.ignore {
            builder.add(globset::Glob::new(lic)?);
        }
        builder.build()
    }

    pub fn check_new_ignore_is_likely_buggy(&self) {
        for dir in self.ignore {
            assert!(
                dir.starts_with("**/"),
                "Matching on the root filesystem is likely unintended"
            );
        }
    }
}
