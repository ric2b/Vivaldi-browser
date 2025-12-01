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

# This file uses repository rules to fetch external dependencies.
#
# We require that all uses of repository rules are effectively deterministic -
# they may fail, but otherwise must always produce exactly the same output for
# given parameters. For the http_archive rule, this means that you must specify
# a sha256 parameter; consequently, make sure to use stable URLs, rather than
# e.g. one that names a git branch.
#
# This is so that all(*) inputs to the build are fully determined (i.e. a
# change to build inputs requires a change to our sources), which avoids
# confusing outcomes from caching. If it is ever productive to clear your Bazel
# cache, that's a bug.
#
# (*) A Bazel build depends on local compiler toolchains (and Bazel itself), so
# it can be useful to pick a particular container image too (like some version
# of http://l.gcr.io/bazel).
#
# The repository namespace
# ------------------------
#
# Bazel's handling of @repository_names// is very broken. There is a single,
# global namespace for repositories. Conflicts are silently ignored. This is
# problematic for common dependencies. As much as possible, we use TensorFlow's
# dependencies. Things become especially difficult if we try to use our own
# version of grpc or protobuf. Current overrides:
#
#  - @com_github_gflags_gflags: tf uses 2.2.1 and we need 2.2.2 apparently.
#  - @com_google_googletest: Need to patch in support for absl flags

workspace(name = "com_google_fcp")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")  # buildifier: disable=load-on-top
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")  # buildifier: disable=load-on-top

# Needed for user-defined configs
http_archive(
    name = "bazel_skylib",
    sha256 = "74d544d96f4a5bb630d465ca8bbcfe231e3594e5aae57e1edbf17a6eb3ca2506",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.3.0/bazel-skylib-1.3.0.tar.gz",
    ],
)

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")  # buildifier: disable=load-on-top

bazel_skylib_workspace()

http_archive(
    name = "rules_python",
    sha256 = "690e0141724abb568267e003c7b6d9a54925df40c275a870a4d934161dc9dd53",
    strip_prefix = "rules_python-0.40.0",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.40.0/rules_python-0.40.0.tar.gz",
)

load("@rules_python//python:repositories.bzl", "py_repositories", "python_register_toolchains")  # buildifier: disable=load-on-top

py_repositories()

python_register_toolchains(
    name = "python",
    ignore_root_user_error = True,
    python_version = "3.9",
)

load("@python//:defs.bzl", "interpreter")  # buildifier: disable=load-on-top
load("@rules_python//python:pip.bzl", "package_annotation", "pip_parse")  # buildifier: disable=load-on-top
load("//fcp/tensorflow/pip_tf:defs.bzl", "TF_ADDITIVE_BUILD_CONTENT")  # buildifier: disable=load-on-top

pip_parse(
    name = "pypi",
    annotations = {
        "tensorflow": package_annotation(
            additive_build_content = TF_ADDITIVE_BUILD_CONTENT,
        ),
    },
    python_interpreter_target = interpreter,
    requirements_lock = "//fcp:requirements.txt",
)

load("@pypi//:requirements.bzl", "install_deps")  # buildifier: disable=load-on-top

install_deps()

# Use a newer version of BoringSSL than what TF gives us, so we can use
# functions like `EC_group_p256` and `ECDSA_sign_p1363`(which was added in commit
# 34492c89a8e381e0e856a686cc71b1eb5bd728db).
http_archive(
    name = "boringssl",
    sha256 = "61f9abae8ccc7a3d0aa27db97364ab6187383441c8d11dfe315065cd612c5e8f",
    strip_prefix = "boringssl-34492c89a8e381e0e856a686cc71b1eb5bd728db",
    urls = ["https://github.com/google/boringssl/archive/34492c89a8e381e0e856a686cc71b1eb5bd728db.tar.gz"],
)

http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "76900ab068da86378395a8e125b5cc43dfae671e09ff6462ddfef18676e2165a",
    strip_prefix = "grpc-1.50.0",
    urls = ["https://github.com/grpc/grpc/archive/refs/tags/v1.50.0.tar.gz"],
)

