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

#[cfg(feature = "capi")]
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=cbindgen.toml");
    #[cfg(feature = "libgav1")]
    {
        // libgav1 needs libstdc++ on *nix/windows and libc++ on mac.
        if cfg!(target_os = "macos") {
            println!("cargo:rustc-link-arg=-lc++");
        } else {
            println!("cargo:rustc-link-arg=-lstdc++");
        }
    }
    #[cfg(feature = "capi")]
    {
        // Generate the C header.
        let crate_path = env!("CARGO_MANIFEST_DIR");
        let config = cbindgen::Config::from_root_or_default(crate_path);
        let header_file = PathBuf::from(crate_path).join("include/avif/avif.h");
        cbindgen::Builder::new()
            .with_crate(crate_path)
            .with_config(config)
            .generate()
            .unwrap()
            .write_to_file(header_file);
    }
}
