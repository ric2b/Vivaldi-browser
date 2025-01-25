
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@crate_index//:defs.bzl", "aliases")
load("@rules_rust//rust:defs.bzl", "rust_library")

cc_library(
    name = "ldt_np_adv_ffi",
    hdrs = ["nearby/presence/ldt_np_adv_ffi/c/include/np_ldt.h"],
    includes = ["nearby/presence/ldt_np_adv_ffi/c/include"],
    visibility = ["//visibility:public"],
    deps = [":ldt_np_adv_ffi_rust"],
)

cc_library(
    name = "np_cpp_ffi",
    srcs = ["nearby/presence/np_cpp_ffi/nearby_protocol.cc"],
    hdrs = ["nearby/presence/np_cpp_ffi/include/nearby_protocol.h"],
    includes = [
        "nearby/presence/np_c_ffi/include/cpp",
        "nearby/presence/np_cpp_ffi/include",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":np_c_ffi",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "np_c_ffi_types",
    hdrs = ["nearby/presence/np_c_ffi/include/cpp/np_cpp_ffi_types.h"],
    includes = ["nearby/presence/np_c_ffi/include/cpp/"],
    visibility = ["//visibility:public"],
)

cc_binary(
    name = "np_cpp_sample",
    srcs = ["nearby/presence/np_cpp_ffi/sample/main.cc"],
    deps = [
        ":np_c_ffi_types",
        ":np_cpp_ffi",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "np_c_ffi",
    hdrs = [
        "nearby/presence/np_c_ffi/include/cpp/np_cpp_ffi_functions.h",
        "nearby/presence/np_c_ffi/include/cpp/np_cpp_ffi_types.h",
    ],
    includes = ["nearby/presence/np_cpp_ffi/include/cpp"],
    deps = [":np_c_ffi_rust"],
)

rust_library(
    name = "np_c_ffi_rust",
    srcs = glob(include = ["nearby/presence/np_c_ffi/src/**/*.rs"]),
    crate_features = ["std"],
    deps = [
        ":lock_adapter",
        ":np_ffi_core",
    ],
)

rust_library(
    name = "np_ffi_core",
    srcs = glob(include = ["nearby/presence/np_ffi_core/src/**/*.rs"]),
    crate_features = ["crypto_provider/raw_private_key_permit"],
    proc_macro_deps = ["@crate_index//:strum_macros"],
    deps = [
        ":array_view",
        ":crypto_provider",
        ":crypto_provider_default",
        ":handle_map",
        ":ldt_np_adv",
        ":lock_adapter",
        ":np_adv",
        ":np_adv_dynamic",
        ":np_hkdf",
        "@crate_index//:lazy_static",
        "@crate_index//:strum",
    ],
)

rust_library(
    name = "np_adv_dynamic",
    srcs = glob(include = ["nearby/presence/np_adv_dynamic/src/**/*.rs"]),
    deps = [
        ":array_view",
        ":crypto_provider",
        ":np_adv",
        ":sink",
        "@crate_index//:thiserror",
    ],
)

rust_library(
    name = "handle_map",
    srcs = glob(include = ["common/handle_map/src/**/*.rs"]),
    deps = [
        ":lock_adapter",
        "@crate_index//:lazy_static",
    ],
)

rust_library(
    name = "lock_adapter",
    srcs = glob(include = ["common/lock_adapter/src/**/*.rs"]),
    crate_features = ["std"],
)

rust_library(
    name = "np_adv",
    srcs = glob(include = ["nearby/presence/np_adv/src/**/*.rs"]),
    crate_features = ["alloc"],
    proc_macro_deps = ["@crate_index//:strum_macros"],
    deps = [
        ":array_view",
        ":crypto_provider",
        ":ldt",
        ":ldt_np_adv",
        ":np_ed25519",
        ":np_hkdf",
        ":sink",
        ":xts_aes",
        "@crate_index//:lazy_static",
        "@crate_index//:nom",
        "@crate_index//:strum",
        "@crate_index//:tinyvec",
        "@crate_index//:itertools"
    ],
)

rust_library(
    name = "np_ed25519",
    srcs = glob(
        include = ["nearby/presence/np_ed25519/src/**/*.rs"],
    ),
    deps = [
        ":array_view",
        ":crypto_provider",
        ":ldt",
        ":sink",
        ":xts_aes",
        "@crate_index//:tinyvec",
    ],
)

cc_binary(
    name = "main",
    srcs = ["nearby/presence/ldt_np_adv_ffi/c/sample/main.c"],
    deps = [":ldt_np_adv_ffi"],
)

rust_library(
    name = "ldt_np_adv_ffi_rust",
    srcs = glob(["nearby/presence/ldt_np_adv_ffi/src/**/*.rs"]),
    crate_features = ["std"],
    deps = [
        ":crypto_provider",
        ":crypto_provider_default",
        ":ldt",
        ":ldt_np_adv",
        ":np_hkdf",
        "@crate_index//:cfg-if",
        "@crate_index//:lazy_static",
    ],
)

rust_library(
    name = "ldt_np_adv",
    srcs = glob(["nearby/presence/ldt_np_adv/src/**/*.rs"]),
    deps = [
        ":array_view",
        ":crypto_provider",
        ":ldt",
        ":np_hkdf",
        ":xts_aes",
    ],
)

rust_library(
    name = "np_hkdf",
    srcs = glob(["nearby/presence/np_hkdf/src/**/*.rs"]),
    deps = [
        ":crypto_provider",
        ":ldt",
        ":xts_aes",
    ],
)

rust_library(
    name = "crypto_provider",
    srcs = glob(["nearby/crypto/crypto_provider/src/**/*.rs"]),
    crate_features = [
        "raw_private_key_permit",
        "std",
        "alloc",
    ],
    deps = ["@crate_index//:tinyvec"],
)

rust_library(
    name = "crypto_provider_default",
    srcs = glob(["nearby/crypto/crypto_provider_default/src/**/*.rs"]),
    crate_features = [
        "rustcrypto",
        "std",
    ],
    deps = [
        ":crypto_provider",
        ":crypto_provider_rustcrypto",
        "@crate_index//:cfg-if",
    ],
)

rust_library(
    name = "crypto_provider_rustcrypto",
    srcs = glob(["nearby/crypto/crypto_provider_rustcrypto/src/**/*.rs"]),
    crate_features = [
        "std",
        "alloc",
    ],
    deps = [
        ":crypto_provider",
        "@crate_index//:aead",
        "@crate_index//:aes",
        "@crate_index//:aes-gcm",
        "@crate_index//:aes-gcm-siv",
        "@crate_index//:cbc",
        "@crate_index//:cfg-if",
        "@crate_index//:ctr",
        "@crate_index//:ed25519-dalek",
        "@crate_index//:hkdf",
        "@crate_index//:hmac",
        "@crate_index//:p256",
        "@crate_index//:rand",
        "@crate_index//:rand_chacha",
        "@crate_index//:rand_core",
        "@crate_index//:sec1",
        "@crate_index//:sha2",
        "@crate_index//:subtle",
        "@crate_index//:x25519-dalek",
    ],
)

rust_library(
    name = "ldt_tbc",
    srcs = glob(["nearby/presence/ldt_tbc/src/**/*.rs"]),
    deps = [":crypto_provider"],
)

rust_library(
    name = "ldt",
    srcs = glob(["nearby/presence/ldt/src/**/*.rs"]),
    deps = [
        ":crypto_provider",
        ":ldt_tbc",
    ],
)

rust_library(
    name = "xts_aes",
    srcs = glob(["nearby/presence/xts_aes/src/**/*.rs"]),
    deps = [
        ":array_ref",
        ":crypto_provider",
        ":ldt_tbc",
    ],
)

rust_library(
    name = "array_view",
    srcs = glob(["nearby/presence/array_view/src/**/*.rs"]),
)

rust_library(
    name = "array_ref",
    srcs = glob(["nearby/presence/array_ref/src/**/*.rs"]),
)

rust_library(
    name = "sink",
    srcs = glob(["nearby/presence/sink/src/**/*.rs"]),
    deps = ["@crate_index//:tinyvec"],
)
