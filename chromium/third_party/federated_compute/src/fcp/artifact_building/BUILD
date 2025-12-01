load("@rules_python//python:defs.bzl", "py_library", "py_test")

default_visibility = ["//fcp:internal"]

py_library(
    name = "artifact_constants",
    srcs = ["artifact_constants.py"],
    visibility = default_visibility + [
    ],
)

py_library(
    name = "checkpoint_type",
    srcs = ["checkpoint_type.py"],
    visibility = default_visibility + [
    ],
)

py_library(
    name = "checkpoint_utils",
    srcs = ["checkpoint_utils.py"],
    visibility = default_visibility + [
    ],
    deps = [
        ":artifact_constants",
        ":tensor_utils",
        ":type_checks",
        ":variable_helpers",
        "//fcp/protos:plan_py_pb2",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_dm_tree//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "checkpoint_utils_test",
    srcs = [
        "checkpoint_utils_test.py",
    ],
    deps = [
        ":checkpoint_utils",
        "//fcp/protos:plan_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "data_spec",
    srcs = ["data_spec.py"],
    visibility = default_visibility + [
    ],
    deps = [
        ":type_checks",
        "//fcp/protos:plan_py_pb2",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "data_spec_test",
    srcs = [
        "data_spec_test.py",
    ],
    deps = [
        ":data_spec",
        "//fcp/protos:plan_py_pb2",
        "@federated_language//federated_language",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "federated_compute_plan_builder",
    srcs = ["federated_compute_plan_builder.py"],
    visibility = default_visibility + [
    ],
    deps = [
        ":artifact_constants",
        ":checkpoint_type",
        ":checkpoint_utils",
        ":data_spec",
        ":graph_helpers",
        ":proto_helpers",
        ":tensor_utils",
        ":type_checks",
        ":variable_helpers",
        "//fcp/protos:plan_py_pb2",
        "//fcp/tensorflow:append_slices_py",
        "//fcp/tensorflow:delete_file_py",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_attrs//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "graph_helpers",
    srcs = ["graph_helpers.py"],
    visibility = default_visibility + [
    ],
    deps = [
        ":data_spec",
        ":tensor_utils",
        ":type_checks",
        "//fcp/tensorflow:external_dataset_py",
        "@federated_language//federated_language",
        "@federated_language//federated_language/proto:computation_py_pb2",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "graph_helpers_test",
    size = "small",
    timeout = "moderate",
    srcs = ["graph_helpers_test.py"],
    deps = [
        ":data_spec",
        ":graph_helpers",
        ":variable_helpers",
        "//fcp/protos:plan_py_pb2",
        "@federated_language//federated_language",
        "@federated_language//federated_language/proto:computation_py_pb2",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "plan_utils",
    srcs = ["plan_utils.py"],
    visibility = default_visibility + [
    ],
    deps = [
        ":tensor_utils",
        "//fcp/protos:plan_py_pb2",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "plan_utils_test",
    srcs = [
        "plan_utils_test.py",
    ],
    deps = [
        ":checkpoint_utils",
        ":plan_utils",
        ":test_utils",
        "//fcp/protos:plan_py_pb2",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "proto_helpers",
    srcs = ["proto_helpers.py"],
    visibility = default_visibility + [
    ],
    deps = [
        ":tensor_utils",
        ":type_checks",
        "//fcp/protos:plan_py_pb2",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "proto_helpers_test",
    size = "small",
    timeout = "moderate",
    srcs = ["proto_helpers_test.py"],
    deps = [
        ":proto_helpers",
        ":variable_helpers",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "tensor_utils",
    srcs = ["tensor_utils.py"],
    visibility = default_visibility + [
    ],
    deps = [
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "tensor_utils_test",
    srcs = [
        "tensor_utils_test.py",
    ],
    deps = [
        ":tensor_utils",
        "@com_google_protobuf//:protobuf_python",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "test_utils",
    srcs = ["test_utils.py"],
    visibility = default_visibility + [
    ],
    deps = [
        "//fcp/protos:plan_py_pb2",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "test_utils_test",
    srcs = [
        "test_utils_test.py",
    ],
    deps = [
        ":checkpoint_utils",
        ":test_utils",
        "//fcp/protos:plan_py_pb2",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "type_checks",
    srcs = ["type_checks.py"],
    visibility = default_visibility + [
    ],
    deps = [
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "type_checks_test",
    srcs = [
        "type_checks_test.py",
    ],
    deps = [
        ":type_checks",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "variable_helpers",
    srcs = ["variable_helpers.py"],
    visibility = default_visibility + [
    ],
    deps = [
        ":tensor_utils",
        ":type_checks",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_dm_tree//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "variable_helpers_test",
    size = "small",
    timeout = "moderate",
    srcs = ["variable_helpers_test.py"],
    deps = [
        ":artifact_constants",
        ":variable_helpers",
        "@federated_language//federated_language",
        "@org_tensorflow_federated//tensorflow_federated",
        "@pypi_absl_py//:pkg",
        "@pypi_numpy//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

py_library(
    name = "data_upload_plan_builder",
    srcs = ["data_upload_plan_builder.py"],
    visibility = default_visibility + [
    ],
    deps = [
        "//fcp/protos:plan_py_pb2",
    ],
)

py_test(
    name = "data_upload_plan_builder_test",
    srcs = ["data_upload_plan_builder_test.py"],
    deps = [
        ":data_upload_plan_builder",
        "//fcp/protos:data_type_py_pb2",
        "//fcp/protos:plan_py_pb2",
        "@pypi_absl_py//:pkg",
    ],
)
