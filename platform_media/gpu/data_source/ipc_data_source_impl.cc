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

void IPCDataSourceImpl::Read(ipc_data_source::Buffer buffer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(buffer);
  DCHECK(!buffer.IsReadError());

  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " size=" << buffer.GetRequestedSize()
          << " position=" << buffer.GetReadPosition()
          << " stopped_=" << !channel_
          << " tag=" << (last_message_tag_ + 1);
  bool error = false;
  if (!channel_) {
    error = true;
  } else if (buffer_) {
    LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
               << " attempt to read when another request is active";
    error = true;
  }
  if (error) {
    buffer.SetReadError();
    ipc_data_source::Buffer::SendReply(std::move(buffer));
    return;
  }

  int64_t tag = ++last_message_tag_;
  buffer_ = std::move(buffer);

  channel_->Send(new MediaPipelineMsg_ReadRawData(
      routing_id_, tag, buffer_.GetReadPosition(), buffer_.GetRequestedSize()));
}

void IPCDataSourceImpl::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " stopped_=" << !channel_
          << " has_buffer=" << !!buffer_
          << " last_message_tag_=" << last_message_tag_;

  channel_ = nullptr;

  if (buffer_) {
    buffer_.SetReadError();
    ipc_data_source::Buffer::SendReply(std::move(buffer_));
  }
}

void IPCDataSourceImpl::OnRawDataReady(int64_t tag, int read_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(4) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " read_size=" << read_size
          << " requested_size=" << (buffer_ ? buffer_.GetRequestedSize() : -1)
          << " read_position=" << (buffer_ ? buffer_.GetReadPosition() : -1)
          << " tag=" << tag << " last_message_tag_=" << last_message_tag_
          << " tag_match=" << (tag == last_message_tag_);

  do {
    if (!buffer_ || tag != last_message_tag_) {
      // This should never happen unless the renderer process in a bad state as
      // we never send a new request until we get a reply.
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " unexpected reply";
      break;
    }
    if (read_size > buffer_.GetRequestedSize()) {
      LOG(ERROR) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " the size from the reply exceeded the requested size "
                 << buffer_.GetRequestedSize();
      break;
    }

    buffer_.SetReadSize(read_size);
    if (buffer_.IsReadError())
      break;

#if defined(CONTENT_LOG_FOLDER)
    if (read_size > 0) {
      WriteMediaLog(this, buffer_.GetReadPosition(), buffer_.GetReadData(),
                    read_size);
    }
#endif  // CONTENT_LOG_FOLDER
    ipc_data_source::Buffer::SendReply(std::move(buffer_));
    return;

  } while (false);
  Stop();
}

}  // namespace media
