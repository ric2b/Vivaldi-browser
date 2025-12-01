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
#include "fcp/client/http/in_memory_request_response.h"

#include <cstdint>
#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "fcp/base/clock.h"
#include "fcp/base/compression.h"
#include "fcp/base/monitoring.h"
#include "fcp/base/simulated_clock.h"
#include "fcp/client/cache/file_backed_resource_cache.h"
#include "fcp/client/cache/test_helpers.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/http_client_util.h"
#include "fcp/client/http/http_resource_metadata.pb.h"
#include "fcp/client/http/testing/test_helpers.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/test_helpers.h"
#include "fcp/testing/testing.h"

namespace fcp::client::http {
namespace {

using ::fcp::IsCode;
using ::fcp::client::http::FakeHttpResponse;
using ::fcp::client::http::MockableHttpClient;
using ::fcp::client::http::MockHttpClient;
using ::testing::_;
using ::testing::ContainerEq;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::Ge;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::StrictMock;

using CompressionFormat =
    ::fcp::client::http::UriOrInlineData::InlineData::CompressionFormat;

constexpr absl::string_view kOctetStream = "application/octet-stream";
int64_t kMaxCacheSizeBytes = 10000000;

google::protobuf::Any MetadataForUncompressedResource() {
  HttpResourceMetadata metadata;
  google::protobuf::Any any;
  any.PackFrom(metadata);
  return any;
}

google::protobuf::Any MetadataForCompressedResource() {
  HttpResourceMetadata metadata;
  metadata.set_compression_format(
      ResourceCompressionFormat::RESOURCE_COMPRESSION_FORMAT_GZIP);
  google::protobuf::Any any;
  any.PackFrom(metadata);
  return any;
}

TEST(InMemoryHttpRequestTest, NonHttpsUriFails) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("http://invalid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  EXPECT_THAT(request.status(), IsCode(INVALID_ARGUMENT));
}

TEST(InMemoryHttpRequestTest, GetWithRequestBodyFails) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kGet, {},
          "non_empty_request_body", /*use_compression=*/false);
  EXPECT_THAT(request.status(), IsCode(INVALID_ARGUMENT));
}

// Ensures that providing a Content-Length header results in an error (since it
// is automatically generated instead).
TEST(InMemoryHttpRequestTest, ContentLengthHeaderFails) {
  // Note that we purposely use a mixed-case "Content-length" header name, to
  // ensure that the check is correctly case-insensitive.
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kPost,
          {{"Content-length", "1234"}}, "non_empty_request_body",
          /*use_compression=*/false);
  EXPECT_THAT(request.status(), IsCode(INVALID_ARGUMENT));
}

TEST(InMemoryHttpRequestTest, ValidGetRequest) {
  const std::string expected_uri = "https://valid.com";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(expected_uri, HttpRequest::Method::kGet, {},
                                  "", /*use_compression=*/false);
  ASSERT_OK(request);
  EXPECT_THAT((*request)->uri(), StrEq(expected_uri));
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kGet);
  // Because no request body is present, the Content-Length header shouldn't
  // have been added.
  EXPECT_THAT((*request)->extra_headers(), IsEmpty());
  EXPECT_FALSE((*request)->HasBody());
}

TEST(InMemoryHttpRequestTest, ValidGetRequestWithHeaders) {
  const HeaderList expected_headers{{"Foo", "Bar"}, {"Baz", "123"}};
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, expected_headers,
                                  "", /*use_compression=*/false);
  ASSERT_OK(request);
  EXPECT_THAT((*request)->extra_headers(), ContainerEq(expected_headers));
}

TEST(InMemoryHttpRequestTest, ValidPostRequestWithoutBody) {
  const std::string expected_uri = "https://valid.com";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(expected_uri, HttpRequest::Method::kPost, {},
                                  "", /*use_compression=*/false);
  ASSERT_OK(request);
  EXPECT_THAT((*request)->uri(), StrEq(expected_uri));
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kPost);
  // Because no request body is present, the Content-Length header shouldn't
  // have been added.
  EXPECT_THAT((*request)->extra_headers(), IsEmpty());
  EXPECT_FALSE((*request)->HasBody());
}

TEST(InMemoryHttpRequestTest, ValidPostRequestWithBody) {
  const std::string expected_uri = "https://valid.com";
  const std::string expected_body = "request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(expected_uri, HttpRequest::Method::kPost, {},
                                  expected_body,
                                  /*use_compression=*/false);
  ASSERT_OK(request);
  EXPECT_THAT((*request)->uri(), StrEq(expected_uri));
  EXPECT_EQ((*request)->method(), HttpRequest::Method::kPost);
  EXPECT_THAT((*request)->extra_headers(),
              ElementsAre(Pair("Content-Length",
                               std::to_string(expected_body.size()))));
  EXPECT_TRUE((*request)->HasBody());
}

// Checks that the automatically generated Content-Length header is
// appropriately added to any specified extra headers (rather than replacing
// them completely).
TEST(InMemoryHttpRequestTest, ValidPostRequestWithBodyAndHeaders) {
  const HeaderList original_headers{{"Foo", "Bar"}, {"Baz", "123"}};
  const std::string expected_body = "request_body";
  const HeaderList expected_headers{
      {"Foo", "Bar"},
      {"Baz", "123"},
      {"Content-Length", std::to_string(expected_body.size())}};
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kPost, original_headers,
                                  expected_body,
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  EXPECT_THAT((*request)->extra_headers(), ContainerEq(expected_headers));
}

TEST(InMemoryHttpRequestTest, ReadBodySimple) {
  const std::string expected_body = "request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kPost, {}, expected_body,
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  std::string actual_body;
  actual_body.resize(expected_body.size());
  // Read the body in one go (the "simple" case).
  auto read_result =
      (*request)->ReadBody(actual_body.data(), actual_body.size());
  ASSERT_OK(read_result);
  EXPECT_THAT(*read_result, actual_body.size());
  EXPECT_THAT(actual_body, StrEq(expected_body));
  // Expect the second read to indicate the end of the stream.
  EXPECT_THAT((*request)->ReadBody(nullptr, 1), IsCode(OUT_OF_RANGE));
}

TEST(InMemoryHttpRequestTest, ReadBodyChunked) {
  // This test reads the body in chunks of 3 bytes at a time (rather than all at
  // once, like previous test). To test some edge cases, we ensure that the
  // request body's length is not evenly dividable by 3.
  const std::string expected_body = "12345678";
  ASSERT_THAT(expected_body.size() % 3, Ne(0));
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kPost, {}, expected_body,
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  std::string actual_body;
  // Pre-size the buffer with 'X' characters. This will allow us to check
  // whether each read modifies the expected range of the buffer.
  actual_body.resize(expected_body.size(), 'X');

  // Read the body 3 bytes at a time.
  // The first read tests the case where less data is requested than is
  // available.
  absl::StatusOr<int64_t> read_result =
      (*request)->ReadBody(actual_body.data(), 3);
  ASSERT_OK(read_result);
  EXPECT_THAT(*read_result, 3);
  EXPECT_THAT(actual_body, StrEq("123XXXXX"));

  read_result = (*request)->ReadBody(actual_body.data() + 3, 3);
  ASSERT_OK(read_result);
  EXPECT_THAT(*read_result, 3);
  EXPECT_THAT(actual_body, StrEq("123456XX"));

  // The last read should only read 2 bytes. This tests the case where more data
  // is requested than is available. A correct implementation should not write
  // more bytes than it has available, which should ensure that no writes will
  // occur beyond actual_body.data()'s buffer size (which is only 8 bytes long).
  read_result = (*request)->ReadBody(actual_body.data() + 6, 3);
  ASSERT_OK(read_result);
  EXPECT_THAT(*read_result, 2);
  EXPECT_THAT(actual_body, StrEq(expected_body));

  // Expect the last read to indicate the end of the stream.
  EXPECT_THAT((*request)->ReadBody(nullptr, 1), IsCode(OUT_OF_RANGE));
}

