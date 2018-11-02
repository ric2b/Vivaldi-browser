// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_REQUEST_HANDLER_H_
#define PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_REQUEST_HANDLER_H_

#include "platform_media/common/feature_toggles.h"

#include <algorithm>

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/data_source.h"
#include "media/base/media_export.h"

#import <AVFoundation/AVFoundation.h>

namespace media {

class IPCDataSource;

// Roughly maps to AVAssetResourceLoadingDataRequest, only it's mutable.
struct ResourceLoadingDataRequest {
  ResourceLoadingDataRequest() : offset(0), length(0) {}

  explicit ResourceLoadingDataRequest(
      // Casting to NSUInteger is required in case 32 bit builds and
      // data_request.length value exceeds (2^32)/2. chunk_size will return
      // negative value, and AVFMediaPipeline will not be able to decode media.
      AVAssetResourceLoadingDataRequest* dataRequest)
      : offset([dataRequest requestedOffset]),
        length(static_cast<NSUInteger>([dataRequest requestedLength])) {}

  int64_t offset;
  int64_t length;
};

// Handles requests to fetch specified amounts of data from an IPCDataSource.
// It is able to handle one request at a time, and it can respond with smaller
// chunks of data until the full request is fulfilled.
//
// All reads and responses are serialized on one dispatch queue.
class MEDIA_EXPORT DataRequestHandler
    : public base::RefCountedThreadSafe<DataRequestHandler> {
 public:
  enum Status {
    SUCCESS,
    ERROR,
    CANCELED,
  };

  using RespondWithDataCB =
      base::Callback<void(id request_handle, uint8_t* data, int size)>;
  using FinishLoadingCB =
      base::Callback<void(id request_handle, Status status)>;

  enum { kBufferSize = 64 * 1024 };

  DataRequestHandler(media::DataSource* data_source,
                     const RespondWithDataCB& respond_with_data_cb,
                     const FinishLoadingCB& finish_loading_cb,
                     dispatch_queue_t queue);

  void HandleDataRequest(id request_handle,
                         const ResourceLoadingDataRequest& data_request);

  // Cancels a data request being handled.  As this must be called on the
  // serial dispatch queue, it only affects the chunks of the original data
  // request that haven't been processed yet.
  void CancelDataRequest(id request_handle);

  // Forces all pending data requests to finish with an error.
  void AbortAllDataRequests();

  bool IsHandlingDataRequests() const;

 private:
  friend class base::RefCountedThreadSafe<DataRequestHandler>;

  struct Request;
  class OrderedRequests;

  static int chunk_size(const ResourceLoadingDataRequest& data_request) {
    return std::min(static_cast<int64_t>(kBufferSize), data_request.length);
  }

  ~DataRequestHandler();

  void DispatchBlockingRead();
  void ReadNextChunk();
  void DidReadNextChunk(int size);
  void ProcessChunk();
  void FinishLoading(id request_handle, Status status);

  media::DataSource* const data_source_;
  const dispatch_queue_t queue_;

  std::unique_ptr<OrderedRequests> requests_;

  uint8_t buffer_[kBufferSize];
  int last_size_read_;

  RespondWithDataCB respond_with_data_cb_;
  FinishLoadingCB finish_loading_cb_;

  base::WaitableEvent read_complete_;

  DISALLOW_COPY_AND_ASSIGN(DataRequestHandler);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_REQUEST_HANDLER_H_
