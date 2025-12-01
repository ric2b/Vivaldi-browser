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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(
    default_visibility = [
        "//fcp:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "http_client",
    hdrs = ["http_client.h"],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "http_client_util",
    srcs = ["http_client_util.cc"],
    hdrs = ["http_client_util.h"],
    deps = [
        ":http_client",
        "//fcp/base",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:status_cc_proto",
    ],
)

cc_test(
    name = "http_client_util_test",
    srcs = ["http_client_util_test.cc"],
    deps = [
        ":http_client_util",
        "//fcp/base",
        "//fcp/testing",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:status_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "http_resource_metadata_proto",
    srcs = ["http_resource_metadata.proto"],
)

cc_proto_library(
    name = "http_resource_metadata_cc_proto",
    deps = [":http_resource_metadata_proto"],
)

cc_library(
    name = "in_memory_request_response",
    srcs = ["in_memory_request_response.cc"],
    hdrs = ["in_memory_request_response.h"],
    deps = [
        ":http_client",
        ":http_client_util",
        ":http_resource_metadata_cc_proto",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:compression",
        "//fcp/client:interruptible_runner",
        "//fcp/client/cache:resource_cache",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "in_memory_request_response_test",
    srcs = ["in_memory_request_response_test.cc"],
    deps = [
        ":http_client",
        ":http_client_util",
        ":http_resource_metadata_cc_proto",
        ":in_memory_request_response",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:compression",
        "//fcp/base:simulated_clock",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:interruptible_runner",
        "//fcp/client:test_helpers",
        "//fcp/client/cache:file_backed_resource_cache",
        "//fcp/client/cache:test_helpers",
        "//fcp/client/http/testing:test_helpers",
        "//fcp/testing",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "http_federated_protocol",
    srcs = ["http_federated_protocol.cc"],
    hdrs = [
        "http_federated_protocol.h",
    ],
    deps = [
        ":http_client",
        ":http_client_util",
        ":http_secagg_send_to_server_impl",
        ":in_memory_request_response",
        ":protocol_request_helper",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:compression",
        "//fcp/base:random_token",
        "//fcp/base:time_util",
        "//fcp/base:wall_clock_stopwatch",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:federated_protocol",
        "//fcp/client:federated_protocol_util",
        "//fcp/client:fl_runner_cc_proto",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "//fcp/client:parsing_utils",
        "//fcp/client:secagg_runner",
        "//fcp/client:selector_context_cc_proto",
        "//fcp/client/attestation:attestation_verifier",
        "//fcp/client/cache:resource_cache",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/confidentialcompute:client_payload",
        "//fcp/confidentialcompute:crypto",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "//fcp/protos/confidentialcompute:blob_header_cc_proto",
        "//fcp/protos/confidentialcompute:payload_metadata_cc_proto",
        "//fcp/protos/confidentialcompute:signed_endorsements_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/longrunning:longrunning_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "http_federated_protocol_test",
    srcs = ["http_federated_protocol_test.cc"],
    deps = [
        ":http_client",
        ":http_federated_protocol",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:compression",
        "//fcp/base:digest",
        "//fcp/base:time_util",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:event_time_range_cc_proto",
        "//fcp/client:federated_protocol",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "//fcp/client:test_helpers",
        "//fcp/client/attestation:attestation_verifier",
        "//fcp/client/cache:test_helpers",
        "//fcp/client/engine:engine_cc_proto",
        "//fcp/client/http/testing:test_helpers",
        "//fcp/confidentialcompute:client_payload",
        "//fcp/confidentialcompute:cose",
        "//fcp/confidentialcompute:crypto",
        "//fcp/protos:federated_api_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "//fcp/protos:population_eligibility_spec_cc_proto",
        "//fcp/protos/confidentialcompute:blob_header_cc_proto",
        "//fcp/protos/confidentialcompute:payload_metadata_cc_proto",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "//fcp/secagg/shared:cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/longrunning:longrunning_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googleapis//google/type:datetime_cc_proto",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@oak//proto/attestation:endorsement_cc_proto",
    ],
)

cc_library(
    name = "http_secagg_send_to_server_impl",
    srcs = ["http_secagg_send_to_server_impl.cc"],
    hdrs = ["http_secagg_send_to_server_impl.h"],
    visibility = ["//visibility:private"],
    deps = [
        ":http_client",
        ":http_client_util",
        ":in_memory_request_response",
        ":protocol_request_helper",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/client:interruptible_runner",
        "//fcp/client:secagg_runner",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "//fcp/secagg/shared:cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "http_secagg_send_to_server_impl_test",
    srcs = ["http_secagg_send_to_server_impl_test.cc"],
    deps = [
        ":http_client",
        ":http_secagg_send_to_server_impl",
        ":protocol_request_helper",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:simulated_clock",
        "//fcp/base:wall_clock_stopwatch",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:interruptible_runner",
        "//fcp/client:test_helpers",
        "//fcp/client/http/testing:test_helpers",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "//fcp/secagg/shared:cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/longrunning:longrunning_cc_proto",
        "@com_google_googleapis//google/rpc:code_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "protocol_request_helper",
    srcs = ["protocol_request_helper.cc"],
    hdrs = ["protocol_request_helper.h"],
    deps = [
        ":http_client",
        ":http_client_util",
        ":in_memory_request_response",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:time_util",
        "//fcp/base:wall_clock_stopwatch",
        "//fcp/client:interruptible_runner",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googleapis//google/longrunning:longrunning_cc_proto",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "protocol_request_helper_test",
    srcs = ["protocol_request_helper_test.cc"],
    deps = [
        ":http_client",
        ":in_memory_request_response",
        ":protocol_request_helper",
        "//fcp/base",
        "//fcp/base:clock",
        "//fcp/base:time_util",
        "//fcp/base:wall_clock_stopwatch",
        "//fcp/client:diag_codes_cc_proto",
        "//fcp/client:interruptible_runner",
        "//fcp/client:test_helpers",
        "//fcp/client/http/testing:test_helpers",
        "//fcp/protos/federatedcompute:federated_compute_cc_proto",
        "//fcp/testing",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)
