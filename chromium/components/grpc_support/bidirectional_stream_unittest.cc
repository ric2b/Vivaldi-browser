// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "components/grpc_support/include/bidirectional_stream_c.h"
#include "components/grpc_support/test/get_stream_engine.h"
#include "net/base/net_errors.h"
#include "net/test/quic_simple_test_server.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

bidirectional_stream_header kTestHeaders[] = {
    {"header1", "foo"},
    {"header2", "bar"},
};
const bidirectional_stream_header_array kTestHeadersArray = {2, 2,
                                                             kTestHeaders};
}  // namespace

namespace grpc_support {

class BidirectionalStreamTest : public ::testing::TestWithParam<bool> {
 public:
  BidirectionalStreamTest(const BidirectionalStreamTest&) = delete;
  BidirectionalStreamTest& operator=(const BidirectionalStreamTest&) = delete;

 protected:
  BidirectionalStreamTest() = default;

  void SetUp() override {
    net::QuicSimpleTestServer::Start();
    test_stream_engine_getter_ =
        TestStreamEngineGetter::Create(net::QuicSimpleTestServer::GetPort());
    quic_server_hello_url_ = net::QuicSimpleTestServer::GetHelloURL().spec();
  }

  void TearDown() override {
    test_stream_engine_getter_.reset();
    net::QuicSimpleTestServer::Shutdown();
  }

  stream_engine* engine() { return test_stream_engine_getter_->Get(); }

  const char* test_hello_url() const { return quic_server_hello_url_.c_str(); }

 private:
  std::string quic_server_hello_url_;

  std::unique_ptr<TestStreamEngineGetter> test_stream_engine_getter_;
};

class TestBidirectionalStreamCallback {
 public:
  enum ResponseStep {
    NOTHING,
    ON_STREAM_READY,
    ON_RESPONSE_STARTED,
    ON_READ_COMPLETED,
    ON_WRITE_COMPLETED,
    ON_TRAILERS,
    ON_CANCELED,
    ON_FAILED,
    ON_SUCCEEDED
  };

  struct WriteData {
    // Data must be hard-coded constants, as the raw pointers will be used on
    // the network thread, possibly after the `this` is destroyed.
    std::string_view buffer;

    // If |flush| is true, then bidirectional_stream_flush() will be
    // called after writing of the |buffer|.
    bool flush;

    WriteData(std::string_view buffer, bool flush);

    WriteData(const WriteData&) = delete;
    WriteData& operator=(const WriteData&) = delete;

    ~WriteData();
  };

  raw_ptr<bidirectional_stream, AcrossTasksDanglingUntriaged> stream = nullptr;
  base::WaitableEvent stream_done_event;

  // Test parameters.
  std::list<std::unique_ptr<WriteData>> write_data;
  std::string expected_negotiated_protocol;
  ResponseStep cancel_from_step = NOTHING;
  size_t read_buffer_size = 32768;

  // Test results.
  ResponseStep response_step = NOTHING;
  raw_ptr<char, AcrossTasksDanglingUntriaged> read_buffer = nullptr;
  std::map<std::string, std::string> response_headers;
  std::map<std::string, std::string> response_trailers;
  std::vector<std::string> read_data;
  int net_error = 0;

  TestBidirectionalStreamCallback()
      : stream_done_event(base::WaitableEvent::ResetPolicy::MANUAL,
                          base::WaitableEvent::InitialState::NOT_SIGNALED),
        expected_negotiated_protocol("quic/1+spdy/3") {}

  ~TestBidirectionalStreamCallback() { delete[] read_buffer; }

  static TestBidirectionalStreamCallback* FromStream(
      bidirectional_stream* stream) {
    DCHECK(stream);
    return reinterpret_cast<TestBidirectionalStreamCallback*>(
        stream->annotation);
  }

  virtual bool MaybeCancel(bidirectional_stream* bidir_stream,
                           ResponseStep step) {
    DCHECK_EQ(bidir_stream, stream);
    response_step = step;
    DVLOG(3) << "Step: " << step;

    if (step != cancel_from_step)
      return false;

    bidirectional_stream_cancel(stream);
    bidirectional_stream_write(stream, "abc", 3, false);

    return true;
  }

  void SignalDone() { stream_done_event.Signal(); }

  void BlockForDone() { stream_done_event.Wait(); }

  void AddWriteData(std::string_view data) { AddWriteData(data, true); }
  void AddWriteData(std::string_view data, bool flush) {
    write_data.push_back(std::make_unique<WriteData>(data, flush));
  }

