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

use cmd_runner::run_cmd_shell;
use std::path;

pub(crate) fn check_platform(root: &path::Path) -> anyhow::Result<()> {
    log::info!("Checking Platform");
    let mut ffi_dir = root.to_path_buf();
    ffi_dir.push("platform");

    run_cmd_shell(&ffi_dir, "cargo build --quiet --release --lib")?;
    run_cmd_shell(&ffi_dir, "cargo test --quiet -- --color=always")?;
    run_cmd_shell(&ffi_dir, "cargo doc --quiet --no-deps")?;
    run_cmd_shell(&ffi_dir, "cargo deny check")?;

    Ok(())
}
