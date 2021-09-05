// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/data_request_handler.h"

#include <algorithm>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/stl_util.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

class TestDataSource {
 public:
  TestDataSource()
      : eos_(false),
        error_(false) {}

  void set_data(const std::vector<uint8_t>& data) { data_ = data; }
  void set_eos(bool eos) { eos_ = eos; }
  void set_error(bool error) { error_ = error; }

  void Read(int64_t position,
            int size,
            ipc_data_source::ReadCB read_cb) {
    ASSERT_GE(position, 0);
    ASSERT_GE(size, 0);
    ASSERT_LE(size, DataRequestHandler::kMaxReadChunkSize);
    ASSERT_TRUE(pending_reply_callback_.is_null());

    if (eos_) {
      size = 0;
    } else if (error_) {
      size = ipc_data_source::kReadError;
    } else {
      size = std::max(0, std::min(size, int(data_.size() - position)));
    }
    pending_reply_callback_ = std::move(read_cb);
    pending_position_ = position;
    pending_reply_size_ = size;
  }

  void CallPendingCallback() {
    CHECK(pending_reply_callback_);
    std::move(pending_reply_callback_)
        .Run(pending_reply_size_, data_.data() + pending_position_);
  }

  ipc_data_source::ReadCB pending_reply_callback_;
  int64_t pending_position_ = 0;
  int pending_reply_size_ = -1;

 private:
  std::vector<uint8_t> data_;
  bool eos_;
  bool error_;
};

// The data members describing the state of the request are accessed from two
// different dispatch queues.  The tests must take care (e.g., by calling
// WaitUntilFinished()) to avoid races.
class DataRequestHandlerTest : public testing::Test {
 public:
  DataRequestHandlerTest()
      : handler_(base::MakeRefCounted<DataRequestHandler>()) {
    handler_->InitSourceReader(base::BindRepeating(
        &TestDataSource::Read, base::Unretained(&data_source_)));
    handler_->InitCallbacks(
        base::BindRepeating(&DataRequestHandlerTest::DataAvailable,
                            base::Unretained(this)),
        base::BindRepeating(&DataRequestHandlerTest::LoadingFinished,
                            base::Unretained(this)));
  }

  ~DataRequestHandlerTest() override {
    handler_->Stop();
    requests_.clear();
  }

 protected:
  struct Generator {
    Generator() : i(0) {}
    int operator()() { return i++; }
    int i;
  };

  void CallHandleDataRequest(id request_handle,
                             int64_t offset, int64_t length) {
    CHECK_EQ(requests_.count(request_handle), 0u);
    requests_[request_handle] = Request();

    handler_->HandleDataRequest(request_handle, offset, length);
  }

  void RunUntilDataAvailable(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    while (!requests_[request_handle].data_available &&
           data_source_.pending_reply_callback_) {
      data_source_.CallPendingCallback();
    }
    ASSERT_TRUE(requests_[request_handle].data_available);
  }

  void RunUntilFinished(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    while (!requests_[request_handle].status &&
           data_source_.pending_reply_callback_) {
      data_source_.CallPendingCallback();
    }
    ASSERT_TRUE(requests_[request_handle].status);
  }

  bool is_finished(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    return requests_[request_handle].status.has_value();
  }
  DataRequestHandler::Status status(id request_handle) {
    CHECK_EQ(requests_.count(request_handle), 1u);
    CHECK(requests_[request_handle].status);
    return *requests_[request_handle].status;
  }
  const std::vector<uint8_t>& loaded_data() const { return loaded_data_; }

  TestDataSource data_source_;
  scoped_refptr<DataRequestHandler> handler_;

 private:
  struct Request {
    bool data_available = false;
    base::Optional<DataRequestHandler::Status> status;
  };

  void DataAvailable(id request_handle, const uint8_t* data, int size) {
    CHECK_EQ(requests_.count(request_handle), 1u);

    loaded_data_.insert(loaded_data_.end(), data, data + size);

    Request& request = requests_[request_handle];
    ASSERT_FALSE(request.status.has_value());
    request.data_available = true;
  }

  void LoadingFinished(id request_handle,
                       DataRequestHandler::Status status) {
    CHECK_EQ(requests_.count(request_handle), 1u);

    Request& request = requests_[request_handle];
    ASSERT_FALSE(request.status);
    request.status = status;
  }

  std::map<id, Request> requests_;
  std::vector<uint8_t> loaded_data_;
};

TEST_F(DataRequestHandlerTest, FinishesWithErrorOnDataSourceError) {
  data_source_.set_error(true);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 1500));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_EQ(DataRequestHandler::Status::kError, status(@0));
}

TEST_F(DataRequestHandlerTest, FinishesWithEOSOnDataSourceEOS) {
  data_source_.set_eos(true);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 1500));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_EQ(DataRequestHandler::Status::kEOS, status(@0));
}

