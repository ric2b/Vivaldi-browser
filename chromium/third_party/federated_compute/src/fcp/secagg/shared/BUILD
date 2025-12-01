# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Description:
#   SecAgg components shared between client and server.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

proto_library(
    name = "proto",
    srcs = ["secagg_messages.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "cc_proto",
    visibility = ["//visibility:public"],
    deps = [
        ":proto",
    ],
)

cc_library(
    name = "shared",
    srcs = [
        "aes_ctr_prng.cc",
        "aes_ctr_prng_factory.cc",
        "aes_gcm_encryption.cc",
        "aes_key.cc",
        "compute_session_id.cc",
        "crypto_rand_prng.cc",
        "ecdh_key_agreement.cc",
        "input_vector_specification.cc",
        "map_of_masks.cc",
        "secagg_vector.cc",
        "shamir_secret_sharing.cc",
    ],
    hdrs = [
        "aes_ctr_prng.h",
        "aes_ctr_prng_factory.h",
        "aes_gcm_encryption.h",
        "aes_key.h",
        "aes_prng_factory.h",
        "async_abort.h",
        "compute_session_id.h",
        "crypto_rand_prng.h",
        "ecdh_key_agreement.h",
        "ecdh_keys.h",
        "input_vector_specification.h",
        "key.h",
        "map_of_masks.h",
        "math.h",
        "prng.h",
        "secagg_vector.h",
        "shamir_secret_sharing.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":cc_proto",
        "//fcp/base",
        "@boringssl//:crypto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:endian",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "aes_gcm_encryption_test",
    size = "small",
    srcs = [
        "aes_gcm_encryption_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "aes_key_test",
    size = "small",
    srcs = [
        "aes_key_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "aes_prng_tests",
    size = "small",
    srcs = [
        "aes_ctr_prng_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "compute_session_id_test",
    size = "small",
    srcs = [
        "compute_session_id_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ecdh_test",
    size = "small",
    srcs = ["ecdh_key_agreement_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "//fcp/secagg/testing:common_mocks",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "input_vector_specification_test",
    size = "small",
    srcs = [
        "input_vector_specification_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "map_of_masks_test",
    size = "small",
    srcs = [
        "map_of_masks_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "map_of_masks_bench",
    size = "large",
    srcs = [
        "map_of_masks_bench.cc",
    ],
    copts = FCP_COPTS,
    linkstatic = 1,
    deps = [
        ":shared",
        "@com_google_absl//absl/numeric:bits",
        "@com_google_absl//absl/strings",
        "@com_google_benchmark//:benchmark_main",
    ],
)

cc_test(
    name = "math_test",
    size = "small",
    srcs = [
        "math_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "secagg_vector_test",
    size = "large",
    srcs = [
        "secagg_vector_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "secagg_vector_bench",
    size = "large",
    srcs = [
        "secagg_vector_bench.cc",
    ],
    copts = FCP_COPTS,
    linkstatic = 1,
    deps = [
        ":shared",
        "@com_google_benchmark//:benchmark_main",
    ],
)

cc_test(
    name = "shamir_secret_sharing_test",
    size = "small",
    srcs = [
        "shamir_secret_sharing_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "//fcp/secagg/testing:common_mocks",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "add_maps_bench",
    testonly = 1,
    srcs = [
        "add_maps_bench.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":shared",
        "@com_google_benchmark//:benchmark_main",
    ],
)
