// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/gpu/data_source/ipc_data_source_impl.h"

#include "platform_media/common/media_pipeline_messages.h"
#include "platform_media/common/platform_ipc_util.h"

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "ipc/ipc_sender.h"

#include <algorithm>
#include <memory>

// Uncomment to turn on content logging
//#define CONTENT_LOG_FOLDER FILE_PATH_LITERAL("D:\\logs")

#ifdef CONTENT_LOG_FOLDER

#include "base/files/file.h"
#include "base/files/file_util.h"

#endif  // CONTENT_LOG_FOLDER

namespace media {

// The media log must never be included into official builds even by an
// accident.
#if defined(CONTENT_LOG_FOLDER) && !defined(OFFICIAL_BUILD)

namespace {

struct MediaLogItem {
  FILE* fp = nullptr;
};

std::map<IPCDataSourceImpl*, MediaLogItem> media_log_items;

void OpenMediaLog(IPCDataSourceImpl* source) {
  base::FilePath path;
  FILE* fp = CreateAndOpenTemporaryFileInDir(base::FilePath(CONTENT_LOG_FOLDER),
                                             &path);
  // Will fail if we are sandboxed
  if (!fp) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " failed to open a file to capture the media, check that "
                  "--disable-gpu-sandbox is on";
    return;
  }
  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " capturing media to "
            << path.AsUTF8Unsafe();
  MediaLogItem item;
  item.fp = fp;
  media_log_items.emplace(source, std::move(item));
}

void CloseMediaLog(IPCDataSourceImpl* source) {
  auto i = media_log_items.find(source);
  if (i == media_log_items.end())
    return;
  fclose(i->second.fp);
  media_log_items.erase(i);
}

void WriteMediaLog(IPCDataSourceImpl* source,
                   int64_t position,
                   const void* data,
                   size_t size) {
  auto i = media_log_items.find(source);
  if (i == media_log_items.end())
    return;
#ifdef OS_WIN
  _fseeki64(i->second.fp, position, SEEK_SET);
#else
  fseeko(i->second.fp, position, SEEK_SET);
#endif  // OS_WIN
  fwrite(data, 1, size, i->second.fp);
  LOG(INFO) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " position=" << position << " write_size=" << size;
}

}  // namespace

#endif  // CONTENT_LOG_FOLDER

IPCDataSourceImpl::IPCDataSourceImpl(IPC::Sender* channel, int32_t routing_id)
    : channel_(channel), routing_id_(routing_id) {
  DCHECK(channel_);
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__;

#if defined(CONTENT_LOG_FOLDER)
  OpenMediaLog(this);
#endif // CONTENT_LOG_FOLDER
}

IPCDataSourceImpl::~IPCDataSourceImpl() {
  // The caller must call the Stop() method to ensure that there is no pending
  // reads.
  DCHECK(!channel_);
#if defined(CONTENT_LOG_FOLDER)
  CloseMediaLog(this);
#endif // CONTENT_LOG_FOLDER
}

void IPCDataSourceImpl::Read(int64_t position,
                             int size,
                             ipc_data_source::ReadCB read_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_GE(size, 0);

  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " size=" << size
          << " position=" << position << " stopped_=" << !channel_
          << " read_callback_" << !!read_callback_
          << " tag=" << (last_message_tag_ + 1);
  int error = 0;
  if (!channel_) {
    error = ipc_data_source::kReadError;
  } else if (read_callback_) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " attempt to read when another request is active";
    error = ipc_data_source::kReadError;
  }
  if (error) {
    std::move(read_cb).Run(error, nullptr);
    return;
  }

  // On Mac we can be asked for the last read region.
  if (position == last_read_position_ && size == last_requested_size_ &&
      last_read_size_ > 0) {
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " cached_read=" << last_read_size_;
    DCHECK(last_read_size_ <= size);
    DCHECK(static_cast<size_t>(last_read_size_) <= raw_mapping_.size());
    std::move(read_cb).Run(last_read_size_,
                           raw_mapping_.GetMemoryAs<uint8_t>());
    return;
  }

  int64_t tag = ++last_message_tag_;
  last_read_position_ = position;
  last_requested_size_ = size;
  last_read_size_ = -1;
  read_callback_ = std::move(read_cb);

  channel_->Send(
      new MediaPipelineMsg_ReadRawData(routing_id_, tag, position, size));
}

void IPCDataSourceImpl::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " stopped_=" << !channel_
          << " read_callback_=" << !!read_callback_
          << " last_message_tag_=" << last_message_tag_;

  channel_ = nullptr;

  // Release no longer used mapping.
  raw_mapping_ = base::ReadOnlySharedMemoryMapping();
  last_read_size_ = -1;

  FinishRead();
}

void IPCDataSourceImpl::OnRawDataReady(
    int64_t tag,
    int read_size,
    base::ReadOnlySharedMemoryRegion raw_region) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  do {
    VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " read_size=" << read_size
            << " requested_size=" << last_requested_size_
            << " read_position=" << last_read_position_
            << " tag=" << tag
            << " last_message_tag_=" << last_message_tag_
            << " tag_match=" << (tag == last_message_tag_);

    if (!read_callback_ || tag != last_message_tag_) {
      // This should never happen unless the renderer process in a bad state as
      // we never send a new request until we get a reply.
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " unexpected reply";
      read_size = ipc_data_source::kReadError;
      break;
    }
    if (read_size > last_requested_size_) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " the size from the reply exceeded the requested size "
                 << last_requested_size_;
      read_size = ipc_data_source::kReadError;
      break;
    }

    // Check for a new region before we check for end-of-stream as the renderer
    // can prepare the region before it knows that it will send end-of-stream
    // for a particular request.
    if (raw_region.IsValid()) {
      if (raw_region.GetSize() > kMaxSharedMemorySize) {
        LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " shared region is too big";
        read_size = ipc_data_source::kReadError;
        break;
      }
      raw_mapping_ = raw_region.Map();

      // We no longer need the region as we created the mapping.
      raw_region = base::ReadOnlySharedMemoryRegion();

      if (!raw_mapping_.IsValid()) {
        LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                   << " failed to map a new region of size "
                   << raw_region.GetSize();
        read_size = ipc_data_source::kReadError;
        break;
      }
    }

    if (read_size <= 0) {
      // end-of-stream or read error
      break;
    }

    size_t mapping_size = raw_mapping_.IsValid() ? raw_mapping_.size() : 0;
    if (static_cast<size_t>(read_size) > mapping_size) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " the shared memory buffer is null or too small: "
                 << mapping_size;
      read_size = ipc_data_source::kReadError;
      break;
    }

#if defined(CONTENT_LOG_FOLDER)
    WriteMediaLog(this, last_read_position_,
                  raw_mapping_.GetMemoryAs<uint8_t>(), read_size);
#endif // CONTENT_LOG_FOLDER
  } while (false);

  last_read_size_ = read_size;
  FinishRead();
  if (read_size < 0) {
    Stop();
  }
}

void IPCDataSourceImpl::FinishRead() {
  DCHECK(last_read_size_ <= 0 ||
         (raw_mapping_.IsValid() &&
          raw_mapping_.mapped_size() >= static_cast<size_t>(last_read_size_)));
  if (read_callback_) {
    const uint8_t* data =
        (last_read_size_ <= 0) ? nullptr : raw_mapping_.GetMemoryAs<uint8_t>();
    std::move(read_callback_).Run(last_read_size_, data);
  }
}

}  // namespace media