TEST(InMemoryHttpRequestTest, RequestWithCompressedBody) {
  const std::string uncompressed_body =
      "request_body_AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kPost, {},
                                  uncompressed_body,
                                  /*use_compression=*/true);
  ASSERT_OK(request);
  auto content_encoding_header =
      FindHeader((*request)->extra_headers(), kContentEncodingHdr);
  ASSERT_TRUE(content_encoding_header.has_value());
  ASSERT_EQ(content_encoding_header.value(), kGzipEncodingHdrValue);

  auto content_length_header =
      FindHeader((*request)->extra_headers(), kContentLengthHdr);
  ASSERT_TRUE(content_length_header.has_value());
  int compressed_length = std::stoi(content_length_header.value());
  ASSERT_GT(compressed_length, 0);
  ASSERT_LT(compressed_length, uncompressed_body.size());

  std::string actual_body;
  actual_body.resize(compressed_length);
  // Read the body in one go (the "simple" case).
  auto read_result =
      (*request)->ReadBody(actual_body.data(), actual_body.size());
  ASSERT_OK(read_result);
  EXPECT_THAT(*read_result, actual_body.size());

  // Expect the second read to indicate the end of the stream.
  EXPECT_THAT((*request)->ReadBody(nullptr, 1), IsCode(OUT_OF_RANGE));

  auto recovered_body = UncompressWithGzip(actual_body);
  ASSERT_OK(recovered_body);
  EXPECT_EQ(*recovered_body, uncompressed_body);
}

TEST(InMemoryHttpRequestCallbackTest, ResponseFailsBeforeHeaders) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  InMemoryHttpRequestCallback callback;
  callback.OnResponseError(**request, absl::UnimplementedError("foobar"));

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  EXPECT_THAT(actual_response, IsCode(UNIMPLEMENTED));
  EXPECT_THAT(actual_response.status().message(), HasSubstr("foobar"));
}

TEST(InMemoryHttpRequestCallbackTest, ResponseFailsAfterHeaders) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  auto fake_response = FakeHttpResponse(kHttpOk, {});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  callback.OnResponseBodyError(**request, fake_response,
                               absl::UnimplementedError("foobar"));

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  EXPECT_THAT(actual_response, IsCode(UNIMPLEMENTED));
  EXPECT_THAT(actual_response.status().message(), HasSubstr("foobar"));
}

TEST(InMemoryHttpRequestCallbackTest, ResponseFailsAfterPartialBody) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  auto fake_response = FakeHttpResponse(kHttpOk, {});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "response_"));
  callback.OnResponseBodyError(**request, fake_response,
                               absl::UnimplementedError("foobar"));

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  EXPECT_THAT(actual_response, IsCode(UNIMPLEMENTED));
  EXPECT_THAT(actual_response.status().message(), HasSubstr("foobar"));
}

TEST(InMemoryHttpRequestCallbackTest,
     TestResponseWithContentLengthAndBodyTooShort) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  auto fake_response = FakeHttpResponse(kHttpOk, {{"Content-Length", "5"}});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  // Return a partial response (only 4 characters instead of the expected 5
  // indicated by the Content-Length header).
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "12"));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "34"));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  EXPECT_THAT(actual_response, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(actual_response.status().message(),
              HasSubstr("Too little response body data received"));
}

TEST(InMemoryHttpRequestCallbackTest,
     TestResponseWithContentLengthAndBodyTooLong) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  auto fake_response = FakeHttpResponse(kHttpOk, {{"Content-Length", "5"}});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  // Return a more response data than expected (6 characters instead of the
  // expected 5 indicated by the Content-Length header).
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "12"));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "34"));
  absl::Status response_body_result =
      callback.OnResponseBody(**request, fake_response, "56");
  EXPECT_THAT(response_body_result, IsCode(OUT_OF_RANGE));
  EXPECT_THAT(response_body_result.message(),
              HasSubstr("Too much response body data received"));
}

TEST(InMemoryHttpRequestCallbackTest, ResponseWithContentLengthNegative) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  // Set the Content-Length to an invalid negative value.
  auto fake_response = FakeHttpResponse(kHttpOk, {{"Content-Length", "-1"}});

  InMemoryHttpRequestCallback callback;
  absl::Status response_started_result =
      callback.OnResponseStarted(**request, fake_response);
  EXPECT_THAT(response_started_result, IsCode(OUT_OF_RANGE));
  EXPECT_THAT(response_started_result.message(),
              HasSubstr("Invalid Content-Length response header"));
}

TEST(InMemoryHttpRequestCallbackTest, ResponseWithContentLengthNonInteger) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  // Set the Content-Length to an invalid non-integer, non-ASCII value.
  auto fake_response =
      FakeHttpResponse(kHttpOk, {{"Content-Length", "\U0001F600"}});

  InMemoryHttpRequestCallback callback;
  absl::Status response_started_result =
      callback.OnResponseStarted(**request, fake_response);
  EXPECT_THAT(response_started_result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(response_started_result.message(),
              HasSubstr("Could not parse Content-Length response header"));
}

TEST(InMemoryHttpRequestCallbackTest, ResponseWithContentEncodingHeader) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  // Add a Content-Encoding header, which implementations should never provide
  // to us, unless we advertised an Accept-Encoding header in the request.
  auto fake_response = FakeHttpResponse(kHttpOk, {{"Content-Encoding", "foo"}});

  InMemoryHttpRequestCallback callback;
  absl::Status response_started_result =
      callback.OnResponseStarted(**request, fake_response);
  EXPECT_THAT(response_started_result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(response_started_result.message(),
              HasSubstr("Unexpected header: Content-Encoding"));
}

TEST(InMemoryHttpRequestCallbackTest, ResponseWithTransferEncodingHeader) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  // Add a Transfer-Encoding header, which implementations should never provide
  // to us.
  auto fake_response =
      FakeHttpResponse(kHttpOk, {{"Transfer-Encoding", "foo"}});

  InMemoryHttpRequestCallback callback;
  absl::Status response_started_result =
      callback.OnResponseStarted(**request, fake_response);
  EXPECT_THAT(response_started_result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(response_started_result.message(),
              HasSubstr("Unexpected header: Transfer-Encoding"));
}