# TensorFlow 2.14.0 pins an old version of upb that's compatible with their old
# version of gRPC, but not with the newer version we use. Pin the version that
# would be added by gRPC 1.50.0.
http_archive(
    name = "upb",
    sha256 = "017a7e8e4e842d01dba5dc8aa316323eee080cd1b75986a7d1f94d87220e6502",
    strip_prefix = "upb-e4635f223e7d36dfbea3b722a4ca4807a7e882e2",
    urls = [
        "https://storage.googleapis.com/grpc-bazel-mirror/github.com/protocolbuffers/upb/archive/e4635f223e7d36dfbea3b722a4ca4807a7e882e2.tar.gz",
        "https://github.com/protocolbuffers/upb/archive/e4635f223e7d36dfbea3b722a4ca4807a7e882e2.tar.gz",
    ],
)

# The version provided by TensorFlow 2.13.0 doesn't support equality checks for
# absl::Status.
http_archive(
    name = "pybind11_abseil",
    sha256 = "6481888831cd548858c09371ea892329b36c8d4d961f559876c64e009d0bc630",
    strip_prefix = "pybind11_abseil-3922b3861a2b27d4111e3ac971e6697ea030a36e",
    url = "https://github.com/pybind/pybind11_abseil/archive/3922b3861a2b27d4111e3ac971e6697ea030a36e.tar.gz",
)

http_archive(
    name = "pybind11_protobuf",
    sha256 = "fe2b8bf12a65997b853709a5e719f7561b2e86a4cdbb9d8b051e654dd0fd8d11",
    strip_prefix = "pybind11_protobuf-a50899c2eb604fc5f25deeb8901eff6231b8b3c0",
    url = "https://github.com/pybind/pybind11_protobuf/archive/a50899c2eb604fc5f25deeb8901eff6231b8b3c0.tar.gz",
)

# Define the @io_grpc_grpc_java repository, which is used by the
# @com_google_googleapis repository to define the Java protobuf targets such as
# @com_google_googleapis//google/rpc:rpc_java_proto). The pattern we use here is
# the same as @com_google_googleapis' WORKSPACE file uses.
#
# Note that the @com_google_googleapis repository is actually defined
# transitively by the @org_tensorflow workspace rules.
http_archive(
    name = "com_google_api_gax_java",
    sha256 = "7c172c20dc52c09f42b3077a5195dc5fbcb30e023831918593d1b81a1aea650e",
    strip_prefix = "gax-java-2.20.0",
    urls = ["https://github.com/googleapis/gax-java/archive/v2.20.0.zip"],
)

load("@com_google_api_gax_java//:repository_rules.bzl", "com_google_api_gax_java_properties")  # buildifier: disable=load-on-top

com_google_api_gax_java_properties(
    name = "com_google_api_gax_java_properties",
    file = "@com_google_api_gax_java//:dependencies.properties",
)

load("@com_google_api_gax_java//:repositories.bzl", "com_google_api_gax_java_repositories")  # buildifier: disable=load-on-top

com_google_api_gax_java_repositories()

http_archive(
    name = "org_tensorflow_federated",
    patches = [
        "//fcp/patches:tensorflow_federated_deps.patch",
        "//fcp/patches:tensorflow_federated_executors.patch",
    ],
    sha256 = "599b886aa63d10728d3acd9d1aea5c71f571110fd99c2b3cad317f3c33d725eb",
    strip_prefix = "tensorflow-federated-4b152349a49cb8131835106243334f4f43bcff14",
    url = "https://github.com/google-parfait/tensorflow-federated/archive/4b152349a49cb8131835106243334f4f43bcff14.tar.gz",
)

