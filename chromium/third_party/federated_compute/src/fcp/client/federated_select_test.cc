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
#include "fcp/client/federated_select.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "fcp/base/compression.h"
#include "fcp/base/monitoring.h"
#include "fcp/client/client_runner.h"
#include "fcp/client/diag_codes.pb.h"
#include "fcp/client/engine/example_iterator_factory.h"
#include "fcp/client/http/http_client.h"
#include "fcp/client/http/testing/test_helpers.h"
#include "fcp/client/interruptible_runner.h"
#include "fcp/client/simple_task_environment.h"
#include "fcp/client/stats.h"
#include "fcp/client/test_helpers.h"
#include "fcp/protos/plan.pb.h"
#include "fcp/testing/testing.h"

namespace fcp::client {
namespace {

using ::fcp::CompressWithGzip;
using ::fcp::IsCode;
using ::fcp::client::ExampleIterator;
using ::fcp::client::engine::ExampleIteratorFactory;
using ::fcp::client::http::FakeHttpResponse;
using ::fcp::client::http::HeaderList;
using ::fcp::client::http::HttpRequest;
using ::fcp::client::http::HttpRequestHandle;
using ::fcp::client::http::MockHttpClient;
using ::fcp::client::http::SimpleHttpRequestMatcher;
using ::google::internal::federated::plan::ExampleSelector;
using ::google::internal::federated::plan::SlicesSelector;
using ::testing::_;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrictMock;

ExampleSelector CreateExampleSelector(const std::string& served_at_id,
                                      std::vector<int32_t> keys) {
  ExampleSelector example_selector;
  *example_selector.mutable_collection_uri() = "internal:/federated_select";
  SlicesSelector slices_selector;
  *slices_selector.mutable_served_at_id() = served_at_id;
  slices_selector.mutable_keys()->Add(keys.begin(), keys.end());
  example_selector.mutable_criteria()->PackFrom(slices_selector);
  return example_selector;
}

bool FileExists(const std::string& path) {
  std::ifstream istream(path);
  return istream.good();
}

std::string ReadFile(const std::string& path) {
  std::ifstream istream(path);
  FCP_CHECK(istream);
  std::stringstream stringstream;
  stringstream << istream.rdbuf();
  return stringstream.str();
}

class HttpFederatedSelectManagerTest : public ::testing::Test {
 protected:
  HttpFederatedSelectManagerTest()
      : fedselect_manager_(
            &mock_log_manager_, &files_impl_, &mock_http_client_,
            mock_should_abort_.AsStdFunction(),
            InterruptibleRunner::TimingConfig{
                .polling_period = absl::ZeroDuration(),
                .graceful_shutdown_period = absl::InfiniteDuration(),
                .extended_shutdown_period = absl::InfiniteDuration()}) {}

  void SetUp() override {
    EXPECT_CALL(mock_flags_, enable_federated_select())
        .WillRepeatedly(Return(true));
  }

  void TearDown() override {
    // Regardless of the outcome of the test (or the protocol interaction being
    // tested), network usage must always be reflected in the network stats
    // methods.
    HttpRequestHandle::SentReceivedBytes sent_received_bytes =
        mock_http_client_.TotalSentReceivedBytes();
    NetworkStats network_stats = fedselect_manager_.GetNetworkStats();
    EXPECT_EQ(network_stats.bytes_downloaded,
              sent_received_bytes.received_bytes);
    EXPECT_EQ(network_stats.bytes_uploaded, sent_received_bytes.sent_bytes);
    // If any network traffic occurred, we expect to see some time reflected in
    // the duration.
    if (network_stats.bytes_uploaded > 0) {
      EXPECT_THAT(network_stats.network_duration, Gt(absl::ZeroDuration()));
    }
  }

  NiceMock<MockLogManager> mock_log_manager_;
  MockFlags mock_flags_;
  fcp::client::FilesImpl files_impl_;
  StrictMock<MockHttpClient> mock_http_client_;
  NiceMock<MockFunction<bool()>> mock_should_abort_;