TEST(InMemoryHttpRequestCallbackTest, OkResponseWithoutContentLength) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  // Note that the fake response does not contain a Content-Length header. The
  // InMemoryHttpRequestCallback should be able to handle this (accumulating
  // response data until OnResponseCompleted is called).
  int expected_code = 201;  // "201 Created"
  auto fake_response = FakeHttpResponse(expected_code, {});
  const std::string expected_body = "response_body";

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  // We return the response body in one go (the "simple" case).
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, expected_body));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  ASSERT_OK(actual_response);
  EXPECT_THAT(*actual_response, FieldsAre(expected_code, IsEmpty(), IsEmpty(),
                                          StrEq(expected_body)));
}

TEST(InMemoryHttpRequestCallbackTest, OkResponseWithContentLength) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  int expected_code = kHttpOk;
  const std::string expected_body = "response_body";
  auto fake_response = FakeHttpResponse(
      expected_code,
      {{"Content-Length", std::to_string(expected_body.size())}});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, expected_body));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  ASSERT_OK(actual_response);
  EXPECT_THAT(*actual_response, FieldsAre(expected_code, IsEmpty(), IsEmpty(),
                                          StrEq(expected_body)));
}

TEST(InMemoryHttpRequestCallbackTest, OkResponseChunkedBody) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  // Note that the fake response does not contain a Content-Length header. The
  // InMemoryHttpRequestCallback should be able to handle this (accumulating
  // response data until OnResponseCompleted is called).
  auto fake_response = FakeHttpResponse(kHttpOk, {});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  // This test returns the body in chunks of 3 bytes at a time (rather than
  // all at once, like previous test). To test some edge cases, we ensure that
  // the request body's length is not evenly dividable by 3.
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "123"));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "456"));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, "78"));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  ASSERT_OK(actual_response);
  EXPECT_THAT(actual_response->body, StrEq("12345678"));
}

TEST(InMemoryHttpRequestCallbackTest,
     TestOkResponseWithEmptyBodyWithoutContentLength) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  int expected_code = kHttpOk;
  auto fake_response = FakeHttpResponse(expected_code, {});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, ""));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  ASSERT_OK(actual_response);
  EXPECT_THAT(*actual_response,
              FieldsAre(expected_code, IsEmpty(), IsEmpty(), IsEmpty()));
}

TEST(InMemoryHttpRequestCallbackTest,
     TestOkResponseWithEmptyBodyWithContentLength) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  int expected_code = kHttpOk;
  auto fake_response =
      FakeHttpResponse(expected_code, {{"Content-Length", "0"}});

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, ""));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  ASSERT_OK(actual_response);
  EXPECT_THAT(*actual_response,
              FieldsAre(expected_code, IsEmpty(), IsEmpty(), IsEmpty()));
}

TEST(InMemoryHttpRequestCallbackTest,
     TestOkResponsWithAcceptEncodingRequestHeader) {
  const std::string expected_content_encoding = "foo";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kGet,
          {{"Accept-Encoding", expected_content_encoding}}, "",
          /*use_compression=*/false);
  ASSERT_OK(request);

  // Note that the fake response contains a Content-Encoding header, which
  // should be allowed because the request contained an Accept-Encoding header.
  int expected_code = kHttpOk;
  auto fake_response = FakeHttpResponse(
      expected_code, {{"Some-Response-Header", "foo"},
                      {"Content-Encoding", expected_content_encoding}});
  const std::string expected_body = "response_body";

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  // We return the response body in one go (the "simple" case).
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, expected_body));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  ASSERT_OK(actual_response);
  EXPECT_THAT(*actual_response,
              FieldsAre(expected_code, StrEq(expected_content_encoding),
                        IsEmpty(), StrEq(expected_body)));
}

TEST(InMemoryHttpRequestCallbackTest, NotFoundResponse) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  // Return an HTTP error response.
  auto fake_response = FakeHttpResponse(kHttpNotFound, {});
  const std::string expected_body = "response_body";

  InMemoryHttpRequestCallback callback;
  ASSERT_OK(callback.OnResponseStarted(**request, fake_response));
  ASSERT_OK(callback.OnResponseBody(**request, fake_response, expected_body));
  callback.OnResponseCompleted(**request, fake_response);

  absl::StatusOr<InMemoryHttpResponse> actual_response = callback.Response();
  EXPECT_THAT(actual_response, IsCode(NOT_FOUND));
  EXPECT_THAT(actual_response.status().message(), HasSubstr("404"));
}

class PerformRequestsTest : public ::testing::Test {
 protected:
  PerformRequestsTest()
      : interruptible_runner_(
            &mock_log_manager_, mock_should_abort_.AsStdFunction(),
            InterruptibleRunner::TimingConfig{
                .polling_period = absl::ZeroDuration(),
                .graceful_shutdown_period = absl::InfiniteDuration(),
                .extended_shutdown_period = absl::InfiniteDuration()},
            InterruptibleRunner::DiagnosticsConfig{
                .interrupted = ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP,
                .interrupt_timeout =
                    ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP_TIMED_OUT,
                .interrupted_extended = ProdDiagCode::
                    BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_COMPLETED,
                .interrupt_timeout_extended = ProdDiagCode::
                    BACKGROUND_TRAINING_INTERRUPT_HTTP_EXTENDED_TIMED_OUT}) {}
  void SetUp() override {
    root_cache_dir_ = testing::TempDir();
    root_files_dir_ = testing::TempDir();
  }

  void TearDown() override {
    std::filesystem::remove_all(root_cache_dir_);
    std::filesystem::remove_all(root_files_dir_);
  }

  absl::BitGen bit_gen_;
  NiceMock<MockLogManager> mock_log_manager_;
  NiceMock<MockFunction<bool()>> mock_should_abort_;
  InterruptibleRunner interruptible_runner_;
  StrictMock<MockHttpClient> mock_http_client_;
  testing::StrictMock<MockLogManager> log_manager_;
  SimulatedClock clock_;
  std::string root_cache_dir_;
  std::string root_files_dir_;
};

TEST_F(PerformRequestsTest, PerformRequestInMemoryOk) {
  std::string expected_request_body = "request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kPost,
          {{"Some-Request-Header", "foo"}}, expected_request_body,
          /*use_compression=*/false);
  ASSERT_OK(request);

  int expected_response_code = kHttpOk;
  std::string expected_response_body = "response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre((*request)->uri(), (*request)->method(),
                            Contains(Header{"Some-Request-Header", "foo"}),
                            StrEq(expected_request_body))))
      .WillOnce(Return(FakeHttpResponse(expected_response_code,
                                        {{"Some-Response-Header", "bar"}},
                                        expected_response_body)));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  absl::StatusOr<InMemoryHttpResponse> result = PerformRequestInMemory(
      mock_http_client_, interruptible_runner_, *std::move(request),
      // We pass in non-null pointers for the network stats, to ensure they are
      // correctly updated.
      &bytes_received, &bytes_sent, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  ASSERT_OK(result);
  EXPECT_THAT(*result, FieldsAre(expected_response_code, IsEmpty(), IsEmpty(),
                                 StrEq(expected_response_body)));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent, Ge(expected_request_body.size()));
  EXPECT_THAT(bytes_received, Ge(expected_response_body.size()));
}

