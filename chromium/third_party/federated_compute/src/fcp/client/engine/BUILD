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

load("@org_tensorflow//tensorflow/core/platform:build_config.bzl", "tf_proto_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")

default_visibility = [
    "//fcp:internal",
]

package(
    default_visibility = default_visibility,
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "plan_engine",
    srcs = [
        "simple_plan_engine.cc",
    ],
    hdrs = [
        "simple_plan_engine.h",
    ],
    copts = FCP_COPTS,
    visibility = default_visibility,
    deps = [
        ":common",
        ":example_iterator_factory",
        ":plan_engine_helpers",
        ":tensorflow_utils",
        ":tf_wrapper",
        "//fcp/base",
        "//fcp/client:example_iterator_query_recorder",
        "//fcp/client:histogram_counters_cc_proto",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/protos:plan_cc_proto",
        "//fcp/tensorflow:host_object",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
    ],
)

cc_library(
    name = "privacy_id_utils",
    srcs = [
        "privacy_id_utils.cc",
    ],
    hdrs = [
        "privacy_id_utils.h",
    ],
    copts = FCP_COPTS,
    visibility = default_visibility,
    deps = [
        "//fcp/base",
        "//fcp/base:digest",
        "//fcp/client:event_time_range_cc_proto",
        "//fcp/client:example_query_result_cc_proto",
        "//fcp/confidentialcompute:constants",
        "//fcp/confidentialcompute:time_window_utilities",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos/confidentialcompute:windowing_schedule_proto_cc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/type:datetime_cc_proto",
    ],
)

