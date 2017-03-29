// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef CONTENT_COMMON_GPU_MEDIA_WMF_BYTE_STREAM_H_
#define CONTENT_COMMON_GPU_MEDIA_WMF_BYTE_STREAM_H_

// Windows Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/win/iunknown_impl.h"
#include "base/win/scoped_comptr.h"
#include "media/base/data_source.h"

namespace content {

class WMFByteStream : public IMFByteStream,
                      public base::win::IUnknownImpl,
                      public IMFAttributes {
 public:
  explicit WMFByteStream(media::DataSource* data_source);
  ~WMFByteStream() override;

  HRESULT Initialize(LPCWSTR mime_type);
  void Stop();

  // Overrides from IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override;
  // It is assumed that WMFByteStream object's lifetime will be controlled by
  // the classes that create and use it, as giving control to WMF can cause some
  // subtle problems (e.g. DNA-34245). Methods below are provided as they are a
  // part of IUnknown interfae, but they usage is limited to debugging purposes.
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
  static const int64 kUnknownSize;

  void OnReadData(int size);

  media::DataSource* data_source_;
  media::DataSource::ReadCB read_cb_;

  IMFAsyncResult* async_result_;

  // We implement IMFAttributes by forwarding all calls to an instance of the
  // standard IMFAttributes class, which we store a reference to here.
  base::win::ScopedComPtr<IMFAttributes> attributes_;

  // Cached number of bytes last read from the data source.
  int last_read_bytes_;

  // Cached position within the data source.
  int64 read_position_;

  bool stopped_;

  // Used only for debugging purposes.
  base::AtomicRefCount ref_count_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<WMFByteStream> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_MEDIA_WMF_BYTE_STREAM_H_
