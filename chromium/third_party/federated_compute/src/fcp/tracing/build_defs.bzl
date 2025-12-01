# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Build rule for tracing schemas to be used with fcp/tracing library.
"""

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//fcp:config.bzl", "FCP_COPTS")
load("@flatbuffers//:build_defs.bzl", "flatbuffer_cc_library")

def tracing_schema_cc_library(
        name,
        srcs,
        includes = [],
        visibility = None,
        testonly = None):
    """Rule to generate tracing-schema C++ files from .fbs file.

    This macro produces the following output:

    <name>: a cc_library including C++ code generated from .fbs file.

    <srcs>.h: header file to be included by the code using the tracing schema.

    <srcs>.bfbs: binary representation of the tracing schema.

    <srcs>_generated.h: a header file produced by flatc. Note: this file is
      already included by <srcs>.h, not to be consumed directly.

    Args:
      name: The label for the library, typically "tracing_schema".
      srcs: Single .fbs source file, typically [ "tracing_schema.fbs" ].
      includes: optional list of .fbs includes
      visibility: standard visibility
      testonly: standard testonly
    """

    # Validate only 1 src file is specified in the build rule.
    if (len(srcs) != 1):
        fail("Only 1 .fbs file can be specified per build rule.")

    # Rule to invoke flatc on the fbs file (produces <name>_generated.h):
    flatbuffer_cc_library(
        name = name + "_fb",
        srcs = srcs,
        includes =
            includes + ["//fcp/tracing:tracing_schema_common_fbs"],
        flatc_args = [
            "--gen-object-api",
            "--gen-generated",
            "--reflect-names",
            "--bfbs-comments",
            "--keep-prefix",
        ],
        gen_reflections = True,
        include_paths = [".", "third_party/fcp/tracing"],
    )

    # Get generated flatbuff files from flatbuffer_cc_library rule.
    src_bfbs = srcs[0].replace(".fbs", ".bfbs")
    src_generated_h = srcs[0].replace(".fbs", "_generated.h")
    src_generated_h_rootpath = "$(rootpath " + src_generated_h + ") "
    src_bfbs_rootpath = "$(location " + src_bfbs + ") "
    src_fbs_rootpath = "$(rootpath " + srcs[0] + ")"
    out_header = srcs[0].replace(".fbs", ".h")

    # Generating <name>.h with additional traits
    native.genrule(
        name = name + "_h",
        srcs = [
            src_bfbs,
            src_generated_h,
            srcs[0],
        ],
        outs = [out_header],
        cmd = ("$(location //fcp/tracing/tools:tracing_traits_generator) " +
               src_generated_h_rootpath + src_bfbs_rootpath + src_fbs_rootpath + "> $@"),
        tools = [
            "//fcp/tracing/tools:tracing_traits_generator",
        ],
    )

    # Packaging everything into cc_library:
    cc_library(
        name = name,
        hdrs = [
            ":" + out_header,
            "//fcp/tracing:tracing_schema_common_generated.h",
            "//fcp/tracing:tracing_severity.h",
            "//fcp/tracing:tracing_tag.h",
            "//fcp/tracing:tracing_traits.h",
        ],
        deps = [
            ":" + name + "_fb",
            "@com_google_absl//absl/base:core_headers",
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/memory",
            "@flatbuffers//:flatbuffers",
            "//fcp/base",
        ],
        data = [
            srcs[0],
            "//fcp/tracing:tracing_schema_common.fbs",
        ],
        copts = FCP_COPTS,
        visibility = visibility,
        testonly = testonly,
    )
