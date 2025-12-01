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

load("@rules_python//python:defs.bzl", "py_library", "py_test")

# The public interface for this package, providing the various Federated Program platform
# components needed to run a Federated Program using Federated Compute clients.
py_library(
    name = "demo",
    srcs = ["__init__.py"],
    visibility = ["//visibility:public"],
    deps = [
        ":federated_computation",
        ":federated_context",
        ":federated_data_source",
    ],
)

py_library(
    name = "aggregations",
    srcs = ["aggregations.py"],
    data = ["@pybind11_abseil//pybind11_abseil:status.so"],
    deps = [
        ":http_actions",
        ":media",
        "//fcp/protos:plan_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@com_google_googleapis//google/longrunning:operations_py_proto",
        "@com_google_googleapis//google/rpc:code_py_proto",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/core:tensor_py_pb2",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol:aggregation_protocol_messages_py_pb2",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol:configuration_py_pb2",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol/python:aggregation_protocol",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/tensorflow/python:aggregation_protocols",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "aggregations_test",
    srcs = ["aggregations_test.py"],
    data = ["@pybind11_abseil//pybind11_abseil:status.so"],
    deps = [
        ":aggregations",
        ":http_actions",
        ":media",
        ":test_utils",
        "//fcp/protos:plan_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol:aggregation_protocol_messages_py_pb2",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/protocol/python:aggregation_protocol",
        "@org_tensorflow_federated//tensorflow_federated/cc/core/impl/aggregation/tensorflow/python:aggregation_protocols",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "checkpoint_tensor_reference",
    srcs = ["checkpoint_tensor_reference.py"],
    deps = [
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "checkpoint_tensor_reference_test",
    srcs = ["checkpoint_tensor_reference_test.py"],
    deps = [
        ":checkpoint_tensor_reference",
        ":test_utils",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "eligibility_eval_tasks",
    srcs = ["eligibility_eval_tasks.py"],
    deps = [
        ":http_actions",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@com_google_googleapis//google/rpc:code_py_proto",
        "@pypi_absl_py//:pkg",
    ],
)

py_test(
    name = "eligibility_eval_tasks_test",
    srcs = ["eligibility_eval_tasks_test.py"],
    deps = [
        ":eligibility_eval_tasks",
        ":http_actions",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@com_google_googleapis//google/rpc:code_py_proto",
        "@pypi_absl_py//:pkg",
    ],
)

py_library(
    name = "federated_computation",
    srcs = ["federated_computation.py"],
    deps = [
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
    ],
)

py_test(
    name = "federated_computation_test",
    srcs = ["federated_computation_test.py"],
    deps = [
        ":federated_computation",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "federated_context",
    srcs = ["federated_context.py"],
    deps = [
        ":checkpoint_tensor_reference",
        ":federated_computation",
        ":federated_data_source",
        ":server",
        "//fcp/artifact_building:artifact_constants",
        "//fcp/artifact_building:checkpoint_utils",
        "//fcp/artifact_building:data_spec",
        "//fcp/artifact_building:federated_compute_plan_builder",
        "//fcp/artifact_building:plan_utils",
        "//fcp/artifact_building:variable_helpers",
        "//fcp/protos:plan_py_pb2",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_attrs//:pkg",
        "@pypi_dm_tree//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "federated_context_test",
    size = "medium",
    srcs = ["federated_context_test.py"],
    deps = [
        ":federated_computation",
        ":federated_context",
        ":federated_data_source",
        ":server",
        ":test_utils",
        "//fcp/artifact_building:artifact_constants",
        "//fcp/artifact_building:federated_compute_plan_builder",
        "//fcp/artifact_building:plan_utils",
        "//fcp/artifact_building:variable_helpers",
        "//fcp/protos:plan_py_pb2",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_attrs//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "federated_data_source",
    srcs = ["federated_data_source.py"],
    deps = [
        "//fcp/protos:plan_py_pb2",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "@federated_language//federated_language",
        "@pypi_numpy//:pkg",
    ],
)

py_test(
    name = "federated_data_source_test",
    srcs = ["federated_data_source_test.py"],
    deps = [
        ":federated_data_source",
        "//fcp/protos:plan_py_pb2",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "@federated_language//federated_language",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
    ],
)

py_test(
    name = "federated_program_test",
    size = "medium",
    srcs = ["federated_program_test.py"],
    data = ["//fcp/client:client_runner_main"],
    deps = [
        ":demo",
        "//fcp/client:client_runner_example_data_py_pb2",
        "//fcp/protos:plan_py_pb2",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "http_actions",
    srcs = ["http_actions.py"],
    deps = [
        "@com_google_googleapis//google/api:annotations_py_proto",
        "@com_google_protobuf//:protobuf_python",
        "@pypi_absl_py//:pkg",
    ],
)

py_test(
    name = "http_actions_test",
    size = "medium",
    srcs = ["http_actions_test.py"],
    deps = [
        ":http_actions",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@pypi_absl_py//:pkg",
    ],
)

py_library(
    name = "media",
    srcs = ["media.py"],
    deps = [
        ":http_actions",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
    ],
)

py_test(
    name = "media_test",
    srcs = ["media_test.py"],
    deps = [
        ":http_actions",
        ":media",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@pypi_absl_py//:pkg",
    ],
)

py_library(
    name = "plan_utils",
    srcs = ["plan_utils.py"],
    deps = [
        "//fcp/protos:plan_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "plan_utils_test",
    size = "medium",
    srcs = ["plan_utils_test.py"],
    deps = [
        ":plan_utils",
        ":test_utils",
        "//fcp/protos:plan_py_pb2",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "server",
    srcs = ["server.py"],
    deps = [
        ":aggregations",
        ":eligibility_eval_tasks",
        ":http_actions",
        ":media",
        ":plan_utils",
        ":task_assignments",
        "//fcp/protos:plan_py_pb2",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@pypi_absl_py//:pkg",
    ],
)

py_test(
    name = "server_test",
    size = "medium",
    srcs = ["server_test.py"],
    data = ["//fcp/client:client_runner_main"],
    deps = [
        ":plan_utils",
        ":server",
        ":test_utils",
        "//fcp/protos:plan_py_pb2",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "//fcp/tensorflow:external_dataset_py",
        "@com_google_googleapis//google/longrunning:operations_py_proto",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "task_assignments",
    srcs = ["task_assignments.py"],
    deps = [
        ":aggregations",
        ":http_actions",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@com_google_googleapis//google/longrunning:operations_py_proto",
        "@com_google_googleapis//google/rpc:code_py_proto",
        "@com_google_protobuf//:protobuf_python",
        "@pypi_absl_py//:pkg",
    ],
)

py_test(
    name = "task_assignments_test",
    srcs = ["task_assignments_test.py"],
    deps = [
        ":aggregations",
        ":http_actions",
        ":task_assignments",
        "//fcp/protos:population_eligibility_spec_py_pb2",
        "//fcp/protos/federatedcompute:federated_compute_py_pb2",
        "@com_google_googleapis//google/rpc:code_py_proto",
        "@pypi_absl_py//:pkg",
    ],
)

py_library(
    name = "test_utils",
    testonly = True,
    srcs = ["test_utils.py"],
    deps = [
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "test_utils_test",
    size = "medium",
    srcs = ["test_utils_test.py"],
    deps = [
        ":test_utils",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)
