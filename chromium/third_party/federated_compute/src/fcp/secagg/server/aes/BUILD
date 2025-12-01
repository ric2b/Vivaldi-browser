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
#   AES SecAgg server protocol implementation.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "aes",
    srcs = [
        "aes_secagg_server_protocol_impl.cc",
    ],
    hdrs = [
        "aes_secagg_server_protocol_impl.h",
    ],
    copts = FCP_COPTS,
    deps = [
        "//fcp/base",
        "//fcp/secagg/server:experiments_names",
        "//fcp/secagg/server:secagg_scheduler",
        "//fcp/secagg/server:secagg_server_protocol_impl",
        "//fcp/secagg/server:server_cc_proto",
        "//fcp/secagg/server:tracing_schema",
        "//fcp/secagg/shared",
        "//fcp/tracing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
    ],
)
