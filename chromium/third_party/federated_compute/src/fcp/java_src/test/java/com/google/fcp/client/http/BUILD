load("@rules_java//java:defs.bzl", "java_test")

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
package(
    licenses = ["notice"],  # Apache 2.0
)

java_test(
    name = "HttpClientForNativeImplTest",
    size = "small",
    srcs = ["HttpClientForNativeImplTest.java"],
    deps = [
        "//fcp/client/http/java:jni_java_proto",
        "//fcp/java_src/main/java/com/google/fcp/client",
        "//fcp/java_src/main/java/com/google/fcp/client/http",
        "@com_google_googleapis//google/rpc:rpc_java_proto",
        "@com_google_protobuf//:protobuf_java",
        "@fcp_maven//:com_google_guava_guava",
        "@fcp_maven//:com_google_truth_truth",
        "@fcp_maven//:junit_junit",
        "@fcp_maven//:org_mockito_mockito_core",
    ],
)