http_archive(
    name = "federated_language",
    patches = [
        "@org_tensorflow_federated//third_party/federated_language:proto_library_loads.patch",
        "//fcp/patches:federated_language_structure_visibility.patch",
    ],
    repo_mapping = {
        "@protobuf": "@com_google_protobuf",
    },
    sha256 = "de4bbfd93ee10c3797463d6a96911963d8969fb5ed0b264ae4f3f3088013fed4",
    strip_prefix = "federated-language-5405fd4b2965e2f7c6c240b386f0540e4114818e",
    url = "https://github.com/google-parfait/federated-language/archive/5405fd4b2965e2f7c6c240b386f0540e4114818e.tar.gz",
)

# Tensorflow v2.14.0
http_archive(
    name = "org_tensorflow",
    patches = [
        # This patch enables googleapi Java and Python proto rules such as
        # @com_google_googleapis//google/rpc:rpc_java_proto.
        "//fcp/patches:tensorflow_googleapis_proto_rules.patch",
        # This patch works around failures in GitHub infrastructure to
        # download versions of LLVM pointed to by non-HEAD TensorFlow.
        # TODO(team): Remove this patch when resolved.
        "//fcp/patches:tensorflow_llvm_url.patch",
        # The version of googleapis imported by TensorFlow 2.14 doesn't provide
        # `py_proto_library` targets for //google/longrunning.
        "//fcp/patches:tensorflow_googleapis.patch",
        # This patch replaces tf_gen_op_wrapper_py's dependency on @tensorflow
        # with @pypi_tensorflow.
        "//fcp/patches:tensorflow_tf_gen_op_wrapper_py.patch",
        # gRPC v1.48.0-pre1 and later include zconf.h in addition to zlib.h;
        # TensorFlow's build rule for zlib only exports the latter.
        "//fcp/patches:tensorflow_zlib.patch",
        # These patches enables building TensorFlow Federated from source by
        # fixing visibility in TensorFlow.
        "@org_tensorflow_federated//third_party/tensorflow:internal_visibility.patch",
        "@org_tensorflow_federated//third_party/tensorflow:tf2xla_visibility.patch",
    ],
    sha256 = "ce357fd0728f0d1b0831d1653f475591662ec5bca736a94ff789e6b1944df19f",
    strip_prefix = "tensorflow-2.14.0",
    urls = [
        "https://github.com/tensorflow/tensorflow/archive/v2.14.0.tar.gz",
    ],
)

# The following is copied from TensorFlow's own WORKSPACE, see
# https://github.com/tensorflow/tensorflow/blob/v2.8.0/WORKSPACE#L9

load("@org_tensorflow//tensorflow:workspace3.bzl", "tf_workspace3")  # buildifier: disable=load-on-top

tf_workspace3()

load("@org_tensorflow//tensorflow:workspace2.bzl", "tf_workspace2")  # buildifier: disable=load-on-top

tf_workspace2()

load("@org_tensorflow//tensorflow:workspace1.bzl", "tf_workspace1")  # buildifier: disable=load-on-top

tf_workspace1(with_rules_cc = False)

load("@org_tensorflow//tensorflow:workspace0.bzl", "tf_workspace0")  # buildifier: disable=load-on-top

tf_workspace0()

# Cpp ProtoDataStore
http_archive(
    name = "protodatastore_cpp",
    sha256 = "d2627231fce0c9944100812b42d33203f7e03e78215d10123e78041b491005c3",
    strip_prefix = "protodatastore-cpp-0cd8b124bc65bcac105bce4a706923218ae5d625",
    url = "https://github.com/google/protodatastore-cpp/archive/0cd8b124bc65bcac105bce4a706923218ae5d625.zip",
)

# libcurl is used in the http client.
http_archive(
    name = "libcurl",
    build_file = "//third_party:curl.BUILD.bzl",
    sha256 = "e9d74b8586e0d2e6b45dc948bbe77525a1fa7f7c004ad5192f12e72c365e376e",
    strip_prefix = "curl-curl-7_84_0",
    url = "https://github.com/curl/curl/archive/refs/tags/curl-7_84_0.zip",
)

