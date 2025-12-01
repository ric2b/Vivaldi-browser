/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef FCP_CLIENT_HTTP_TESTING_TEST_HELPERS_H_
#define FCP_CLIENT_HTTP_TESTING_TEST_HELPERS_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/longrunning/operations.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "fcp/client/http/http_client.h"

namespace fcp {
namespace client {
namespace http {

// A simple `HttpResponse` implementation consisting of a code, headers (which
// are returned via the `HttpResponse` interface methods), and an optional
// in-memory response body which `MockableHttpClient` can use to return the data
// via the `HttpRequestCallback::OnResponseBody` method.
class FakeHttpResponse : public HttpResponse {
 public:
  FakeHttpResponse(int code, HeaderList headers)
      : code_(code), headers_(headers), body_("") {}
  FakeHttpResponse(int code, HeaderList headers, std::string body)
      : code_(code), headers_(headers), body_(body) {}

  int code() const override { return code_; }
  const HeaderList& headers() const override { return headers_; }
  const std::string& body() const { return body_; }

 private:
  const int code_;
  const HeaderList headers_;
  const std::string body_;
};

// A simplified version of the `HttpClient` interface for use in tests. Enables
// easy use with gMock via the simplified `PerformSingleRequest` interface.
class MockableHttpClient : public HttpClient {
 public:
  // A simple container holding all important properties of an incoming
  // `HttpRequest`. This is the parameter type passed to `PerformSingleRequest`,
  // and because it is a simple struct (as opposed to `HttpRequest`, which uses
  // a number of methods that are hard to mock such as `HttpRequest::ReadBody`)
  // it makes it easy to use gMock matchers to match against parts or all of the
  // request's properties.
  struct SimpleHttpRequest {
    const std::string uri;
    const HttpRequest::Method method;
    const HeaderList headers;
    const std::string body;
  };

  MockableHttpClient() = default;

  ABSL_MUST_USE_RESULT std::unique_ptr<HttpRequestHandle> EnqueueRequest(
      std::unique_ptr<HttpRequest> request) override;

  absl::Status PerformRequests(
      std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests)
      override;

  // Implement this method (e.g. using gMock's MOCK_METHOD) for a simple way to
  // mock a single request. See `MockHttpClient` below.
  virtual absl::StatusOr<FakeHttpResponse> PerformSingleRequest(
      SimpleHttpRequest request) = 0;

  // Registers a callback that will be called when any request receives a
  // `HttpRequestHandle::Cancel` call.
  virtual void SetCancellationListener(std::function<void()> listener) {
    cancellation_listener_ = listener;
  }

  // Returns the (fake) number of bytes that the mock client has sent/received.
  // This number will match the sum of all `HttpRequestHandle`'s
  // `TotalSentReceivedBytes()` methods after they were processed by the mock
  // client.
  virtual HttpRequestHandle::SentReceivedBytes TotalSentReceivedBytes() {
    return sent_received_bytes_;
  }

 private:
  std::function<void()> cancellation_listener_ = []() {};

  // A running (fake) tally of the number of bytes that have been
  // downloaded/uploaded so far.
  HttpRequestHandle::SentReceivedBytes sent_received_bytes_;
};

// A convenient to use mock HttpClient implementation.
class MockHttpClient : public MockableHttpClient {
 public:
  MockHttpClient() = default;

  MOCK_METHOD(absl::StatusOr<FakeHttpResponse>, PerformSingleRequest,
              (SimpleHttpRequest request), (override));
};

::testing::Matcher<MockableHttpClient::SimpleHttpRequest>
SimpleHttpRequestMatcher(
    const ::testing::Matcher<std::string>& uri_matcher,
    const ::testing::Matcher<HttpRequest::Method>& method_matcher,
    const ::testing::Matcher<HeaderList>& headers_matcher,
    const ::testing::Matcher<std::string>& body_matcher);

// A mock request callback.
class MockHttpRequestCallback : public HttpRequestCallback {
 public:
  explicit MockHttpRequestCallback() = default;
  ~MockHttpRequestCallback() override = default;
  MOCK_METHOD(absl::Status, OnResponseStarted,
              (const HttpRequest& request, const HttpResponse& response),
              (override));

  MOCK_METHOD(void, OnResponseError,
              (const HttpRequest& request, const absl::Status& error),
              (override));

  MOCK_METHOD(absl::Status, OnResponseBody,
              (const HttpRequest& request, const HttpResponse& response,
               absl::string_view data),
              (override));

  MOCK_METHOD(void, OnResponseBodyError,
              (const HttpRequest& request, const HttpResponse& response,
               const absl::Status& error),
              (override));

  MOCK_METHOD(void, OnResponseCompleted,
              (const HttpRequest& request, const HttpResponse& response),
              (override));
};

// Creates a 'pending' `Operation`.
::google::longrunning::Operation CreatePendingOperation(
    absl::string_view operation_name);

// Creates a 'done' `Operation`, packing the given message into an `Any`.
::google::longrunning::Operation CreateDoneOperation(
    absl::string_view operation_name, const google::protobuf::Message& inner_result);

// Creates an `Operation` with the specified error information.
::google::longrunning::Operation CreateErrorOperation(
    absl::string_view operation_name, const absl::StatusCode error_code,
    absl::string_view error_message);

}  // namespace http
}  // namespace client
}  // namespace fcp

#endif  // FCP_CLIENT_HTTP_TESTING_TEST_HELPERS_H_
