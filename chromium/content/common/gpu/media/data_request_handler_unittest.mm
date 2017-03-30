// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "content/common/gpu/media/data_request_handler.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "content/common/gpu/media/ipc_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestDataSource : public IPCDataSource {
 public:
  TestDataSource()
      : eos_(false),
        error_(false),
        suspended_(false),
        unblock_event_(nullptr) {}

  void set_task_runner(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
    task_runner_ = task_runner;
  }

  void set_data(const std::vector<uint8_t>& data) { data_ = data; }
  void set_eos(bool eos) { eos_ = eos; }
  void set_error(bool error) { error_ = error; }
  void set_blocked_on(base::WaitableEvent* event) {
    unblock_event_ = event;
  }

  void Suspend() override { suspended_ = true; }
  void Resume() override { suspended_ = false; }

  void Read(int64_t position,
            int size,
            uint8_t* data,
            const ReadCB& read_cb) override {
    if (unblock_event_ != nullptr)
      unblock_event_->Wait();

    ASSERT_GE(position, 0);
    ASSERT_GE(size, 0);
    ASSERT_LE(size, DataRequestHandler::kBufferSize);

    if (suspended_) {
      size = kReadInterrupted;
    } else if (eos_) {
      size = 0;
    } else if (error_) {
      size = kReadError;
    } else {
      size = std::max(0, std::min(size, int(data_.size() - position)));
      std::copy(
          data_.begin() + position, data_.begin() + position + size, data);
    }

    task_runner_->PostTask(FROM_HERE, base::Bind(read_cb, size));
  }

  void Stop() override {}
  bool GetSize(int64_t* size_out) override { return false; }
  bool IsStreaming() override { return false; }
  void SetBitrate(int bitrate) override {}

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::vector<uint8_t> data_;
  bool eos_;
  bool error_;
  bool suspended_;
  base::WaitableEvent* unblock_event_;
};

dispatch_queue_t GetTestQueue() {
  static dispatch_queue_t the_queue;
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
      the_queue = dispatch_queue_create("com.operasoftware.TestQueue",
                                        DISPATCH_QUEUE_SERIAL);
  });
  return the_queue;
}

// The data members describing the state of the request are accessed from two
// different dispatch queues.  The tests must take care (e.g., by calling
// WaitUntilFinished()) to avoid races.
class DataRequestHandlerTest : public testing::Test {
 public:
  DataRequestHandlerTest()
      : data_source_thread_("DataRequestHandlerTest"),
        handler_(new DataRequestHandler(
            &data_source_,
            base::Bind(&DataRequestHandlerTest::DataAvailable,
                       base::Unretained(this)),
            base::Bind(&DataRequestHandlerTest::LoadingFinished,
                       base::Unretained(this)),
            GetTestQueue())) {
    data_source_thread_.Start();
    data_source_.set_task_runner(data_source_thread_.task_runner());
  }

  ~DataRequestHandlerTest() override {
    base::STLDeleteContainerPairSecondPointers(requests_.begin(), requests_.end());
  }

 protected:
  struct Generator {
    Generator() : i(0) {}
    int operator()() { return i++; }
    int i;
  };

  void CallHandleDataRequest(id request_handle,
                             ResourceLoadingDataRequest data_request) {
    CHECK_EQ(requests_.count(request_handle), 0u);
    requests_[request_handle] = new Request;

    dispatch_async(GetTestQueue(), ^{
      handler_->HandleDataRequest(request_handle, data_request);
    });
  }

  void WaitUntilDataAvailable(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    ASSERT_TRUE(requests_[request_handle]->data_available_event.TimedWait(
        base::TimeDelta::FromSeconds(3)));
  }

  void WaitUntilFinished(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    ASSERT_TRUE(requests_[request_handle]->finished_event.TimedWait(
        base::TimeDelta::FromSeconds(3)));
  }

  bool is_finished(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    return requests_[request_handle]->finished_event.IsSignaled();
  }
  DataRequestHandler::Status status(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    CHECK(requests_[request_handle]->finished_event.IsSignaled());
    return requests_[request_handle]->status;
  }
  const std::vector<uint8_t>& loaded_data() const { return loaded_data_; }

