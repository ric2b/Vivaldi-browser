use std::path::PathBuf;
use std::fs;

fn main() {
    // Directory for the fontconfig build

    let cargo_manifest_dir =
        std::env::var("CARGO_MANIFEST_DIR").expect("CARGO_MANIFEST_DIR not set");

    let mut build_dir = PathBuf::from(cargo_manifest_dir);
    build_dir = build_dir.join("../../build");
    build_dir = fs::canonicalize(build_dir).expect("Failed to canonicalize path.");

    let fontconfig_lib = build_dir.join("libfontconfig.a");

    if !fontconfig_lib.exists() {
        panic!("{:?} not found, build FontConfig with -Dfontations=enabled into dir build/ first.", fontconfig_lib);
    }

    // Tell cargo to look for fontconfig in the build directory
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=static=fontconfig");

    // FreeType and Expat from the system.
    println!("cargo:rustc-link-lib=dylib=freetype");
    println!("cargo:rustc-link-lib=dylib=expat");

    // Rerun this build script if the fontconfig source code changes
    println!("cargo:rerun-if-changed=src");
}
