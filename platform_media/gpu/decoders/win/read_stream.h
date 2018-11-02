// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORM_MEDIA_GPU_DECODERS_WIN_READ_STREAM_H_
#define PLATFORM_MEDIA_GPU_DECODERS_WIN_READ_STREAM_H_

#include "platform_media/common/feature_toggles.h"

#include "media/base/data_source.h"

namespace media {

class ReadStreamListener;

class ReadStream {
public:
  explicit ReadStream(DataSource* data_source);
  ~ReadStream();

  void Initialize(ReadStreamListener * listener);
  void Stop();
  bool HasStopped() const;
  bool IsStreaming();

  int64_t Size();
  bool HasSize();

  int64_t CurrentPosition();
  void SetCurrentPosition(int64_t position);

  bool IsEndOfStream();

  int SyncRead(uint8_t* destination, size_t len);
  void AsyncRead(uint8_t* destination, size_t len);

private:
  // The Media Framework expects -1 when the size is unknown.
  static const int64_t kUnknownSize = -1;

  struct CurrentRead {
    void Init(uint8_t* buff, size_t len) {
      accumulated_size_ = 0;
      requested_size_ = len;
      requested_buff_ = buff;
    }

    void Reset() {
      accumulated_size_ = 0;
      requested_size_ = 0;
      requested_buff_ = nullptr;
    }

    void ReceivedBytes(size_t bytes_read) {
      accumulated_size_ += bytes_read;
      requested_buff_ += bytes_read;
    }

    size_t Total() const { return accumulated_size_; }
    bool Incomplete() const { return accumulated_size_< requested_size_; }
    size_t RemainingBytes() const { return requested_size_ - accumulated_size_;}
    uint8_t* BufferPos() const { return requested_buff_; }

  private:
    size_t accumulated_size_ = 0;
    size_t requested_size_ = 0;
    uint8_t* requested_buff_ = nullptr;
  };

  struct StreamState {

    bool ReceivedBytes(int bytes_read) {
      is_end_of_stream_ = (bytes_read <= 0);
      if(!is_end_of_stream_) {
        read_position_ += bytes_read;
      } else {
        LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
                  << " No bytes read, assuming end of stream";
      }
      return !is_end_of_stream_;
    }

    void Stop() {
      stopped_ = true;
    }
    bool HasStopped() const { return stopped_; }
    int64_t CurrentPosition() const { return read_position_; }
    void SetCurrentPosition(int64_t position) { read_position_ = position; }
    bool HasReceivedEOF() const { return is_end_of_stream_; }
    void SetNextPosition(int64_t position) {
      next_position_ = position;
      next_position_set_ = true;
      LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
                << " Postpone Current Position change for pos : "
                << position;
    }
    void UpdateCurrentPosition() {
      if (next_position_set_) {
          LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
                    << " Update Current Position - before : " << read_position_
                    << " after : " << next_position_;
          next_position_set_ = false;
          read_position_ = next_position_;
      }
    }

  private:
    int64_t read_position_ = 0;
    int64_t next_position_ = 0;
    bool next_position_set_ = false;
    bool stopped_ = false;
    bool is_end_of_stream_ = false;
  };

  void Read();
  void FinishRead();
  void OnReadData(int bytes_read);

  StreamState stream_;
  CurrentRead current_read_;
  DataSource* data_source_;
  ReadStreamListener * listener_;
  DataSource::ReadCB read_cb_;
};

}  // namespace media

#endif // PLATFORM_MEDIA_GPU_DECODERS_WIN_READ_STREAM_H_
