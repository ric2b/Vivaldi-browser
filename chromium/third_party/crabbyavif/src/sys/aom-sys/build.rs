// Copyright 2025 Google LLC
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

// Build rust library and bindings for libaom.

use std::env;
use std::path::Path;
use std::path::PathBuf;

extern crate pkg_config;

fn main() -> Result<(), String> {
    println!("cargo:rerun-if-changed=build.rs");
    if !cfg!(feature = "aom") {
        // The feature is disabled at the top level. Do not build this dependency.
        return Ok(());
    }

    let build_target = std::env::var("TARGET").unwrap();
    let build_dir = if build_target.contains("android") {
        if build_target.contains("x86_64") {
            "build.android/x86_64"
        } else if build_target.contains("x86") {
            "build.android/x86"
        } else if build_target.contains("aarch64") {
            "build.android/aarch64"
        } else if build_target.contains("arm") {
            "build.android/arm"
        } else {
            return Err(
                "Unknown target_arch for android. Must be one of x86, x86_64, arm, aarch64.".into(),
            );
        }
    } else {
        "build.libavif"
    };

    let project_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    // Prefer locally built libaom if available.
    let abs_library_dir = PathBuf::from(&project_root).join("aom");
    let abs_object_dir = PathBuf::from(&abs_library_dir).join(build_dir);
    let library_file = PathBuf::from(&abs_object_dir).join("libaom.a");
    let mut include_paths: Vec<String> = Vec::new();
    if Path::new(&library_file).exists() {
        println!("cargo:rustc-link-search={}", abs_object_dir.display());
        println!("cargo:rustc-link-lib=static=aom");
        let version_dir = PathBuf::from(&abs_library_dir)
            .join(build_dir)
            .join("config");
        include_paths.push(format!("-I{}", version_dir.display()));
        let include_dir = PathBuf::from(&abs_library_dir);
        include_paths.push(format!("-I{}", include_dir.display()));
    } else {
        match pkg_config::Config::new().probe("aom") {
            Ok(library) => {
                for lib in &library.libs {
                    println!("cargo:rustc-link-lib={lib}");
                }
                for link_path in &library.link_paths {
                    println!("cargo:rustc-link-search={}", link_path.display());
                }
                for include_path in &library.include_paths {
                    include_paths.push(format!("-I{}", include_path.display()));
                }
            }
            Err(_) => {
                return Err(
                    "aom binaries could not be found locally or with pkg-config. \
                    Disable the aom feature, install the libaom-dev system library, \
                    or build the dependency locally by running aom.cmd from sys/aom-sys."
                        .into(),
                );
            }
        }
    }

    // Generate bindings.
    let header_file = PathBuf::from(&project_root).join("wrapper.h");
    let outfile = PathBuf::from(&project_root).join("aom.rs");
    let bindings = bindgen::Builder::default()
        .header(header_file.into_os_string().into_string().unwrap())
        .clang_args(&include_paths)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .layout_tests(false)
        .generate_comments(false);
    // TODO: b/402941742 - Add an allowlist to only generate bindings for necessary items.
    let bindings = bindings.generate().map_err(|err| err.to_string())?;
    bindings
        .write_to_file(outfile.as_path())
        .map_err(|err| err.to_string())?;
    println!(
        "cargo:rustc-env=CRABBYAVIF_AOM_BINDINGS_RS={}",
        outfile.display()
    );
    Ok(())
}