  HttpFederatedSelectManager fedselect_manager_;
};

TEST_F(HttpFederatedSelectManagerTest,
       IteratorFactoryShouldHandleValidSelector) {
  // Should be handled by the factory.
  ExampleSelector selector =
      CreateExampleSelector(/*served_at_id=*/"foo", /*keys=*/{1});

  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          "https://foo.bar");

  EXPECT_TRUE(iterator_factory->CanHandle(selector));
}

TEST_F(HttpFederatedSelectManagerTest,
       IteratorFactoryShouldNotHandleUnrelatedSelector) {
  // Should not be handled by the factory.
  ExampleSelector selector;
  *selector.mutable_collection_uri() = "internal:/foo";

  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          "https://foo.bar");

  EXPECT_FALSE(iterator_factory->CanHandle(selector));
  EXPECT_THAT(iterator_factory->CreateExampleIterator(selector),
              IsCode(INVALID_ARGUMENT));
}

TEST_F(HttpFederatedSelectManagerTest, IteratorFactoryShouldNotCollectStats) {
  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          "https://foo.bar");

  EXPECT_FALSE(iterator_factory->ShouldCollectStats());
}

TEST_F(HttpFederatedSelectManagerTest,
       EmptyUriTemplateShouldFailAllExampleQueries) {
  ExampleSelector selector =
      CreateExampleSelector(/*served_at_id=*/"foo", /*keys=*/{1});

  EXPECT_CALL(
      mock_log_manager_,
      LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_REQUESTED_BUT_DISABLED));

  // Create an iterator factory using an empty base URI (indicating that the
  // server didn't provide us with a federated select URI template, i.e. the
  // feature is disabled on the server or the plan doesn't use the Federated
  // Select feature).
  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          /*uri_template=*/"");
  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator =
      iterator_factory->CreateExampleIterator(selector);

  // The iterator creation should have failed, since the URI template was empty.
  EXPECT_THAT(iterator, IsCode(INVALID_ARGUMENT));
  EXPECT_THAT(iterator.status().message(), HasSubstr("disabled"));
  // Even though creating an iterator should fail since the feature is disabled,
  // the iterator factory should still handle all internal:/federated_select
  // example queries (rather than let them bubble up to the default
  // environment-provided example iterator factory, which won't know how to
  // handle them anyway).
  EXPECT_TRUE(iterator_factory->CanHandle(selector));
}

/** Tests the "happy" path. Two separate federated select slice fetching queries
 * are received by a single iterator factory, each serving different slice data
 * to the client. All slice fetches are successful and should result in the
 * correct data being returned, in the right order.*/
