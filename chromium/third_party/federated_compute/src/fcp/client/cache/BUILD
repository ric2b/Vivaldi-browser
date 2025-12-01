# Copyright 2022 Google LLC
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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(
    default_visibility = [
        "//fcp:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "temp_files",
    srcs = ["temp_files.cc"],
    hdrs = ["temp_files.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:interfaces",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "temp_files_test",
    srcs = ["temp_files_test.cc"],
    deps = [
        ":temp_files",
        "//fcp/base",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:test_helpers",
        "//fcp/testing",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "cache_manifest_proto",
    srcs = ["cache_manifest.proto"],
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "cache_manifest_cc_proto",
    deps = [":cache_manifest_proto"],
)

cc_library(
    name = "resource_cache",
    hdrs = ["resource_cache.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "file_backed_resource_cache",
    srcs = [
        "file_backed_resource_cache.cc",
    ],
    hdrs = [
        "file_backed_resource_cache.h",
    ],
    deps = [
        ":cache_manifest_cc_proto",
        ":resource_cache",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:time_util",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:interfaces",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@protodatastore_cpp//protostore:file-storage",
        "@protodatastore_cpp//protostore:proto-data-store",
    ],
)

cc_test(
    name = "file_backed_resource_cache_test",
    srcs = ["file_backed_resource_cache_test.cc"],
    deps = [
        ":file_backed_resource_cache",
        "//fcp/base",
        "//fcp/base:simulated_clock",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:selector_context_cc_proto",
        "//fcp/client:test_helpers",
        "//fcp/testing",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_helpers",
    testonly = 1,
    hdrs = ["test_helpers.h"],
    deps = [
        ":resource_cache",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