TEST_F(PerformRequestsTest, PerformRequestInMemoryNotFound) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  int expected_response_code = kHttpNotFound;
  std::string expected_response_body = "response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre((*request)->uri(), (*request)->method(), _, _)))
      .WillOnce(Return(FakeHttpResponse(expected_response_code, {},
                                        expected_response_body)));

  absl::StatusOr<InMemoryHttpResponse> result = PerformRequestInMemory(
      mock_http_client_, interruptible_runner_, *std::move(request), nullptr,
      nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr, /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  EXPECT_THAT(result, IsCode(NOT_FOUND));
  EXPECT_THAT(result.status().message(), HasSubstr("404"));
}

TEST_F(PerformRequestsTest, PerformRequestInMemoryWithRetryUnavailableThenOk) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  int unavailable_expected_response_code = kHttpServiceUnavailable;
  std::string unavailable_expected_response_body = "response_body1";
  int ok_expected_response_code = kHttpOk;
  std::string ok_expected_response_body = "response_body2";
  // We expect the first two requests to return an unavailable error, and the
  // third to return an OK response. Since we retry 3 times, we expect the
  // third request to be the one that succeeds.
  {
    InSequence in_sequence;
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(
                    FieldsAre((*request)->uri(), (*request)->method(), _, _)))
        .WillOnce(
            Return(FakeHttpResponse(unavailable_expected_response_code, {},
                                    unavailable_expected_response_body)));
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(
                    FieldsAre((*request)->uri(), (*request)->method(), _, _)))
        .WillOnce(
            Return(FakeHttpResponse(unavailable_expected_response_code, {},
                                    unavailable_expected_response_body)));
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(
                    FieldsAre((*request)->uri(), (*request)->method(), _, _)))
        .WillOnce(Return(FakeHttpResponse(ok_expected_response_code, {},
                                          ok_expected_response_body)));
  }

  absl::StatusOr<InMemoryHttpResponse> result = PerformRequestInMemory(
      mock_http_client_, interruptible_runner_, *std::move(request), nullptr,
      nullptr, /*clock=*/Clock::RealClock(), &bit_gen_,
      /*retry_max_attempts=*/2,
      /*retry_delay_ms=*/50);
  EXPECT_THAT(result, IsCode(OK));
}

TEST_F(PerformRequestsTest, PerformRequestInMemoryWithRetryUnavailableForever) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  int expected_response_code = kHttpServiceUnavailable;
  std::string expected_response_body = "response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre((*request)->uri(), (*request)->method(), _, _)))
      .WillRepeatedly(Return(FakeHttpResponse(expected_response_code, {},
                                              expected_response_body)));

  absl::StatusOr<InMemoryHttpResponse> result = PerformRequestInMemory(
      mock_http_client_, interruptible_runner_, *std::move(request), nullptr,
      nullptr, /*clock=*/Clock::RealClock(), &bit_gen_,
      /*retry_max_attempts=*/2,
      /*retry_delay_ms=*/50);
  EXPECT_THAT(result, IsCode(UNAVAILABLE));
}

TEST_F(PerformRequestsTest, PerformRequestInMemoryEarlyError) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kPost, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre((*request)->uri(), (*request)->method(), _, _)))
      // Make the call to PerformSingleRequest (and therefore the call to
      // HttpClient::PerformRequests) fail as a whole (rather than having the
      // individual request return a failure).
      .WillOnce(Return(absl::InvalidArgumentError("PerformRequests failed")));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  absl::StatusOr<InMemoryHttpResponse> result = PerformRequestInMemory(
      mock_http_client_, interruptible_runner_, *std::move(request),
      &bytes_received, &bytes_sent, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(result.status().message(), HasSubstr("PerformRequests failed"));

  // We know that MockHttpClient will have updated the 'sent' network stat
  // before having called into PerformSingleRequest, so that should be
  // reflected.
  EXPECT_THAT(bytes_sent, Ge(0));
  // The 'received' network stat should be 0 OTOH, since issuing the request
  // failed.
  EXPECT_EQ(bytes_received, 0);
}

// Tests the case where the request gets interrupted.
TEST_F(PerformRequestsTest, PerformRequestInMemoryCancellation) {
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kGet, {}, "",
                                  /*use_compression=*/false);
  ASSERT_OK(request);

  absl::Notification request_issued;
  // We expect one calls to the cancellation listener.
  absl::BlockingCounter counter_should_abort(1);
  // When the HttpClient receives a HttpRequestHandle::Cancel call, we decrement
  // the counter.
  mock_http_client_.SetCancellationListener(
      [&counter_should_abort]() { counter_should_abort.DecrementCount(); });

  // Make HttpClient::PerformRequests() block until the counter is decremented.
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre((*request)->uri(), (*request)->method(), _, _)))
      .WillOnce([&request_issued, &counter_should_abort](
                    MockableHttpClient::SimpleHttpRequest ignored) {
        request_issued.Notify();
        counter_should_abort.Wait();
        return FakeHttpResponse(503, {}, "");
      });
  // Make should_abort return false until we know that the request was issued
  // (i.e. once InterruptibleRunner has actually started running the code it was
  // given), and then make it return true, triggering an abort sequence and
  // unblocking the PerformRequests() call we caused to block above.
  EXPECT_CALL(mock_should_abort_, Call()).WillRepeatedly([&request_issued] {
    return request_issued.HasBeenNotified();
  });

  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  // The request should result in a CANCELLED outcome.
  absl::StatusOr<InMemoryHttpResponse> result = PerformRequestInMemory(
      mock_http_client_, interruptible_runner_, *std::move(request),
      &bytes_received, &bytes_sent, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);

  EXPECT_THAT(result, IsCode(CANCELLED));
  EXPECT_THAT(result.status().message(),
              HasSubstr("cancelled after graceful wait"));

  // The network stats should still have been updated though (to reflect the
  // data sent and received up until the point of interruption).
  EXPECT_THAT(bytes_sent, Ge(0));
  EXPECT_THAT(bytes_received, Ge(0));
}

