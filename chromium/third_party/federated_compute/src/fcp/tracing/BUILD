# Copyright 2019 Google LLC
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

load("@flatbuffers//:build_defs.bzl", "flatbuffer_cc_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "tracing_schema_common.fbs",
    "tracing_severity.h",
    "tracing_tag.h",
    "tracing_traits.h",
])

flatbuffer_cc_library(
    name = "tracing_schema_common",
    srcs = ["tracing_schema_common.fbs"],
    srcs_filegroup_name = "tracing_schema_common_fbs",
)

cc_library(
    name = "tracing",
    srcs = [
        "text_tracing_recorder_impl.cc",
        "text_tracing_span_impl.cc",
        "tracing_recorder_impl.cc",
        "tracing_span_id.cc",
        "tracing_span_impl.cc",
        "tracing_span_ref.cc",
        "tracing_traits.cc",
    ],
    hdrs = [
        "scoped_tracing_recorder.h",
        "text_tracing_recorder.h",
        "text_tracing_recorder_impl.h",
        "text_tracing_span_impl.h",
        "tracing_recorder.h",
        "tracing_recorder_impl.h",
        "tracing_span.h",
        "tracing_span_id.h",
        "tracing_span_impl.h",
        "tracing_span_ref.h",
        "tracing_tag.h",
        "tracing_traits.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":tracing_severity",
        "//fcp/base",
        "//fcp/base:error",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@flatbuffers",
    ],
)

cc_library(
    name = "test_tracing_recorder",
    testonly = True,
    srcs = [
        "test_tracing_recorder.cc",
        "test_tracing_recorder_impl.cc",
        "test_tracing_span_impl.cc",
    ],
    hdrs = [
        "test_tracing_recorder.h",
        "test_tracing_recorder_impl.h",
        "test_tracing_span_impl.h",
        "tracing_traits.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":tracing",
        ":tracing_severity",
        "//fcp/base:source_location",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
        "@flatbuffers",
    ],
)

cc_library(
    name = "tracing_severity",
    hdrs = ["tracing_severity.h"],
)

cc_library(
    name = "tracing_context_utils",
    srcs = ["tracing_context_utils.cc"],
    hdrs = ["tracing_context_utils.h"],
    deps = [
        ":tracing",
        "//fcp/base",
        "@com_google_protobuf//:protobuf",
        "@flatbuffers",
    ],
)

cc_test(
    name = "tracing_tag_test",
    srcs = [
        "tracing_tag_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":tracing",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_googletest//:gtest_main",
    ],
)
