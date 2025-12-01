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

default_visibility = [
    "//fcp:internal",
]

package(
    default_visibility = default_visibility,
    licenses = ["notice"],  # Apache 2.0
)

# --------------------------------------------------------------------

# The protos used to pass data across the JNI boundary.
proto_library(
    name = "jni_proto",
    srcs = ["jni.proto"],
)

java_proto_library(
    name = "jni_java_proto",
    deps = [":jni_proto"],
)

cc_proto_library(
    name = "jni_cc_proto",
    deps = [":jni_proto"],
)

cc_library(
    name = "java_http_client",
    srcs = ["java_http_client.cc"],
    hdrs = ["java_http_client.h"],
    deps = [
        ":jni_cc_proto",
        "//fcp/base",
        "//fcp/client/http:http_client",
        "//fcp/client/http:http_client_util",
        "//fcp/jni:jni_util",
        "@bazel_tools//tools/jdk:jni",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
    alwayslink = 1,
)