TEST_F(PerformRequestsTest, PerformTwoRequestsInMemoryOk) {
  std::string expected_request_body = "request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kPost,
          {{"Some-Request-Header", "foo"}}, expected_request_body,
          /*use_compression=*/false);
  ASSERT_OK(request);
  std::string another_expected_request_body = "request_body_2";
  absl::StatusOr<std::unique_ptr<HttpRequest>> another_request =
      InMemoryHttpRequest::Create("https://valid.com",
                                  HttpRequest::Method::kPost,
                                  {{"Some-Other-Request-Header", "foo2"}},
                                  another_expected_request_body,
                                  /*use_compression=*/false);
  ASSERT_OK(another_request);

  std::string expected_response_body = "response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre((*request)->uri(), (*request)->method(),
                            Contains(Header{"Some-Request-Header", "foo"}),
                            StrEq(expected_request_body))))
      .WillOnce(Return(FakeHttpResponse(
          kHttpOk, {{"Some-Response-Header", "bar"}}, expected_response_body)));
  std::string another_expected_response_body = "another_response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(FieldsAre(
                  (*request)->uri(), (*request)->method(),
                  Contains(Header{"Some-Other-Request-Header", "foo2"}),
                  StrEq(another_expected_request_body))))
      .WillOnce(Return(
          FakeHttpResponse(kHttpOk, {{"Some-Other-Response-Header", "bar2"}},
                           another_expected_response_body)));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  std::vector<std::unique_ptr<HttpRequest>> requests;
  requests.push_back(*std::move(request));
  requests.push_back(*std::move(another_request));
  absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>> results =
      PerformMultipleRequestsInMemoryWithRetry(
          mock_http_client_, interruptible_runner_, std::move(requests),
          // We pass in non-null pointers for the network
          // stats, to ensure they are correctly updated.
          &bytes_received, &bytes_sent, /*clock=*/nullptr, /*bit_gen=*/nullptr,
          /*retry_max_attempts=*/0,
          /*retry_delay_ms=*/0);
  ASSERT_OK(results);
  ASSERT_EQ(results->size(), 2);

  auto first_response = (*results)[0];
  ASSERT_OK(first_response);
  EXPECT_THAT(*first_response, FieldsAre(kHttpOk, IsEmpty(), IsEmpty(),
                                         StrEq(expected_response_body)));
  auto second_response = (*results)[1];
  ASSERT_OK(second_response);
  EXPECT_THAT(*second_response,
              FieldsAre(kHttpOk, IsEmpty(), IsEmpty(),
                        StrEq(another_expected_response_body)));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent, Ge(expected_request_body.size() +
                             another_expected_request_body.size()));
  EXPECT_THAT(bytes_received, Ge(expected_response_body.size() +
                                 another_expected_response_body.size()));
}

TEST_F(PerformRequestsTest, PerformTwoRequestsWithOneFailedOneSuccess) {
  std::string success_request_body = "success_request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> success_request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kPost,
          {{"Some-Request-Header", "foo"}}, success_request_body,
          /*use_compression=*/false);
  ASSERT_OK(success_request);
  std::string failure_request_body = "failure_request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> failure_request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kPost,
          {{"Some-Other-Request-Header", "foo2"}}, failure_request_body,
          /*use_compression=*/false);
  ASSERT_OK(failure_request);

  int ok_response_code = kHttpOk;
  std::string success_response_body = "response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(FieldsAre(
                  (*success_request)->uri(), (*success_request)->method(),
                  Contains(Header{"Some-Request-Header", "foo"}),
                  StrEq(success_request_body))))
      .WillOnce(Return(FakeHttpResponse(ok_response_code,
                                        {{"Some-Response-Header", "bar"}},
                                        success_response_body)));
  std::string failure_response_body = "failure_response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(FieldsAre(
                  (*failure_request)->uri(), (*failure_request)->method(),
                  Contains(Header{"Some-Other-Request-Header", "foo2"}),
                  StrEq(failure_request_body))))
      .WillOnce(
          Return(FakeHttpResponse(kHttpNotFound, {}, failure_response_body)));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  std::vector<std::unique_ptr<HttpRequest>> requests;
  requests.push_back(*std::move(success_request));
  requests.push_back(*std::move(failure_request));
  absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>> results =
      PerformMultipleRequestsInMemoryWithRetry(
          mock_http_client_, interruptible_runner_, std::move(requests),
          // We pass in non-null pointers for the network
          // stats, to ensure they are correctly updated.
          &bytes_received, &bytes_sent, /*clock=*/nullptr, /*bit_gen=*/nullptr,
          /*retry_max_attempts=*/0,
          /*retry_delay_ms=*/0);
  ASSERT_OK(results);
  ASSERT_EQ(results->size(), 2);
  auto first_response = (*results)[0];
  ASSERT_OK(first_response);
  EXPECT_THAT(*first_response, FieldsAre(ok_response_code, IsEmpty(), IsEmpty(),
                                         StrEq(success_response_body)));

  EXPECT_THAT(results->at(1), IsCode(NOT_FOUND));
  EXPECT_THAT(results->at(1).status().message(), HasSubstr("404"));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent,
              Ge(success_request_body.size() + failure_request_body.size()));
  EXPECT_THAT(bytes_received,
              Ge(success_response_body.size() + failure_response_body.size()));
}

TEST_F(PerformRequestsTest, PerformTwoRequestsWithOneFailThenRetryBothSuccess) {
  std::string success_request_body = "success_request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> success_request =
      InMemoryHttpRequest::Create(
          "https://valid.com", HttpRequest::Method::kPost,
          {{"Some-Request-Header", "foo"}}, success_request_body,
          /*use_compression=*/false);
  ASSERT_OK(success_request);
  std::string failure_request_body = "failure_request_body";
  absl::StatusOr<std::unique_ptr<HttpRequest>> failure_request =
      InMemoryHttpRequest::Create(
          "https://valid2.com", HttpRequest::Method::kPost,
          {{"Some-Other-Request-Header", "foo2"}}, failure_request_body,
          /*use_compression=*/false);
  ASSERT_OK(failure_request);

  int ok_response_code = kHttpOk;
  std::string success_response_body = "response_body";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(FieldsAre(
                  (*success_request)->uri(), (*success_request)->method(),
                  Contains(Header{"Some-Request-Header", "foo"}),
                  StrEq(success_request_body))))
      .WillOnce(Return(FakeHttpResponse(ok_response_code,
                                        {{"Some-Response-Header", "bar"}},
                                        success_response_body)));

  std::string failure_response_body = "failure_response_body";
  {
    InSequence seq;
    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(FieldsAre(
                    (*failure_request)->uri(), (*failure_request)->method(),
                    Contains(Header{"Some-Other-Request-Header", "foo2"}),
                    StrEq(failure_request_body))))
        .WillOnce(Return(FakeHttpResponse(kHttpServiceUnavailable, {},
                                          failure_response_body)));

    EXPECT_CALL(mock_http_client_,
                PerformSingleRequest(FieldsAre(
                    (*failure_request)->uri(), (*failure_request)->method(),
                    Contains(Header{"Some-Other-Request-Header", "foo2"}),
                    StrEq(failure_request_body))))
        .WillOnce(Return(
            FakeHttpResponse(ok_response_code, {}, success_response_body)));
  }

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  std::vector<std::unique_ptr<HttpRequest>> requests;
  requests.push_back(*std::move(success_request));
  requests.push_back(*std::move(failure_request));
  absl::StatusOr<std::vector<absl::StatusOr<InMemoryHttpResponse>>> results =
      PerformMultipleRequestsInMemoryWithRetry(
          mock_http_client_, interruptible_runner_, std::move(requests),
          // We pass in non-null pointers for the network
          // stats, to ensure they are correctly updated.
          &bytes_received, &bytes_sent, /*clock=*/Clock::RealClock(), &bit_gen_,
          /*retry_max_attempts=*/1,
          /*retry_delay_ms=*/50);
  ASSERT_OK(results);
  ASSERT_EQ(results->size(), 2);
  auto first_response = (*results)[0];
  ASSERT_OK(first_response);
  EXPECT_THAT(*first_response, FieldsAre(ok_response_code, IsEmpty(), IsEmpty(),
                                         StrEq(success_response_body)));

  ASSERT_OK(results->at(1));
  EXPECT_THAT(*results->at(1), FieldsAre(kHttpOk, IsEmpty(), IsEmpty(),
                                         StrEq(success_response_body)));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent, Ge(success_request_body.size() +
                             failure_request_body.size() * 2));
  EXPECT_THAT(bytes_received, Ge(failure_response_body.size() +
                                 success_response_body.size() * 2));
}

