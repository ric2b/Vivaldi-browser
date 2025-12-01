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
#   Mocks for SecAgg.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "common_mocks",
    testonly = 1,
    srcs = [
        "ecdh_pregenerated_test_keys.cc",
    ],
    hdrs = [
        "ecdh_pregenerated_test_keys.h",
        "fake_prng.h",
    ],
    deps = [
        "//fcp/secagg/shared",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "client_mocks",
    testonly = 1,
    hdrs = [
        "mock_send_to_server_interface.h",
        "mock_state_transition_listener.h",
    ],
    copts = FCP_COPTS,
    deps = [
        "//fcp/secagg/client",
        "//fcp/secagg/client:state_transition_listener",
        "//fcp/secagg/shared:cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "testing",
    testonly = 1,
    srcs = [
        "test_matchers.cc",
    ],
    hdrs = [
        "test_matchers.h",
    ],
    copts = FCP_COPTS,
    deps = [
        "//fcp/secagg/shared",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)