cc_test(
    name = "privacy_id_utils_test",
    srcs = [
        "privacy_id_utils_test.cc",
    ],
    copts = FCP_COPTS,
    visibility = default_visibility,
    deps = [
        ":privacy_id_utils",
        "//fcp/client:event_time_range_cc_proto",
        "//fcp/client:example_query_result_cc_proto",
        "//fcp/confidentialcompute:constants",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos/confidentialcompute:windowing_schedule_proto_cc",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/type:datetime_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "example_query_plan_engine",
    srcs = [
        "example_query_plan_engine.cc",
    ],
    hdrs = [
        "example_query_plan_engine.h",
    ],
    copts = FCP_COPTS,
    visibility = default_visibility,
    deps = [
        ":common",
        ":example_iterator_factory",
        ":privacy_id_utils",
        "//fcp/base",
        "//fcp/client:converters",
        "//fcp/client:event_time_range_cc_proto",
        "//fcp/client:example_iterator_query_recorder",
        "//fcp/client:example_query_result_cc_proto",
        "//fcp/client:simple_task_environment",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/client/tensorflow:tensorflow_runner",
        "//fcp/confidentialcompute:constants",
        "//fcp/protos:data_type_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/type:datetime_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor_cc_proto",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol:checkpoint_builder",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol:federated_compute_checkpoint_builder",
    ],
)

cc_test(
    name = "example_query_plan_engine_test",
    srcs = ["example_query_plan_engine_test.cc"],
    deps = [
        ":common",
        ":example_iterator_factory",
        ":example_query_plan_engine",
        "//fcp/client:client_runner",
        "//fcp/client:event_time_range_cc_proto",
        "//fcp/client:example_query_result_cc_proto",
        "//fcp/client:simple_task_environment",
        "//fcp/client:test_helpers",
        "//fcp/client/tensorflow:tensorflow_runner",
        "//fcp/client/tensorflow:tensorflow_runner_impl",
        "//fcp/confidentialcompute:constants",
        "//fcp/protos:data_type_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos/confidentialcompute:windowing_schedule_proto_cc",
        "//fcp/tensorflow:external_dataset_op_lib",
        "//fcp/testing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/functional:any_invocable",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googleapis//google/type:datetime_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/c:checkpoint_reader",
        "@org_tensorflow//tensorflow/c:tf_status_headers",
        "@org_tensorflow//tensorflow/c:tf_status_helper",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core/platform:tstring",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor_cc_proto",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol:checkpoint_header",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/testing:test_data",
    ],
)

# A plan-engine independent wrapper around TF that supports cancellation.
cc_library(
    name = "tf_wrapper",
    srcs = ["tf_wrapper.cc"],
    hdrs = ["tf_wrapper.h"],
    copts = FCP_COPTS,
    visibility = ["//visibility:private"],
    deps = [
        ":plan_engine_helpers",
        "//fcp/base",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core:core_cpu",
        "@org_tensorflow//tensorflow/core:framework",
    ],
)

cc_test(
    name = "tf_wrapper_test",
    srcs = ["tf_wrapper_test.cc"],
    deps = [
        ":tf_wrapper",
        "//fcp/base",
        "//fcp/testing",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
    ],
)

cc_library(
    name = "plan_engine_helpers",
    srcs = ["plan_engine_helpers.cc"],
    hdrs = ["plan_engine_helpers.h"],
    copts = FCP_COPTS,
    visibility = default_visibility,
    deps = [
        ":common",
        ":example_iterator_factory",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:example_iterator_query_recorder",
        "//fcp/client:simple_task_environment",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/protos:plan_cc_proto",
        "//fcp/tensorflow:external_dataset",
        "//fcp/tensorflow:host_object",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core/platform:tstring",
    ],
)

cc_library(
    name = "tflite_wrapper",
    srcs = ["tflite_wrapper.cc"],
    hdrs = ["tflite_wrapper.h"],
    deps = [
        ":caching_error_reporter",
        "//fcp/base",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite:framework_experimental",
        "@org_tensorflow//tensorflow/lite:string_util",
        "@org_tensorflow//tensorflow/lite/c:c_api_types",
        "@org_tensorflow//tensorflow/lite/delegates/flex:delegate_only_runtime",
        "@org_tensorflow//tensorflow/lite/delegates/flex:util",
        "@org_tensorflow//tensorflow/lite/delegates/utils:simple_delegate",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
    ],
)

cc_test(
    name = "tflite_wrapper_test",
    srcs = ["tflite_wrapper_test.cc"],
    data = [
        "//fcp/client/engine/data:join_model.flatbuffer",
        "//fcp/client/engine/data:length_model.flatbuffer",
    ],
    deps = [
        ":tflite_wrapper",
        "//fcp/base",
        "//fcp/client:interruptible_runner",
        "//fcp/client:test_helpers",
        "//fcp/testing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow//tensorflow/core/kernels:string_join_op",
        "@org_tensorflow//tensorflow/core/ops:string_ops_op_lib",
        "@org_tensorflow//tensorflow/core/platform:tstring",
    ],
)

cc_library(
    name = "tflite_plan_engine",
    srcs = [
        "tflite_plan_engine.cc",
    ],
    hdrs = [
        "tflite_plan_engine.h",
    ],
    copts = FCP_COPTS,
    deps = [
        ":common",
        ":example_iterator_factory",
        ":plan_engine_helpers",
        ":tensorflow_utils",
        ":tflite_wrapper",
        "//fcp/base",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:example_iterator_query_recorder",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/protos:plan_cc_proto",
        "//fcp/tensorflow:host_object",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core/platform:tstring",
    ],
)

cc_library(
    name = "caching_error_reporter",
    srcs = ["caching_error_reporter.cc"],
    hdrs = ["caching_error_reporter.h"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/synchronization",
        "@org_tensorflow//tensorflow/lite/core/api:error_reporter",
    ],
)

cc_test(
    name = "caching_error_reporter_test",
    srcs = ["caching_error_reporter_test.cc"],
    deps = [
        ":caching_error_reporter",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow//tensorflow/lite/core/api:error_reporter",
    ],
)

cc_library(
    name = "common",
    srcs = ["common.cc"],
    hdrs = ["common.h"],
    deps = [
        ":engine_cc_proto",
        ":example_iterator_factory",
        "//fcp/base",
        "//fcp/client:example_iterator_query_recorder",
        "//fcp/client:federated_protocol",
        "//fcp/client:interfaces",
        "//fcp/client:simple_task_environment",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/protos:data_type_cc_proto",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos/confidentialcompute:payload_metadata_cc_proto",
        "//fcp/tensorflow:external_dataset",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
    ],
)

cc_library(
    name = "example_iterator_factory",
    hdrs = ["example_iterator_factory.h"],
    copts = FCP_COPTS,
    visibility = default_visibility,
    deps = [
        "//fcp/client:selector_context_cc_proto",
        "//fcp/client:simple_task_environment",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "tensorflow_utils",
    srcs = ["tensorflow_utils.cc"],
    hdrs = ["tensorflow_utils.h"],
    deps = [
        "//fcp/client:federated_protocol",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core/platform:tstring",
    ],
)

cc_test(
    name = "tensorflow_utils_test",
    srcs = ["tensorflow_utils_test.cc"],
    deps = [
        ":tensorflow_utils",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/status",
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core/platform:tstring",
    ],
)

# Runtime protos. Those do not typically get serialized.
tf_proto_library(
    name = "engine_proto",
    srcs = ["engine.proto"],
    visibility = ["//visibility:public"],
)

java_proto_library(
    name = "engine_java_proto",
    deps = [":engine_proto"],
)

# Allowing to refer to the cc library generated by the rule above in usual way:
alias(
    name = "engine_cc_proto",
    actual = "engine_proto_cc",
    visibility = default_visibility,
)
