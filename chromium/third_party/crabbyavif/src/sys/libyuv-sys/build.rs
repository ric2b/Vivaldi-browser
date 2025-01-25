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

// Build rust library and bindings for libyuv.

use std::env;
use std::path::Path;
use std::path::PathBuf;

fn path_buf(inputs: &[&str]) -> PathBuf {
    let path: PathBuf = inputs.iter().collect();
    path
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let build_target = std::env::var("TARGET").unwrap();
    let build_dir = if build_target.contains("android") {
        if build_target.contains("x86_64") {
            "build.android/x86_64"
        } else if build_target.contains("x86") {
            "build.android/x86"
        } else if build_target.contains("aarch64") {
            "build.android/arm64-v8a"
        } else if build_target.contains("arm") {
            "build.android/armeabi-v7a"
        } else {
            panic!("Unknown target_arch for android. Must be one of x86, x86_64, arm, aarch64.");
        }
    } else {
        "build"
    };

    let project_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let abs_library_dir = PathBuf::from(&project_root).join("libyuv");
    let abs_object_dir = PathBuf::from(&abs_library_dir).join(build_dir);
    let library_file = PathBuf::from(&abs_object_dir).join(if cfg!(target_os = "windows") {
        "yuv.lib"
    } else {
        "libyuv.a"
    });
    let extra_includes_str;
    if Path::new(&library_file).exists() {
        println!("cargo:rustc-link-lib=static=yuv");
        println!("cargo:rustc-link-search={}", abs_object_dir.display());
        let version_dir = PathBuf::from(&abs_library_dir).join(path_buf(&["include"]));
        extra_includes_str = format!("-I{}", version_dir.display());
    } else {
        // Local library was not found. Look for a system library.
        match pkg_config::Config::new().probe("yuv") {
            Ok(library) => {
                for lib in &library.libs {
                    println!("cargo:rustc-link-lib={lib}");
                }
                for link_path in &library.link_paths {
                    println!("cargo:rustc-link-search={}", link_path.display());
                }
                let mut include_str = String::new();
                for include_path in &library.include_paths {
                    include_str.push_str("-I");
                    include_str.push_str(include_path.to_str().unwrap());
                }
                extra_includes_str = include_str;
            }
            Err(_) => {
                // Try to build without any extra flags.
                println!("cargo:rustc-link-lib=yuv");
                extra_includes_str = String::new();
            }
        }
    }

    // Generate bindings.
    let header_file = PathBuf::from(&project_root).join("wrapper.h");
    let outdir = std::env::var("OUT_DIR").expect("OUT_DIR not set");
    let outfile = PathBuf::from(&outdir).join("libyuv_bindgen.rs");
    let mut bindings = bindgen::Builder::default()
        .header(header_file.into_os_string().into_string().unwrap())
        .clang_arg(extra_includes_str)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .layout_tests(false)
        .generate_comments(false);
    let allowlist_items = &[
        "YuvConstants",
        "FilterMode",
        "ARGBAttenuate",
        "ARGBToABGR",
        "ARGBUnattenuate",
        "Convert16To8Plane",
        "HalfFloatPlane",
        "ScalePlane_12",
        "ScalePlane",
        "FilterMode_kFilterBilinear",
        "FilterMode_kFilterBox",
        "FilterMode_kFilterNone",
        "I010AlphaToARGBMatrixFilter",
        "I010AlphaToARGBMatrix",
        "I010ToARGBMatrixFilter",
        "I010ToARGBMatrix",
        "I012ToARGBMatrix",
        "I210AlphaToARGBMatrixFilter",
        "I210AlphaToARGBMatrix",
        "I210ToARGBMatrixFilter",
        "I210ToARGBMatrix",
        "I400ToARGBMatrix",
        "I410AlphaToARGBMatrix",
        "I410ToARGBMatrix",
        "I420AlphaToARGBMatrixFilter",
        "I420AlphaToARGBMatrix",
        "I420ToARGBMatrixFilter",
        "I420ToARGBMatrix",
        "I420ToRGB24MatrixFilter",
        "I420ToRGB24Matrix",
        "I420ToRGB565Matrix",
        "I420ToRGBAMatrix",
        "I422AlphaToARGBMatrixFilter",
        "I422AlphaToARGBMatrix",
        "I422ToARGBMatrixFilter",
        "I422ToARGBMatrix",
        "I422ToRGB24MatrixFilter",
        "I422ToRGB565Matrix",
        "I422ToRGBAMatrix",
        "I444AlphaToARGBMatrix",
        "I444ToARGBMatrix",
        "I444ToRGB24Matrix",
        "NV12ToARGBMatrix",
        "NV21ToARGBMatrix",
        "P010ToAR30Matrix",
        "P010ToARGBMatrix",
        "AR30ToAB30",
        "kYuv2020Constants",
        "kYuvF709Constants",
        "kYuvH709Constants",
        "kYuvI601Constants",
        "kYuvJPEGConstants",
        "kYuvV2020Constants",
        "kYvu2020Constants",
        "kYvuF709Constants",
        "kYvuH709Constants",
        "kYvuI601Constants",
        "kYvuJPEGConstants",
        "kYvuV2020Constants",
    ];
    for allowlist_item in allowlist_items {
        bindings = bindings.allowlist_item(allowlist_item);
    }
    let bindings = bindings
        .generate()
        .unwrap_or_else(|_| panic!("Unable to generate bindings for libyuv."));
    bindings
        .write_to_file(outfile.as_path())
        .unwrap_or_else(|_| panic!("Couldn't write bindings for libyuv"));
}