TEST_F(HttpFederatedSelectManagerTest,
       SuccessfullyFetchMultipleSlicesAcrossMultipleIterators) {
  const std::string uri_template =
      "https://foo.bar/{served_at_id}/baz/{key_base10}/bazz";

  // Create an iterator factory with a valid (non-empty) URI template.
  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          uri_template);

  // Once the first iterator is created we expect the following slice fetch
  // requests to be issued immediately.
  const std::string expected_key1_data = "key1_data";
  EXPECT_CALL(
      mock_http_client_,
      PerformSingleRequest(SimpleHttpRequestMatcher(
          // Note that the '/' in "id/X" is *not* URI-escaped.
          "https://foo.bar/id/X/baz/1/bazz", HttpRequest::Method::kGet, _, "")))
      .WillOnce(
          Return(FakeHttpResponse(200, HeaderList(), expected_key1_data)));

  const std::string expected_key2_data = "key2_data";
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://foo.bar/id/X/baz/2/bazz",
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(
          Return(FakeHttpResponse(200, HeaderList(), expected_key2_data)));

  {
    InSequence in_sequence;
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_REQUESTED));
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_SUCCEEDED));
  }

  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator1 =
      iterator_factory->CreateExampleIterator(CreateExampleSelector(
          // Note that we use a served_at_id value with a '/' in it. It should
          // *not* get URI-escaped, as per the FederatedSelectUriInfo docs.
          /*served_at_id=*/"id/X",
          // Also note that we request slices 2 and 1, in that exact order. I.e.
          // the first slice data we receive should be slice 2, and the second
          // slice data we receive should be slice 1.
          /*keys=*/{2, 1}));

  // The iterator creation should have succeeded.
  ASSERT_OK(iterator1);

  // Reading the data for each of the slices should now succeed.
  absl::StatusOr<std::string> first_slice = (*iterator1)->Next();
  ASSERT_OK(first_slice);
  ASSERT_TRUE(FileExists(*first_slice));
  EXPECT_THAT(ReadFile(*first_slice), expected_key2_data);

  absl::StatusOr<std::string> second_slice = (*iterator1)->Next();
  ASSERT_OK(second_slice);
  ASSERT_TRUE(FileExists(*second_slice));
  EXPECT_THAT(ReadFile(*second_slice), expected_key1_data);

  // We should now have reached the end of the first iterator.
  EXPECT_THAT((*iterator1)->Next(), IsCode(OUT_OF_RANGE));

  // Closing the iterator should not fail/crash.
  (*iterator1)->Close();
  // The slice files we saw earlier (possibly all the same file) should now be
  // deleted.
  ASSERT_FALSE(FileExists(*first_slice));
  ASSERT_FALSE(FileExists(*second_slice));

  const std::string expected_key99_data = "key99_data";
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://foo.bar/id/Y/baz/99/bazz",
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(
          Return(FakeHttpResponse(200, HeaderList(), expected_key99_data)));

  {
    InSequence in_sequence;
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_REQUESTED));
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_SUCCEEDED));
  }

  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator2 =
      iterator_factory->CreateExampleIterator(
          CreateExampleSelector(/*served_at_id=*/"id/Y", /*keys=*/{99}));

  // The iterator creation should have succeeded.
  ASSERT_OK(iterator2);

  // Reading the data for the slices should now succeed.
  absl::StatusOr<std::string> third_slice = (*iterator2)->Next();
  ASSERT_OK(third_slice);
  ASSERT_TRUE(FileExists(*third_slice));
  EXPECT_THAT(ReadFile(*third_slice), expected_key99_data);

  // We purposely do not close the 2nd iterator, nor iterate it all the way to
  // the end until we receive OUT_OF_RANGE, but instead simply destroy it. This
  // should have the same effect as closing it, and cause the file to be
  // deleted.
  *iterator2 = nullptr;
  ASSERT_FALSE(FileExists(*third_slice));
}

/** Tests the case where the fetched resources are compressed using the
 * "Content-Type: ...+gzip" approach. The data should be decompressed before
 * being returned.
 */
TEST_F(HttpFederatedSelectManagerTest, SuccessfullyFetchCompressedSlice) {
  const std::string uri_template =
      "https://foo.bar/{served_at_id}/{key_base10}";

  // Create an iterator factory with a valid (non-empty) URI template.
  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          uri_template);

  // Once the first iterator is created we expect the following slice fetch
  // requests to be issued immediately.
  const std::string expected_key1_data = "key1_data";
  EXPECT_CALL(mock_http_client_,
              PerformSingleRequest(SimpleHttpRequestMatcher(
                  "https://foo.bar/id-X/1", HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(
          200, HeaderList{{"Content-Type", "application/octet-stream+gzip"}},
          *CompressWithGzip(expected_key1_data))));

  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator =
      iterator_factory->CreateExampleIterator(CreateExampleSelector(
          /*served_at_id=*/"id-X", /*keys=*/{1}));

  // The iterator creation should have succeeded.
  ASSERT_OK(iterator);

  // Reading the data for the slice should now succeed and return the expected
  // (uncompressed) data.
  absl::StatusOr<std::string> slice = (*iterator)->Next();
  ASSERT_OK(slice);
  ASSERT_TRUE(FileExists(*slice));
  EXPECT_THAT(ReadFile(*slice), expected_key1_data);
}

/** Tests the case where the URI template contains the substitution strings more
 * than once. The client should replace *all* of them, not just the first one.
 */
