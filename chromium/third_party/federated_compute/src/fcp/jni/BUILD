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

cc_library(
    name = "jni_util",
    hdrs = ["jni_util.h"],
    deps = [
        "//fcp/base",
        "@bazel_tools//tools/jdk:jni",
        "@com_google_absl//absl/cleanup",
        "@com_google_absl//absl/container:fixed_array",
    ],
)
