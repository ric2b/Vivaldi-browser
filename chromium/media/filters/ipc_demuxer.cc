// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "media/filters/ipc_demuxer.h"

#include <algorithm>

#include "base/callback_helpers.h"
#include "base/strings/string_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/platform_mime_util.h"
#include "media/filters/ipc_demuxer_stream.h"
#include "media/filters/ipc_media_pipeline_host.h"
#include "media/filters/platform_media_pipeline_types.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

namespace {

static const char* const kIPCMediaPipelineSupportedMimeTypes[] = {
    "video/mp4",       "video/m4v",   "video/x-m4v", "video/mpeg",
    "audio/mp4",       "audio/x-m4a", "audio/mp3",   "audio/x-mp3",
    "audio/mpeg",      "audio/mpeg3", "audio/aac",   "audio/aacp",
    "audio/3gpp",      "audio/3gpp2", "video/3gpp",  "video/3gpp2",
#if defined(OS_MACOSX)
    "video/quicktime",
#endif
};

std::string MimeTypeFromContentTypeOrURL(const std::string& content_type,
                                         const GURL& url) {
  std::string mime_type = base::StringToLowerASCII(content_type);
  if (mime_type.empty()) {
#if defined(OS_WIN)
    base::FilePath file(base::FilePath::FromUTF8Unsafe(url.ExtractFileName()));
#elif defined(OS_POSIX)
    base::FilePath file(url.ExtractFileName());
#endif
    net::GetMimeTypeFromFile(file, &mime_type);
  }
  return mime_type;
}

}  // namespace

namespace media {

IPCDemuxer::IPCDemuxer(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    DataSource* data_source,
    scoped_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host,
    const std::string& content_type,
    const GURL& url)
    : task_runner_(task_runner),
      host_(NULL),
      data_source_(data_source),
      mimetype_(MimeTypeFromContentTypeOrURL(content_type, url)),
      stopping_(false),
      ipc_media_pipeline_host_(ipc_media_pipeline_host.Pass()),
      weak_ptr_factory_(this) {
  DCHECK(data_source_);
  DCHECK(ipc_media_pipeline_host_.get());
}

IPCDemuxer::~IPCDemuxer() {
  // We hand out weak pointers on the |task_runner_| thread.  Make sure they are
  // all invalidated by the time we are destroyed (on the render thread).
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

/* static */
bool IPCDemuxer::IsSupported(const std::string& content_type, const GURL& url) {
  if (!IsPlatformMediaPipelineAvailable(PlatformMediaCheckType::BASIC))
    return false;

  std::string mime_type = MimeTypeFromContentTypeOrURL(content_type, url);
  for (size_t i = 0; i < arraysize(kIPCMediaPipelineSupportedMimeTypes); i++) {
    if (!mime_type.compare(kIPCMediaPipelineSupportedMimeTypes[i])) {
      return true;
    }
  }

  return false;
}

std::string IPCDemuxer::GetDisplayName() const {
  return "IPCDemuxer";
}

void IPCDemuxer::Initialize(DemuxerHost* host,
                            const PipelineStatusCB& status_cb,
                            bool enable_text_tracks) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!stopping_);

  host_ = host;
  ipc_media_pipeline_host_->Initialize(
      mimetype_,
      base::Bind(&IPCDemuxer::OnInitialized,
                 weak_ptr_factory_.GetWeakPtr(),
                 status_cb));
}

void IPCDemuxer::StartWaitingForSeek() {
  if (!stopping_)
    ipc_media_pipeline_host_->StartWaitingForSeek();
}

void IPCDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& status_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (stopping_) {
    status_cb.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  ipc_media_pipeline_host_->Seek(time, status_cb);
}

void IPCDemuxer::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!stopping_);

  stopping_ = true;

  if (audio_stream_.get() != NULL)
    audio_stream_->Stop();
  if (video_stream_.get() != NULL)
    video_stream_->Stop();

  ipc_media_pipeline_host_->Stop();
  // IPCMediaPipelineHost must only live on the |task_runner_| thread.
  ipc_media_pipeline_host_.reset();

  // We will be destroyed soon.  Invalidate all weak pointers while we're still
  // on the |task_runner_| thread.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

DemuxerStream* IPCDemuxer::GetStream(DemuxerStream::Type type) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  switch (type) {
    case DemuxerStream::AUDIO:
      return audio_stream_.get();
    case DemuxerStream::VIDEO:
      return video_stream_.get();
    default:
      return NULL;
  }
}

base::TimeDelta IPCDemuxer::GetStartTime() const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  return start_time_;
}

base::Time IPCDemuxer::GetTimelineOffset() const {
  DCHECK(task_runner_->BelongsToCurrentThread());

  return base::Time();
}

void IPCDemuxer::OnInitialized(const PipelineStatusCB& callback,
                               bool success,
                               int bitrate,
                               const PlatformMediaTimeInfo& time_info,
                               const PlatformAudioConfig& audio_config,
                               const PlatformVideoConfig& video_config) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (stopping_) {
    callback.Run(PIPELINE_ERROR_ABORT);
    return;
  }

  if (!success) {
    callback.Run(PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (audio_config.is_valid()) {
    audio_stream_.reset(new IPCDemuxerStream(DemuxerStream::AUDIO,
                                             ipc_media_pipeline_host_.get()));
  }

  if (video_config.is_valid()) {
    video_stream_.reset(new IPCDemuxerStream(DemuxerStream::VIDEO,
                                             ipc_media_pipeline_host_.get()));
  }

  host_->SetDuration(time_info.duration);
  data_source_->SetBitrate(bitrate);

  // Make sure we abide by the Demuxer::GetStartTime() contract.  We cannot
  // guarantee that the platform decoders return a non-negative value.
  start_time_ = std::max(time_info.start_time, base::TimeDelta());

  callback.Run(PIPELINE_OK);
}

}  // namespace media
