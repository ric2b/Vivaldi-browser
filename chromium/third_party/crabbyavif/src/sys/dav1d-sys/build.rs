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

// Build rust library and bindings for dav1d.

use std::env;
use std::path::Path;
use std::path::PathBuf;

extern crate pkg_config;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

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
            panic!("Unknown target_arch for android. Must be one of x86, x86_64, arm, aarch64.");
        }
    } else {
        "build"
    };

    let project_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    // Prefer locally built dav1d if available.
    let abs_library_dir = PathBuf::from(&project_root).join("dav1d");
    let abs_object_dir = PathBuf::from(&abs_library_dir).join(build_dir).join("src");
    let library_file = PathBuf::from(&abs_object_dir).join("libdav1d.a");
    let mut include_paths: Vec<String> = Vec::new();
    if Path::new(&library_file).exists() {
        println!("cargo:rustc-link-search={}", abs_object_dir.display());
        println!("cargo:rustc-link-lib=static=dav1d");
        let version_dir = PathBuf::from(&abs_library_dir)
            .join(build_dir)
            .join("include")
            .join("dav1d");
        include_paths.push(format!("-I{}", version_dir.display()));
        let include_dir = PathBuf::from(&abs_library_dir).join("include");
        include_paths.push(format!("-I{}", include_dir.display()));
    } else {
        let library = pkg_config::Config::new().probe("dav1d");
        if library.is_err() {
            println!(
                "dav1d could not be found with pkg-config. Install the system library or run dav1d.cmd"
            );
        }
        let library = library.unwrap();
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

    // Generate bindings.
    let header_file = PathBuf::from(&project_root).join("wrapper.h");
    let outfile = PathBuf::from(&project_root).join("dav1d.rs");
    let mut bindings = bindgen::Builder::default()
        .header(header_file.into_os_string().into_string().unwrap())
        .clang_args(&include_paths)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .layout_tests(false)
        .generate_comments(false);
    let allowlist_items = &[
        "dav1d_close",
        "dav1d_data_unref",
        "dav1d_data_wrap",
        "dav1d_default_settings",
        "dav1d_error",
        "dav1d_get_picture",
        "dav1d_open",
        "dav1d_picture_unref",
        "dav1d_send_data",
    ];
    for allowlist_item in allowlist_items {
        bindings = bindings.allowlist_item(allowlist_item);
    }
    let bindings = bindings
        .generate()
        .unwrap_or_else(|_| panic!("Unable to generate bindings for dav1d."));
    bindings
        .write_to_file(outfile.as_path())
        .unwrap_or_else(|_| panic!("Couldn't write bindings for dav1d"));
    println!(
        "cargo:rustc-env=CRABBYAVIF_DAV1D_BINDINGS_RS={}",
        outfile.display()
    );
}
