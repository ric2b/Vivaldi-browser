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

# Description
# Federated Computation Platform (FCP) is a computational framework,
# which allows to orchestrate decentralized computations across
# large set of dynamically available nodes (e.g. mobile devices).

load("@bazel_toolchains//rules/exec_properties:exec_properties.bzl", "create_rbe_exec_properties_dict")
load("@rules_python//python:pip.bzl", "compile_pip_requirements")

package(
    default_visibility = [":internal"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "LICENSE",
])

package_group(
    name = "internal",
    includes = [
    ],
    packages = [
        "//fcp/...",
    ],
)

platform(
    name = "remote_platform",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    exec_properties = create_rbe_exec_properties_dict(
        container_image = "docker://gcr.io/fcp-infra/fcp-build@sha256:4a33c4b91e3d146ed654a731ee4827629d8f2ebbcad21998082d7555c79a3f42",
        os_family = "Linux",
    ),
    parents = ["@local_config_platform//:host"],
)

compile_pip_requirements(
    name = "requirements",
    timeout = "moderate",
    srcs = [
        "requirements.in",
        "@org_tensorflow_federated//:requirements.txt",
    ],
    extra_args = [
        "--allow-unsafe",
        "--no-annotate",
        "--resolver=backtracking",
    ],
    requirements_txt = "requirements.txt",
)
