// Copyright 2019 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generates the Rust D-Bus bindings and protobuf definitions for system_api.
// The generated bindings are included in the published crate since the source XML files are only
// available from the original path or the ebuild.

use std::env;
use std::error::Error;
use std::fs::{self, File};
use std::io::Write;
use std::path::{Path, PathBuf};

type Result<T> = std::result::Result<T, Box<dyn Error>>;

use chromeos_dbus_bindings::{generate_module, BindingsType};

// The parent path of system_api.
const SOURCE_DIR: &str = "..";

const OPTS: Option<&[&str]> = None;

// (<module name>, <relative path to source xml>)
// When adding additional bindings, remember to include the source project and subtree in the
// ebuild. Otherwise, the source files will not be accessible when building dev-rust/system_api.
const BINDINGS_TO_GENERATE: &[(&str, &str, BindingsType)] = &[
    (
        "org_chromium_debugd",
        "debugd/dbus_bindings/org.chromium.debugd.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_dlcservice",
        "dlcservice/dbus_adaptors/org.chromium.DlcServiceInterface.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_flimflam_manager",
        "shill/dbus_bindings/org.chromium.flimflam.Manager.dbus-xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_flimflam_service",
        "shill/dbus_bindings/org.chromium.flimflam.Service.dbus-xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_permission_broker",
        "permission_broker/dbus_bindings/org.chromium.PermissionBroker.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_power_manager",
        "power_manager/dbus_bindings/org.chromium.PowerManager.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_printscanmgr",
        "printscanmgr/dbus_bindings/org.chromium.printscanmgr.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_sessionmanagerinterface",
        "login_manager/dbus_bindings/org.chromium.SessionManagerInterface.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_spaced",
        "spaced/dbus_bindings/org.chromium.Spaced.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_swapmanagement",
        "swap_management/dbus_bindings/org.chromium.SwapManagement.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_userdataauth",
        "cryptohome/dbus_bindings/org.chromium.UserDataAuth.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_vm_concierge",
        "vm_tools/dbus_bindings/org.chromium.VmConcierge.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_vm_cicerone",
        "vm_tools/dbus_bindings/org.chromium.VmCicerone.xml",
        BindingsType::Client(OPTS),
    ),
    (
        "org_chromium_vtpm",
        "vtpm/dbus_bindings/org.chromium.Vtpm.xml",
        BindingsType::Client(OPTS),
    ),
];

// (<module name>, <relative path to .proto file>)
// When adding additional protos, remember to include the source project and subtree in the
// ebuild. Otherwise, the source files will not be accessible when building dev-rust/system_api.
const PROTOS_TO_GENERATE: &[(&str, &str)] = &[
    ("apps", "system_api/dbus/vm_applications/apps.proto"),
    ("arc", "system_api/dbus/arc/arc.proto"),
    (
        "auth_factor",
        "system_api/dbus/cryptohome/auth_factor.proto",
    ),
    (
        "battery_saver",
        "system_api/dbus/power_manager/battery_saver.proto",
    ),
    (
        "chunneld_service",
        "system_api/dbus/chunneld/chunneld_service.proto",
    ),
    (
        "cicerone_service",
        "system_api/dbus/vm_cicerone/cicerone_service.proto",
    ),
    (
        "concierge_service",
        "system_api/dbus/vm_concierge/concierge_service.proto",
    ),
    ("dlcservice", "system_api/dbus/dlcservice/dlcservice.proto"),
    ("fido", "system_api/dbus/cryptohome/fido.proto"),
    ("key", "system_api/dbus/cryptohome/key.proto"),
    (
        "printscanmgr_service",
        "system_api/dbus/printscanmgr/printscanmgr_service.proto",
    ),
    (
        "metrics_event",
        "system_api/dbus/metrics_event/metrics_event.proto",
    ),
    (
        "recoverable_key_store",
        "system_api/dbus/cryptohome/recoverable_key_store.proto",
    ),
    (
        "resource_manager",
        "system_api/dbus/resource_manager/resource_manager.proto",
    ),
    ("rpc", "system_api/dbus/cryptohome/rpc.proto"),
    (
        "shadercached",
        "system_api/dbus/shadercached/shadercached.proto",
    ),
    ("spaced", "system_api/dbus/spaced/spaced.proto"),
    (
        "UserDataAuth",
        "system_api/dbus/cryptohome/UserDataAuth.proto",
    ),
    (
        "vm_memory_management",
        "system_api/non_standard_ipc/vm_memory_management/vm_memory_management.proto",
    ),
    ("vtpm_interface", "vtpm/vtpm_interface.proto"),
    (
        "update_engine",
        "system_api/dbus/update_engine/update_engine.proto",
    ),
];

fn generate_protos(source_dir: &Path, protos: &[(&str, &str)]) -> Result<()> {
    let out_dir = PathBuf::from("src/protos");
    if out_dir.exists() {
        // If CROS_RUST is set, skip generation.
        if env::var("CROS_RUST") == Ok(String::from("1")) {
            return Ok(());
        }
        fs::remove_dir_all(&out_dir)?;
    }
    fs::create_dir_all(&out_dir)?;

    let mut out = File::create(out_dir.join("include_protos.rs"))?;

    for (module, input_path) in protos {
        let input_path = source_dir.join(input_path);
        let input_dir = input_path.parent().unwrap();
        let parent_input_dir = source_dir.join("system_api/dbus");

        // Invoke protobuf compiler.
        protobuf_codegen::Codegen::new()
            .input(input_path.as_os_str().to_str().unwrap())
            .include(input_dir.as_os_str().to_str().unwrap())
            .include(parent_input_dir)
            .out_dir(&out_dir)
            .run()
            .expect("protoc");

        // Write out a `mod` that refers to the generated module.
        writeln!(out, "pub mod {};", module)?;
    }
    Ok(())
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=dbus");
    for (_, directory, _) in BINDINGS_TO_GENERATE {
        println!("cargo:rerun-if-changed=../{}", directory);
    }
    generate_module(Path::new(SOURCE_DIR), BINDINGS_TO_GENERATE).unwrap();
    generate_protos(Path::new(SOURCE_DIR), PROTOS_TO_GENERATE).unwrap();
}
