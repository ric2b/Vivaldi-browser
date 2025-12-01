/*
 * Copyright 2022 Google LLC
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

#include "fcp/client/http/curl/curl_http_client.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "fcp/base/scheduler.h"
#include "fcp/client/http/curl/curl_api.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/in_memory_request_response.h"
#include "fcp/client/http/testing/http_test_server.h"
#include "fcp/client/http/testing/test_helpers.h"

namespace fcp::client::http::curl {
namespace {
using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::FieldsAre;
using ::testing::StrictMock;
using ::testing::UnorderedElementsAreArray;

void SetUpOnResponseStarted(
    StrictMock<MockHttpRequestCallback>* request_callback,
    const std::string& request_uri, HttpRequest::Method method,
    int response_code, const HeaderList& expected_request_extra_headers,
    bool has_body, const HeaderList& expected_response_headers) {
  EXPECT_CALL(*request_callback, OnResponseStarted(_, _))
      .WillOnce(::testing::Invoke([=, &request_uri](
                                      const HttpRequest& request,
                                      const HttpResponse& response) {
        EXPECT_THAT(request.uri(), request_uri);
        EXPECT_THAT(request.method(), method);
        EXPECT_THAT(request.extra_headers(),
                    UnorderedElementsAreArray(expected_request_extra_headers));
        EXPECT_THAT(request.HasBody(), has_body);
        EXPECT_THAT(response.code(), response_code);
        EXPECT_THAT(response.headers(),
                    UnorderedElementsAreArray(expected_response_headers));
        return absl::OkStatus();
      }));
}

void SetUpOnResponseBody(StrictMock<MockHttpRequestCallback>* request_callback,
                         const std::string& request_uri,
                         HttpRequest::Method method, int response_code,
                         const HeaderList& expected_request_extra_headers,
                         bool has_body,
                         const HeaderList& expected_response_headers,
                         const std::string& expected_response_body) {
  EXPECT_CALL(*request_callback, OnResponseBody(_, _, _))
      .WillOnce(::testing::Invoke([=](const HttpRequest& request,
                                      const HttpResponse& response,
                                      absl::string_view data) {
        EXPECT_THAT(request.uri(), request_uri);
        EXPECT_THAT(request.method(), method);
        EXPECT_THAT(request.extra_headers(),
                    UnorderedElementsAreArray(expected_request_extra_headers));
        EXPECT_THAT(request.HasBody(), has_body);
        EXPECT_THAT(response.code(), response_code);
        EXPECT_THAT(response.headers(),
                    UnorderedElementsAreArray(expected_response_headers));
        EXPECT_THAT(data, expected_response_body);
        return absl::OkStatus();
      }));
}

void SetUpOnResponseBody(StrictMock<MockHttpRequestCallback>* request_callback,
                         const std::string& request_uri,
                         HttpRequest::Method method, int response_code,
                         const HeaderList& expected_request_extra_headers,
                         bool has_body,
                         const HeaderList& expected_response_headers,
                         const std::string& expected_response_body,
                         size_t& total_bytes_downloaded) {
  EXPECT_CALL(*request_callback, OnResponseBody(_, _, _))
      .WillOnce(::testing::Invoke(
          [=, &total_bytes_downloaded](const HttpRequest& request,
                                       const HttpResponse& response,
                                       absl::string_view data) {
            EXPECT_THAT(request.uri(), request_uri);
            EXPECT_THAT(request.method(), method);
            EXPECT_THAT(
                request.extra_headers(),
                UnorderedElementsAreArray(expected_request_extra_headers));
            EXPECT_THAT(request.HasBody(), has_body);
            EXPECT_THAT(response.code(), response_code);
            EXPECT_THAT(response.headers(),
                        UnorderedElementsAreArray(expected_response_headers));
            EXPECT_THAT(data, expected_response_body);
            total_bytes_downloaded += data.size();
            return absl::OkStatus();
          }));
}

void SetUpOnResponseCompleted(
    StrictMock<MockHttpRequestCallback>* request_callback,
    const std::string& request_uri, HttpRequest::Method method,
    int response_code, const HeaderList& expected_request_extra_headers,
    bool has_body, const HeaderList& expected_response_headers) {
  EXPECT_CALL(*request_callback, OnResponseCompleted(_, _))
      .WillOnce(::testing::Invoke([=, &request_uri](
                                      const HttpRequest& request,
                                      const HttpResponse& response) {
        EXPECT_THAT(request.uri(), request_uri);
        EXPECT_THAT(request.method(), method);
        EXPECT_THAT(request.extra_headers(),
                    UnorderedElementsAreArray(expected_request_extra_headers));
        EXPECT_THAT(request.HasBody(), has_body);
        EXPECT_THAT(response.code(), response_code);
        EXPECT_THAT(response.headers(),
                    UnorderedElementsAreArray(expected_response_headers));
      }));
}

void SetUpGetRequestCallback(
    StrictMock<MockHttpRequestCallback>* request_callback,
    const std::string& request_uri,
    const HeaderList& expected_request_extra_headers,
    const HeaderList& expected_response_headers,
    const std::string& expected_response_body, size_t& total_bytes_downloaded) {
  SetUpOnResponseStarted(request_callback, request_uri,
                         HttpRequest::Method::kGet, /*response_code*/ 200,
                         expected_request_extra_headers,
                         /*has_body*/ false, expected_response_headers);
  SetUpOnResponseBody(request_callback, request_uri, HttpRequest::Method::kGet,
                      /*response_code*/ 200, expected_request_extra_headers,
                      /*has_body*/ false, expected_response_headers,
                      expected_response_body, total_bytes_downloaded);

  SetUpOnResponseCompleted(request_callback, request_uri,
                           HttpRequest::Method::kGet, /*response_code*/ 200,
                           expected_request_extra_headers,
                           /*has_body*/ false, expected_response_headers);
}

