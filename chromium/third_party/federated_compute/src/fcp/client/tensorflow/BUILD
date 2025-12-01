load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    default_visibility = [
        "//fcp:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
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

cc_library(
    name = "tensorflow_runner",
    hdrs = ["tensorflow_runner.h"],
    deps = [
        "//fcp/client:example_iterator_query_recorder",
        "//fcp/client:example_query_result_cc_proto",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "//fcp/client:runner_common",
        "//fcp/client/engine:common",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/opstats:opstats_logger",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "tensorflow_runner_impl",
    srcs = ["tensorflow_runner_impl.cc"],
    hdrs = ["tensorflow_runner_impl.h"],
    copts = FCP_COPTS,
    defines = TF_OPTIONAL_DEFINES,
    deps = [
        ":tensorflow_runner",
        "//fcp/base",
        "//fcp/client:example_iterator_query_recorder",
        "//fcp/client:example_query_result_cc_proto",
        "//fcp/client:interfaces",
        "//fcp/client:interruptible_runner",
        "//fcp/client:runner_common",
        "//fcp/client/engine:common",
        "//fcp/client/engine:example_iterator_factory",
        "//fcp/client/engine:tflite_plan_engine",
        "//fcp/client/opstats:opstats_logger",
        "//fcp/protos:data_type_cc_proto",
        "//fcp/protos:plan_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@org_tensorflow//tensorflow/core:framework",
        "@org_tensorflow//tensorflow/core:protos_all_cc",
        "@org_tensorflow//tensorflow/core/platform:tstring",
    ] + TF_OPTIONAL_DEPS,
)

cc_library(
    name = "tensorflow_runner_factory",
    srcs = ["tensorflow_runner_factory.cc"],
    hdrs = ["tensorflow_runner_factory.h"],
    deps = [
        ":tensorflow_runner",
        "//fcp/base:function_registry",
    ],
)

cc_library(
    name = "tensorflow_runner_impl_registration",
    srcs = ["tensorflow_runner_impl_registration.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":tensorflow_runner_factory",
        ":tensorflow_runner_impl",
        "//fcp/base:function_registry",
    ],
    alwayslink = 1,
)
