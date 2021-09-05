// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_REQUEST_HANDLER_H_
#define PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_REQUEST_HANDLER_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/common/platform_ipc_util.h"
#include "platform_media/gpu/data_source/ipc_data_source.h"

#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "media/base/data_source.h"
#include "media/base/media_export.h"

#import <AVFoundation/AVFoundation.h>

namespace media {

class IPCDataSource;

// Bridge between AVAssetResourceLoader that sometimes makes overlapping
// read requests an IPCDataSource that cannot handle overlapping reads.
//
// The code assumes that all execution is serialized including calls to the
// read callback.
class MEDIA_EXPORT DataRequestHandler
    : public base::RefCountedThreadSafe<DataRequestHandler> {
 public:
  enum class Status {
    kSuccess,

    // Renderer reported end-of-stream.
    kEOS,

    // Renderer reported read error.
    kError,

    // The request was aborted due to Stop() or Suspend() calls or due to errors
    // with earlier requests.
    kAborted,

    // The request was cancelled with the explicit Cancel call.
    kCancelled,
  };

  using RespondWithDataCB = base::RepeatingCallback<
      void(id request_handle, const uint8_t* data, int size)>;
  using FinishLoadingCB =
      base::RepeatingCallback<void(id request_handle, Status status)>;

  enum { kMaxReadChunkSize = 64 * 1024 };
  static_assert(
      kMaxReadChunkSize >= kMinimalSharedMemorySize,
      "read limit should at least be the minimal size of shared memory");

  DataRequestHandler();

  void InitSourceReader(ipc_data_source::Reader source_reader);

  void InitCallbacks(RespondWithDataCB respond_with_data_cb,
                     FinishLoadingCB finish_loading_cb);

  void HandleDataRequest(id request_handle, int64_t offset, int64_t length);

  // Cancels a data request being handled.  As this must be called on the
  // serial dispatch queue, it only affects the chunks of the original data
  // request that haven't been processed yet.
  void CancelDataRequest(id request_handle);

  // Forces all pending data requests to finish with an error.
  void Stop();
  bool IsStopped() const;

  void Suspend();
  void Resume();

  // Return true when HandleDataRequest() can be called to submit requests
  // for further processing.
  bool CanHandleRequests();

  // Return true if we have requests that we have not yet replied.
  bool IsHandlingDataRequests() const;

  int64_t stream_position() const { return stream_position_; }

 private:
  friend class base::RefCountedThreadSafe<DataRequestHandler>;

  // Move-only holder of the request and its offset and length.
  struct Request {
    Request();
    Request(id request_handle, int64_t offset, int64_t length);
    Request(Request&&);
    ~Request();
    Request& operator=(Request&&);

    // Remember to update the above assignment operator implementation when
    // adding new members.

    // The handle must be reset before the destructor to indicate that the
    // finish request was called or cancelled.
    base::scoped_nsobject<id> handle;
    int64_t offset = 0;
    int64_t length = -1;

    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  // A container for Requests uniquely identified by handles, but ordered by
  // requested offset.
  class OrderedRequests {
  public:
    OrderedRequests();
    ~OrderedRequests();

    void Add(Request* request);
    Request Remove(id handle);
    Request Pop();

    bool is_empty() const { return requests_.empty(); }
    size_t size() const { return requests_.size(); }

  private:
    using Container = std::vector<Request>;

    Container::iterator Find(id handle);

    Container requests_;
  };

  ~DataRequestHandler();

  void AbortAllDataRequests();
  void DispatchRead(Request* request);
  void DidReadNextChunk(int read_size, const uint8_t* data);
  void FinishLoading(Request* request, Status status);

  ipc_data_source::Reader source_reader_;
  RespondWithDataCB respond_with_data_cb_;
  FinishLoadingCB finish_loading_cb_;

  bool suspended_ = false;
  bool waiting_for_reply_ = false;

  int64_t stream_position_ = -1;

  // The request we are waiting a reply for. When cancelled or aborted its
  // handler is reset to prevent further calls to callbacks, but the
  // waiting_for_reply_ flag remain active until we get a reply from the source.
  Request active_request_;

  // Other requests besides the active ordered by the offset.
  OrderedRequests pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(DataRequestHandler);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_REQUEST_HANDLER_H_