void SetUpPostRequestCallback(
    StrictMock<MockHttpRequestCallback>* request_callback,
    const std::string& request_uri,
    const HeaderList& expected_request_extra_headers,
    const HeaderList& expected_response_headers,
    const std::string& expected_response_body, size_t& total_bytes_downloaded) {
  SetUpOnResponseStarted(request_callback, request_uri,
                         HttpRequest::Method::kPost, /*response_code*/ 200,
                         expected_request_extra_headers,
                         /*has_body*/ true, expected_response_headers);

  SetUpOnResponseBody(request_callback, request_uri, HttpRequest::Method::kPost,
                      /*response_code*/ 200, expected_request_extra_headers,
                      /*has_body*/ true, expected_response_headers,
                      expected_response_body, total_bytes_downloaded);

  SetUpOnResponseCompleted(request_callback, request_uri,
                           HttpRequest::Method::kPost, /*response_code*/ 200,
                           expected_request_extra_headers,
                           /*has_body*/ true, expected_response_headers);
}

void SetUpPutRequestCallback(
    StrictMock<MockHttpRequestCallback>* request_callback,
    const std::string& request_uri,
    const HeaderList& expected_request_extra_headers,
    const HeaderList& expected_response_headers,
    const std::string& expected_response_body, size_t& total_bytes_downloaded) {
  SetUpOnResponseStarted(request_callback, request_uri,
                         HttpRequest::Method::kPut, /*response_code*/ 200,
                         expected_request_extra_headers,
                         /*has_body*/ true, expected_response_headers);

  SetUpOnResponseBody(request_callback, request_uri, HttpRequest::Method::kPut,
                      /*response_code*/ 200, expected_request_extra_headers,
                      /*has_body*/ true, expected_response_headers,
                      expected_response_body, total_bytes_downloaded);

  SetUpOnResponseCompleted(request_callback, request_uri,
                           HttpRequest::Method::kPut, /*response_code*/ 200,
                           expected_request_extra_headers,
                           /*has_body*/ true, expected_response_headers);
}

