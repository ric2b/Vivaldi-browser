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
#   SecAgg server-specific components.

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
    name = "experiments_interface",
    hdrs = ["experiments_interface.h"],
    copts = FCP_COPTS,
    deps = ["@com_google_absl//absl/strings"],
)

cc_library(
    name = "experiments_names",
    hdrs = ["experiments_names.h"],
    copts = FCP_COPTS,
)

cc_library(
    name = "secagg_server_protocol_impl",
    srcs = ["secagg_server_protocol_impl.cc"],
    hdrs = ["secagg_server_protocol_impl.h"],
    copts = FCP_COPTS,
    deps = [
        ":experiments_interface",
        ":secagg_scheduler",
        ":secagg_server_metrics_listener",
        ":secret_sharing_graph",
        ":send_to_clients_interface",
        ":server_cc_proto",
        ":tracing_schema",
        "//fcp/base",
        "//fcp/secagg/shared",
        "//fcp/secagg/shared:cc_proto",
        "//fcp/tracing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "secagg_server_metrics_listener",
    hdrs = [
        "secagg_server_metrics_listener.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":server_cc_proto",
        "//fcp/secagg/shared:cc_proto",
    ],
)

cc_library(
    name = "send_to_clients_interface",
    hdrs = [
        "send_to_clients_interface.h",
    ],
    copts = FCP_COPTS,
    deps = [
        "//fcp/secagg/shared:cc_proto",
    ],
)

cc_library(
    name = "server",
    srcs = [
        "secagg_server.cc",
        "secagg_server_aborted_state.cc",
        "secagg_server_completed_state.cc",
        "secagg_server_prng_running_state.cc",
        "secagg_server_r0_advertise_keys_state.cc",
        "secagg_server_r1_share_keys_state.cc",
        "secagg_server_r2_masked_input_coll_state.cc",
        "secagg_server_r3_unmasking_state.cc",
        "secagg_server_state.cc",
        "secagg_trace_utility.cc",
    ],
    hdrs = [
        "secagg_server.h",
        "secagg_server_aborted_state.h",
        "secagg_server_completed_state.h",
        "secagg_server_prng_running_state.h",
        "secagg_server_r0_advertise_keys_state.h",
        "secagg_server_r1_share_keys_state.h",
        "secagg_server_r2_masked_input_coll_state.h",
        "secagg_server_r3_unmasking_state.h",
        "secagg_server_state.h",
        "secagg_trace_utility.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":experiments_interface",
        ":experiments_names",
        ":graph_parameter_finder",
        ":secagg_scheduler",
        ":secagg_server_metrics_listener",
        ":secagg_server_protocol_impl",
        ":secret_sharing_graph",
        ":secret_sharing_graph_factory",
        ":send_to_clients_interface",
        ":server_cc_proto",
        ":tracing_schema",
        "//fcp/base",
        "//fcp/base:scheduler",
        "//fcp/secagg/server/aes",
        "//fcp/secagg/shared",
        "//fcp/secagg/shared:cc_proto",
        "//fcp/tracing",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "secret_sharing_graph_factory",
    hdrs = ["secret_sharing_graph_factory.h"],
    deps = [
        ":secret_sharing_complete_graph",
        ":secret_sharing_graph",
        ":secret_sharing_harary_graph",
        "//fcp/base",
    ],
)

cc_library(
    name = "secret_sharing_graph",
    hdrs = ["secret_sharing_graph.h"],
    deps = ["//fcp/base"],
)

cc_library(
    name = "secret_sharing_complete_graph",
    hdrs = ["secret_sharing_complete_graph.h"],
    deps = [
        ":secret_sharing_graph",
        "//fcp/base",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "secret_sharing_harary_graph",
    srcs = [
        "secret_sharing_harary_graph.cc",
        "ssl_bit_gen.cc",
    ],
    hdrs = [
        "secret_sharing_harary_graph.h",
        "ssl_bit_gen.h",
    ],
    deps = [
        ":secret_sharing_graph",
        "//fcp/base",
        "@boringssl//:crypto",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "graph_parameter_finder",
    srcs = ["graph_parameter_finder.cc"],
    hdrs = [
        "graph_parameter_finder.h",
    ],
    deps = [
        ":distribution_utilities",
        ":server_cc_proto",
        "//fcp/base",
    ],
)

cc_library(
    name = "distribution_utilities",
    srcs = ["distribution_utilities.cc"],
    hdrs = [
        "distribution_utilities.h",
    ],
    deps = ["//fcp/base"],
)

cc_test(
    name = "distribution_utilities_test",
    srcs = ["distribution_utilities_test.cc"],
    deps = [
        ":distribution_utilities",
        "//fcp/base",
        "//fcp/testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "server-test",
    size = "small",
    srcs = [
        "secagg_server_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":server",
        ":server_cc_proto",
        ":tracing_schema",
        "//fcp/base",
        "//fcp/secagg/shared",
        "//fcp/secagg/shared:cc_proto",
        "//fcp/secagg/testing:common_mocks",
        "//fcp/secagg/testing/server:experiments",
        "//fcp/secagg/testing/server:server_mocks",
        "//fcp/testing",
        "//fcp/testing:parse_text_proto",
        "//fcp/tracing:test_tracing_recorder",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "state-test",
    size = "small",
    srcs = [
        "secagg_server_aborted_state_test.cc",
        "secagg_server_completed_state_test.cc",
        "secagg_server_prng_running_state_test.cc",
        "secagg_server_r0_advertise_keys_state_test.cc",
        "secagg_server_r1_share_keys_state_test.cc",
        "secagg_server_r2_masked_input_coll_state_test.cc",
        "secagg_server_r3_unmasking_state_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":experiments_interface",
        ":experiments_names",
        ":secagg_scheduler",
        ":secret_sharing_graph_factory",
        ":send_to_clients_interface",
        ":server",
        ":server_cc_proto",
        "//fcp/base",
        "//fcp/base:scheduler",
        "//fcp/secagg/server/aes",
        "//fcp/secagg/shared",
        "//fcp/secagg/shared:cc_proto",
        "//fcp/secagg/testing",
        "//fcp/secagg/testing:common_mocks",
        "//fcp/secagg/testing/server:async_runner",
        "//fcp/secagg/testing/server:experiments",
        "//fcp/secagg/testing/server:server_mocks",
        "//fcp/testing",
        "//fcp/tracing:test_tracing_recorder",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "secret_sharing_harary_graph_test",
    srcs = ["secret_sharing_harary_graph_test.cc"],
    deps = [
        ":secret_sharing_graph",
        ":secret_sharing_graph_factory",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "secret_sharing_complete_graph_test",
    srcs = ["secret_sharing_complete_graph_test.cc"],
    deps = [
        ":secret_sharing_graph",
        ":secret_sharing_graph_factory",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "graph_parameter_finder_test",
    srcs = ["graph_parameter_finder_test.cc"],
    deps = [
        ":graph_parameter_finder",
        ":server_cc_proto",
        "//fcp/testing",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "secagg_scheduler",
    srcs = [
        "secagg_scheduler.cc",
    ],
    hdrs = [
        "secagg_scheduler.h",
    ],
    deps = [
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:reentrancy_guard",
        "//fcp/base:scheduler",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "secagg_scheduler_test",
    srcs = ["secagg_scheduler_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":secagg_scheduler",
        "//fcp/base",
        "//fcp/base:scheduler",
        "//fcp/base:simulated_clock",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "server_proto",
    srcs = [
        "secagg_server_enums.proto",
        "secagg_server_messages.proto",
    ],
    visibility = ["//visibility:public"],
)

java_proto_library(
    name = "server_java_proto",
    deps = [":server_proto"],
)

cc_proto_library(
    name = "server_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":server_proto"],
)
