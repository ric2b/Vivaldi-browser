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
#include "base/memory/read_only_shared_memory_region.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {
class DataRequestHandlerTest;
}  // namespace
}  // namespace media

// AVAssetResourceLoadingRequest and AVAssetResourceLoadingDataRequest do not
// provide a publically accessible init methods so the mock cannot subclass it.
// So we just implement sufficient functionality to cover the usage of the
// interfaces in DataRequestHandler and use reinterpret_cast to get a mock of
// the original interfaces. This is safe as the type is is erased during
// compilation and Objective-C runtime searches methods by their signature.
@interface MockAVAssetResourceLoadingDataRequest : NSObject {
 @private
  media::DataRequestHandlerTest* test_;
  NSNumber* key_;
  int requested_length_;
  long long requested_offset_;
  long long current_offset_;
  std::vector<uint8_t> read_data_;
}

- (id)initWithTest:(media::DataRequestHandlerTest*)test
           withKey:(NSNumber*)key
        withLength:(int)length
        withOffset:(int64_t)offset;

- (void)dealloc;

@property(nonatomic, readonly) NSNumber* key;
@property(nonatomic, readonly) const std::vector<uint8_t>& readData;

@property(nonatomic, readonly) NSInteger requestedLength;
@property(nonatomic, readonly) long long requestedOffset;
@property(nonatomic, readonly) long long currentOffset;
@property(nonatomic, readonly) BOOL requestsAllDataToEndOfResource;

- (void)respondWithData:(NSData *)data;

@end

@interface MockAVAssetResourceLoadingRequest : NSObject {
 @private
  media::DataRequestHandlerTest* test_;
  NSNumber* key_;
  MockAVAssetResourceLoadingDataRequest* data_request_;
  bool finished_;
}

- (id)initWithTest:(media::DataRequestHandlerTest*)test withKey:(NSNumber*)key;
- (void)dealloc;

@property(nonatomic, readonly, getter=isFinished) BOOL finished;

@property(nonatomic, readonly)
    AVAssetResourceLoadingContentInformationRequest* contentInformationRequest;

@property(nonatomic, retain) MockAVAssetResourceLoadingDataRequest* dataRequest;

- (void)finishLoading;
- (void)finishLoadingWithError:(NSError*)error;

@end

namespace media {
namespace {

constexpr int kMaxReadChunkSize = kIPCSourceSharedMemorySize;

class TestDataSource {
 public:
  TestDataSource()
      : eos_(false),
        error_(false) {}

  void set_data(const std::vector<uint8_t>& data) { data_ = data; }
  void set_eos(bool eos) { eos_ = eos; }
  void set_error(bool error) { error_ = error; }

  void Read(ipc_data_source::Buffer buffer) {
    int64_t position = buffer.GetReadPosition();
    int size = buffer.GetRequestedSize();
    ASSERT_GE(position, 0);
    ASSERT_LE(size, buffer.GetCapacity());
    ASSERT_TRUE(!buffer_);
    ASSERT_TRUE(buffer);

    if (eos_) {
      size = 0;
    } else if (error_) {
      size = ipc_data_source::kReadError;
    } else if (data_.size() <= static_cast<uint64_t>(position)) {
      size = 0;
    } else {
      size_t tail = data_.size() - static_cast<size_t>(position);
      if (static_cast<size_t>(size) > tail) {
        size = static_cast<int>(tail);
      }
    }
    if (size > 0) {
      memcpy(writable_mapping_.memory(),
             data_.data() + buffer.GetReadPosition(), size);
    }
    buffer.SetReadSize(size);

    buffer_ = std::move(buffer);
  }

  void CallPendingCallback() {
    CHECK(buffer_);
    ipc_data_source::Buffer::SendReply(std::move(buffer_));
  }

  ipc_data_source::Buffer buffer_;
  base::WritableSharedMemoryMapping writable_mapping_;

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
  struct Generator {
    Generator() : i(0) {}
    int operator()() { return i++; }
    int i;
  };

