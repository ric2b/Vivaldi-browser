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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "server_mocks",
    testonly = 1,
    hdrs = [
        "mock_secagg_server_metrics_listener.h",
        "mock_send_to_clients_interface.h",
    ],
    copts = FCP_COPTS,
    deps = [
        "//fcp/secagg/server:secagg_server_metrics_listener",
        "//fcp/secagg/server:send_to_clients_interface",
        "//fcp/secagg/server:server_cc_proto",
        "//fcp/secagg/shared:cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "experiments",
    testonly = 1,
    hdrs = [
        "test_secagg_experiments.h",
    ],
    deps = [
        "//fcp/secagg/server:experiments_interface",
    ],
)

cc_library(
    name = "async_runner",
    testonly = 1,
    hdrs = [
        "test_async_runner.h",
    ],
    deps = [
        "//fcp/base:scheduler",
        "//fcp/secagg/server:secagg_scheduler",
    ],
)