void PerformTwoRequests(CurlHttpClient* http_client, int port,
                        const std::string& request_uri1,
                        const std::string& request_uri2) {
  std::string request1_body;
  std::string request2_body = "test: 123-45";

  auto request1 = InMemoryHttpRequest::Create(
      request_uri1, HttpRequest::Method::kGet, HeaderList(), request1_body,
      /*use_compression*/ false);
  ASSERT_OK(request1);

  auto request2 = InMemoryHttpRequest::Create(
      request_uri2, HttpRequest::Method::kPost, HeaderList(), request2_body,
      /*use_compression*/ false);
  ASSERT_OK(request2);

  auto handle1 = http_client->EnqueueRequest(std::move(request1.value()));
  auto handle2 = http_client->EnqueueRequest(std::move(request2.value()));

  auto request_callback1 =
      std::make_unique<StrictMock<MockHttpRequestCallback>>();
  auto request_callback2 =
      std::make_unique<StrictMock<MockHttpRequestCallback>>();

  size_t total_bytes_downloaded_handle1 = 0;
  size_t total_bytes_downloaded_handle2 = 0;

  HeaderList expected_request_extra_headers{{"Content-Length", "12"}};
  HeaderList expected_response_headers{
      {"Content-Type", "text/html"},
      {"Date",
       absl::FormatTime("%a, %d %b %Y %H", absl::Now(), absl::UTCTimeZone())}};

  auto expected_response_body1 = absl::StrCat(
      "HTTP Method: GET\n", "Request Uri: /test\n", "Request Headers:\n",
      "Host: localhost:", port, "\nAccept: */*\n", "Accept-Encoding: gzip\n",
      "Request Body:\n");

  auto expected_response_body2 = absl::StrCat(
      "HTTP Method: POST\nRequest Uri: /test\n",
      "Request Headers:\nHost: localhost:", port,
      "\nAccept: */*\nAccept-Encoding: gzip\n", "Content-Length: 12\n",
      "Content-Type: application/x-www-form-urlencoded\n",
      "Request Body:\ntest: 123-45");

  SetUpGetRequestCallback(request_callback1.get(), request_uri1,
                          /*expected_request_extra_headers*/ {},
                          expected_response_headers, expected_response_body1,
                          total_bytes_downloaded_handle1);
  SetUpPostRequestCallback(request_callback2.get(), request_uri2,
                           expected_request_extra_headers,
                           expected_response_headers, expected_response_body2,
                           total_bytes_downloaded_handle2);

  std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests{
      std::make_pair(handle1.get(), request_callback1.get()),
      std::make_pair(handle2.get(), request_callback2.get())};

  EXPECT_THAT(handle1->TotalSentReceivedBytes(), FieldsAre(0, 0));
  EXPECT_THAT(handle2->TotalSentReceivedBytes(), FieldsAre(0, 0));

  absl::Status status = http_client->PerformRequests(requests);

  EXPECT_THAT(status, absl::OkStatus());
  EXPECT_THAT(handle1->TotalSentReceivedBytes(),
              AllOf(Field(&HttpRequestHandle::SentReceivedBytes::sent_bytes,
                          request1_body.size()),
                    Field(&HttpRequestHandle::SentReceivedBytes::received_bytes,
                          total_bytes_downloaded_handle1)));
  EXPECT_THAT(handle2->TotalSentReceivedBytes(),
              AllOf(Field(&HttpRequestHandle::SentReceivedBytes::sent_bytes,
                          request2_body.size()),
                    Field(&HttpRequestHandle::SentReceivedBytes::received_bytes,
                          total_bytes_downloaded_handle2)));
}

// Runs PerformRequests once in one thread.
TEST(CurlHttpClientTest, PerformTwoRequestsInParallelInOneThread) {
  const int port = 4568;
  const std::string request_uri =
      absl::StrCat("http://localhost:", port, "/test");

  auto curl_api = std::make_unique<CurlApi>();
  auto http_client = std::make_unique<CurlHttpClient>(curl_api.get());
  auto http_server = CreateHttpTestServer("/test", port, /*num_threads*/ 5);
  EXPECT_THAT(http_server.ok(), true);
  EXPECT_THAT(http_server.value()->StartAcceptingRequests(), true);

  PerformTwoRequests(http_client.get(), port, request_uri, request_uri);

  curl_api.reset();
  http_server.value()->Terminate();
  http_server.value()->WaitForTermination();
}