  struct Request {
    Request() = default;
    ~Request() {
      CHECK(!loading_request);
    }

    bool finished() const {
      return success || read_error || aborted;
    }

    AVAssetResourceLoadingRequest* loading_request = nil;
    bool data_available = false;
    bool success = false;
    bool read_error = false;
    bool aborted = false;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  DataRequestHandlerTest()
      : handler_(base::MakeRefCounted<DataRequestHandler>()) {
    base::MappedReadOnlyRegion region_and_mapping =
        base::ReadOnlySharedMemoryRegion::Create(kMaxReadChunkSize);
    CHECK(region_and_mapping.IsValid());
    base::ReadOnlySharedMemoryMapping data_source_mapping =
        region_and_mapping.region.Map();
    CHECK(data_source_mapping.IsValid());

    ipc_data_source::Info source_info;
    source_info.mime_type = "video/mp4";
    source_info.buffer.Init(
        std::move(data_source_mapping),
        base::BindRepeating(&TestDataSource::Read,
                            base::Unretained(&data_source_)));
    data_source_.writable_mapping_ = std::move(region_and_mapping.mapping);
    handler_->Init(std::move(source_info), dispatch_get_main_queue());
  }

  ~DataRequestHandlerTest() override {
    handler_->Stop();
    requests_.clear();
    CHECK_EQ(loading_request_instance_count_, 0u);
  }

  // key is used to refer to created requests. It is NSNumber so it visually
  // distinctive with @1 form.
  void CallHandleDataRequest(NSNumber* key, int64_t offset, int64_t length) {
    CHECK_EQ(requests_.count(key), 0u);
    requests_[key] = std::make_unique<Request>();

    MockAVAssetResourceLoadingDataRequest* data_request =
        [[MockAVAssetResourceLoadingDataRequest alloc]
            initWithTest:this
                 withKey:key
              withLength:length
              withOffset:offset];

    MockAVAssetResourceLoadingRequest* loading_request_mock =
        [[MockAVAssetResourceLoadingRequest alloc]
            initWithTest:this withKey:key];
    loading_request_mock.dataRequest = data_request;

    // Pretend the mock is a real thing.
    AVAssetResourceLoadingRequest* loading_request =
        reinterpret_cast<AVAssetResourceLoadingRequest*>(loading_request_mock);
    requests_[key]->loading_request = loading_request;

    handler_->Load(loading_request);
    CHECK(!request(key)->finished());

    // Test that the handler retains the request until the -finishLoading is
    // called. So release now even if loading_request is stored inside Request
    // to force crashes if the handler is broken.
    [loading_request release];
  }

  void CallCancelRequest(NSNumber* key) {
    AVAssetResourceLoadingRequest* loading_request =
        request(key)->loading_request;
    CHECK(loading_request);
    handler_->CancelRequest(loading_request);

    // Cancel should not trigger a call to -finishLoading, so clear the request
    // manually.
    CHECK(request(key)->loading_request);
    request(key)->loading_request = nil;
  }

  void RunUntilDataAvailable(NSNumber* key) {
    CHECK_EQ(requests_.count(key), 1u);
    while (!requests_[key]->data_available && data_source_.buffer_) {
      data_source_.CallPendingCallback();
    }
    ASSERT_TRUE(requests_[key]->data_available);
  }

  void RunUntilFinished(NSNumber* key) {
    CHECK_EQ(requests_.count(key), 1u);
    while (!requests_[key]->finished() && data_source_.buffer_) {
      data_source_.CallPendingCallback();
    }
    ASSERT_TRUE(requests_[key]->finished());
  }

  Request* request(NSNumber* key) {
    CHECK_EQ(requests_.count(key), 1u);
    return requests_[key].get();
  }

  bool is_finished(NSNumber* key) {
    return request(key)->finished();
  }

