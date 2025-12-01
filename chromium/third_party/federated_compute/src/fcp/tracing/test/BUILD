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

load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")
load("//fcp/tracing:build_defs.bzl", "tracing_schema_cc_library")

package(
    licenses = ["notice"],  # Apache 2.0
)

tracing_schema_cc_library(
    name = "tracing_schema",
    srcs = ["tracing_schema.fbs"],
)

cc_test(
    name = "tracing_test",
    srcs = [
        "tracing_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":tracing_schema",
        "//fcp/tracing:test_tracing_recorder",
        "@com_google_googletest//:gtest_main",
        "@flatbuffers",
    ],
)

cc_test(
    name = "text_tracing_test",
    srcs = ["text_tracing_test.cc"],
    copts = FCP_COPTS,
    data = glob([
        "testdata/*.baseline",
    ]),
    deps = [
        ":tracing_schema",
        "//fcp/base",
        "//fcp/testing",
        "//fcp/tracing",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_googlesource_code_re2//:re2",
    ],
)

cc_test(
    name = "tracing_context_utils_test",
    srcs = ["tracing_context_utils_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":test_api_message_cc_proto",
        "//fcp/tracing:tracing_context_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "thread_local_tracing_recorder_test",
    srcs = ["thread_local_tracing_recorder_test.cc"],
    copts = FCP_COPTS,
    data = glob([
        "testdata/*.baseline",
    ]),
    deps = [
        ":tracing_schema",
        "//fcp/base",
        "//fcp/base:scheduler",
        "//fcp/testing",
        "//fcp/tracing",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_googlesource_code_re2//:re2",
    ],
)

proto_library(
    name = "test_api_message_proto",
    srcs = ["test_api_message.proto"],
)

cc_proto_library(
    name = "test_api_message_cc_proto",
    deps = [":test_api_message_proto"],
)
