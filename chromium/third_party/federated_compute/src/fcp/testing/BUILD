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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")
load("//fcp/tracing:build_defs.bzl", "tracing_schema_cc_library")

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

tracing_schema_cc_library(
    name = "tracing_schema",
    srcs = ["tracing_schema.fbs"],
)

cc_library(
    name = "result_matchers",
    testonly = True,
    srcs = [
    ],
    hdrs = [
        "result_matchers.h",
    ],
    copts = FCP_COPTS,
    deps = [
        "//fcp/base:error",
        "//fcp/base:result",
        "@com_google_googletest//:gtest",
    ],
)

cc_library(
    name = "parse_text_proto",
    testonly = 1,
    hdrs = ["parse_text_proto.h"],
    copts = FCP_COPTS,
    deps = [
        "//fcp/base",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "testing",
    testonly = 1,
    srcs = ["testing.cc"],
    hdrs = ["testing.h"],
    copts = FCP_COPTS,
    deps = [
        ":parse_text_proto",
        "//fcp/base",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "tracing",
    testonly = 1,
    srcs = ["tracing.cc"],
    hdrs = ["tracing.h"],
    copts = FCP_COPTS,
    deps = [
        ":tracing_schema",
        "//fcp/base:error",
        "//fcp/base:source_location",
        "//fcp/tracing",
    ],
)

cc_test(
    name = "result_matchers_test",
    srcs = [
        "result_matchers_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":result_matchers",
        ":testing",
        ":tracing",
        "//fcp/base:error",
        "//fcp/base:result",
        "//fcp/base:tracing_schema",
        "//fcp/tracing:test_tracing_recorder",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "testing_test",
    size = "small",
    srcs = ["testing_test.cc"],
    args = ["--baseline_path=$(location :testdata/verify_baseline_test.baseline)"],
    copts = FCP_COPTS,
    data = [":testdata/verify_baseline_test.baseline"],
    deps = [
        ":test_messages_cc_proto",
        ":testing",
        "//fcp/base",
        "@com_google_absl//absl/flags:flag",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "test_messages_proto",
    srcs = ["test_messages.proto"],
)

cc_proto_library(
    name = "test_messages_cc_proto",
    deps = [":test_messages_proto"],
)