// Tests the case where a zero-length vector of UriOrInlineData is passed in. It
// should result in a zero-length result vector (as opposed to an error or a
// crash).
TEST_F(PerformRequestsTest, FetchResourcesInMemoryEmptyInputVector) {
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {}, nullptr, nullptr,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  ASSERT_OK(result);
  EXPECT_THAT(*result, IsEmpty());
}

// Tests the case where both fields of UriOrInlineData are empty. The empty
// inline_data field should be returned.
TEST_F(PerformRequestsTest, FetchResourcesInMemoryEmptyUriAndInline) {
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_,
      {UriOrInlineData::CreateInlineData(absl::Cord(),
                                         CompressionFormat::kUncompressed)},
      nullptr, nullptr,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(kHttpOk, IsEmpty(), kOctetStream, IsEmpty()));
}

// Tests the case where one of the URIs is invalid. The whole request should
// result in an error in that case.
TEST_F(PerformRequestsTest, FetchResourcesInMemoryInvalidUri) {
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_,
      {UriOrInlineData::CreateUri("https://valid.com", "",
                                  absl::ZeroDuration()),
       UriOrInlineData::CreateUri("http://invalid.com", "",
                                  absl::ZeroDuration())},
      nullptr, nullptr,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  EXPECT_THAT(result, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(result.status().message(), HasSubstr("Non-HTTPS"));
}

// Tests the case where all of the requested resources must be fetched via URI.
TEST_F(PerformRequestsTest, FetchResourcesInMemoryAllUris) {
  const std::string uri1 = "https://valid.com/1";
  const std::string uri2 = "https://valid.com/2";
  const std::string uri3 = "https://valid.com/3";
  const std::string uri4 = "https://valid.com/4";
  auto resource1 = UriOrInlineData::CreateUri(uri1, "", absl::ZeroDuration());
  auto resource2 = UriOrInlineData::CreateUri(uri2, "", absl::ZeroDuration());
  auto resource3 = UriOrInlineData::CreateUri(uri3, "", absl::ZeroDuration());
  auto resource4 = UriOrInlineData::CreateUri(uri4, "", absl::ZeroDuration());

  int expected_response_code1 = kHttpOk;
  int expected_response_code2 = kHttpNotFound;
  int expected_response_code3 = kHttpServiceUnavailable;
  int expected_response_code4 = 204;  // "204 No Content"
  std::string expected_response_body1 = "response_body1";
  std::string expected_response_body4 = "response_body4";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri1, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code1, {},
                                        expected_response_body1)));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri2, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code2, {}, "")));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri3, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code3, {}, "")));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri4, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code4, {},
                                        expected_response_body4)));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_,
      {resource1, resource2, resource3, resource4},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(expected_response_code1, IsEmpty(), IsEmpty(),
                        StrEq(expected_response_body1)));
  EXPECT_THAT((*result)[1], IsCode(NOT_FOUND));
  EXPECT_THAT((*result)[1].status().message(), HasSubstr("404"));
  EXPECT_THAT((*result)[2], IsCode(UNAVAILABLE));
  EXPECT_THAT((*result)[2].status().message(), HasSubstr("503"));
  ASSERT_OK((*result)[3]);
  EXPECT_THAT(*(*result)[3],
              FieldsAre(expected_response_code4, IsEmpty(), IsEmpty(),
                        StrEq(expected_response_body4)));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent, Ge(0));
  EXPECT_THAT(bytes_received, Ge(0));
}

// Tests the case where some of the requested resources have inline data
// available.
TEST_F(PerformRequestsTest, FetchResourcesInMemorySomeInlineData) {
  const std::string uri1 = "https://valid.com/1";
  const std::string uri3 = "https://valid.com/3";
  std::string expected_response_body2 = "response_body2";
  std::string expected_response_body4 = "response_body4";
  auto resource1 = UriOrInlineData::CreateUri(uri1, "", absl::ZeroDuration());
  auto resource2 = UriOrInlineData::CreateInlineData(
      absl::Cord(expected_response_body2), CompressionFormat::kUncompressed);
  auto resource3 = UriOrInlineData::CreateUri(uri3, "", absl::ZeroDuration());
  auto resource4 = UriOrInlineData::CreateInlineData(
      absl::Cord(expected_response_body4), CompressionFormat::kUncompressed);

  int expected_response_code1 = kHttpServiceUnavailable;
  int expected_response_code3 = 204;  // "204 No Content"
  std::string expected_response_body3 = "response_body3";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri1, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code1, {}, "")));
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri3, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code3, {},
                                        expected_response_body3)));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_,
      {resource1, resource2, resource3, resource4}, &bytes_received,
      &bytes_sent,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  EXPECT_THAT((*result)[0], IsCode(UNAVAILABLE));
  EXPECT_THAT((*result)[0].status().message(), HasSubstr("503"));
  ASSERT_OK((*result)[1]);
  EXPECT_THAT(*(*result)[1], FieldsAre(kHttpOk, IsEmpty(), kOctetStream,
                                       StrEq(expected_response_body2)));
  ASSERT_OK((*result)[2]);
  EXPECT_THAT(*(*result)[2],
              FieldsAre(expected_response_code3, IsEmpty(), IsEmpty(),
                        StrEq(expected_response_body3)));
  ASSERT_OK((*result)[3]);
  EXPECT_THAT(*(*result)[3], FieldsAre(kHttpOk, IsEmpty(), kOctetStream,
                                       StrEq(expected_response_body4)));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent, Ge(0));
  EXPECT_THAT(bytes_received, Ge(0));
}

// Tests the case where all of the requested resources have inline data
// available (and hence no HTTP requests are expected to be issued).
TEST_F(PerformRequestsTest, FetchResourcesInMemoryOnlyInlineData) {
  std::string expected_response_body1 = "response_body1";
  std::string expected_response_body2 = "response_body2";
  auto resource1 = UriOrInlineData::CreateInlineData(
      absl::Cord(expected_response_body1), CompressionFormat::kUncompressed);
  auto resource2 = UriOrInlineData::CreateInlineData(
      absl::Cord(expected_response_body2), CompressionFormat::kUncompressed);

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource1, resource2},
      &bytes_received, &bytes_sent,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0], FieldsAre(kHttpOk, IsEmpty(), kOctetStream,
                                       StrEq(expected_response_body1)));
  ASSERT_OK((*result)[1]);
  EXPECT_THAT(*(*result)[1], FieldsAre(kHttpOk, IsEmpty(), kOctetStream,
                                       StrEq(expected_response_body2)));

  // The network stats should be untouched, since no network requests were
  // issued.
  EXPECT_EQ(bytes_sent, 0);
  EXPECT_EQ(bytes_received, 0);
}

