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

fn main() -> Result<(), String> {
    println!("cargo:rerun-if-changed=build.rs");
    if !cfg!(feature = "libyuv") {
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
            "build.android/arm64-v8a"
        } else if build_target.contains("arm") {
            "build.android/armeabi-v7a"
        } else {
            return Err(
                "Unknown target_arch for android. Must be one of x86, x86_64, arm, aarch64.".into(),
            );
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
    let custom_error;
    if Path::new(&library_file).exists() {
        println!("cargo:rustc-link-lib=static=yuv");
        println!("cargo:rustc-link-search={}", abs_object_dir.display());
        let version_dir = PathBuf::from(&abs_library_dir).join(path_buf(&["include"]));
        extra_includes_str = format!("-I{}", version_dir.display());
        custom_error = None;
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
                custom_error = None;
            }
            Err(_) => {
                custom_error = Some(
                    "libyuv binaries could not be found locally or with pkg-config. \
                    Disable the libyuv feature, install the system library libyuv-dev, \
                    or build the dependency locally by running libyuv.cmd from sys/libyuv-sys.",
                );
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
        "ABGRToI420",
        "ABGRToJ400",
        "ABGRToJ420",
        "ABGRToJ422",
        "AR30ToAB30",
        "ARGBAttenuate",
        "ARGBToABGR",
        "ARGBToI400",
        "ARGBToI420",
        "ARGBToI422",
        "ARGBToI444",
        "ARGBToJ400",
        "ARGBToJ420",
        "ARGBToJ422",
        "ARGBUnattenuate",
        "BGRAToI420",
        "Convert16To8Plane",
        "FilterMode",
        "FilterMode_kFilterBilinear",
        "FilterMode_kFilterBox",
        "FilterMode_kFilterNone",
        "HalfFloatPlane",
        "I010AlphaToARGBMatrix",
        "I010AlphaToARGBMatrixFilter",
        "I010ToAR30Matrix",
        "I010ToARGBMatrix",
        "I010ToARGBMatrixFilter",
        "I012ToARGBMatrix",
        "I210AlphaToARGBMatrix",
        "I210AlphaToARGBMatrixFilter",
        "I210ToARGBMatrix",
        "I210ToARGBMatrixFilter",
        "I400ToARGBMatrix",
        "I410AlphaToARGBMatrix",
        "I410ToARGBMatrix",
        "I420AlphaToARGBMatrix",
        "I420AlphaToARGBMatrixFilter",
        "I420ToARGBMatrix",
        "I420ToARGBMatrixFilter",
        "I420ToRGB24Matrix",
        "I420ToRGB24MatrixFilter",
        "I420ToRGB565Matrix",
        "I420ToRGBAMatrix",
        "I422AlphaToARGBMatrix",
        "I422AlphaToARGBMatrixFilter",
        "I422ToARGBMatrix",
        "I422ToARGBMatrixFilter",
        "I422ToRGB24MatrixFilter",
        "I422ToRGB565Matrix",
        "I422ToRGBAMatrix",
        "I444AlphaToARGBMatrix",
        "I444ToARGBMatrix",
        "I444ToRGB24Matrix",
        "LIBYUV_VERSION",
        "NV12Scale",
        "NV12ToARGBMatrix",
        "NV12ToRGB565Matrix",
        "NV21ToARGBMatrix",
        "P010ToAR30Matrix",
        "P010ToARGBMatrix",
        "P010ToI010",
        "RAWToI420",
        "RAWToJ400",
        "RAWToJ420",
        "RGB24ToI420",
        "RGB24ToJ400",
        "RGB24ToJ420",
        "RGBAToI420",
        "RGBAToJ400",
        "ScalePlane",
        "ScalePlane_12",
        "YuvConstants",
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
    let bindings = bindings.generate().map_err(|err| {
        if let Some(custom_error) = custom_error {
            custom_error.into()
        } else {
            err.to_string()
        }
    })?;
    bindings
        .write_to_file(outfile.as_path())
        .map_err(|err| err.to_string())?;
    Ok(())
}