  void OnDataAvailable(AVAssetResourceLoadingDataRequest* data_request,
                       NSNumber* key,
                       const uint8_t* data,
                       size_t read_size) {
    CHECK_EQ(requests_.count(key), 1u);

    CHECK(data);
    CHECK_GT(read_size, 0u);
    loaded_data_.insert(loaded_data_.end(), data, data + read_size);

    Request* r = request(key);
    ASSERT_FALSE(r->finished());
    ASSERT_EQ(r->loading_request.dataRequest, data_request);
    r->data_available = true;
  }

  void OnFinishLoading(AVAssetResourceLoadingRequest* loading_request,
                       NSNumber* key,
                       NSError* error) {
    CHECK_EQ(requests_.count(key), 1u);
    Request* r = request(key);
    ASSERT_TRUE(r->loading_request);
    ASSERT_EQ(r->loading_request, loading_request);
    r->loading_request = nil;
    if (!error) {
      r->success = true;
    } else {
      EXPECT_EQ(error.domain, NSPOSIXErrorDomain);
      if (error.code == EIO) {
        r->read_error = true;
      } else {
        CHECK_EQ(error.code, EINTR);
        r->aborted = true;
      }
    }
  }

  const std::vector<uint8_t>& loaded_data() const { return loaded_data_; }

  TestDataSource data_source_;
  scoped_refptr<DataRequestHandler> handler_;

  std::map<NSNumber*, std::unique_ptr<Request>> requests_;
  std::vector<uint8_t> loaded_data_;
  size_t loading_request_instance_count_ = 0;
};


}  // namespace
}  // namespace media

@implementation MockAVAssetResourceLoadingDataRequest

- (id)initWithTest:(media::DataRequestHandlerTest*)test
           withKey:(NSNumber*)key
        withLength:(int)length
        withOffset:(int64_t)offset {
  test_ = test;
  key_ = key;
  [key_ retain];
  requested_length_ = length;
  requested_offset_ = offset;
  current_offset_ = offset;

  return self;
}

- (void)dealloc {
  [key_ release];
  key_ = nil;
  [super dealloc];
}

@synthesize key = key_;

@synthesize readData = read_data_;

- (NSInteger)requestedLength {
  if (requested_length_ < 0) {
    return 1024 * 1024 * 1024;
  }
  return requested_length_;
}

@synthesize requestedOffset = requested_offset_;
@synthesize currentOffset = current_offset_;

 -(BOOL)requestsAllDataToEndOfResource {
   return requested_length_ < 0;
 }

- (void)respondWithData:(NSData *)data {
  size_t read_size = [data length];
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.bytes);
  current_offset_ += read_size;
  test_->OnDataAvailable(
      reinterpret_cast<AVAssetResourceLoadingDataRequest*>(self), key_, bytes,
      read_size);
}

@end

@implementation MockAVAssetResourceLoadingRequest

- (id)initWithTest:(media::DataRequestHandlerTest*)test withKey:(NSNumber*)key {
  test_ = test;
  key_ = key;
  [key_ retain];
  finished_ = false;
  test_->loading_request_instance_count_++;

  return self;
}

- (void)dealloc {
  CHECK_GT(test_->loading_request_instance_count_, 0u);
  test_->loading_request_instance_count_--;
  [key_ release];
  key_ = nil;
  if (data_request_) {
    [data_request_ release];
    data_request_ = nil;
  }
  [super dealloc];
}

- (BOOL)isFinished {
  return finished_;
}

@synthesize dataRequest = data_request_;

- (AVAssetResourceLoadingContentInformationRequest*)contentInformationRequest {
  return nil;
}

- (void)finishLoading {
  [self finishLoadingWithError:nil];
}

- (void)finishLoadingWithError:(NSError *)error {
  finished_ = false;
  test_->OnFinishLoading(reinterpret_cast<AVAssetResourceLoadingRequest*>(self),
                         key_, error);
}

@end