  virtual void MaybeWriteNextData(bidirectional_stream* bidir_stream) {
    DCHECK_EQ(bidir_stream, stream);
    if (write_data.empty())
      return;
    for (const auto& data : write_data) {
      bidirectional_stream_write(stream, data->buffer.data(),
                                 data->buffer.size(),
                                 data == write_data.back());
      if (data->flush) {
        bidirectional_stream_flush(stream);
        break;
      }
    }
  }

  const bidirectional_stream_callback* callback() const {
    static constexpr bidirectional_stream_callback callback = {
        on_stream_ready_callback,
        on_response_headers_received_callback,
        on_read_completed_callback,
        on_write_completed_callback,
        on_response_trailers_received_callback,
        on_succeded_callback,
        on_failed_callback,
        on_canceled_callback};
    return &callback;
  }

 private:
  // C callbacks.
  static void on_stream_ready_callback(bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    if (test->MaybeCancel(stream, ON_STREAM_READY))
      return;
    test->MaybeWriteNextData(stream);
  }

  static void on_response_headers_received_callback(
      bidirectional_stream* stream,
      const bidirectional_stream_header_array* headers,
      const char* negotiated_protocol) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    ASSERT_EQ(test->expected_negotiated_protocol,
              std::string(negotiated_protocol));
    for (size_t i = 0; i < headers->count; ++i) {
      if (test->response_headers.find(headers->headers[i].key) ==
          test->response_headers.end()) {
        test->response_headers[headers->headers[i].key] =
            headers->headers[i].value;
      } else {
        // For testing purposes, headers with the same key are combined with
        // comma.
        test->response_headers[headers->headers[i].key] =
            test->response_headers[headers->headers[i].key] + ", " +
            headers->headers[i].value;
      }
    }
    if (test->MaybeCancel(stream, ON_RESPONSE_STARTED))
      return;
    test->read_buffer = new char[test->read_buffer_size];
    bidirectional_stream_read(stream, test->read_buffer,
                              test->read_buffer_size);
  }

  static void on_read_completed_callback(bidirectional_stream* stream,
                                         char* data,
                                         int count) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->read_data.push_back(std::string(data, count));
    if (test->MaybeCancel(stream, ON_READ_COMPLETED))
      return;
    if (count == 0)
      return;
    bidirectional_stream_read(stream, test->read_buffer,
                              test->read_buffer_size);
  }

  static void on_write_completed_callback(bidirectional_stream* stream,
                                          const char* data) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    ASSERT_EQ(test->write_data.front()->buffer, data);
    if (test->MaybeCancel(stream, ON_WRITE_COMPLETED))
      return;
    bool continue_writing = test->write_data.front()->flush;
    test->write_data.pop_front();
    if (continue_writing)
      test->MaybeWriteNextData(stream);
  }

  static void on_response_trailers_received_callback(
      bidirectional_stream* stream,
      const bidirectional_stream_header_array* trailers) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    for (size_t i = 0; i < trailers->count; ++i) {
      test->response_trailers[trailers->headers[i].key] =
          trailers->headers[i].value;
    }

    if (test->MaybeCancel(stream, ON_TRAILERS))
      return;
  }

  static void on_succeded_callback(bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    ASSERT_TRUE(test->write_data.empty());
    test->MaybeCancel(stream, ON_SUCCEEDED);
    test->SignalDone();
  }

  static void on_failed_callback(bidirectional_stream* stream, int net_error) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->net_error = net_error;
    test->MaybeCancel(stream, ON_FAILED);
    test->SignalDone();
  }

  static void on_canceled_callback(bidirectional_stream* stream) {
    TestBidirectionalStreamCallback* test = FromStream(stream);
    test->MaybeCancel(stream, ON_CANCELED);
    test->SignalDone();
  }
};

TestBidirectionalStreamCallback::WriteData::WriteData(std::string_view data,
                                                      bool flush_after)
    : buffer(data), flush(flush_after) {}

TestBidirectionalStreamCallback::WriteData::~WriteData() = default;

