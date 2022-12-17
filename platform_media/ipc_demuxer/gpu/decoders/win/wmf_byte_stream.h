// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_IPC_DEMUXER_GPU_DECODERS_WIN_WMF_BYTE_STREAM_H_
#define PLATFORM_MEDIA_IPC_DEMUXER_GPU_DECODERS_WIN_WMF_BYTE_STREAM_H_

#include "platform_media/ipc_demuxer/gpu/data_source/ipc_data_source.h"

#include "base/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"

// Windows Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>

#include <wrl/client.h>
#include <wrl/implements.h>

namespace media {

typedef Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IMFByteStream>
    WMFByteStream_UnknownBase;

class WMFByteStream : public WMFByteStream_UnknownBase {
 public:
  WMFByteStream();
  ~WMFByteStream() override;

  void Initialize(scoped_refptr<base::SequencedTaskRunner> main_task_runner,
                  ipc_data_source::Buffer source_buffer,
                  bool is_streaming,
                  int64_t stream_length);

  // Overrides from IMFByteStream
  HRESULT STDMETHODCALLTYPE GetCapabilities(DWORD* capabilities) override;
  HRESULT STDMETHODCALLTYPE GetLength(QWORD* length) override;
  HRESULT STDMETHODCALLTYPE SetLength(QWORD length) override;
  HRESULT STDMETHODCALLTYPE GetCurrentPosition(QWORD* position) override;
  HRESULT STDMETHODCALLTYPE SetCurrentPosition(QWORD position) override;
  HRESULT STDMETHODCALLTYPE IsEndOfStream(BOOL* end_of_stream) override;
  HRESULT STDMETHODCALLTYPE Read(BYTE* buff, ULONG len, ULONG* read) override;
  HRESULT STDMETHODCALLTYPE BeginRead(BYTE* buff,
                                      ULONG len,
                                      IMFAsyncCallback* callback,
                                      IUnknown* punk_State) override;
  HRESULT STDMETHODCALLTYPE EndRead(IMFAsyncResult* result,
                                    ULONG* read) override;
  HRESULT STDMETHODCALLTYPE Write(const BYTE* buff,
                                  ULONG len,
                                  ULONG* written) override;
  HRESULT STDMETHODCALLTYPE BeginWrite(const BYTE* buff,
                                       ULONG len,
                                       IMFAsyncCallback* callback,
                                       IUnknown* punk_state) override;
  HRESULT STDMETHODCALLTYPE EndWrite(IMFAsyncResult* result,
                                     ULONG* written) override;
  HRESULT STDMETHODCALLTYPE Seek(MFBYTESTREAM_SEEK_ORIGIN seek_origin,
                                 LONGLONG seek_offset,
                                 DWORD seek_flags,
                                 QWORD* current_position) override;
  HRESULT STDMETHODCALLTYPE Flush() override;
  HRESULT STDMETHODCALLTYPE Close() override;

 private:
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // source_buffer_ is null when we are waiting for the media data reply.
  ipc_data_source::Buffer source_buffer_;
  int64_t stream_length_ = -1;
  bool is_streaming_ = false;
  int64_t stream_position_ = 0;
  bool received_eos_ = false;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_IPC_DEMUXER_GPU_DECODERS_WIN_WMF_BYTE_STREAM_H_
