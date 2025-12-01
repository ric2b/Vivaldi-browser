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
"""Provides rules for building custom TensorFlow ops compatible with pip."""

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

# Additional BUILD content that should be added to @pypi_tensorflow.
TF_ADDITIVE_BUILD_CONTENT = """
cc_library(
    name = "tf_headers",
    hdrs = glob(["site-packages/tensorflow/include/**"]),
    includes = ["site-packages/tensorflow/include"],
)

cc_library(
    name = "libtensorflow_framework",
    srcs = ["site-packages/tensorflow/libtensorflow_framework.so.2"],
)
"""

# Build flags for using the pip-provided TensorFlow package. pip_tf_flags_test ensures that these
# values stay in sync with the currently-used TF version.
PIP_TF_COPTS = ["-DEIGEN_MAX_ALIGN_BYTES=64", "-D_GLIBCXX_USE_CXX11_ABI=1"]
PIP_TF_CXXOPTS = ["--std=c++17"]
PIP_TF_LINKOPTS = []

def _force_pip_tf_transition_impl(settings, _attr):
    copts = list(settings["//command_line_option:copt"])
    cxxopts = list(settings["//command_line_option:cxxopt"])
    linkopts = list(settings["//command_line_option:linkopt"])
    copts += PIP_TF_COPTS
    cxxopts += PIP_TF_CXXOPTS
    linkopts += PIP_TF_LINKOPTS

    # TensorFlow's pip package was built with libstdc++.
    cxxopts.append("-stdlib=libstdc++")
    linkopts.append("-stdlib=libstdc++")

    return {
        "//command_line_option:copt": copts,
        "//command_line_option:cxxopt": cxxopts,
        "//command_line_option:linkopt": linkopts,
    }

_force_pip_tf_transition = transition(
    implementation = _force_pip_tf_transition_impl,
    inputs = [
        "//command_line_option:copt",
        "//command_line_option:cxxopt",
        "//command_line_option:linkopt",
    ],
    outputs = [
        "//command_line_option:copt",
        "//command_line_option:cxxopt",
        "//command_line_option:linkopt",
    ],
)

def _force_pip_tf_impl(ctx):
    cc_binary = ctx.attr.cc_binary[0]
    output_file = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.symlink(
        output = output_file,
        target_file = cc_binary.files.to_list()[0],
    )
    return DefaultInfo(
        files = depset([output_file]),
        data_runfiles = ctx.runfiles(transitive_files = depset([output_file])),
    )

_force_pip_tf = rule(
    doc = """Forces a shared library to be built in a way that's compatible
with the pip-provided Python TensorFlow package.""",
    implementation = _force_pip_tf_impl,
    attrs = {
        "cc_binary": attr.label(
            cfg = _force_pip_tf_transition,
            mandatory = True,
            doc = "The cc_binary target to build with TensorFlow compatibility.",
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
)

def tf_custom_op_library(
        name,
        srcs = [],
        deps = [],
        tags = [],
        visibility = None,
        **kwargs):
    """Replacement for TF's custom_op_library that targets pip-provided TF.

    This rule will force a transition to an environment that targets the
    pip-provided TF library. This means that all deps of this target and the
    target's own sources will be compiled with the necessary compiler flags to
    correctly target a pip TF library.
    """
    cc_binary(
        name = name + "_lib",
        srcs = srcs,
        linkshared = 1,
        deps = deps + [
            "@pypi_tensorflow//:libtensorflow_framework",
            "@pypi_tensorflow//:tf_headers",
        ],
        tags = tags + ["manual"],
        visibility = ["//visibility:private"],
        **kwargs
    )

    _force_pip_tf(
        name = name,
        cc_binary = name + "_lib",
        visibility = visibility,
        tags = tags,
    )