// Regression test for b/144733928. Test that coalesced headers will be split by
// cronet by '\0' separator.
TEST_P(BidirectionalStreamTest, CoalescedHeadersAreSplit) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Hello, ");
  test.AddWriteData("world!");
  test.read_buffer_size = 2;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  // Assert the original "foo\0bar" is split into "foo" and "bar".
  ASSERT_EQ("foo, bar",
            test.response_headers
                [net::QuicSimpleTestServer::GetCombinedHeaderName()]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(std::string(net::QuicSimpleTestServer::GetHelloBodyValue(), 0, 2),
            test.read_data.front());
  // Verify that individual read data joined using empty separator match
  // expected body.
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            base::StrCat(test.read_data));
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloTrailerValue(),
      test.response_trailers[net::QuicSimpleTestServer::GetHelloTrailerName()]);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, StartExampleBidiStream) {
  TestBidirectionalStreamCallback test_callback;
  test_callback.AddWriteData("Hello, ");
  test_callback.AddWriteData("world!");
  // Use small read buffer size to test that response is split properly.
  test_callback.read_buffer_size = 2;
  test_callback.stream = bidirectional_stream_create(engine(), &test_callback,
                                                     test_callback.callback());
  DCHECK(test_callback.stream);
  bidirectional_stream_delay_request_headers_until_flush(test_callback.stream,
                                                         GetParam());
  bidirectional_stream_start(test_callback.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test_callback.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test_callback
          .response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloHeaderValue(),
      test_callback
          .response_headers[net::QuicSimpleTestServer::GetHelloHeaderName()]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED,
            test_callback.response_step);
  ASSERT_EQ(std::string(net::QuicSimpleTestServer::GetHelloBodyValue(), 0, 2),
            test_callback.read_data.front());
  // Verify that individual read data joined using empty separator match
  // expected body.
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            base::StrCat(test_callback.read_data));
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloTrailerValue(),
      test_callback
          .response_trailers[net::QuicSimpleTestServer::GetHelloTrailerName()]);
  bidirectional_stream_destroy(test_callback.stream);
}

TEST_P(BidirectionalStreamTest, SimplePutWithEmptyWriteDataAtTheEnd) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Hello, ");
  test.AddWriteData("world!");
  test.AddWriteData("");
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "PUT",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloHeaderValue(),
      test.response_headers[net::QuicSimpleTestServer::GetHelloHeaderName()]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            test.read_data.front());
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloTrailerValue(),
      test.response_trailers[net::QuicSimpleTestServer::GetHelloTrailerName()]);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimpleGetWithFlush) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "GET",
                             &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloHeaderValue(),
      test.response_headers[net::QuicSimpleTestServer::GetHelloHeaderName()]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            test.read_data.front());
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloTrailerValue(),
      test.response_trailers[net::QuicSimpleTestServer::GetHelloTrailerName()]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimplePostWithFlush) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloHeaderValue(),
      test.response_headers[net::QuicSimpleTestServer::GetHelloHeaderName()]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            base::StrCat(test.read_data));
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloTrailerValue(),
      test.response_trailers[net::QuicSimpleTestServer::GetHelloTrailerName()]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimplePostWithFlushTwice) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloHeaderValue(),
      test.response_headers[net::QuicSimpleTestServer::GetHelloHeaderName()]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            base::StrCat(test.read_data));
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloTrailerValue(),
      test.response_trailers[net::QuicSimpleTestServer::GetHelloTrailerName()]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, SimplePostWithFlushAfterOneWrite) {
  TestBidirectionalStreamCallback test;
  test.AddWriteData("Test String", false);
  test.AddWriteData("1234567890", false);
  test.AddWriteData("woot!", true);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloHeaderValue(),
      test.response_headers[net::QuicSimpleTestServer::GetHelloHeaderName()]);
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            base::StrCat(test.read_data));
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloTrailerValue(),
      test.response_trailers[net::QuicSimpleTestServer::GetHelloTrailerName()]);
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, TestDelayedFlush) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    void MaybeWriteNextData(bidirectional_stream* stream) override {
      DCHECK_EQ(stream, this->stream);
      if (write_data.empty())
        return;
      // Write all buffers when stream is ready.
      // Flush after "3" and "5".
      // EndOfStream is set with "6" but not flushed, so it is not sent.
      if (write_data.front()->buffer == "1") {
        for (const auto& data : write_data) {
          bidirectional_stream_write(stream, data->buffer.data(),
                                     data->buffer.size(),
                                     data == write_data.back());
          if (data->flush) {
            bidirectional_stream_flush(stream);
          }
        }
      }
      // Flush the final buffer with endOfStream flag.
      if (write_data.front()->buffer == "6")
        bidirectional_stream_flush(stream);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("1", false);
  test.AddWriteData("2", false);
  test.AddWriteData("3", true);
  test.AddWriteData("4", false);
  test.AddWriteData("5", true);
  test.AddWriteData("6", false);
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_disable_auto_flush(test.stream, true);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  // Flush before start is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  // Flush after done is ignored.
  bidirectional_stream_flush(test.stream);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, CancelOnRead) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_READ_COMPLETED;
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            test.read_data.front());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_CANCELED, test.response_step);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, CancelOnResponse) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_RESPONSE_STARTED;
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_CANCELED, test.response_step);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, CancelOnSucceeded) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_SUCCEEDED;
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_EQ(
      net::QuicSimpleTestServer::GetHelloStatus(),
      test.response_headers[net::QuicSimpleTestServer::GetStatusHeaderName()]);
  ASSERT_EQ(net::QuicSimpleTestServer::GetHelloBodyValue(),
            test.read_data.front());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_SUCCEEDED, test.response_step);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, ReadFailsBeforeRequestStarted) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  char read_buffer[1];
  bidirectional_stream_read(test.stream, read_buffer, sizeof(read_buffer));
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_UNEXPECTED, test.net_error);
  bidirectional_stream_destroy(test.stream);
}

