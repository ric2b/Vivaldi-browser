# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(
    default_visibility = [
        "//fcp:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "attestation_verifier",
    srcs = ["attestation_verifier.cc"],
    hdrs = ["attestation_verifier.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//fcp/base:digest",
        "//fcp/confidentialcompute:cose",
        "//fcp/protos/confidentialcompute:signed_endorsements_cc_proto",
        "//fcp/protos/federatedcompute:confidential_encryption_config_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "log_attestation_records",
    srcs = ["log_attestation_records.cc"],
    hdrs = ["log_attestation_records.h"],
    deps = [
        "//fcp/base",
        "//fcp/base:compression",
        "//fcp/protos/confidentialcompute:access_policy_cc_proto",
        "//fcp/protos/confidentialcompute:verification_record_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@oak//proto/attestation:endorsement_cc_proto",
        "@oak//proto/attestation:evidence_cc_proto",
        "@oak//proto/attestation:reference_value_cc_proto",
        "@oak//proto/attestation:verification_cc_proto",
    ],
)

cc_test(
    name = "log_attestation_records_test",
    srcs = ["log_attestation_records_test.cc"],
    deps = [
        ":log_attestation_records",
        ":test_values",
        "//fcp/base:compression",
        "//fcp/client:parsing_utils",
        "//fcp/protos/confidentialcompute:access_policy_cc_proto",
        "//fcp/protos/confidentialcompute:verification_record_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
        "@oak//proto/attestation:endorsement_cc_proto",
        "@oak//proto/attestation:evidence_cc_proto",
    ],
)

cc_library(
    name = "extract_attestation_records_lib",
    srcs = ["extract_attestation_records.cc"],
    hdrs = ["extract_attestation_records.h"],
    deps = [
        "//fcp/base",
        "//fcp/base:compression",
        "//fcp/base:digest",
        "//fcp/client:parsing_utils",
        "//fcp/protos/confidentialcompute:verification_record_cc_proto",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "extract_attestation_records_test",
    srcs = ["extract_attestation_records_test.cc"],
    deps = [
        ":extract_attestation_records_lib",
        ":log_attestation_records",
        ":test_values",
        "//fcp/client:parsing_utils",
        "//fcp/protos/confidentialcompute:access_policy_cc_proto",
        "//fcp/protos/confidentialcompute:verification_record_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/log:log_entry",
        "@com_google_absl//absl/log:log_sink",
        "@com_google_absl//absl/log:log_sink_registry",
        "@com_google_googletest//:gtest_main",
        "@oak//proto/attestation:endorsement_cc_proto",
        "@oak//proto/attestation:evidence_cc_proto",
    ],
)

# A command line executable for extracting serialized attestation records (as
# output by `oak_rust_attestation_verifier.cc`) from a stream of text.
cc_binary(
    name = "extract_attestation_records",
    srcs = ["extract_attestation_records_main.cc"],
    deps = [
        ":extract_attestation_records_lib",
        "//fcp/base",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "test_values",
    testonly = 1,
    srcs = ["test_values.cc"],
    hdrs = ["test_values.h"],
    deps = [
        "//fcp/protos/federatedcompute:confidential_encryption_config_cc_proto",
        "//fcp/testing",
        "@oak//proto/attestation:reference_value_cc_proto",
    ],
)
