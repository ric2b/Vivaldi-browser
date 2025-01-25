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
use std::path::PathBuf;

fn path_buf(inputs: &[&str]) -> PathBuf {
    let path: PathBuf = inputs.iter().collect();
    path
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let build_target = std::env::var("TARGET").unwrap();
    if !build_target.contains("android") {
        panic!("Not an android target: {build_target}");
    };

    // Generate bindings.
    let project_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let header_file = PathBuf::from(&project_root).join("wrapper.h");
    let outfile = PathBuf::from(&project_root).join(path_buf(&["src", "bindings.rs"]));
    let host_tag = "linux-x86_64"; // TODO: Support windows and mac.
    let sysroot = format!(
        "{}/toolchains/llvm/prebuilt/{}/sysroot/",
        env!("ANDROID_NDK_ROOT"),
        host_tag
    );
    let mut bindings = bindgen::Builder::default()
        .header(header_file.into_os_string().into_string().unwrap())
        .clang_arg(format!("--sysroot={sysroot}"))
        .clang_arg(format!("--target={build_target}"))
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .derive_default(true)
        .layout_tests(false)
        .generate_comments(false);
    let allowlist_items = &[
        "AMediaCodec",
        "AMediaCodecBufferInfo",
        "AMediaCodec_configure",
        "AMediaCodec_createCodecByName",
        "AMediaCodec_createDecoderByType",
        "AMediaCodec_delete",
        "AMediaCodec_dequeueInputBuffer",
        "AMediaCodec_dequeueOutputBuffer",
        "AMediaCodec_getInputBuffer",
        "AMediaCodec_getOutputBuffer",
        "AMediaCodec_getOutputFormat",
        "AMediaCodec_queueInputBuffer",
        "AMediaCodec_releaseOutputBuffer",
        "AMediaCodec_releaseOutputBuffer",
        "AMediaCodec_start",
        "AMediaCodec_stop",
        "AMediaFormat",
        "AMediaFormat_delete",
        "AMediaFormat_getInt32",
        "AMediaFormat_new",
        "AMediaFormat_setBuffer",
        "AMediaFormat_setInt32",
        "AMediaFormat_setString",
    ];
    for allowlist_item in allowlist_items {
        bindings = bindings.allowlist_item(allowlist_item);
    }
    // Ideally, we should be able to merge this list with the one above. But somehow bindgen does
    // not generate bindings for these when they are called with allowlist_item instead of
    // allowlist_var.
    let allowlist_vars = &[
        "AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED",
        "AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED",
        "AMEDIACODEC_INFO_TRY_AGAIN_LATER",
        "AMEDIAFORMAT_KEY_COLOR_FORMAT",
        "AMEDIAFORMAT_KEY_CSD_0",
        "AMEDIAFORMAT_KEY_HEIGHT",
        "AMEDIAFORMAT_KEY_MAX_INPUT_SIZE",
        "AMEDIAFORMAT_KEY_MIME",
        "AMEDIAFORMAT_KEY_SLICE_HEIGHT",
        "AMEDIAFORMAT_KEY_STRIDE",
        "AMEDIAFORMAT_KEY_WIDTH",
    ];
    for allowlist_var in allowlist_vars {
        bindings = bindings.allowlist_var(allowlist_var);
    }
    let bindings = bindings
        .generate()
        .unwrap_or_else(|_| panic!("Unable to generate bindings for dav1d."));
    bindings
        .write_to_file(outfile.as_path())
        .unwrap_or_else(|_| panic!("Couldn't write bindings for dav1d"));
    println!(
        "cargo:rustc-env=CRABBYAVIF_ANDROID_NDK_MEDIA_BINDINGS_RS={}",
        outfile.display()
    );
    println!("cargo:rustc-link-lib=mediandk");
}