// TODO(crbug.com/345248264): deflake this test. The issue is likely that
// CustomTestBidirectionalStreamCallback owns the memory for a read buffer
// passed to the BidirectionalStream, and the task posted to tear down the
// BidirectionalStream on the network thread races with destroying the
// CustomTestBidirectionalStreamCallback on the main thread.
TEST_P(BidirectionalStreamTest,
       DISABLED_StreamFailBeforeReadIsExecutedOnNetworkThread) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    bool MaybeCancel(bidirectional_stream* stream, ResponseStep step) override {
      if (step == ResponseStep::ON_READ_COMPLETED) {
        // Shut down the server dispatcher, and the stream should error out.
        net::QuicSimpleTestServer::ShutdownDispatcherForTesting();
      }
      return TestBidirectionalStreamCallback::MaybeCancel(stream, step);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("Hello, ");
  test.AddWriteData("world!");
  test.read_buffer_size = 2;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_TRUE(test.net_error == net::ERR_QUIC_PROTOCOL_ERROR ||
              test.net_error == net::ERR_CONNECTION_REFUSED);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, WriteFailsBeforeRequestStarted) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_write(test.stream, "1", 1, false);
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_UNEXPECTED, test.net_error);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, StreamFailAfterStreamReadyCallback) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    bool MaybeCancel(bidirectional_stream* stream, ResponseStep step) override {
      if (step == ResponseStep::ON_STREAM_READY) {
        // Shut down the server dispatcher, and the stream should error out.
        net::QuicSimpleTestServer::ShutdownDispatcherForTesting();
      }
      return TestBidirectionalStreamCallback::MaybeCancel(stream, step);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("Test String");
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_TRUE(test.net_error == net::ERR_QUIC_PROTOCOL_ERROR ||
              test.net_error == net::ERR_QUIC_HANDSHAKE_FAILED ||
              test.net_error == net::ERR_CONNECTION_REFUSED ||
              test.net_error == net::ERR_QUIC_GOAWAY_REQUEST_CAN_BE_RETRIED)
      << net::ErrorToString(test.net_error);
  bidirectional_stream_destroy(test.stream);
}

// TODO(crbug.com/345248264): deflake this test.
TEST_P(BidirectionalStreamTest,
       DISABLED_StreamFailBeforeWriteIsExecutedOnNetworkThread) {
  class CustomTestBidirectionalStreamCallback
      : public TestBidirectionalStreamCallback {
    bool MaybeCancel(bidirectional_stream* stream, ResponseStep step) override {
      if (step == ResponseStep::ON_WRITE_COMPLETED) {
        // Shut down the server dispatcher, and the stream should error out.
        net::QuicSimpleTestServer::ShutdownDispatcherForTesting();
      }
      return TestBidirectionalStreamCallback::MaybeCancel(stream, step);
    }
  };

  CustomTestBidirectionalStreamCallback test;
  test.AddWriteData("Test String");
  test.AddWriteData("1234567890");
  test.AddWriteData("woot!");
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  bidirectional_stream_start(test.stream, test_hello_url(), 0, "POST",
                             &kTestHeadersArray, false);
  test.BlockForDone();
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_TRUE(test.net_error == net::ERR_QUIC_PROTOCOL_ERROR ||
              test.net_error == net::ERR_QUIC_HANDSHAKE_FAILED);
  bidirectional_stream_destroy(test.stream);
}

TEST_P(BidirectionalStreamTest, FailedResolution) {
  TestBidirectionalStreamCallback test;
  test.stream = bidirectional_stream_create(engine(), &test, test.callback());
  DCHECK(test.stream);
  bidirectional_stream_delay_request_headers_until_flush(test.stream,
                                                         GetParam());
  test.cancel_from_step = TestBidirectionalStreamCallback::ON_FAILED;
  bidirectional_stream_start(test.stream, "https://notfound.example.com", 0,
                             "GET", &kTestHeadersArray, true);
  test.BlockForDone();
  ASSERT_TRUE(test.read_data.empty());
  ASSERT_EQ(TestBidirectionalStreamCallback::ON_FAILED, test.response_step);
  ASSERT_EQ(net::ERR_NAME_NOT_RESOLVED, test.net_error);
  bidirectional_stream_destroy(test.stream);
}

INSTANTIATE_TEST_SUITE_P(BidirectionalStreamDelayRequestHeadersUntilFlush,
                         BidirectionalStreamTest,
                         ::testing::Values(true, false));

}  // namespace grpc_support