TEST(CurlHttpClientTest, PutRequest) {
  const int port = 4568;
  const std::string request_uri =
      absl::StrCat("http://localhost:", port, "/test");

  auto curl_api = std::make_unique<CurlApi>();
  auto http_client = std::make_unique<CurlHttpClient>(curl_api.get());
  auto http_server = CreateHttpTestServer("/test", port, /*num_threads*/ 5);
  EXPECT_THAT(http_server.ok(), true);
  EXPECT_THAT(http_server.value()->StartAcceptingRequests(), true);

  std::string request_body = "test: 123-45";

  auto request = InMemoryHttpRequest::Create(
      request_uri, HttpRequest::Method::kPut, HeaderList(), request_body,
      /*use_compression*/ false);
  ASSERT_OK(request);

  auto handle = http_client->EnqueueRequest(std::move(request.value()));

  auto request_callback =
      std::make_unique<StrictMock<MockHttpRequestCallback>>();

  size_t total_bytes_downloaded_handle = 0;

  HeaderList expected_request_extra_headers{{"Content-Length", "12"}};
  HeaderList expected_response_headers{
      {"Content-Type", "text/html"},
      {"Date",
       absl::FormatTime("%a, %d %b %Y %H", absl::Now(), absl::UTCTimeZone())}};

  auto expected_response_body =
      absl::StrCat("HTTP Method: PUT\nRequest Uri: /test\n",
                   "Request Headers:\nHost: localhost:", port,
                   "\nAccept: */*\nAccept-Encoding: gzip\n",
                   "Content-Length: 12\n", "Request Body:\ntest: 123-45");

  SetUpPutRequestCallback(request_callback.get(), request_uri,
                          expected_request_extra_headers,
                          expected_response_headers, expected_response_body,
                          total_bytes_downloaded_handle);

  std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests{
      std::make_pair(handle.get(), request_callback.get())};

  EXPECT_THAT(handle->TotalSentReceivedBytes(), FieldsAre(0, 0));

  absl::Status status = http_client->PerformRequests(requests);

  EXPECT_THAT(status, absl::OkStatus());
  EXPECT_THAT(handle->TotalSentReceivedBytes(),
              AllOf(Field(&HttpRequestHandle::SentReceivedBytes::sent_bytes,
                          request_body.size()),
                    Field(&HttpRequestHandle::SentReceivedBytes::received_bytes,
                          total_bytes_downloaded_handle)));

  curl_api.reset();
  http_server.value()->Terminate();
  http_server.value()->WaitForTermination();
}

// Runs PerformRequests in five times in five threads.
TEST(CurlHttpClientTest, PerformTwoRequestsInParallelFiveTimesInFiveThreads) {
  const int port = 4568;
  const std::string request_uri =
      absl::StrCat("http://localhost:", port, "/test");

  auto curl_api = std::make_unique<CurlApi>();
  auto http_client = std::make_unique<CurlHttpClient>(curl_api.get());
  auto http_server = CreateHttpTestServer("/test", port, /*num_threads*/ 10);
  EXPECT_THAT(http_server.ok(), true);
  EXPECT_THAT(http_server.value()->StartAcceptingRequests(), true);

  auto thread_pool_scheduler = CreateThreadPoolScheduler(5);

  thread_pool_scheduler->Schedule([&http_client, request_uri]() {
    PerformTwoRequests(http_client.get(), port, request_uri, request_uri);
  });
  thread_pool_scheduler->Schedule([&http_client, request_uri]() {
    PerformTwoRequests(http_client.get(), port, request_uri, request_uri);
  });
  thread_pool_scheduler->Schedule([&http_client, request_uri]() {
    PerformTwoRequests(http_client.get(), port, request_uri, request_uri);
  });
  thread_pool_scheduler->Schedule([&http_client, request_uri]() {
    PerformTwoRequests(http_client.get(), port, request_uri, request_uri);
  });
  thread_pool_scheduler->Schedule([&http_client, request_uri]() {
    PerformTwoRequests(http_client.get(), port, request_uri, request_uri);
  });

  thread_pool_scheduler->WaitUntilIdle();

  curl_api.reset();
  http_server.value()->Terminate();
  http_server.value()->WaitForTermination();
}