TEST_F(HttpFederatedSelectManagerTest,
       SuccessfullyFetchFromUriTemplateWithMultipleTemplateEntries) {
  const std::string uri_template =
      "https://{served_at_id}.foo.bar/{key_base10}{served_at_id}/baz/"
      "{key_base10}/bazz";

  // Create an iterator factory with a valid (non-empty) URI template.
  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          uri_template);

  // Once the first iterator is created we expect the following slice fetch
  // requests to be issued immediately.
  const std::string expected_key1_data = "key1_data";
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://id-X.foo.bar/1id-X/baz/1/bazz",
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(
          Return(FakeHttpResponse(200, HeaderList(), expected_key1_data)));

  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator =
      iterator_factory->CreateExampleIterator(CreateExampleSelector(
          /*served_at_id=*/"id-X", /*keys=*/{1}));

  // The iterator creation should have succeeded.
  ASSERT_OK(iterator);

  // Reading the data should now succeed.
  absl::StatusOr<std::string> slice = (*iterator)->Next();
  ASSERT_OK(slice);
  ASSERT_TRUE(FileExists(*slice));
  EXPECT_THAT(ReadFile(*slice), expected_key1_data);

  // We should now have reached the end of the first iterator.
  EXPECT_THAT((*iterator)->Next(), IsCode(OUT_OF_RANGE));

  // Closing the iterator should not fail/crash.
  (*iterator)->Close();
  // The slice files we saw earlier (possibly all the same file) should now be
  // deleted.
  ASSERT_FALSE(FileExists(*slice));
}

/** Tests the case where the URI template contains the substitution strings more
 * than once. The client should replace *all* of them, not just the first one.
 */
TEST_F(HttpFederatedSelectManagerTest, ErrorDuringFetch) {
  const std::string uri_template =
      "https://foo.bar/{served_at_id}/{key_base10}";

  // Create an iterator factory with a valid (non-empty) URI template.
  std::unique_ptr<ExampleIteratorFactory> iterator_factory =
      fedselect_manager_.CreateExampleIteratorFactoryForUriTemplate(
          uri_template);

  // Once the first iterator is created we expect the following slice fetch
  // requests to be issued immediately. We'll make the 2nd slice's HTTP request
  // return an error.
  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://foo.bar/id-X/998",
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(200, HeaderList(), "")));

  EXPECT_CALL(mock_http_client_, PerformSingleRequest(SimpleHttpRequestMatcher(
                                     "https://foo.bar/id-X/999",
                                     HttpRequest::Method::kGet, _, "")))
      .WillOnce(Return(FakeHttpResponse(404, HeaderList(), "")));

  {
    InSequence in_sequence;
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_REQUESTED));
    EXPECT_CALL(mock_log_manager_,
                LogDiag(ProdDiagCode::FEDSELECT_SLICE_HTTP_FETCH_FAILED));
  }

  absl::StatusOr<std::unique_ptr<ExampleIterator>> iterator =
      iterator_factory->CreateExampleIterator(CreateExampleSelector(
          /*served_at_id=*/"id-X", /*keys=*/{998, 999}));

  // The iterator creation should fail, since if we can't fetch all of the
  // slices successfully there is no way the plan can continue executing.
  // The error code should be UNAVAILABLE and the error message should include
  // the original NOT_FOUND error code that HTTP's 404 maps to, as well as the
  // URI template to aid debugging.
  EXPECT_THAT(iterator, IsCode(UNAVAILABLE));
  EXPECT_THAT(iterator.status().message(), HasSubstr("fetch request failed"));
  EXPECT_THAT(iterator.status().message(), HasSubstr(uri_template));
  EXPECT_THAT(iterator.status().message(), HasSubstr("NOT_FOUND"));
  // The error message should not contain the exact original HTTP code (since we
  // expect the HTTP layer's error *messages* to not be included in the message
  // returned to the plan).
  EXPECT_THAT(iterator.status().message(), Not(HasSubstr("404")));
  // The error message should not contain the slice IDs either.
  EXPECT_THAT(iterator.status().message(), Not(HasSubstr("998")));
  EXPECT_THAT(iterator.status().message(), Not(HasSubstr("999")));
}

}  // anonymous namespace
}  // namespace fcp::client
