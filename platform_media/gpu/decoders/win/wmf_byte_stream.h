// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DECODERS_WIN_WMF_BYTE_STREAM_H_
#define PLATFORM_MEDIA_GPU_DECODERS_WIN_WMF_BYTE_STREAM_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/gpu/decoders/win/read_stream_listener.h"

// Windows Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>

#include <vector>

#include <wrl/client.h>

#include "base/threading/thread_checker.h"
#include "base/win/iunknown_impl.h"

namespace media {
class DataSource;
}

namespace media {

class ReadStream;

class WMFByteStream : public ReadStreamListener,
                      public IMFByteStream,
                      public base::win::IUnknownImpl,
                      public IMFAttributes {
 public:
  explicit WMFByteStream(DataSource* data_source);
  ~WMFByteStream() override;

  HRESULT Initialize(LPCWSTR mime_type);
  void Stop();

  // Overrides from IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  // Overrides from IMFByteStream
  HRESULT STDMETHODCALLTYPE GetCapabilities(DWORD* capabilities) override;
  HRESULT STDMETHODCALLTYPE GetLength(QWORD* length) override;
  HRESULT STDMETHODCALLTYPE SetLength(QWORD length) override;
  HRESULT STDMETHODCALLTYPE GetCurrentPosition(QWORD* position) override;
  HRESULT STDMETHODCALLTYPE SetCurrentPosition(QWORD position) override;
  HRESULT STDMETHODCALLTYPE IsEndOfStream(BOOL* end_of_stream) override;
  HRESULT STDMETHODCALLTYPE Read(BYTE* buff, ULONG len, ULONG* read) override;
  HRESULT STDMETHODCALLTYPE BeginRead(BYTE* buff, ULONG len,
                                      IMFAsyncCallback* callback,
                                      IUnknown* punk_State) override;
  HRESULT STDMETHODCALLTYPE EndRead(IMFAsyncResult* result,
                                    ULONG* read) override;
  HRESULT STDMETHODCALLTYPE Write(const BYTE* buff, ULONG len,
                                  ULONG* written) override;
  HRESULT STDMETHODCALLTYPE BeginWrite(const BYTE* buff, ULONG len,
                                       IMFAsyncCallback* callback,
                                       IUnknown* punk_state) override;
  HRESULT STDMETHODCALLTYPE EndWrite(IMFAsyncResult* result,
                                     ULONG* written) override;
  HRESULT STDMETHODCALLTYPE Seek(MFBYTESTREAM_SEEK_ORIGIN seek_origin,
                                 LONGLONG seek_offset, DWORD seek_flags,
                                 QWORD* current_position) override;
  HRESULT STDMETHODCALLTYPE Flush() override;
  HRESULT STDMETHODCALLTYPE Close() override;

  // IMFAttributes methods
  HRESULT STDMETHODCALLTYPE GetItem(REFGUID guid_key,
                                    PROPVARIANT* value) override;
  HRESULT STDMETHODCALLTYPE GetItemType(REFGUID guid_key,
                                        MF_ATTRIBUTE_TYPE* type) override;
  HRESULT STDMETHODCALLTYPE CompareItem(REFGUID guid_key, REFPROPVARIANT value,
                                        BOOL* result) override;
  HRESULT STDMETHODCALLTYPE Compare(IMFAttributes* theirs,
                                    MF_ATTRIBUTES_MATCH_TYPE match_type,
                                    BOOL* result) override;
  HRESULT STDMETHODCALLTYPE GetUINT32(REFGUID guid_key, UINT32* value) override;
  HRESULT STDMETHODCALLTYPE GetUINT64(REFGUID guid_vey, UINT64* value) override;
  HRESULT STDMETHODCALLTYPE GetDouble(REFGUID guid_key, double* value) override;
  HRESULT STDMETHODCALLTYPE GetGUID(REFGUID guid_key,
                                    GUID* guid_value) override;
  HRESULT STDMETHODCALLTYPE GetStringLength(REFGUID guid_key,
                                            UINT32* length) override;
  HRESULT STDMETHODCALLTYPE GetString(REFGUID guid_key, LPWSTR value,
                                      UINT32 buf_size, UINT32* length) override;
  HRESULT STDMETHODCALLTYPE GetAllocatedString(REFGUID guid_key, LPWSTR* value,
                                               UINT32* length) override;
  HRESULT STDMETHODCALLTYPE GetBlobSize(REFGUID guid_key,
                                        UINT32* blob_size) override;
  HRESULT STDMETHODCALLTYPE GetBlob(REFGUID guid_key, UINT8* buf,
                                    UINT32 buf_size,
                                    UINT32* blob_size) override;
  HRESULT STDMETHODCALLTYPE GetAllocatedBlob(REFGUID guid_key, UINT8** buf,
                                             UINT32* size) override;
  HRESULT STDMETHODCALLTYPE GetUnknown(REFGUID guid_key, REFIID riid,
                                       LPVOID* ppv) override;
  HRESULT STDMETHODCALLTYPE SetItem(REFGUID guid_key,
                                    REFPROPVARIANT value) override;
  HRESULT STDMETHODCALLTYPE DeleteItem(REFGUID guid_key) override;
  HRESULT STDMETHODCALLTYPE DeleteAllItems() override;
  HRESULT STDMETHODCALLTYPE SetUINT32(REFGUID guid_key, UINT32 value) override;
  HRESULT STDMETHODCALLTYPE SetUINT64(REFGUID guid_key, UINT64 value) override;
  HRESULT STDMETHODCALLTYPE SetDouble(REFGUID guid_key, double value) override;
  HRESULT STDMETHODCALLTYPE SetGUID(REFGUID guid_key,
                                    REFGUID guid_value) override;
  HRESULT STDMETHODCALLTYPE SetString(REFGUID guid_key, LPCWSTR value) override;
  HRESULT STDMETHODCALLTYPE SetBlob(REFGUID guid_key, const UINT8* buf,
                                    UINT32 buf_size) override;
  HRESULT STDMETHODCALLTYPE SetUnknown(REFGUID guid_key,
                                       IUnknown* unknown) override;
  HRESULT STDMETHODCALLTYPE LockStore() override;
  HRESULT STDMETHODCALLTYPE UnlockStore() override;
  HRESULT STDMETHODCALLTYPE GetCount(UINT32* items) override;
  HRESULT STDMETHODCALLTYPE GetItemByIndex(UINT32 index, GUID* guid_key,
                                           PROPVARIANT* value) override;
  HRESULT STDMETHODCALLTYPE CopyAllItems(IMFAttributes* dest) override;

 private:

  void OnReadData(int size) override;

  IMFAsyncResult* async_result_;
  ReadStream* read_stream_;

  // We implement IMFAttributes by forwarding all calls to an instance of the
  // standard IMFAttributes class, which we store a reference to here.
  Microsoft::WRL::ComPtr<IMFAttributes> attributes_;

  base::ThreadChecker thread_checker_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_WIN_WMF_BYTE_STREAM_H_
