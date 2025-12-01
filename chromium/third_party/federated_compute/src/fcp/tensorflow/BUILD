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

load("@com_github_grpc_grpc//bazel:python_rules.bzl", "py_proto_library")
load("@org_tensorflow//tensorflow:tensorflow.bzl", "tf_cc_test", "tf_custom_op_py_library", "tf_gen_op_wrapper_py")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@rules_python//python:defs.bzl", "py_binary", "py_test")
load("//fcp:config.bzl", "FCP_COPTS")
load("//fcp/tensorflow/pip_tf:defs.bzl", "tf_custom_op_library")
load("//fcp/tracing:build_defs.bzl", "tracing_schema_cc_library")

default_visibility = ["//fcp:internal"]

package(
    default_visibility = default_visibility,
    licenses = ["notice"],  # Apache 2.0
)

tf_cc_test(
    name = "tf_smoke_test",
    srcs = ["tf_smoke_test.cc"],
    extra_copts = FCP_COPTS,
    deps = [
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow//tensorflow/cc:cc_ops",
        "@org_tensorflow//tensorflow/cc:client_session",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:tensorflow_opensource",
        "@org_tensorflow//tensorflow/core:testlib",
    ],
)

py_test(
    name = "tf_py_smoke_test",
    srcs = ["tf_py_smoke_test.py"],
    deps = ["@pypi_tensorflow//:pkg"],
)

