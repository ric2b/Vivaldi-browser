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

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_binary(
    name = "tracing_traits_generator",
    srcs = ["tracing_traits_generator.cc"],
    deps = [
        "//fcp/tracing:tracing_severity",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@flatbuffers",
    ],
)

cc_test(
    name = "tracing_traits_generator_test",
    srcs = ["tracing_traits_generator_test.cc"],
    args = ["--codegen_tool_path=$(location :test_codegen_runner.sh)"],
    copts = FCP_COPTS,
    data = glob([
        "testdata/*.baseline",
        "testdata/*.fbs",
    ]) + [
        ":test_codegen_runner.sh",
        ":tracing_traits_generator",
        "//fcp/tracing:tracing_schema_common.fbs",
        "@flatbuffers//:flatc",
    ],
    deps = [
        "//fcp/base",
        "//fcp/testing",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