  base::Thread data_source_thread_;
  TestDataSource data_source_;
  scoped_refptr<DataRequestHandler> handler_;

 private:
  struct Request {
    Request()
        : data_available_event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED),
          finished_event(base::WaitableEvent::ResetPolicy::MANUAL,
                         base::WaitableEvent::InitialState::NOT_SIGNALED),
          status(DataRequestHandler::SUCCESS) {}

    base::WaitableEvent data_available_event;
    base::WaitableEvent finished_event;
    DataRequestHandler::Status status;
  };

  void DataAvailable(id request_handle, uint8_t* data, int size) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    ASSERT_EQ(GetTestQueue(), dispatch_get_current_queue());

    loaded_data_.insert(loaded_data_.end(), data, data + size);

    Request* request = requests_[request_handle];
    ASSERT_FALSE(request->finished_event.IsSignaled());
    request->data_available_event.Signal();
  }

  void LoadingFinished(id request_handle,
                       DataRequestHandler::Status status) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    ASSERT_EQ(GetTestQueue(), dispatch_get_current_queue());

    Request* request = requests_[request_handle];
    ASSERT_FALSE(request->finished_event.IsSignaled());
    request->status = status;
    request->finished_event.Signal();
  }

  std::map<id, Request*> requests_;
  std::vector<uint8_t> loaded_data_;
};


TEST_F(DataRequestHandlerTest, FinishesWithErrorOnDataSourceError) {
  data_source_.set_error(true);

  ResourceLoadingDataRequest request;
  request.length = 1500;

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(nil, request));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(nil));

  EXPECT_EQ(DataRequestHandler::ERROR, status(nil));
}

TEST_F(DataRequestHandlerTest, FinishesWithErrorOnDataSourceEOS) {
  data_source_.set_eos(true);

  ResourceLoadingDataRequest request;
  request.length = 1500;

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(nil, request));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(nil));

  EXPECT_EQ(DataRequestHandler::ERROR, status(nil));
}

TEST_F(DataRequestHandlerTest, DataSourceSizeSmallerThanRequested) {
  std::vector<uint8_t> data(5);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ResourceLoadingDataRequest request;
  request.length = 10;

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(nil, request));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(nil));

  EXPECT_EQ(DataRequestHandler::SUCCESS, status(nil));
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, DataSourceSizeEqualToRequested) {
  std::vector<uint8_t> data(5);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ResourceLoadingDataRequest request;
  request.length = 5;

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(nil, request));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(nil));

  EXPECT_EQ(DataRequestHandler::SUCCESS, status(nil));
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, RequestedLengthGreaterThanBufferSize) {
  std::vector<uint8_t> data(DataRequestHandler::kBufferSize * 2 + 1);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ResourceLoadingDataRequest request;
  request.length = data.size();

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(nil, request));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(nil));

  EXPECT_EQ(DataRequestHandler::SUCCESS, status(nil));
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, LoadRangeOfDataSource) {
  std::vector<uint8_t> data(DataRequestHandler::kBufferSize * 2 + 15);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ResourceLoadingDataRequest request;
  request.offset = 5;
  request.length = data.size() - 10;

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(nil, request));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(nil));

  EXPECT_EQ(DataRequestHandler::SUCCESS, status(nil));

  data.erase(data.begin(), data.begin() + 5);
  data.erase(data.end() - 5, data.end());
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, CanBeCanceled) {
  // Pick IPCDataSource and data request sizes so that the IPCDataSource can't
  // satisfy the request in one go.
  std::vector<uint8_t> data(DataRequestHandler::kBufferSize * 2 + 15);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ResourceLoadingDataRequest request;
  request.length = data.size();

  // Make sure our IPCDataSource will not respond with the first piece of data
  // until we tell it to.
  base::WaitableEvent unblock_data_source(base::WaitableEvent::ResetPolicy::MANUAL,
                                          base::WaitableEvent::InitialState::NOT_SIGNALED);
  data_source_.set_blocked_on(&unblock_data_source);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(nil, request));

  dispatch_async(GetTestQueue(), ^{
    handler_->CancelDataRequest(nil);
  });

  unblock_data_source.Signal();

  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(nil));

  EXPECT_EQ(DataRequestHandler::CANCELED, status(nil));
}

