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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")
load("@org_tensorflow//tensorflow/core/platform:build_config.bzl", "tf_proto_library")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = [
        "//fcp:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

# Interfaces used by the engine & protocol. These require platform specific
# implementations.
cc_library(
    name = "interfaces",
    srcs = [],
    hdrs = [
        "event_publisher.h",
        "files.h",
        "flags.h",
        "log_manager.h",
        "secagg_event_publisher.h",
        "stats.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":diag_codes_cc_proto",
        ":histogram_counters_cc_proto",
        "//fcp/client/engine:engine_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "federated_protocol",
    hdrs = ["federated_protocol.h"],
    deps = [
        ":interfaces",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "//fcp/protos/confidentialcompute:payload_metadata_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "federated_protocol_util",
    srcs = ["federated_protocol_util.cc"],
    hdrs = ["federated_protocol_util.h"],
    deps = [
        ":diag_codes_cc_proto",
        "//fcp/base:time_util",
        "//fcp/protos:federated_api_cc_proto",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "simple_task_environment",
    srcs = ["simple_task_environment.cc"],
    hdrs = ["simple_task_environment.h"],
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = [
        ":selector_context_cc_proto",
        ":task_result_info_cc_proto",
        "//fcp/client/attestation:attestation_verifier",
        "//fcp/client/http:http_client",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos/confidentialcompute:access_policy_endorsement_options_cc_proto",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "simple_task_environment_test",
    srcs = ["simple_task_environment_test.cc"],
    deps = [
        ":test_helpers",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

bool_flag(
    name = "support_tfmobile",
    build_setting_default = True,
)

config_setting(
    name = "client_support_tfmobile",
    flag_values = {":support_tfmobile": "True"},
)

TF_OPTIONAL_DEPS = select({
    ":client_support_tfmobile": [
        "//fcp/client/engine:plan_engine",
    ],
    "//conditions:default": [],
})

TF_OPTIONAL_DEFINES = select({
    ":client_support_tfmobile": [
        "FCP_CLIENT_SUPPORT_TFMOBILE",
    ],
    "//conditions:default": [],
})

# This target only support example query plans, and it doesn't support TensorFlow based plans.
# For TensorFlow based plans, make sure to also link in
# //fcp/client/tensorflow:tensorflow_runner_impl_registration.
cc_library(
    name = "fl_runner",
    srcs = ["fl_runner.cc"],
    hdrs = [
        "fl_runner.h",
    ],
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = [
        ":diag_codes_cc_proto",
        ":eligibility_decider",
        ":example_iterator_query_recorder",
        ":federated_protocol",
        ":federated_select",
        ":fl_runner_cc_proto",
        ":fl_runner_tensorflow_spec_result_cc_proto",
        ":interfaces",
        ":interruptible_runner",
        ":parsing_utils",
        ":phase_logger",
        ":phase_logger_impl",
        ":runner_common",
        ":secagg_runner",
        ":selector_context_cc_proto",
        ":simple_task_environment",
        ":task_result_info_cc_proto",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:digest",
        "//fcp/base:random_token",
        "//fcp/client/cache:file_backed_resource_cache",
        "//fcp/client/cache:resource_cache",
        "//fcp/client/engine:common",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/engine:example_query_plan_engine",
        "//fcp/client/http:http_client",
        "//fcp/client/http:http_federated_protocol",
        "//fcp/client/opstats:opstats_example_store",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/client/opstats:opstats_logger_impl",
        "//fcp/client/opstats:opstats_utils",
        "//fcp/client/tensorflow:tensorflow_runner",
        "//fcp/client/tensorflow:tensorflow_runner_factory",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:opstats_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "//fcp/protos/confidentialcompute:payload_metadata_cc_proto",
        "@boringssl//:crypto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "fl_runner_test",
    srcs = ["fl_runner_test.cc"],
    deps = [
        ":client_runner",
        ":event_time_range_cc_proto",
        ":example_query_result_cc_proto",
        ":federated_protocol",
        ":fl_runner",
        ":fl_runner_cc_proto",
        ":interfaces",
        ":interruptible_runner",
        ":runner_common",
        ":selector_context_cc_proto",
        ":task_result_info_cc_proto",
        ":test_helpers",
        "//fcp/base:digest",
        "//fcp/base:function_registry",
        "//fcp/base:simulated_clock",
        "//fcp/client/cache:temp_files",
        "//fcp/client/engine:common",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/client/tensorflow:tensorflow_runner_factory",
        "//fcp/protos:data_type_cc_proto",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:opstats_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "//fcp/protos/confidentialcompute:payload_metadata_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/type:datetime_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
    ],
)

cc_library(
    name = "lc_runner",
    srcs = ["lc_runner.cc"],
    hdrs = [
        "lc_runner.h",
    ],
    copts = FCP_COPTS,
    defines = TF_OPTIONAL_DEFINES,
    visibility = ["//visibility:public"],
    deps = [
        ":diag_codes_cc_proto",
        ":example_iterator_query_recorder",
        ":interfaces",
        ":interruptible_runner",
        ":phase_logger",
        ":phase_logger_impl",
        ":runner_common",
        ":selector_context_cc_proto",
        ":simple_task_environment",
        "//fcp/base",
        "//fcp/client/engine:common",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/engine:plan_engine_helpers",
        "//fcp/client/engine:tflite_plan_engine",
        "//fcp/client/opstats:opstats_example_store",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/client/opstats:opstats_logger_impl",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core/platform:tstring",
    ] + TF_OPTIONAL_DEPS,
)

cc_library(
    name = "eligibility_decider",
    srcs = ["eligibility_decider.cc"],
    hdrs = ["eligibility_decider.h"],
    deps = [
        ":example_query_result_cc_proto",
        ":interfaces",
        ":phase_logger",
        ":selector_context_cc_proto",
        ":simple_task_environment",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:digest",
        "//fcp/base:time_util",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/opstats:opstats_utils",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:opstats_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
        "@com_googlesource_code_re2//:re2",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
    ],
)

cc_test(
    name = "eligibility_decider_test",
    srcs = ["eligibility_decider_test.cc"],
    deps = [
        ":diag_codes_cc_proto",
        ":eligibility_decider",
        ":test_helpers",
        "//fcp/base:simulated_clock",
        "//fcp/client/engine:common",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:opstats_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core/example:example_protos_cc",
    ],
)

cc_library(
    name = "phase_logger",
    hdrs = ["phase_logger.h"],
    deps = [
        ":interfaces",
        "//fcp/protos:federated_api_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "phase_logger_impl",
    srcs = ["phase_logger_impl.cc"],
    hdrs = ["phase_logger_impl.h"],
    deps = [
        ":diag_codes_cc_proto",
        ":histogram_counters_cc_proto",
        ":interfaces",
        ":phase_logger",
        "//fcp/base",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:opstats_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "phase_logger_impl_test",
    srcs = ["phase_logger_impl_test.cc"],
    deps = [
        ":diag_codes_cc_proto",
        ":histogram_counters_cc_proto",
        ":interfaces",
        ":phase_logger_impl",
        ":test_helpers",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:opstats_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "secagg_runner",
    srcs = ["secagg_runner.cc"],
    hdrs = ["secagg_runner.h"],
    deps = [
        ":diag_codes_cc_proto",
        ":federated_protocol",
        ":interfaces",
        ":interruptible_runner",
        "//fcp/base",
        "//fcp/secagg/client",
        "//fcp/secagg/client:state_transition_listener",
        "//fcp/secagg/shared",
        "//fcp/secagg/shared:cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "federated_select",
    srcs = ["federated_select.cc"],
    hdrs = ["federated_select.h"],
    deps = [
        ":diag_codes_cc_proto",
        ":interfaces",
        ":interruptible_runner",
        ":simple_task_environment",
        "//fcp/base:wall_clock_stopwatch",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/http:http_client",
        "//fcp/client/http:in_memory_request_response",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "parsing_utils",
    hdrs = ["parsing_utils.h"],
    deps = ["@com_google_absl//absl/strings:cord"],
)

cc_test(
    name = "federated_select_test",
    srcs = ["federated_select_test.cc"],
    deps = [
        ":client_runner",
        ":diag_codes_cc_proto",
        ":federated_select",
        ":interfaces",
        ":interruptible_runner",
        ":simple_task_environment",
        ":test_helpers",
        "//fcp/base",
        "//fcp/base:compression",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/http:http_client",
        "//fcp/client/http/testing:test_helpers",
        "//fcp/protos:plan_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "interruptible_runner",
    srcs = ["interruptible_runner.cc"],
    hdrs = ["interruptible_runner.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":diag_codes_cc_proto",
        ":interfaces",
        "//fcp/base",
        "//fcp/base:future",
        "//fcp/base:scheduler",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "runner_common",
    hdrs = ["runner_common.h"],
    deps = ["//fcp/client/engine:common"],
)

cc_library(
    name = "fake_log_manager",
    hdrs = ["fake_log_manager.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":diag_codes_cc_proto",
        ":histogram_counters_cc_proto",
        ":interfaces",
        "//fcp/base",
        "//fcp/client/engine:engine_cc_proto",
    ],
)

cc_library(
    name = "fake_event_publisher",
    hdrs = ["fake_event_publisher.h"],
    visibility = ["//visibility:public"],
    deps = [
        ":interfaces",
        "//fcp/base",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "interruptible_runner_test",
    srcs = ["interruptible_runner_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":diag_codes_cc_proto",
        ":interruptible_runner",
        ":test_helpers",
        "//fcp/base",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

# Misc. classes to support embedding a fake client in unit or integration tests.
cc_library(
    name = "client_runner",
    testonly = True,
    hdrs = ["client_runner.h"],
    deps = [
        ":client_runner_example_data_cc_proto",
        ":diag_codes_cc_proto",
        ":fake_event_publisher",
        ":histogram_counters_cc_proto",
        ":interfaces",
        ":selector_context_cc_proto",
        ":simple_task_environment",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/client/http:http_client",
        "//fcp/client/http/curl:curl_http_client",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_protobuf//:protobuf",
    ],
)

# A command line executable running most of the client side code to
# use for debugging and illustrating an example integration.
cc_library(
    name = "client_runner_main_lib",
    testonly = True,
    srcs = ["client_runner_main.cc"],
    deps = [
        ":client_runner",
        ":client_runner_example_data_cc_proto",
        ":fake_event_publisher",
        ":fl_runner",
        "//fcp/base",
        "//fcp/client/tensorflow:tensorflow_runner_impl_registration",
        "//fcp/tensorflow:external_dataset_op_lib",
        "//fcp/tensorflow:task_eligibility_info_ops_lib",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
    ],
)

cc_binary(
    name = "client_runner_main",
    testonly = True,
    deps = [
        ":client_runner_main_lib",
        "@org_tensorflow//tensorflow/core:tensorflow_opensource",
    ],
)

cc_library(
    name = "converters",
    srcs = ["converters.cc"],
    hdrs = ["converters.h"],
    copts = FCP_COPTS,
    deps = [
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor_cc_proto",
    ],
)

cc_test(
    name = "converters_test",
    srcs = ["converters_test.cc"],
    deps = [
        ":converters",
        ":example_query_result_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor_cc_proto",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/testing",
    ],
)

cc_library(
    name = "example_iterator_query_recorder",
    srcs = ["example_iterator_query_recorder.cc"],
    hdrs = ["example_iterator_query_recorder.h"],
    deps = [
        ":selector_context_cc_proto",
        ":task_result_info_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "example_iterator_query_recorder_test",
    srcs = ["example_iterator_query_recorder_test.cc"],
    deps = [
        ":example_iterator_query_recorder",
        ":selector_context_cc_proto",
        ":task_result_info_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "test_helpers",
    testonly = True,
    srcs = ["test_helpers.cc"],
    hdrs = ["test_helpers.h"],
    deps = [
        ":diag_codes_cc_proto",
        ":example_iterator_query_recorder",
        ":federated_protocol",
        ":federated_select",
        ":histogram_counters_cc_proto",
        ":interfaces",
        ":interruptible_runner",
        ":phase_logger",
        ":runner_common",
        ":secagg_runner",
        ":selector_context_cc_proto",
        ":simple_task_environment",
        ":task_result_info_cc_proto",
        "//fcp/base",
        "//fcp/client/attestation:attestation_verifier",
        "//fcp/client/engine:common",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/http:http_client",
        "//fcp/client/opstats:opstats_db",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/client/tensorflow:tensorflow_runner",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:opstats_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "//fcp/protos/confidentialcompute:payload_metadata_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "//fcp/secagg/shared:cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
    ],
)

# Protocol buffers for logging. Those get serialized.
proto_library(
    name = "histogram_counters_proto",
    srcs = ["histogram_counters.proto"],
)

java_proto_library(
    name = "histogram_counters_java_proto",
    deps = [":histogram_counters_proto"],
)

cc_proto_library(
    name = "histogram_counters_cc_proto",
    visibility = [
        "//assistant/ondevice/federated/plan_engine:__pkg__",
        "//fcp:internal",
    ],
    deps = [":histogram_counters_proto"],
)

# Protocol buffers for FL Runner. These do not get serialized to disk.
proto_library(
    name = "fl_runner_proto",
    srcs = ["fl_runner.proto"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_protobuf//:duration_proto",
    ],
)

java_proto_library(
    name = "fl_runner_java_proto",
    visibility = ["//visibility:public"],
    deps = [":fl_runner_proto"],
)

cc_proto_library(
    name = "fl_runner_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":fl_runner_proto"],
)

# Protocol buffers for testing FL Runner. These do not get serialized to disk.
tf_proto_library(
    name = "fl_runner_tensorflow_spec_result_proto",
    srcs = ["fl_runner_tensorflow_spec_result.proto"],
    protodeps = [
        "//fcp/client/engine:engine_proto",
        "@org_tensorflow//tensorflow/core:protos_all",
    ],
    visibility = ["//visibility:public"],
)

java_proto_library(
    name = "fl_runner_tensorflow_spec_result_java_proto",
    visibility = ["//visibility:public"],
    deps = [":fl_runner_tensorflow_spec_result_proto"],
)

# Allowing to refer to the cc library generated by the rule above in usual way:
alias(
    name = "fl_runner_tensorflow_spec_result_cc_proto",
    actual = "fl_runner_tensorflow_spec_result_proto_cc",
    visibility = ["//visibility:public"],
)

# Protocol buffers for logging. Those get serialized.
proto_library(
    name = "diag_codes_proto",
    srcs = ["diag_codes.proto"],
)

java_proto_library(
    name = "diag_codes_java_proto",
    deps = [":diag_codes_proto"],
)

cc_proto_library(
    name = "diag_codes_cc_proto",
    visibility = [
        "//assistant/ondevice/federated/plan_engine:__pkg__",
        "//fcp:internal",
    ],
    deps = [":diag_codes_proto"],
)

# Protocol buffers for providing example data to client_runner_main.
proto_library(
    name = "client_runner_example_data_proto",
    testonly = True,
    srcs = ["client_runner_example_data.proto"],
)

cc_proto_library(
    name = "client_runner_example_data_cc_proto",
    testonly = True,
    deps = [":client_runner_example_data_proto"],
)

py_proto_library(
    name = "client_runner_example_data_py_pb2",
    testonly = True,
    deps = [":client_runner_example_data_proto"],
)

# --------------------------------------------------------------------
# selector_context.proto

proto_library(
    name = "selector_context_proto",
    srcs = ["selector_context.proto"],
    deps = [
        "@com_google_protobuf//:any_proto",
        "@com_google_protobuf//:timestamp_proto",
    ],
)

cc_proto_library(
    name = "selector_context_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":selector_context_proto"],
)

java_proto_library(
    name = "selector_context_java_proto",
    visibility = ["//visibility:public"],
    deps = [":selector_context_proto"],
)

# --------------------------------------------------------------------
# example_query_result.proto

proto_library(
    name = "example_query_result_proto",
    srcs = ["example_query_result.proto"],
    visibility = ["//visibility:public"],
    deps = [
        ":event_time_range_proto",
    ],
)

java_proto_library(
    name = "example_query_result_java_proto",
    deps = [":example_query_result_proto"],
)

cc_proto_library(
    name = "example_query_result_cc_proto",
    visibility = ["//visibility:public"],
    deps = [":example_query_result_proto"],
)

# --------------------------------------------------------------------
# event_time_range.proto

proto_library(
    name = "event_time_range_proto",
    srcs = ["event_time_range.proto"],
    deps = ["@com_google_googleapis//google/type:datetime_proto"],
)

cc_proto_library(
    name = "event_time_range_cc_proto",
    deps = [":event_time_range_proto"],
)

# --------------------------------------------------------------------
# task_result_info.proto

proto_library(
    name = "task_result_info_proto",
    srcs = ["task_result_info.proto"],
    deps = [
        ":selector_context_proto",
        "@com_google_protobuf//:any_proto",
    ],
)

cc_proto_library(
    name = "task_result_info_cc_proto",
    deps = [":task_result_info_proto"],
)