namespace media {
namespace {

TEST_F(DataRequestHandlerTest, FinishesWithErrorOnDataSourceError) {
  data_source_.set_error(true);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 1500));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_TRUE(request(@0)->read_error);
}

TEST_F(DataRequestHandlerTest, FinishesWithEOSOnDataSourceEOS) {
  data_source_.set_eos(true);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 1500));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_TRUE(request(@0)->success);
  EXPECT_EQ(loaded_data().size(), 0u);
}

TEST_F(DataRequestHandlerTest, DataSourceSizeSmallerThanRequested) {
  std::vector<uint8_t> data(5);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 10));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_TRUE(request(@0)->success);
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, DataSourceSizeEqualToRequested) {
  std::vector<uint8_t> data(5);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, 5));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_TRUE(request(@0)->success);
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, RequestedLengthGreaterThanBufferSize) {
  std::vector<uint8_t> data(kMaxReadChunkSize * 2 + 1);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, data.size()));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_TRUE(request(@0)->success);
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, LoadRangeOfDataSource) {
  std::vector<uint8_t> data(kMaxReadChunkSize * 2 + 15);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 5, data.size() - 10));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));

  EXPECT_TRUE(request(@0)->success);

  data.erase(data.begin(), data.begin() + 5);
  data.erase(data.end() - 5, data.end());
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, CanBeCanceled) {
  // Pick IPCDataSource and data request sizes so that the IPCDataSource can't
  // satisfy the request in one go.
  std::vector<uint8_t> data(kMaxReadChunkSize * 2 + 15);
  std::generate(data.begin(), data.end(), Generator());
  data_source_.set_data(data);

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, data.size()));
  ASSERT_NO_FATAL_FAILURE(RunUntilDataAvailable(@0));
  EXPECT_TRUE(handler_->IsHandlingDataRequests());
  CallCancelRequest(@0);
  // The callback should not be called, but the the request should be removed
  // from the queue.
  EXPECT_FALSE(is_finished(@0));
  EXPECT_FALSE(handler_->IsHandlingDataRequests());
}

TEST_F(DataRequestHandlerTest, ConcurrentRequests) {
  // Pick IPCDataSource and data request sizes so that the DataSource can't
  // satisfy the request in one go.
  int64_t length = kMaxReadChunkSize + 15;

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

  EXPECT_TRUE(request(@0)->success);
  EXPECT_TRUE(request(@1)->success);

  // This makes sure data is loaded from lowest offset to highest even when
  // there are concurrent requests.
  EXPECT_EQ(data, loaded_data());
}

TEST_F(DataRequestHandlerTest, ConcurrentRequestsError) {
  data_source_.set_error(true);

  int64_t length = kMaxReadChunkSize + 15;

  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@0, 0, length));
  ASSERT_NO_FATAL_FAILURE(CallHandleDataRequest(@1, length, length));

  // At this point, we should have 2 "concurrent" requests.

  ASSERT_FALSE(is_finished(@0));
  ASSERT_FALSE(is_finished(@1));

  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@0));
  ASSERT_NO_FATAL_FAILURE(RunUntilFinished(@1));

  EXPECT_TRUE(request(@0)->read_error);
  EXPECT_TRUE(request(@1)->aborted);
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
  EXPECT_TRUE(request(@1)->aborted);
  EXPECT_TRUE(request(@2)->aborted);
  EXPECT_TRUE(request(@3)->aborted);
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
  EXPECT_TRUE(request(@1)->aborted);
  EXPECT_TRUE(request(@2)->aborted);
  EXPECT_TRUE(request(@3)->aborted);
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

  EXPECT_TRUE(request(@4)->success);
  EXPECT_TRUE(request(@5)->success);
  EXPECT_TRUE(request(@6)->success);

  EXPECT_FALSE(handler_->IsHandlingDataRequests());

  EXPECT_EQ(data, loaded_data());
}

}  // namespace
}  // namespace media