// Runs PerformRequests with two requests and cancels the second after
// OnResponseStarted received.
TEST(CurlHttpClientTest, CancelRequest) {
  const int port = 4568;
  const std::string request_uri1 =
      absl::StrCat("http://localhost:", port, "/test");
  const std::string request_uri2 =
      absl::StrCat("http://localhost:", port, "/test");

  auto curl_api = std::make_unique<CurlApi>();
  auto http_client = std::make_unique<CurlHttpClient>(curl_api.get());
  auto http_server = CreateHttpTestServer("/test", port, /*num_threads*/ 5);
  EXPECT_THAT(http_server.ok(), true);
  EXPECT_THAT(http_server.value()->StartAcceptingRequests(), true);

  std::string request1_body;
  std::string request2_body = "test: 123-45";

  auto request1 = InMemoryHttpRequest::Create(
      request_uri1, HttpRequest::Method::kGet, HeaderList(), request1_body,
      /*use_compression*/ false);
  ASSERT_OK(request1);

  auto request2 = InMemoryHttpRequest::Create(
      request_uri2, HttpRequest::Method::kPost, HeaderList(), request2_body,
      /*use_compression*/ false);
  ASSERT_OK(request2);

  auto handle1 = http_client->EnqueueRequest(std::move(request1.value()));
  auto handle2 = http_client->EnqueueRequest(std::move(request2.value()));

  auto request_callback1 =
      std::make_unique<StrictMock<MockHttpRequestCallback>>();
  auto request_callback2 =
      std::make_unique<StrictMock<MockHttpRequestCallback>>();

  size_t total_bytes_downloaded_handle1 = 0;

  HeaderList expected_request_extra_headers{{"Content-Length", "12"}};
  HeaderList expected_response_headers{
      {"Content-Type", "text/html"},
      {"Date",
       absl::FormatTime("%a, %d %b %Y %H", absl::Now(), absl::UTCTimeZone())}};

  auto expected_response_body = absl::StrCat(
      "HTTP Method: GET\n", "Request Uri: /test\n", "Request Headers:\n",
      "Host: localhost:", port, "\nAccept: */*\n", "Accept-Encoding: gzip\n",
      "Request Body:\n");

  SetUpGetRequestCallback(request_callback1.get(), request_uri1,
                          /*expected_request_extra_headers*/ {},
                          expected_response_headers, expected_response_body,
                          total_bytes_downloaded_handle1);

  EXPECT_CALL(*request_callback2, OnResponseStarted(_, _))
      .WillOnce(::testing::Invoke(
          [&handle2](const HttpRequest& request, const HttpResponse& response) {
            handle2->Cancel();
            return absl::OkStatus();
          }));

  auto expected_response_body2 = absl::StrCat(
      "HTTP Method: POST\nRequest Uri: /test\n",
      "Request Headers:\nHost: localhost:", port,
      "\nAccept: */*\nAccept-Encoding: gzip\n", "Content-Length: 12\n",
      "Content-Type: application/x-www-form-urlencoded\n",
      "Request Body:\ntest: 123-45");

  SetUpOnResponseBody(
      request_callback2.get(), request_uri2, HttpRequest::Method::kPost,
      /*response_code*/ 200, expected_request_extra_headers,
      /*has_body*/ true, expected_response_headers, expected_response_body2);

  EXPECT_CALL(*request_callback2, OnResponseBodyError(_, _, _))
      .WillOnce(::testing::Invoke([&request_uri2](const HttpRequest& request,
                                                  const HttpResponse& response,
                                                  const absl::Status& error) {
        EXPECT_THAT(request.uri(), request_uri2);
        EXPECT_THAT(request.method(), HttpRequest::Method::kPost);
        EXPECT_THAT(request.extra_headers().size(), 1);
        EXPECT_THAT(request.HasBody(), true);
      }));

  std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests{
      std::make_pair(handle1.get(), request_callback1.get()),
      std::make_pair(handle2.get(), request_callback2.get())};

  auto thread_pool_scheduler = CreateThreadPoolScheduler(2);
  thread_pool_scheduler->Schedule([&http_client, &requests]() {
    absl::Status status = http_client->PerformRequests(requests);

    EXPECT_THAT(status, absl::OkStatus());
  });

  thread_pool_scheduler->WaitUntilIdle();

  EXPECT_THAT(handle1->TotalSentReceivedBytes(),
              AllOf(Field(&HttpRequestHandle::SentReceivedBytes::sent_bytes,
                          request1_body.size()),
                    Field(&HttpRequestHandle::SentReceivedBytes::received_bytes,
                          total_bytes_downloaded_handle1)));

  curl_api.reset();
  http_server.value()->Terminate();
  http_server.value()->WaitForTermination();
}

