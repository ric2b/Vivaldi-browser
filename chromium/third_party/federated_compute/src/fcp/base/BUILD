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
# Base component, containing common functionality used by other FCP components.

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

# Used to detect when we're building for Android, using select().
config_setting(
    name = "android",
    constraint_values = [
        "@platforms//os:android",
    ],
)

cc_library(
    name = "base",
    srcs = [
        "base_name.cc",
        "monitoring.cc",
        "platform.cc",
    ],
    hdrs = [
        "base_name.h",
        "monitoring.h",
        "move_to_lambda.h",
        "platform.h",
    ],
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_library(
    name = "bounds",
    srcs = [
    ],
    hdrs = [
        "bounds.h",
    ],
    copts = FCP_COPTS,
    deps = [":base"],
)

cc_test(
    name = "bounds_test",
    srcs = [
        "bounds_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":bounds",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "meta",
    srcs = [
    ],
    hdrs = [
        "meta.h",
    ],
    copts = FCP_COPTS,
    deps = [":base"],
)

cc_test(
    name = "meta_test",
    srcs = [
        "meta_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":meta",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "reentrancy_guard",
    hdrs = [
        "reentrancy_guard.h",
    ],
    copts = FCP_COPTS,
    deps = [":base"],
)

cc_test(
    name = "reentrancy_guard_test",
    srcs = [
        "reentrancy_guard_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        ":reentrancy_guard",
        ":scheduler",
        "//fcp/testing",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "random_token",
    srcs = [
        "random_token.cc",
    ],
    hdrs = [
        "random_token.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "random_token_test",
    srcs = [
        "random_token_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":random_token",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/hash:hash_testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "future",
    srcs = [
        "future.cc",
    ],
    hdrs = [
        "future.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        ":meta",
        ":scheduler",
        ":unique_value",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "future_test",
    srcs = ["future_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":base",
        ":future",
        ":meta",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "monitoring_test",
    size = "small",
    srcs = [
        "monitoring_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "platform_test",
    size = "small",
    srcs = [
        "platform_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        "//fcp/testing",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "scheduler",
    srcs = [
        "scheduler.cc",
    ],
    hdrs = [
        "scheduler.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "scheduler_test",
    size = "small",
    srcs = [
        "scheduler_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        ":scheduler",
        "//fcp/testing",
        "@com_google_absl//absl/synchronization",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "move_to_lambda_test",
    size = "small",
    srcs = [
        "move_to_lambda_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        ":unique_value",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "unique_value",
    srcs = [
    ],
    hdrs = [
        "unique_value.h",
    ],
    copts = FCP_COPTS,
)

cc_test(
    name = "unique_value_test",
    srcs = [
        "unique_value_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        ":unique_value",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "error",
    hdrs = [
        "error.h",
    ],
    copts = FCP_COPTS,
)

cc_library(
    name = "result",
    srcs = ["result.cc"],
    hdrs = [
        "result.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":error",
        ":meta",
        ":source_location",
        ":tracing_schema",
        "//fcp/tracing",
    ],
)

cc_library(
    name = "status_converters",
    srcs = ["status_converters.cc"],
    hdrs = ["status_converters.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "result_test",
    srcs = [
        "result_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":result",
        ":tracing_schema",
        ":unique_value",
        "//fcp/testing",
        "//fcp/testing:result_matchers",
        "//fcp/testing:tracing",
        "//fcp/tracing:test_tracing_recorder",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "source_location",
    srcs = [
    ],
    hdrs = [
        "source_location.h",
    ],
    copts = FCP_COPTS,
)

cc_test(
    name = "source_location_test",
    srcs = [
        "source_location_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":source_location",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "match",
    srcs = [
    ],
    hdrs = [
        "match.h",
    ],
    copts = FCP_COPTS,
    deps = [":meta"],
)

cc_test(
    name = "match_test",
    srcs = [
        "match_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":match",
        ":result",
        "//fcp/testing",
        "//fcp/testing:result_matchers",
        "//fcp/testing:tracing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "process_unique_id",
    srcs = ["process_unique_id.cc"],
    hdrs = ["process_unique_id.h"],
)

cc_test(
    name = "process_unique_id_test",
    srcs = ["process_unique_id_test.cc"],
    deps = [
        ":process_unique_id",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "clock",
    srcs = [
        "clock.cc",
    ],
    hdrs = [
        "clock.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":base",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "simulated_clock",
    testonly = 1,
    srcs = [
        "simulated_clock.cc",
    ],
    hdrs = [
        "simulated_clock.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":clock",
    ],
)

cc_test(
    name = "simulated_clock_test",
    srcs = [
        "simulated_clock_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":simulated_clock",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "realtime_clock_test",
    srcs = [
        "realtime_clock_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":clock",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "time_util",
    srcs = ["time_util.cc"],
    hdrs = ["time_util.h"],
    deps = [
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "time_util_test",
    srcs = ["time_util_test.cc"],
    deps = [
        ":time_util",
        "//fcp/testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "wall_clock_stopwatch",
    srcs = ["wall_clock_stopwatch.cc"],
    hdrs = ["wall_clock_stopwatch.h"],
    copts = FCP_COPTS,
    deps = [
        ":base",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "wall_clock_stopwatch_test",
    srcs = ["wall_clock_stopwatch_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":clock",
        ":scheduler",
        ":wall_clock_stopwatch",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "digest",
    srcs = [
        "digest.cc",
    ],
    hdrs = [
        "digest.h",
    ],
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = [
        ":base",
        "@boringssl//:crypto",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

cc_test(
    name = "digest_test",
    srcs = ["digest_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":digest",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "compression",
    srcs = [
        "compression.cc",
    ],
    hdrs = [
        "compression.h",
    ],
    copts = FCP_COPTS,
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "compression_test",
    srcs = ["compression_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":compression",
        "//fcp/testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "function_registry",
    hdrs = ["function_registry.h"],
    deps = [
        ":base",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "function_registry_test",
    srcs = ["function_registry_test.cc"],
    deps = [
        ":function_registry",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