TEST_F(DataRequestHandlerTest, ConcurrentRequests) {
  // Pick IPCDataSource and data request sizes so that the DataSource can't
  // satisfy the request in one go.
  ResourceLoadingDataRequest request;
  request.length = DataRequestHandler::kBufferSize + 15;

  std::vector<uint8_t> data(request.length * 2);
  for (int i = 0; i < 2; ++i) {
    std::fill(data.begin() + request.length * i,
              data.begin() + request.length * (i + 1), i);
  }
  data_source_.set_data(data);

  // Make sure our IPCDataSource will not respond with the first piece of data
  // until we tell it to.  Set |manual_reset| to false so that our
  // IPCDataSource blocks again after handling one Read().
  base::WaitableEvent unblock_data_source(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                          base::WaitableEvent::InitialState::NOT_SIGNALED);
  data_source_.set_blocked_on(&unblock_data_source);

  request.offset = 0;
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, request));

  // Tell IPCDataSource to handle exactly one Read().
  unblock_data_source.Signal();
  ASSERT_NO_FATAL_FAILURE(WaitUntilDataAvailable(@0));

  ASSERT_FALSE(is_finished(@0));

  request.offset = request.length;
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, request));

  // At this point, we should have 2 "concurrent" requests.

  data_source_.set_blocked_on(nullptr);
  unblock_data_source.Signal();

  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(@1));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(@0));

  EXPECT_EQ(DataRequestHandler::SUCCESS, status(@0));
  EXPECT_EQ(DataRequestHandler::SUCCESS, status(@1));

  // This makes sure data is loaded from lowest offest to highest even when
  // there are concurrent requests.
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, ConcurrentRequestsError) {
  data_source_.set_error(true);

  ResourceLoadingDataRequest request;
  request.length = DataRequestHandler::kBufferSize + 15;

  // Make sure our IPCDataSource will not respond until we tell it to.
  base::WaitableEvent unblock_data_source(base::WaitableEvent::ResetPolicy::MANUAL,
                                          base::WaitableEvent::InitialState::NOT_SIGNALED);
  data_source_.set_blocked_on(&unblock_data_source);

  request.offset = 0;
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, request));
  request.offset = request.length;
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, request));

  // At this point, we should have 2 "concurrent" requests.

  unblock_data_source.Signal();

  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(@0));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(@1));

  EXPECT_EQ(DataRequestHandler::ERROR, status(@0));
  EXPECT_EQ(DataRequestHandler::ERROR, status(@1));
}

TEST_F(DataRequestHandlerTest, SuspendResume) {
  std::vector<uint8_t> data(12);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ResourceLoadingDataRequest request;
  request.length = data.size();

  // Make sure our IPCDataSource will not respond with the first piece of data
  // until we tell it to.  Set |manual_reset| to false so that our
  // IPCDataSource blocks again after handling one Read().
  base::WaitableEvent unblock_data_source(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                                          base::WaitableEvent::InitialState::NOT_SIGNALED);
  data_source_.set_blocked_on(&unblock_data_source);

  data_source_.Suspend();

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, request));

  unblock_data_source.Signal();

  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(@0));
  EXPECT_EQ(DataRequestHandler::ERROR, status(@0));
  EXPECT_TRUE(loaded_data().empty());

  data_source_.Resume();

  data_source_.set_blocked_on(nullptr);
  unblock_data_source.Signal();

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, request));
  ASSERT_NO_FATAL_FAILURE(WaitUntilFinished(@1));

  EXPECT_EQ(DataRequestHandler::SUCCESS, status(@1));
  EXPECT_EQ(data, loaded_data());
}

}  // namespace
}  // namespace content