# We use only the http test server.
http_archive(
    name = "tensorflow_serving",
    patches = [
        "//fcp/patches:tensorflow_serving.patch",
    ],
    sha256 = "6b428100be7ec4bb34fc7910b7f549b6556854016a3c4cbda5613b5c114797b3",
    strip_prefix = "serving-2.9.0",
    url = "https://github.com/tensorflow/serving/archive/refs/tags/2.9.0.zip",
)

load("@tensorflow_serving//tensorflow_serving:workspace.bzl", "tf_serving_workspace")  # buildifier: disable=load-on-top

tf_serving_workspace()

load("@rules_jvm_external//:repositories.bzl", "rules_jvm_external_deps")  # buildifier: disable=load-on-top

rules_jvm_external_deps()

load("@rules_jvm_external//:setup.bzl", "rules_jvm_external_setup")  # buildifier: disable=load-on-top

rules_jvm_external_setup()

load("@rules_jvm_external//:defs.bzl", "maven_install")  # buildifier: disable=load-on-top

maven_install(
    name = "fcp_maven",
    artifacts = [
        "com.google.code.findbugs:jsr305:3.0.2",
        "com.google.errorprone:error_prone_annotations:2.11.0",
        "com.google.guava:guava:31.0.1-jre",
        "com.google.truth:truth:1.1.3",
        "junit:junit:4.13",
        "org.mockito:mockito-core:4.3.1",
    ],
    repositories = [
        "https://maven.google.com",
        "https://repo1.maven.org/maven2",
    ],
)

http_archive(
    name = "oak",
    sha256 = "676c7af06d97bc2be222fe7a30dbe304260d0d209943a8cb759e03cd4e562548",
    strip_prefix = "oak-4b5d9894da31e8bbd815d90d7fbe3218e366cafc",
    url = "https://github.com/project-oak/oak/archive/4b5d9894da31e8bbd815d90d7fbe3218e366cafc.tar.gz",
)

git_repository(
    name = "libcppbor",
    build_file = "//third_party:libcppbor.BUILD.bzl",
    commit = "20d2be8672d24bfb441d075f82cc317d17d601f8",
    remote = "https://android.googlesource.com/platform/external/libcppbor",
)

# Register a clang C++ toolchain.
http_archive(
    name = "toolchains_llvm",
    sha256 = "b7cd301ef7b0ece28d20d3e778697a5e3b81828393150bed04838c0c52963a01",
    strip_prefix = "toolchains_llvm-0.10.3",
    url = "https://github.com/grailbio/bazel-toolchain/releases/download/0.10.3/toolchains_llvm-0.10.3.tar.gz",
)

load("@toolchains_llvm//toolchain:deps.bzl", "bazel_toolchain_dependencies")  # buildifier: disable=load-on-top

bazel_toolchain_dependencies()

load("@toolchains_llvm//toolchain:rules.bzl", "llvm_toolchain")  # buildifier: disable=load-on-top

llvm_toolchain(
    name = "llvm_toolchain",
    llvm_version = "13.0.0",
)

load("@llvm_toolchain//:toolchains.bzl", "llvm_register_toolchains")  # buildifier: disable=load-on-top

llvm_register_toolchains()

# The following enables the use of the library functions in the differential-
# privacy github repo
http_archive(
    name = "com_google_cc_differential_privacy",
    sha256 = "6e6e1cd7a819695caae408f4fa938129ab7a86e83fe2410137c85e50131abbe0",
    strip_prefix = "differential-privacy-3.0.0/cc",
    url = "https://github.com/google/differential-privacy/archive/refs/tags/v3.0.0.tar.gz",
)

http_archive(
    name = "com_google_differential_privacy",
    sha256 = "6e6e1cd7a819695caae408f4fa938129ab7a86e83fe2410137c85e50131abbe0",
    strip_prefix = "differential-privacy-3.0.0",
    url = "https://github.com/google/differential-privacy/archive/refs/tags/v3.0.0.tar.gz",
)
