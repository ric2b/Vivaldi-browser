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
#include "base/mac/scoped_dispatch_object.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/data_source.h"
#include "media/base/media_export.h"

#import <AVFoundation/AVFoundation.h>

@class DataSourceLoader;

namespace media {

class IPCDataSource;

// Helper for tests to have dispatch_queue_t with SequencedTaskRunner.
class DispatchQueueRunnerProxy {
 public:
  static bool enabled() { return g_proxy != nullptr; }

  static DispatchQueueRunnerProxy* instance() { return g_proxy; }

  virtual ~DispatchQueueRunnerProxy() {}
  virtual dispatch_queue_t GetQueue() = 0;

  class ScopedRunner {
   public:
    ScopedRunner() {
      if (enabled()) {
        instance()->EnterRunner();
      }
    }
    ~ScopedRunner() {
      if (enabled()) {
        instance()->ExitRunner();
      }
    }
  };

 protected:
  // This can only be called once. The proxy pointer must remain valid until the
  // program exit.
  static void init_instance(DispatchQueueRunnerProxy* proxy) {
    DCHECK(!g_proxy);
    DCHECK(proxy);
    g_proxy = proxy;
  }

  virtual void EnterRunner() = 0;
  virtual void ExitRunner() = 0;

 private:
  friend ScopedRunner;

  static MEDIA_EXPORT DispatchQueueRunnerProxy* g_proxy;
};

class IPCPipelineTestSetup;

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

    // Unsupported or invalid request
    kBadRequest,

    // Renderer reported a read error.
    kError,

    // The request was aborted due to Stop() or Suspend() calls or due to errors
    // with earlier requests.
    kAborted,

    // The request was cancelled by the caller.
    kCancelled,
  };

  DataRequestHandler();
  DataRequestHandler(const DataRequestHandler&) = delete;
  DataRequestHandler& operator=(const DataRequestHandler&) = delete;

  void Init(ipc_data_source::Info source_info, dispatch_queue_t ipc_queue);

  dispatch_queue_t GetIPCQueue();
  AVURLAsset* GetAsset();

  void Load(AVAssetResourceLoadingRequest* request);

  void FillContentInformation(AVAssetResourceLoadingContentInformationRequest*);

  void CancelRequest(AVAssetResourceLoadingRequest* request);

  // Forces all pending data requests to finish with an error.
  void Stop();
  bool IsStopped() const;

  void Suspend();
  void Resume();
  bool IsSuspended() const;

  // Return true when HandleDataRequest() can be called to submit requests
  // for further processing.
  bool CanHandleRequests();

  // Return true if we have requests that we have not yet replied.
  bool IsHandlingDataRequests() const;

 private:
  friend base::RefCountedThreadSafe<DataRequestHandler>;

  ~DataRequestHandler();

  void AbortAllDataRequests();
  void DispatchRead(AVAssetResourceLoadingRequest* request,
                    int64_t offset,
                    int64_t length);
  void DidReadNextChunk(ipc_data_source::Buffer source_buffer);
  void CloseRequest(AVAssetResourceLoadingRequest* request, Status status);

  // source_buffer_ is null when we are waiting for the media data reply.
  ipc_data_source::Buffer source_buffer_;
  base::ScopedDispatchObject<dispatch_queue_t> ipc_queue_;
  base::scoped_nsobject<NSString> content_type_;
  base::scoped_nsobject<AVURLAsset> url_asset_;
  base::scoped_nsobject<DataSourceLoader> data_source_loader_;

  int64_t data_size_ = -1;
  bool is_streaming_ = false;
  bool can_read_ = false;
  bool suspended_ = false;
  bool before_first_read_ = true;

  // The request we are waiting a reply for. When cancelled or aborted its
  // handler is reset to prevent further calls to callbacks, but the
  // waiting_for_reply_ flag remain active until we get a reply from the source.
  AVAssetResourceLoadingRequest* active_request_ = nil;

  // Other requests besides the active.
  std::vector<AVAssetResourceLoadingRequest*> pending_requests_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_DATA_REQUEST_HANDLER_H_
