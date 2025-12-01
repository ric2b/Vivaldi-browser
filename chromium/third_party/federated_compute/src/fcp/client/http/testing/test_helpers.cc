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
#include "fcp/client/http/testing/test_helpers.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"
#include "google/protobuf/message.h"

namespace fcp {
namespace client {
namespace http {

using ::google::longrunning::Operation;
using ::google::protobuf::Message;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Matcher;

namespace {

// A simple `HttpRequestHandle` implementation for use with
// `MockableHttpClient`.
class SimpleHttpRequestHandle : public HttpRequestHandle {
 public:
  SimpleHttpRequestHandle(std::unique_ptr<HttpRequest> request,
                          std::function<void()> cancellation_listener)
      : request_(std::move(request)),
        cancellation_listener_(cancellation_listener) {}

  HttpRequestHandle::SentReceivedBytes TotalSentReceivedBytes() const override {
    return sent_received_bytes_;
  }
  void SetSentBytes(int64_t bytes) { sent_received_bytes_.sent_bytes = bytes; }
  void SetReceivedBytes(int64_t bytes) {
    sent_received_bytes_.received_bytes = bytes;
  }

  void Cancel() override { cancellation_listener_(); }

  HttpRequest* request() { return request_.get(); }

  // Marks the handle as having been passed to PerformRequests(...). Returns
  // true if the handle hand't previously been marked performed.
  bool MarkPerformed() {
    bool already_performed = performed_;
    performed_ = true;
    return !already_performed;
  }