cc_library(
    name = "host_object",
    srcs = [
        "host_object.cc",
    ],
    hdrs = [
        "host_object.h",
    ],
    copts = FCP_COPTS,
    visibility = default_visibility + [
    ],
    deps = [
        "//fcp/base",
        "//fcp/base:random_token",
        "//fcp/base:unique_value",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_test(
    name = "host_object_test",
    srcs = [
        "host_object_test.cc",
    ],
    copts = FCP_COPTS,
    deps = [
        ":host_object",
        "@com_google_googletest//:gtest_main",
    ],
)

tracing_schema_cc_library(
    name = "tracing_schema",
    srcs = ["tracing_schema.fbs"],
)

cc_library(
    name = "tf_session",
    srcs = ["tf_session.cc"],
    hdrs = ["tf_session.h"],
    copts = FCP_COPTS,
    deps = [
        ":tracing_schema",
        ":tracing_schema_fb",
        "//fcp/base",
        "//fcp/base:meta",
        "//fcp/base:process_unique_id",
        "//fcp/base:result",
        "//fcp/protos:plan_cc_proto",
        "//fcp/tracing",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@org_tensorflow//tensorflow/core:core_cpu",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core:tensorflow",
    ],
)

cc_test(
    name = "tf_session_test",
    srcs = ["tf_session_test.cc"],
    copts = FCP_COPTS,
    deps = [
        ":tf_session",
        ":tracing_schema",
        "//fcp/base:tracing_schema",
        "//fcp/protos:plan_cc_proto",
        "//fcp/tensorflow/testing:tf_helper",
        "//fcp/testing:result_matchers",
        "//fcp/tracing:test_tracing_recorder",
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow//tensorflow/cc:cc_ops",
        "@org_tensorflow//tensorflow/cc:scope",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core:tensorflow",
        "@org_tensorflow//tensorflow/core:testlib",
    ],
)

# C++ interfaces for implementing an 'external dataset' (a kind of host object).
# Note this does *not* depend on TensorFlow.
cc_library(
    name = "external_dataset",
    srcs = [
    ],
    hdrs = [
        "external_dataset.h",
    ],
    copts = FCP_COPTS,
    visibility = default_visibility + [
    ],
    deps = [
        ":host_object",
        "//fcp/base:bounds",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

# The targets below produce a custom op, which involves a native library as
# well as Python wrappers. There is some significant complexity arising from
# the various ways that an op / kernel might be linked in, which we'll try to
# explain here:
#
#   - Ops / kernels are meant to be buildable as DSOs (dynamic shared objects,
#     i.e. .so files). Yet, all ops and kernels must agree on the same
#     'framework' for registration etc. When building a DSO, ops and kernels
#     can include TensorFlow framework *headers*, with implementations provided
#     by libtensorflow_framework.so at runtime.
#
#   - When using ops / kernels (and TensorFlow) from a standard Python
#     interpreter, they *must* be loaded as DSOs.
#
#   - When using ops / kernels (and TensorFlow) from C++, we have the option of
#     linking a monolithic binary, with Bazel's usual handling of deps. This is
#     in fact necessary to generate Python wrapper code (the generator links in
#     cc_library deps).
#
# Below, we generate *both* a DSO and cc_library variant of the ExternalDataset
# op and kernel:
#   cc_library: :external_dataset_op_lib
#   DSO: _external_dataset_op.so
#
# The ExternalDataset op is a peculiar case, since it is specifically intended
# to use objects provided by the program hosting TensorFlow (beyond the usual
# TensorFlow APIs). This is problematic, since separate host and DSO binaries
# each end up with their own definitions of symbols from common libraries (and
# likely export them!). Though this might appear to work sometimes, it must be
# avoided.
# See e.g. https://github.com/abseil/abseil-cpp/issues/125
#
#                    ---------------------------
#                    | _external_dataset_op.so |
#  -------------  -> |  absl                   |
#  | Host      | /   |  fcp/base               |
#  |  absl     |     ---------------------------
#  |  fcp/base | \           |
#  -------------  \          v
#                  \   ------------------------------
#                   -> | libtensorflow_framework.so |
#                      ------------------------------
#
# When using the cc_library version and Bazel's usual handling of the deps
# graph, this is of course not a problem.
#
# As such, the DSO version is specifically useful for *building graphs in
# Python* and therefore targets the system-provided Python TensorFlow package.
# (C++) host programs must use the cc_library version.

EXTERNAL_DATASET_OP_SRCS = ["external_dataset_op.cc"]

EXTERNAL_DATASET_OP_DEPS = [
    ":external_dataset",
    "@com_google_absl//absl/status",
    "@com_google_absl//absl/status:statusor",
    "@com_google_absl//absl/strings:str_format",
    "@com_google_absl//absl/synchronization",
    "@com_google_absl//absl/types:span",
    "//fcp/base:random_token",
]

# Public: TensorFlow op and op-kernel, that delegates to an ExternalDatasetStub
# host object. This is the cc_library version. See explanation above.
cc_library(
    name = "external_dataset_op_lib",
    srcs = EXTERNAL_DATASET_OP_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = EXTERNAL_DATASET_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

# DSO version of :external_dataset_op_lib, intended to be loaded by Python
# wrappers. See explanation above.
tf_custom_op_library(
    name = "_external_dataset_op.so",
    srcs = EXTERNAL_DATASET_OP_SRCS,
    copts = FCP_COPTS,
    deps = EXTERNAL_DATASET_OP_DEPS,
)

# Generates the basic op wrapper for use in Python. As this is a dataset op,
# it's not useful directly; see :external_dataset_py.
tf_gen_op_wrapper_py(
    name = "gen_external_dataset_py",
    out = "gen_external_dataset_py.py",
    deps = [
        ":external_dataset_op_lib",
    ],
)

# Public: Python library for ExternalDataset.
tf_custom_op_py_library(
    name = "external_dataset_py",
    srcs = ["external_dataset.py"],
    dso = [":_external_dataset_op.so"],
    kernels = [
        ":external_dataset_op_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_external_dataset_py",
        "@pypi_tensorflow//:pkg",
    ],
)

# The dataset API isn't really usable from C++, so we generate a GraphDef for
# testing using Python.
py_binary(
    name = "make_external_dataset_test_graph",
    testonly = True,
    srcs = ["make_external_dataset_test_graph.py"],
    deps = [
        ":external_dataset_py",
        "@pypi_tensorflow//:pkg",
    ],
)

genrule(
    name = "external_dataset_test_graph",
    testonly = True,
    srcs = [],
    outs = ["external_dataset_test.pbtxt"],
    cmd = "$(location :make_external_dataset_test_graph) --output \"$@\"",
    tools = [":make_external_dataset_test_graph"],
)

# Selector proto used in test dataset stubs and example selector fuser op.
proto_library(
    name = "test_selector_proto",
    testonly = True,
    srcs = [
        "test_selector.proto",
    ],
)

cc_proto_library(
    name = "test_selector_cc_proto",
    testonly = True,
    deps = [":test_selector_proto"],
)

tf_cc_test(
    name = "external_dataset_op_test",
    srcs = ["external_dataset_op_test.cc"],
    data = [
        "external_dataset_test.pbtxt",
    ],
    extra_copts = FCP_COPTS,
    deps = [
        ":external_dataset",
        ":external_dataset_op_lib",
        ":test_selector_cc_proto",
        "//fcp/base",
        "//fcp/base:random_token",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core:core_cpu",
        "@org_tensorflow//tensorflow/core:direct_session",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core:tensorflow_opensource",
        "@org_tensorflow//tensorflow/core:testlib",
        "@org_tensorflow//tensorflow/core/platform:tstring",
        "@org_tensorflow//tensorflow/tsl/platform:status",
    ],
)

CRC32_OP_SRCS = [
    "crc32_op.cc",
    "tensor_crc32.cc",
]

CRC32_OP_DEPS = [
    "@com_google_absl//absl/status",
]

# Custom op to compute the CRC32 checksum of a tensor.
cc_library(
    name = "crc32_op_lib",
    srcs = [
        "crc32_op.cc",
        "tensor_crc32.cc",
    ],
    hdrs = ["tensor_crc32.h"],
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = CRC32_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

tf_custom_op_library(
    name = "_crc32_op.so",
    srcs = CRC32_OP_SRCS + ["tensor_crc32.h"],
    copts = FCP_COPTS,
    deps = CRC32_OP_DEPS,
)

# Generates the basic op wrapper for use in Python.
tf_gen_op_wrapper_py(
    name = "gen_crc32_py",
    out = "gen_crc32_py.py",
    deps = [
        ":crc32_op_lib",
    ],
)

tf_custom_op_py_library(
    name = "crc32_py",
    srcs = ["crc32.py"],
    dso = [":_crc32_op.so"],
    kernels = [
        ":crc32_op_lib",
    ],
    deps = [
        ":gen_crc32_py",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "crc32_test",
    srcs = ["crc32_test.py"],
    deps = [
        ":crc32_py",
        "@pypi_tensorflow//:pkg",
    ],
)

EXAMPLE_SELECTOR_FUSER_OP_SRCS = ["example_selector_fuser_op.cc"]

EXAMPLE_SELECTOR_FUSER_OP_DEPS = [
    "@com_google_protobuf//:protobuf",
    "@com_google_absl//absl/status",
    "//fcp/protos:plan_cc_proto",
]

# Custom op to add resumption token to example selector.
cc_library(
    name = "example_selector_fuser_op_lib",
    srcs = EXAMPLE_SELECTOR_FUSER_OP_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = EXAMPLE_SELECTOR_FUSER_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

tf_custom_op_library(
    name = "_example_selector_fuser_op.so",
    srcs = EXAMPLE_SELECTOR_FUSER_OP_SRCS,
    copts = FCP_COPTS,
    deps = EXAMPLE_SELECTOR_FUSER_OP_DEPS,
)

# Generates the basic op wrapper for use in Python.
tf_gen_op_wrapper_py(
    name = "gen_example_selector_fuser_op",
    out = "gen_example_selector_fuser_op.py",
    deps = [
        ":example_selector_fuser_op_lib",
    ],
)

tf_custom_op_py_library(
    name = "example_selector_fuser_py",
    srcs = ["example_selector_fuser.py"],
    dso = [":_example_selector_fuser_op.so"],
    kernels = [
        ":example_selector_fuser_op_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_example_selector_fuser_op",
        "@pypi_tensorflow//:pkg",
    ],
)

py_proto_library(
    name = "test_selector_py_pb2",
    testonly = True,
    deps = [
        ":test_selector_proto",
    ],
)

py_test(
    name = "example_selector_fuser_test",
    srcs = ["example_selector_fuser_test.py"],
    deps = [
        ":example_selector_fuser_py",
        ":test_selector_py_pb2",
        "//fcp/protos:plan_py_pb2",
        "@pypi_tensorflow//:pkg",
    ],
)

# C++ library to set and access callbacks for slice serving requests.
# Used by the `ServeSlices` custom op below.
cc_library(
    name = "serve_slices_registry",
    hdrs = [
        "serve_slices_registry.h",
    ],
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = [
        ":host_object",
    ],
)

cc_test(
    name = "serve_slices_registry_test",
    srcs = ["serve_slices_registry_test.cc"],
    deps = [
        ":host_object",
        ":serve_slices_registry",
        "//fcp/base:random_token",
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow//tensorflow/core:framework",
    ],
)

SERVE_SLICES_OP_SRCS = ["serve_slices_op.cc"]

SERVE_SLICES_OP_DEPS = [
    ":serve_slices_registry",
    "@com_google_absl//absl/status",
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/types:span",
    "@com_google_absl//absl/strings:str_format",
    "//fcp/base:random_token",
]

# Custom op to register slices to serve for a `federated_select`.
cc_library(
    name = "serve_slices_op_lib",
    srcs = SERVE_SLICES_OP_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = SERVE_SLICES_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

# DSO version of `:serve_slices_op_lib`, intended to be loaded by Python
# wrappers. See explanation above starting with "The targets below...".
tf_custom_op_library(
    name = "_serve_slices_op.so",
    srcs = SERVE_SLICES_OP_SRCS + [
        # Bundling the registry and op ensures that the same HostObjectRegistry is used by both.
        "//fcp/tensorflow/python:serve_slices_registry.cc",
    ],
    copts = FCP_COPTS,
    deps = SERVE_SLICES_OP_DEPS + [
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:absl_casters",
    ],
)

# Generates the basic op wrapper for use in Python.
# Don't use this directly: use `:serve_slices_py` to ensure that the
# appropriate shared libraries are loaded.
tf_gen_op_wrapper_py(
    name = "gen_serve_slices_py",
    out = "gen_serve_slices_py.py",
    deps = [
        ":serve_slices_op_lib",
    ],
)

# Public: Python library for ServeSlices.
tf_custom_op_py_library(
    name = "serve_slices_py",
    srcs = ["serve_slices.py"],
    dso = [":_serve_slices_op.so"],
    kernels = [
        ":serve_slices_op_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_serve_slices_py",
        "@pypi_tensorflow//:pkg",
    ],
)

# Generate a GraphDef for testing `ServeSlices` using Python.
py_binary(
    name = "make_serve_slices_test_graph",
    testonly = True,
    srcs = ["make_serve_slices_test_graph.py"],
    deps = [
        ":serve_slices_py",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)

genrule(
    name = "serve_slices_test_graph",
    testonly = True,
    srcs = [],
    outs = ["serve_slices_test.pbtxt"],
    cmd = "$(location :make_serve_slices_test_graph) --output \"$@\"",
    tools = [":make_serve_slices_test_graph"],
)

tf_cc_test(
    name = "serve_slices_op_test",
    srcs = ["serve_slices_op_test.cc"],
    data = [
        "serve_slices_test.pbtxt",
    ],
    extra_copts = FCP_COPTS,
    deps = [
        ":serve_slices_op_lib",
        ":serve_slices_registry",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
        "@org_tensorflow//tensorflow/core:core_cpu",
        "@org_tensorflow//tensorflow/core:direct_session",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core:tensorflow_opensource",
        "@org_tensorflow//tensorflow/core:testlib",
        "@org_tensorflow//tensorflow/core/platform:status_matchers",
    ],
)

MAKE_SLICES_SELECTOR_EXAMPLE_SELECTOR_OP_SRCS = ["make_slices_selector_example_selector_op.cc"]

MAKE_SLICES_SELECTOR_EXAMPLE_SELECTOR_OP_DEPS = [
    "@com_google_absl//absl/strings:str_format",
    "//fcp/protos:plan_cc_proto",
    "//fcp/client:federated_select",
]

# Custom op to serialize an ExampleSelector containing a SlicesSelector proto.
cc_library(
    name = "make_slices_selector_example_selector_op_lib",
    srcs = MAKE_SLICES_SELECTOR_EXAMPLE_SELECTOR_OP_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = ["@com_google_protobuf//:protobuf"] + MAKE_SLICES_SELECTOR_EXAMPLE_SELECTOR_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

tf_custom_op_library(
    name = "_make_slices_selector_example_selector_op.so",
    srcs = MAKE_SLICES_SELECTOR_EXAMPLE_SELECTOR_OP_SRCS,
    copts = FCP_COPTS,
    deps = ["@com_google_protobuf//:protobuf"] + MAKE_SLICES_SELECTOR_EXAMPLE_SELECTOR_OP_DEPS,
)

# Generates the basic op wrapper for use in Python.
# Don't use this directly: use `:make_slices_selector_py` to ensure that the
# appropriate shared libraries are loaded.
tf_gen_op_wrapper_py(
    name = "gen_make_slices_selector_example_selector_py",
    out = "gen_make_slices_selector_example_selector_py.py",
    deps = [
        ":make_slices_selector_example_selector_op_lib",
    ],
)

# Public: Python library for the `MakeSlicesSelectorExampleSelector` op.
tf_custom_op_py_library(
    name = "make_slices_selector_example_selector_py",
    srcs = ["make_slices_selector_example_selector.py"],
    dso = [":_make_slices_selector_example_selector_op.so"],
    kernels = [
        ":make_slices_selector_example_selector_op_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_make_slices_selector_example_selector_py",
        "@pypi_tensorflow//:pkg",
    ],
)

# Test `MakeSlicesSelectorExampleSelector` using Python.
py_test(
    name = "make_slices_selector_example_selector_test",
    testonly = True,
    srcs = ["make_slices_selector_example_selector_test.py"],
    deps = [
        ":make_slices_selector_example_selector_py",
        "//fcp/protos:plan_py_pb2",
        "@pypi_tensorflow//:pkg",
    ],
)

APPEND_SLICES_OP_SRCS = ["append_slices_op.cc"]

APPEND_SLICES_OP_DEPS = [
    "@com_google_absl//absl/base:core_headers",
    "@com_google_absl//absl/status",
    "@com_google_absl//absl/strings",
    "@com_google_absl//absl/synchronization",
    "@org_tensorflow//tensorflow/core/util:saved_tensor_slice_proto_cc",
]

# Custom op to serialize an ExampleSelector containing a SlicesSelector proto.
cc_library(
    name = "append_slices_op_lib",
    srcs = APPEND_SLICES_OP_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = APPEND_SLICES_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core/kernels:save_restore_tensor",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

tf_custom_op_library(
    name = "_append_slices_op.so",
    srcs = APPEND_SLICES_OP_SRCS,
    copts = FCP_COPTS,
    deps = APPEND_SLICES_OP_DEPS,
)

# Generates the basic op wrapper for use in Python.
# Don't use this directly: use `:append_slices_py` to ensure that the
# appropriate shared libraries are loaded.
tf_gen_op_wrapper_py(
    name = "gen_append_slices_py",
    out = "gen_append_slices_py.py",
    deps = [
        ":append_slices_op_lib",
    ],
)

# Public: Python library for the `AppendSlices` and `MergeAppendedSlices` ops.
tf_custom_op_py_library(
    name = "append_slices_py",
    srcs = ["append_slices.py"],
    dso = [":_append_slices_op.so"],
    kernels = [
        ":append_slices_op_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_append_slices_py",
        "@pypi_tensorflow//:pkg",
    ],
)

# Test `AppendSlices` and `MergeAppendedSlices` using Python.
py_test(
    name = "append_slices_test",
    testonly = True,
    srcs = ["append_slices_test.py"],
    deps = [
        ":append_slices_py",
        ":delete_file_py",
        "@pypi_tensorflow//:pkg",
    ],
)

DELETE_FILE_OP_SRCS = ["delete_file_op.cc"]

DELETE_FILE_OP_DEPS = [
    "@com_google_absl//absl/base:core_headers",
    "@com_google_absl//absl/log",
    "@com_google_absl//absl/status",
    "@com_google_absl//absl/synchronization",
]

# Custom op to serialize an ExampleSelector containing a SlicesSelector proto.
cc_library(
    name = "delete_file_op_lib",
    srcs = DELETE_FILE_OP_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = DELETE_FILE_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

tf_custom_op_library(
    name = "_delete_file_op.so",
    srcs = DELETE_FILE_OP_SRCS,
    copts = FCP_COPTS,
    deps = DELETE_FILE_OP_DEPS,
)

# Generates the basic op wrapper for use in Python.
# Don't use this directly: use `:delete_file_py` to ensure that the
# appropriate shared libraries are loaded.
tf_gen_op_wrapper_py(
    name = "gen_delete_file_py",
    out = "gen_delete_file_py.py",
    deps = [
        ":delete_file_op_lib",
    ],
)

# Public: Python library for the `DeleteFile` ops.
tf_custom_op_py_library(
    name = "delete_file_py",
    srcs = ["delete_file.py"],
    dso = [":_delete_file_op.so"],
    kernels = [
        ":delete_file_op_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_delete_file_py",
        "@pypi_tensorflow//:pkg",
    ],
)

# Test `DeleteFile` using Python.
py_test(
    name = "delete_file_test",
    testonly = True,
    srcs = ["delete_file_test.py"],
    deps = [
        ":delete_file_py",
        "@pypi_tensorflow//:pkg",
    ],
)

TENSOR_NAME_OP_SRCS = ["tensor_name_op.cc"]

TENSOR_NAME_OP_DEPS = [
    "@com_google_absl//absl/strings:str_format",
]

# Custom op to get the name of a tensor in the final graph at runtime.
cc_library(
    name = "tensor_name_op_lib",
    srcs = TENSOR_NAME_OP_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = TENSOR_NAME_OP_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

# DSO version of `:tensor_name_op_lib`, intended to be loaded by Python
# wrappers. See explanation above starting with "The targets below...".
tf_custom_op_library(
    name = "_tensor_name_op.so",
    srcs = TENSOR_NAME_OP_SRCS,
    copts = FCP_COPTS,
    deps = TENSOR_NAME_OP_DEPS,
)

# Generates the basic op wrapper for use in Python.
# Don't use this directly: use `:tensor_name_py` to ensure that the
# appropriate shared libraries are loaded.
tf_gen_op_wrapper_py(
    name = "gen_tensor_name_py",
    out = "gen_tensor_name_py.py",
    deps = [
        ":tensor_name_op_lib",
    ],
)

# Public: Python library for the `TensorName` op.
tf_custom_op_py_library(
    name = "tensor_name_py",
    srcs = ["tensor_name.py"],
    dso = [":_tensor_name_op.so"],
    kernels = [
        ":tensor_name_op_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_tensor_name_py",
        "@pypi_tensorflow//:pkg",
    ],
)

# Test `TensorName` using Python.
py_test(
    name = "tensor_name_test",
    testonly = True,
    srcs = ["tensor_name_test.py"],
    deps = [
        ":tensor_name_py",
        "@pypi_tensorflow//:pkg",
    ],
)

TASK_ELIGIBILITY_INFO_OPS_SRCS = ["task_eligibility_info_ops.cc"]

TASK_ELIGIBILITY_INFO_OPS_DEPS = [
    "//fcp/protos:federated_api_cc_proto",
]

cc_library(
    name = "task_eligibility_info_ops_lib",
    srcs = TASK_ELIGIBILITY_INFO_OPS_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = TASK_ELIGIBILITY_INFO_OPS_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

tf_custom_op_library(
    name = "_task_eligibility_info_ops.so",
    srcs = TASK_ELIGIBILITY_INFO_OPS_SRCS,
    copts = FCP_COPTS,
    deps = TASK_ELIGIBILITY_INFO_OPS_DEPS,
)

# Generates the basic op wrapper for use in Python. We don't expose this wrapper
# directly, and rather we create a more user-friendly wrapper below, which uses
# this auto-generated one.
tf_gen_op_wrapper_py(
    name = "gen_task_eligibility_info_ops_py",
    out = "gen_task_eligibility_info_ops.py",
    visibility = ["//visibility:private"],
    deps = [
        ":task_eligibility_info_ops_lib",
    ],
)

# Python library exposing the user-facing task eligibility info ops.
tf_custom_op_py_library(
    name = "task_eligibility_info_ops_py",
    srcs = ["task_eligibility_info_ops.py"],
    dso = [":_task_eligibility_info_ops.so"],
    kernels = [
        ":task_eligibility_info_ops_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_task_eligibility_info_ops_py",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "task_eligibility_info_ops_test",
    srcs = ["task_eligibility_info_ops_test.py"],
    deps = [
        ":task_eligibility_info_ops_py",
        "//fcp/protos:federated_api_py_pb2",
        "@pypi_tensorflow//:pkg",
    ],
)

DICTIONARY_OPS_SRCS = ["dictionary_ops.cc"]

DICTIONARY_OPS_DEPS = [
    "//fcp/base",
    "//fcp/dictionary:dictionary_lib",
    "//fcp/dictionary:dictionary_cc_proto",
    "@com_google_absl//absl/status",
    "@com_google_absl//absl/status:statusor",
    "@com_google_absl//absl/strings",
    "@org_tensorflow//tensorflow/tsl/platform:tstring",
]

cc_library(
    name = "dictionary_ops_lib",
    srcs = DICTIONARY_OPS_SRCS,
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = DICTIONARY_OPS_DEPS + [
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:lib",
    ],
    # Uses TensorFlow's registration macros
    alwayslink = 1,
)

tf_custom_op_library(
    name = "_dictionary_ops.so",
    srcs = DICTIONARY_OPS_SRCS,
    copts = FCP_COPTS,
    deps = DICTIONARY_OPS_DEPS,
)

tf_gen_op_wrapper_py(
    name = "gen_dictionary_ops_py",
    out = "gen_dictionary_ops.py",
    op_allowlist = [
        "DictionarySize",
        "DictionaryLookup",
        "DictionaryReverseLookup",
    ],
    visibility = ["//visibility:private"],
    deps = [":dictionary_ops_lib"],
)

tf_custom_op_py_library(
    name = "dictionary_ops_py",
    srcs = ["dictionary_ops.py"],
    dso = [":_dictionary_ops.so"],
    kernels = [
        ":dictionary_ops_lib",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":gen_dictionary_ops_py",
        "//fcp/dictionary:dictionary_py_pb2",
        "@pypi_tensorflow//:pkg",
    ],
)

py_test(
    name = "dictionary_ops_test",
    srcs = ["dictionary_ops_test.py"],
    deps = [
        ":dictionary_ops_py",
        "//fcp/dictionary:dictionary_py_pb2",
        "@com_google_protobuf//:protobuf_python",
        "@pypi_absl_py//:pkg",
        "@pypi_tensorflow//:pkg",
    ],
)
