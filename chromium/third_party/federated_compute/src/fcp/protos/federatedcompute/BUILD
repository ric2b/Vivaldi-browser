# Copyright 2021 Google LLC
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

load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

# --------------------------------------------------------------------
proto_library(
    name = "confidential_encryption_config_proto",
    srcs = ["confidential_encryption_config.proto"],
    deps = [
        "@oak//proto/attestation:endorsement_proto",
        "@oak//proto/attestation:evidence_proto",
    ],
)

cc_proto_library(
    name = "confidential_encryption_config_cc_proto",
    deps = [":confidential_encryption_config_proto"],
)

java_proto_library(
    name = "confidential_encryption_config_java_proto",
    deps = [":confidential_encryption_config_proto"],
)

# The api protos.
proto_library(
    name = "federated_compute_proto",
    srcs = [
        "aggregations.proto",
        "common.proto",
        "confidential_aggregations.proto",
        "eligibility_eval_tasks.proto",
        "secure_aggregations.proto",
        "task_assignments.proto",
    ],
    deps = [
        "//fcp/protos:federated_api_proto",
        "//fcp/secagg/shared:proto",
        "@com_google_googleapis//google/api:annotations_proto",
        "@com_google_googleapis//google/longrunning:operations_proto",
        "@com_google_googleapis//google/rpc:code_proto",
        "@com_google_googleapis//google/rpc:status_proto",
        "@com_google_protobuf//:duration_proto",
    ],
)

java_proto_library(
    name = "federated_compute_java_proto",
    deps = [":federated_compute_proto"],
)

cc_proto_library(
    name = "federated_compute_cc_proto",
    deps = [":federated_compute_proto"],
)

py_proto_library(
    name = "federated_compute_py_pb2",
    deps = [":federated_compute_proto"],
)