// Runs PerformRequests once in one thread.
TEST(CurlHttpClientTest, TestExtraHeaders) {
  const int port = 4568;
  const std::string request_uri =
      absl::StrCat("http://localhost:", port, "/test");

  auto curl_api = std::make_unique<CurlApi>();
  auto http_client = std::make_unique<CurlHttpClient>(curl_api.get());

  auto http_server = CreateHttpTestServer("/test", port, /*num_threads*/ 3);
  EXPECT_THAT(http_server.ok(), true);
  EXPECT_THAT(http_server.value()->StartAcceptingRequests(), true);

  std::string request_body = "test: 123-45";
  HeaderList extra_headers = {{"Content-Type", "application/x-protobuf"}};

  auto request = InMemoryHttpRequest::Create(
      request_uri, HttpRequest::Method::kPost, extra_headers, request_body,
      /*use_compression*/ false);
  ASSERT_OK(request);

  auto handle = http_client->EnqueueRequest(std::move(request.value()));

  auto request_callback =
      std::make_unique<StrictMock<MockHttpRequestCallback>>();

  HeaderList expected_request_extra_headers{
      {"Content-Length", "12"},
      {"Content-Type", "application/x-protobuf"},
  };
  HeaderList expected_response_headers{
      {"Content-Type", "text/html"},
      {"Date",
       absl::FormatTime("%a, %d %b %Y %H", absl::Now(), absl::UTCTimeZone())}};
  auto expected_response_body =
      absl::StrCat("HTTP Method: POST\nRequest Uri: /test\n",
                   "Request Headers:\nHost: localhost:", port,
                   "\nAccept: */*\nAccept-Encoding: gzip\n",
                   "Content-Type: application/x-protobuf\n",
                   "Content-Length: 12\n", "Request Body:\ntest: 123-45");

  SetUpOnResponseStarted(request_callback.get(), request_uri,
                         HttpRequest::Method::kPost, /*response_code*/ 200,
                         expected_request_extra_headers,
                         /*has_body*/ true, expected_response_headers);

  SetUpOnResponseBody(
      request_callback.get(), request_uri, HttpRequest::Method::kPost,
      /*response_code*/ 200, expected_request_extra_headers,
      /*has_body*/ true, expected_response_headers, expected_response_body);

  SetUpOnResponseCompleted(request_callback.get(), request_uri,
                           HttpRequest::Method::kPost, /*response_code*/ 200,
                           expected_request_extra_headers,
                           /*has_body*/ true, expected_response_headers);

  std::vector<std::pair<HttpRequestHandle*, HttpRequestCallback*>> requests{
      std::make_pair(handle.get(), request_callback.get())};

  absl::Status status = http_client->PerformRequests(requests);
  EXPECT_THAT(status, absl::OkStatus());

  curl_api.reset();
  http_server.value()->Terminate();
  http_server.value()->WaitForTermination();
}

}  // namespace
}  // namespace fcp::client::http::curl
