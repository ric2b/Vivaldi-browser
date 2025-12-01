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

load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = [
        "//fcp:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

proto_library(
    name = "dictionary_proto",
    srcs = ["dictionary.proto"],
)

cc_proto_library(
    name = "dictionary_cc_proto",
    deps = [":dictionary_proto"],
)

py_proto_library(
    name = "dictionary_py_pb2",
    deps = [":dictionary_proto"],
)

cc_library(
    name = "dictionary_lib",
    srcs = ["dictionary.cc"],
    hdrs = ["dictionary.h"],
    copts = FCP_COPTS,
    deps = [
        ":dictionary_cc_proto",
        "//fcp/base",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "dictionary_test",
    srcs = ["dictionary_test.cc"],
    visibility = ["//visibility:private"],
    deps = [
        ":dictionary_cc_proto",
        ":dictionary_lib",
        "//fcp/base",
        "//fcp/testing:parse_text_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)