// Tests the case where the fetches get interrupted.
TEST_F(PerformRequestsTest, FetchResourcesInMemoryCancellation) {
  const std::string uri1 = "https://valid.com/1";
  const std::string uri2 = "https://valid.com/2";
  auto resource1 = UriOrInlineData::CreateUri(uri1, "", absl::ZeroDuration());
  auto resource2 = UriOrInlineData::CreateUri(uri2, "", absl::ZeroDuration());

  EXPECT_CALL(mock_http_client_, PerformSingleRequest(FieldsAre(uri1, _, _, _)))
      .WillOnce(Return(FakeHttpResponse(kHttpOk, {}, "")));

  absl::Notification request_issued;
  // We expect two calls to the cancellation listener, one for each request.
  absl::BlockingCounter counter_should_abort(2);
  // When the HttpClient receives a HttpRequestHandle::Cancel call, we decrement
  // the counter.
  mock_http_client_.SetCancellationListener(
      [&counter_should_abort]() { counter_should_abort.DecrementCount(); });

  // Make HttpClient::PerformRequests() block until the counter is decremented.
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(FieldsAre(uri2, _, _, _)))
      .WillOnce([&request_issued, &counter_should_abort](
                    MockableHttpClient::SimpleHttpRequest ignored) {
        request_issued.Notify();
        counter_should_abort.Wait();
        return FakeHttpResponse(503, {}, "");
      });
  // Make should_abort return false until we know that the 2nd request was
  // issued (i.e. once InterruptibleRunner has actually started running the code
  // it was given), and then make it return true, triggering an abort sequence
  // and unblocking the PerformRequests() call we caused to block above.
  EXPECT_CALL(mock_should_abort_, Call()).WillRepeatedly([&request_issued] {
    return request_issued.HasBeenNotified();
  });

  EXPECT_CALL(mock_log_manager_,
              LogDiag(ProdDiagCode::BACKGROUND_TRAINING_INTERRUPT_HTTP));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  // The request should result in an overall CANCELLED outcome.
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource1, resource2},
      &bytes_received, &bytes_sent,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  EXPECT_THAT(result, IsCode(CANCELLED));
  EXPECT_THAT(result.status().message(),
              HasSubstr("cancelled after graceful wait"));

  // The network stats should still have been updated though (to reflect the
  // data sent and received up until the point of interruption).
  EXPECT_THAT(bytes_sent, Ge(0));
  EXPECT_THAT(bytes_received, Ge(0));
}

TEST_F(PerformRequestsTest, FetchResourcesInMemoryCompressedResources) {
  const std::string uri = "https://valid.com/";
  auto resource1 = UriOrInlineData::CreateUri(uri, "", absl::ZeroDuration());

  int expected_response_code = kHttpOk;
  std::string content_type = "bytes+gzip";
  std::string expected_response_body = "response_body: AAAAAAAAAAAAAAAAAAAAA";
  auto compressed_response_body = CompressWithGzip(expected_response_body);
  ASSERT_OK(compressed_response_body);
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code,
                                        {{kContentTypeHdr, content_type}},
                                        *compressed_response_body)));

  auto resource2 = UriOrInlineData::CreateInlineData(
      absl::Cord(*compressed_response_body), CompressionFormat::kGzip);

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource1, resource2},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(expected_response_code, IsEmpty(), StrEq(content_type),
                        StrEq(expected_response_body)));
  EXPECT_THAT(*(*result)[1],
              FieldsAre(kHttpOk, IsEmpty(), absl::StrCat(kOctetStream, "+gzip"),
                        StrEq(expected_response_body)));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent, Ge(0));
  EXPECT_THAT(bytes_received, Ge(2 * compressed_response_body->size()));
}

TEST_F(PerformRequestsTest,
       FetchResourcesInMemoryCompressedResourcesFailToDecode) {
  const std::string uri = "https://valid.com/";
  auto resource1 = UriOrInlineData::CreateUri(uri, "", absl::ZeroDuration());

  int expected_response_code = kHttpOk;
  std::string content_type = "not-actually-gzipped+gzip";
  std::string expected_response_body = "I am not a valid gzipped body ლ(ಠ益ಠლ)";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code,
                                        {{kContentTypeHdr, content_type}},
                                        expected_response_body)));

  auto resource2 = UriOrInlineData::CreateInlineData(
      absl::Cord(expected_response_body), CompressionFormat::kGzip);

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource1, resource2},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent,
      /*resource_cache=*/nullptr, /*clock=*/nullptr, /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0,
      /*retry_delay_ms=*/0);
  // Fetching will succeed
  ASSERT_OK(result);

  // ...but our responses will have failed to decode.
  EXPECT_THAT((*result)[0], IsCode(INTERNAL));
  EXPECT_THAT((*result)[1], IsCode(INTERNAL));

  EXPECT_THAT(bytes_sent, Ne(bytes_received));
  EXPECT_THAT(bytes_sent, Ge(0));
  EXPECT_THAT(bytes_received, Ge(2 * expected_response_body.size()));
}

TEST_F(PerformRequestsTest, FetchResourcesInMemoryCachedResourceOk) {
  const std::string uri = "https://valid.com/1";
  const std::string cache_id = "(^˵◕ω◕˵^)";
  absl::Cord cached_resource("(((*°▽°*)八(*°▽°*)))");
  absl::Duration max_age = absl::Hours(1);
  int expected_response_code = kHttpOk;
  auto resource = UriOrInlineData::CreateUri(uri, cache_id, max_age);
  auto resource_cache = cache::FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      kMaxCacheSizeBytes);
  ASSERT_OK(resource_cache);
  ASSERT_OK((*resource_cache)
                ->Put(cache_id, cached_resource,
                      MetadataForUncompressedResource(), max_age));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent, resource_cache->get(), /*clock=*/nullptr,
      /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0], FieldsAre(expected_response_code, IsEmpty(),
                                       kOctetStream, StrEq(cached_resource)));

  // Fully from the cache!
  EXPECT_THAT(bytes_sent, Eq(0));
  EXPECT_THAT(bytes_received, Eq(0));
}

TEST_F(PerformRequestsTest,
       FetchResourcesInMemoryCachedResourceOkAndCompressed) {
  const std::string uri = "https://valid.com/1";
  const std::string cache_id = "(^˵◕ω◕˵^)";
  absl::Cord cached_resource("(((*°▽°*)八(*°▽°*)))");
  absl::Cord compressed_cached_resource(
      *CompressWithGzip(std::string(cached_resource)));
  absl::Duration max_age = absl::Hours(1);
  int expected_response_code = kHttpOk;
  auto resource = UriOrInlineData::CreateUri(uri, cache_id, max_age);
  auto resource_cache = cache::FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      kMaxCacheSizeBytes);
  ASSERT_OK(resource_cache);
  ASSERT_OK((*resource_cache)
                ->Put(cache_id, compressed_cached_resource,
                      MetadataForCompressedResource(), max_age));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent, resource_cache->get(), /*clock=*/nullptr,
      /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0], FieldsAre(expected_response_code, IsEmpty(),
                                       kOctetStream, StrEq(cached_resource)));

  // Fully from the cache!
  EXPECT_THAT(bytes_sent, Eq(0));
  EXPECT_THAT(bytes_received, Eq(0));
}