TEST_F(DataRequestHandlerTest, DataSourceSizeSmallerThanRequested) {
  std::vector<uint8_t> data(5);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 10));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_EQ(DataRequestHandler::Status::kEOS, status(@0));
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, DataSourceSizeEqualToRequested) {
  std::vector<uint8_t> data(5);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 5));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@0));
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, RequestedLengthGreaterThanBufferSize) {
  std::vector<uint8_t> data(DataRequestHandler::kMaxReadChunkSize * 2 + 1);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, data.size()));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@0));
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, LoadRangeOfDataSource) {
  std::vector<uint8_t> data(DataRequestHandler::kMaxReadChunkSize * 2 + 15);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 5, data.size() - 10));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@0));

  data.erase(data.begin(), data.begin() + 5);
  data.erase(data.end() - 5, data.end());
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, CanBeCanceled) {
  // Pick IPCDataSource and data request sizes so that the IPCDataSource can't
  // satisfy the request in one go.
  std::vector<uint8_t> data(DataRequestHandler::kMaxReadChunkSize * 2 + 15);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, data.size()));
  ASSERT_NO_FATAL_FAILURE(RunUntilDataAvailable(@0));
  EXPECT_TRUE(handler_->IsHandlingDataRequests());
  handler_->CancelDataRequest(@0);
  // The callback should be called immediately
  EXPECT_EQ(DataRequestHandler::Status::kCancelled, status(@0));
  EXPECT_FALSE(handler_->IsHandlingDataRequests());
}

TEST_F(DataRequestHandlerTest, ConcurrentRequests) {
  // Pick IPCDataSource and data request sizes so that the DataSource can't
  // satisfy the request in one go.
  int64_t length = DataRequestHandler::kMaxReadChunkSize + 15;

  std::vector<uint8_t> data(length * 2);
  for (int i = 0; i < 2; ++i) {
    std::fill(data.begin() + length * i,
              data.begin() + length * (i + 1), i);
  }
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, length));
  ASSERT_NO_FATAL_FAILURE(RunUntilDataAvailable(@0));

  ASSERT_FALSE(is_finished(@0));

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, length, length));

  // At this point, we should have 2 "concurrent" requests.
  ASSERT_FALSE(is_finished(@0));
  ASSERT_FALSE(is_finished(@1));

  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));
  ASSERT_FALSE(is_finished(@1));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@1));

  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@0));
  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@1));

  // This makes sure data is loaded from lowest offset to highest even when
  // there are concurrent requests.
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, ConcurrentRequestsError) {
  data_source_.set_error(true);

  int64_t length = DataRequestHandler::kMaxReadChunkSize + 15;

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, length));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, length, length));

  // At this point, we should have 2 "concurrent" requests.

  ASSERT_FALSE(is_finished(@0));
  ASSERT_FALSE(is_finished(@1));

  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@1));

  EXPECT_EQ(DataRequestHandler::Status::kError, status(@0));
  EXPECT_EQ(DataRequestHandler::Status::kAborted, status(@1));
}

TEST_F(DataRequestHandlerTest, Stop) {
  std::vector<uint8_t> data(30);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, 0, 5));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@2, 25, 5));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@3, 5, 20));

  EXPECT_TRUE(handler_->IsHandlingDataRequests());
  EXPECT_TRUE(handler_->CanHandleRequests());
  EXPECT_FALSE(handler_->IsStopped());
  handler_->Stop();
  // Stop should call the callbacks immediately.
  EXPECT_EQ(DataRequestHandler::Status::kAborted, status(@1));
  EXPECT_EQ(DataRequestHandler::Status::kAborted, status(@2));
  EXPECT_EQ(DataRequestHandler::Status::kAborted, status(@3));
  EXPECT_FALSE(handler_->IsHandlingDataRequests());
  EXPECT_FALSE(handler_->CanHandleRequests());
  EXPECT_TRUE(handler_->IsStopped());

  EXPECT_TRUE(loaded_data().empty());
}

TEST_F(DataRequestHandlerTest, SuspendResume) {
  std::vector<uint8_t> data(30);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, 0, 5));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@2, 25, 5));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@3, 5, 20));

  EXPECT_TRUE(handler_->IsHandlingDataRequests());
  EXPECT_TRUE(handler_->CanHandleRequests());
  handler_->Suspend();
  // Suspend should call the callbacks immediately.
  EXPECT_EQ(DataRequestHandler::Status::kAborted, status(@1));
  EXPECT_EQ(DataRequestHandler::Status::kAborted, status(@2));
  EXPECT_EQ(DataRequestHandler::Status::kAborted, status(@3));
  EXPECT_FALSE(handler_->IsHandlingDataRequests());
  EXPECT_FALSE(handler_->CanHandleRequests());
  EXPECT_FALSE(handler_->IsStopped());

  handler_->Resume();
  EXPECT_TRUE(handler_->CanHandleRequests());
  EXPECT_FALSE(handler_->IsHandlingDataRequests());
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@4, 15, 10));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@5, 0, 15));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@6, 25, 5));
  EXPECT_TRUE(handler_->IsHandlingDataRequests());

  EXPECT_TRUE(loaded_data().empty());

  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@4));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@5));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@6));
  ASSERT_TRUE(is_finished(@4));
  ASSERT_TRUE(is_finished(@5));

  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@4));
  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@5));
  EXPECT_EQ(DataRequestHandler::Status::kSuccess, status(@6));

  EXPECT_FALSE(handler_->IsHandlingDataRequests());

  EXPECT_EQ(data, loaded_data());
}

}  // namespace
}  // namespace media