 private:
  const std::unique_ptr<HttpRequest> request_;
  std::function<void()> cancellation_listener_;
  bool performed_ = false;
  HttpRequestHandle::SentReceivedBytes sent_received_bytes_ = {0, 0};
};

}  // namespace

std::unique_ptr<HttpRequestHandle> MockableHttpClient::EnqueueRequest(
    std::unique_ptr<HttpRequest> request) {
  return std::make_unique<SimpleHttpRequestHandle>(
      std::move(request), [this]() { this->cancellation_listener_(); });
}

absl::Status MockableHttpClient::PerformRequests(
    std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests) {
  for (const auto& [generic_handle, callback] : requests) {
    auto handle = static_cast<SimpleHttpRequestHandle*>(generic_handle);
    if (!handle->MarkPerformed()) {
      return absl::InternalError(
          "MockableHttpClient: handles cannot be used more than once.");
    }

    HttpRequest* request = handle->request();

    std::string request_body;
    if (request->HasBody()) {
      const HeaderList& headers = request->extra_headers();
      std::optional<std::string> content_length_hdr =
          FindHeader(headers, kContentLengthHdr);
      if (!content_length_hdr.has_value()) {
        return absl::InternalError(
            "MockableHttpClient only supports requests with known "
            "Content-Length");
      }
      int64_t content_length;
      if (!absl::SimpleAtoi(*content_length_hdr, &content_length)) {
        return absl::InternalError(absl::StrCat(
            "MockableHttpClient: unexpected Content-Length value: ",
            content_length));
      }
      request_body.resize(content_length);

      // Read the data all at once (our buffer should be big enough for it).
      absl::StatusOr<int64_t> read_result =
          request->ReadBody(&request_body[0], content_length);
      if (!read_result.ok()) {
        return absl::InternalError(
            absl::StrCat("MockableHttpClient: ReadBody failed: ",
                         read_result.status().ToString()));
      }
      if (*read_result != content_length) {
        return absl::InternalError(
            absl::StrCat("MockableHttpClient: 1st ReadBody didn't read all the "
                         "data. Actual: ",
                         *read_result, ", expected: ", content_length));
      }

      // Ensure we've hit the end of the data by checking for OUT_OF_RANGE.
      absl::Status read_body_result =
          request->ReadBody(&request_body[0], 1).status();
      if (read_body_result.code() != absl::StatusCode::kOutOfRange) {
        return absl::InternalError(
            absl::StrCat("MockableHttpClient: 2nd ReadBody failed: ",
                         read_body_result.ToString()));
      }
    }

    // Forward the request to the PerformSingleRequest method (which
    // generally will have been mocked using gMock's MOCK_METHOD). This
    // method will return the response that we should then deliver to the
    // HttpRequestCallback.
    SimpleHttpRequest simple_request = {std::string(request->uri()),
                                        request->method(),
                                        request->extra_headers(), request_body};
    absl::StatusOr<FakeHttpResponse> response =
        PerformSingleRequest(simple_request);

    // Mock some 'sent bytes data'. Users of this class shouldn't rely on the
    // exact value (just as they can't expect to predict how much data a real
    // `HttpClient` would send).
    int64_t fake_sent_bytes = request->uri().size() + request_body.size();
    handle->SetSentBytes(fake_sent_bytes);
    sent_received_bytes_.sent_bytes += fake_sent_bytes;

    if (!response.ok()) {
      return absl::Status(
          response.status().code(),
          absl::StrCat("MockableHttpClient: PerformSingleRequest failed: ",
                       response.status().ToString()));
    }

    // Return the response data to the callback's various methods.
    FCP_LOG(INFO) << "MockableHttpClient: Delivering response headers for: "
                  << request->uri();
    absl::Status response_started_result =
        callback->OnResponseStarted(*request, *response);
    if (!response_started_result.ok()) {
      return absl::InternalError(
          absl::StrCat("MockableHttpClient: OnResponseStarted failed: ",
                       response_started_result.ToString()));
    }

    // Mock some 'received bytes data'. We add 100 bytes to ensure that even
    // responses with empty response bodies do increase the counter, because
    // generally headers will always be received.
    int64_t fake_received_bytes = 100 + response->body().size();
    handle->SetReceivedBytes(fake_received_bytes);
    sent_received_bytes_.received_bytes += fake_received_bytes;

    FCP_LOG(INFO) << "MockableHttpClient: Delivering response body for: "
                  << request->uri();
    absl::Status response_body_result =
        callback->OnResponseBody(*request, *response, response->body());
    if (!response_body_result.ok()) {
      return absl::InternalError(
          absl::StrCat("MockableHttpClient: OnResponseBody failed: ",
                       response_body_result.ToString()));
    }

    FCP_LOG(INFO) << "MockableHttpClient: Delivering response completion for: "
                  << request->uri();
    callback->OnResponseCompleted(*request, *response);
  }
  return absl::OkStatus();
}

Matcher<MockableHttpClient::SimpleHttpRequest> SimpleHttpRequestMatcher(
    const Matcher<std::string>& uri_matcher,
    const Matcher<HttpRequest::Method>& method_matcher,
    const Matcher<HeaderList>& headers_matcher,
    const Matcher<std::string>& body_matcher) {
  return AllOf(
      Field("uri", &MockableHttpClient::SimpleHttpRequest::uri, uri_matcher),
      Field("method", &MockableHttpClient::SimpleHttpRequest::method,
            method_matcher),
      Field("headers", &MockableHttpClient::SimpleHttpRequest::headers,
            headers_matcher),
      Field("body", &MockableHttpClient::SimpleHttpRequest::body,
            body_matcher));
}

Operation CreatePendingOperation(absl::string_view operation_name) {
  Operation operation;
  operation.set_done(false);
  operation.set_name(std::string(operation_name));
  return operation;
}

Operation CreateDoneOperation(absl::string_view operation_name,
                              const Message& inner_result) {
  Operation operation;
  operation.set_name(std::string(operation_name));
  operation.set_done(true);
  operation.mutable_response()->PackFrom(inner_result);
  return operation;
}

Operation CreateErrorOperation(absl::string_view operation_name,
                               const absl::StatusCode error_code,
                               absl::string_view error_message) {
  Operation operation;
  operation.set_name(std::string(operation_name));
  operation.set_done(true);
  operation.mutable_error()->set_code(static_cast<int>(error_code));
  operation.mutable_error()->set_message(std::string(error_message));
  return operation;
}

}  // namespace http
}  // namespace client
}  // namespace fcp