TEST_F(PerformRequestsTest, FetchResourcesInMemoryNotCachedButThenPutInCache) {
  const std::string uri = "https://valid.com/1";
  const std::string cache_id = "(^˵◕ω◕˵^)";
  absl::Cord expected_response_body("(((*°▽°*)八(*°▽°*)))");
  absl::Duration max_age = absl::Hours(1);
  int expected_response_code = kHttpOk;
  std::string content_type = "bytes";
  auto resource = UriOrInlineData::CreateUri(uri, cache_id, max_age);

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code,
                                        {{kContentTypeHdr, content_type}},
                                        std::string(expected_response_body))));

  auto resource_cache = cache::FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      kMaxCacheSizeBytes);
  ASSERT_OK(resource_cache);

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent, resource_cache->get(), /*clock=*/nullptr,
      /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(expected_response_code, IsEmpty(), content_type,
                        StrEq(expected_response_body)));

  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  auto stored_resource = (*resource_cache)->Get(cache_id, std::nullopt);
  ASSERT_OK(stored_resource);
  EXPECT_THAT(*stored_resource,
              FieldsAre(StrEq(expected_response_body),
                        EqualsProto(MetadataForUncompressedResource())));
}

TEST_F(PerformRequestsTest,
       FetchResourcesInMemoryNotCachedButThenPutInCacheCompressed) {
  const std::string uri = "https://valid.com/1";
  const std::string cache_id = "(^˵◕ω◕˵^)";
  absl::Cord expected_response_body("(((*°▽°*)八(*°▽°*)))");
  absl::Duration max_age = absl::Hours(1);
  int expected_response_code = kHttpOk;
  std::string content_type = "bytes+gzip";
  absl::Cord compressed_response_body(
      *CompressWithGzip(std::string(expected_response_body)));
  auto resource = UriOrInlineData::CreateUri(uri, cache_id, max_age);

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(
          expected_response_code, {{kContentTypeHdr, content_type}},
          std::string(compressed_response_body))));

  auto resource_cache = cache::FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      kMaxCacheSizeBytes);
  ASSERT_OK(resource_cache);

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent, resource_cache->get(), /*clock=*/nullptr,
      /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(expected_response_code, IsEmpty(), content_type,
                        StrEq(expected_response_body)));

  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  auto stored_resource = (*resource_cache)->Get(cache_id, std::nullopt);
  ASSERT_OK(stored_resource);
  EXPECT_THAT(*stored_resource,
              FieldsAre(StrEq(compressed_response_body),
                        EqualsProto(MetadataForCompressedResource())));
}

TEST_F(PerformRequestsTest,
       FetchResourcesInMemoryNotCachedPutInCacheWithZeroMaxAgeDoesntCrash) {
  const std::string uri = "https://valid.com/1";
  const std::string cache_id = "(^˵◕ω◕˵^)";
  absl::Cord expected_response_body("(((*°▽°*)八(*°▽°*)))");
  absl::Duration max_age = absl::ZeroDuration();
  int expected_response_code = kHttpOk;
  std::string content_type = "bytes";
  auto resource = UriOrInlineData::CreateUri(uri, cache_id, max_age);

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code,
                                        {{kContentTypeHdr, content_type}},
                                        std::string(expected_response_body))));

  auto resource_cache = cache::FileBackedResourceCache::Create(
      root_files_dir_, root_cache_dir_, &log_manager_, &clock_,
      kMaxCacheSizeBytes);
  ASSERT_OK(resource_cache);

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_MISS));
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent, resource_cache->get(), /*clock=*/nullptr,
      /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(expected_response_code, IsEmpty(), content_type,
                        StrEq(expected_response_body)));
  EXPECT_CALL(log_manager_, LogDiag(DebugDiagCode::RESOURCE_CACHE_HIT));
  auto stored_resource = (*resource_cache)->Get(cache_id, std::nullopt);
  ASSERT_OK(stored_resource);
  EXPECT_THAT(*stored_resource,
              FieldsAre(StrEq(expected_response_body),
                        EqualsProto(MetadataForUncompressedResource())));
}

TEST_F(PerformRequestsTest,
       FetchResourcesInMemoryCachedResourceGetReturnsInternal) {
  const std::string uri = "https://valid.com/1";
  const std::string cache_id = "(^˵◕ω◕˵^)";
  absl::Cord expected_response_body("(((*°▽°*)八(*°▽°*)))");
  absl::Duration max_age = absl::Hours(1);
  int expected_response_code = kHttpOk;
  std::string content_type = "bytes";
  auto resource = UriOrInlineData::CreateUri(uri, cache_id, max_age);
  StrictMock<cache::MockResourceCache> resource_cache;

  EXPECT_CALL(resource_cache, Get(cache_id, std::make_optional(max_age)))
      .WillOnce(Return(absl::InternalError("the cache exploded -_-")));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code,
                                        {{kContentTypeHdr, content_type}},
                                        std::string(expected_response_body))));

  EXPECT_CALL(resource_cache, Put(cache_id, expected_response_body, _, max_age))
      .WillOnce(Return(absl::OkStatus()));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent, &resource_cache, /*clock=*/nullptr,
      /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(expected_response_code, IsEmpty(), content_type,
                        StrEq(expected_response_body)));
}

TEST_F(PerformRequestsTest,
       FetchResourcesInMemoryPutInCacheReturnsInternalDoesntCrash) {
  const std::string uri = "https://valid.com/1";
  const std::string cache_id = "(^˵◕ω◕˵^)";
  absl::Cord expected_response_body("(((*°▽°*)八(*°▽°*)))");
  absl::Duration max_age = absl::Hours(1);
  int expected_response_code = kHttpOk;
  std::string content_type = "bytes";
  auto resource = UriOrInlineData::CreateUri(uri, cache_id, max_age);
  StrictMock<cache::MockResourceCache> resource_cache;

  EXPECT_CALL(resource_cache, Get(cache_id, std::make_optional(max_age)))
      .WillOnce(Return(absl::NotFoundError("not in the cache sorry!")));

  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(
                  FieldsAre(uri, HttpRequest::Method::kGet, HeaderList{}, "")))
      .WillOnce(Return(FakeHttpResponse(expected_response_code,
                                        {{kContentTypeHdr, content_type}},
                                        std::string(expected_response_body))));

  EXPECT_CALL(resource_cache,
              Put(cache_id, expected_response_body,
                  EqualsProto(MetadataForUncompressedResource()), max_age))
      .WillOnce(Return(absl::InternalError("the cache exploded -_-")));

  int64_t bytes_received = 0;
  int64_t bytes_sent = 0;
  auto result = FetchResourcesInMemory(
      mock_http_client_, interruptible_runner_, {resource},
      // We pass in non-null pointers for the network
      // stats, to ensure they are correctly updated.
      &bytes_received, &bytes_sent, &resource_cache, /*clock=*/nullptr,
      /*bit_gen=*/nullptr,
      /*retry_max_attempts=*/0, /*retry_delay_ms=*/0);
  ASSERT_OK(result);

  ASSERT_OK((*result)[0]);
  EXPECT_THAT(*(*result)[0],
              FieldsAre(expected_response_code, IsEmpty(), content_type,
                        StrEq(expected_response_body)));
}

}  // namespace
}  // namespace fcp::client::http
