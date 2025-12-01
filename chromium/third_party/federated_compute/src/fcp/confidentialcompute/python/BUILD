# Copyright 2025 Google LLC
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

package(
    default_visibility = ["//fcp:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "constants",
    srcs = ["constants.py"],
    visibility = ["//visibility:public"],
)

py_library(
    name = "dp_mf_aggregator",
    srcs = ["dp_mf_aggregator.py"],
    tags = ["manual"],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/py/optax",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_jax//:pkg",
        "@pypi_jax_privacy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "dp_mf_aggregator_test",
    srcs = ["dp_mf_aggregator_test.py"],
    tags = ["manual"],
    deps = [
        ":dp_mf_aggregator",
        "//third_party/py/optax",
        "@federated_language//federated_language",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "min_sep_data_source",
    srcs = ["min_sep_data_source.py"],
    visibility = ["//visibility:public"],
    deps = [
        ":constants",
        "//fcp/protos/confidentialcompute:file_info_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        "@federated_language//federated_language",
        "@federated_language//federated_language/proto:computation_py_pb2",
        "@federated_language//federated_language/proto:data_type_py_pb2",
    ],
)

py_test(
    name = "min_sep_data_source_test",
    srcs = ["min_sep_data_source_test.py"],
    deps = [
        ":min_sep_data_source",
        "//fcp/protos/confidentialcompute:file_info_py_pb2",
        "@federated_language//federated_language/proto:computation_py_pb2",
        "@federated_language//federated_language/proto:data_type_py_pb2",
        "@pypi_absl_py//:pkg",
    ],
)

py_library(
    name = "compiler",
    srcs = ["compiler.py"],
    visibility = ["//visibility:public"],
    deps = [
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_immutabledict//:pkg",
    ],
)

py_library(
    name = "program_input_provider",
    srcs = ["program_input_provider.py"],
    visibility = ["//visibility:public"],
    deps = ["@org_tensorflow_federated//tensorflow_federated"],
)

py_test(
    name = "program_input_provider_test",
    srcs = ["program_input_provider_test.py"],
    deps = [
        ":program_input_provider",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "compiler_test",
    srcs = ["compiler_test.py"],
    deps = [
        ":compiler",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
    ],
)
