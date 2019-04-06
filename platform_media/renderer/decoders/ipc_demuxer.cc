// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/decoders/ipc_demuxer.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_util.h"
#include "media/base/media_log.h"
#include "platform_media/renderer/decoders/ipc_demuxer_stream.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"
#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

namespace {

// http://www.iana.org/assignments/media-types/media-types.xhtml#audio
static const char* const kIPCMediaPipelineSupportedMimeTypes[] = {
    "audio/3gpp",  /* 3gpp - mp4 */
    "audio/3gpp2", /* 3gpp2 - mp4 */
    "audio/aac",   /* aac */
    "audio/aacp",  /* aac */
    "audio/mp4",   /* mp4 (aac) */
    "audio/x-m4a", /* mp4 (aac) */
    "video/3gpp",  /**/
    "video/3gpp2", /**/
    "video/m4v",   /**/
    "video/mp4",   /**/
    "video/mpeg",  /**/
    "video/x-m4v", /**/
    "video/quicktime", /**/
#if defined(OS_WIN)
    "video/mpeg4",     /**/
#endif
};

std::string MimeTypeFromContentTypeOrURL(const std::string& content_type,
                                         const GURL& url) {
  std::string mime_type = base::ToLowerASCII(content_type);
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
    std::unique_ptr<IPCMediaPipelineHost> ipc_media_pipeline_host,
    const std::string& content_type,
    const GURL& url,
    MediaLog* media_log)
    : task_runner_(task_runner),
      host_(NULL),
      data_source_(data_source),
      mimetype_(MimeTypeFromContentTypeOrURL(content_type, url)),
      stopping_(false),
      ipc_media_pipeline_host_(std::move(ipc_media_pipeline_host)),
      media_log_(media_log),
      weak_ptr_factory_(this) {
  DCHECK(data_source_);
  DCHECK(ipc_media_pipeline_host_.get());
}

IPCDemuxer::~IPCDemuxer() {
  // We hand out weak pointers on the |task_runner_| thread.  Make sure they are
  // all invalidated by the time we are destroyed (on the render thread).
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

// static
bool IPCDemuxer::CanPlayType(const std::string& content_type, const GURL& url) {
  const std::string mime_type = MimeTypeFromContentTypeOrURL(content_type, url);
  return IPCDemuxer::CanPlayType(mime_type);
}

//static
bool IPCDemuxer::CanPlayType(const std::string& mime_type) {
  for (const char* supported_mime_type : kIPCMediaPipelineSupportedMimeTypes)
    if (mime_type == supported_mime_type)
      return true;

  return false;
}

std::string IPCDemuxer::GetDisplayName() const {
  return "IPCDemuxer";
}

void IPCDemuxer::Initialize(DemuxerHost* host,
                            const PipelineStatusCB& status_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!stopping_);

  host_ = host;
  ipc_media_pipeline_host_->Initialize(
      mimetype_,
      base::Bind(&IPCDemuxer::OnInitialized,
                 weak_ptr_factory_.GetWeakPtr(),
                 status_cb));
}

void IPCDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
  if (!stopping_)
    ipc_media_pipeline_host_->StartWaitingForSeek();
}

void IPCDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
}

void IPCDemuxer::Seek(base::TimeDelta time, const PipelineStatusCB& status_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (stopping_) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << ": PIPELINE_ERROR_ABORT";
    status_cb.Run(PipelineStatus::PIPELINE_ERROR_ABORT);
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

void IPCDemuxer::AbortPendingReads() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

std::vector<DemuxerStream*> IPCDemuxer::GetAllStreams() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  std::vector<DemuxerStream*> result;
  if (audio_stream_)
    result.push_back(audio_stream_.get());
  if (video_stream_)
    result.push_back(video_stream_.get());
  return result;
}

IPCDemuxerStream* IPCDemuxer::GetStream(DemuxerStream::Type type) {
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

int64_t IPCDemuxer::GetMemoryUsage() const {
  // TODO(tmoniuszko): Implement me. DNA-45936
  return 0;
}

void IPCDemuxer::OnEnabledAudioTracksChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta currTime,
    TrackChangeCB change_completed_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  IPCDemuxerStream* audio_stream = GetStream(DemuxerStream::AUDIO);
  CHECK(audio_stream);
  bool enabled = !track_ids.empty();
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " : " << (enabled ? "enabling" : "disabling")
          << " audio stream";
  audio_stream->set_enabled(enabled, currTime);

  std::set<IPCDemuxerStream*> enabled_streams;
  enabled_streams.insert(audio_stream);
  std::vector<DemuxerStream*> streams(enabled_streams.begin(),
                                      enabled_streams.end());
  std::move(change_completed_cb).Run(DemuxerStream::AUDIO, streams);
}

void IPCDemuxer::OnSelectedVideoTrackChanged(
    const std::vector<MediaTrack::Id>& track_ids,
    base::TimeDelta currTime,
    TrackChangeCB change_completed_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  IPCDemuxerStream* video_stream = GetStream(DemuxerStream::VIDEO);
  CHECK(video_stream);
  bool enabled = !track_ids.empty();
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " : " << (enabled ? "enabling" : "disabling")
          << " video stream";
  video_stream->set_enabled(enabled, currTime);

  std::set<IPCDemuxerStream*> enabled_streams;
  enabled_streams.insert(video_stream);
  std::vector<DemuxerStream*> streams(enabled_streams.begin(),
                                      enabled_streams.end());
  std::move(change_completed_cb).Run(DemuxerStream::VIDEO, streams);
}

void IPCDemuxer::OnInitialized(const PipelineStatusCB& callback,
                               bool success,
                               int bitrate,
                               const PlatformMediaTimeInfo& time_info,
                               const PlatformAudioConfig& audio_config,
                               const PlatformVideoConfig& video_config) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (stopping_) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << ": PIPELINE_ERROR_ABORT";
    callback.Run(PipelineStatus::PIPELINE_ERROR_ABORT);
    return;
  }

  if (!success) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << ": PIPELINE_ERROR_INITIALIZATION_FAILED";
    callback.Run(PipelineStatus::PIPELINE_ERROR_INITIALIZATION_FAILED);
    return;
  }

  if (audio_config.is_valid()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << Loggable(audio_config);
    audio_stream_.reset(new IPCDemuxerStream(DemuxerStream::AUDIO,
                                             ipc_media_pipeline_host_.get()));
  } else {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Audio Config is not Valid ";
  }

  if (video_config.is_valid()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << Loggable(video_config);
    video_stream_.reset(new IPCDemuxerStream(DemuxerStream::VIDEO,
                                             ipc_media_pipeline_host_.get()));
    MEDIA_LOG(INFO, media_log_) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                                << " " << GetDisplayName();
  } else {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Video Config is not Valid ";
  }

  host_->SetDuration(time_info.duration);
  data_source_->SetBitrate(bitrate);

  // Make sure we abide by the Demuxer::GetStartTime() contract.  We cannot
  // guarantee that the platform decoders return a non-negative value.
  start_time_ = std::max(time_info.start_time, base::TimeDelta());

  callback.Run(PipelineStatus::PIPELINE_OK);
}

}  // namespace media
