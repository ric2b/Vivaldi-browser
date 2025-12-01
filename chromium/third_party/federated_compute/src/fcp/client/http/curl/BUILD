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

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("//fcp:config.bzl", "FCP_COPTS")

package(
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "curl_http_client",
    srcs = [
        "curl_api.cc",
        "curl_header_parser.cc",
        "curl_http_client.cc",
        "curl_http_request_handle.cc",
        "curl_http_response.cc",
    ],
    hdrs = [
        "curl_api.h",
        "curl_header_parser.h",
        "curl_http_client.h",
        "curl_http_request_handle.h",
        "curl_http_response.h",
    ],
    copts = FCP_COPTS,
    visibility = ["//visibility:public"],
    deps = [
        "//fcp/base",
        "//fcp/client/http:http_client",
        "//fcp/client/http:http_client_util",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
        "@libcurl//:curl",
    ],
)

cc_test(
    name = "curl_header_parser_test",
    srcs = [
        "curl_header_parser_test.cc",
    ],
    deps = [
        ":curl_http_client",
        "//fcp/client/http:http_client_util",
        "@com_google_googletest//:gtest_main",
    ],
)

# TODO(team)
