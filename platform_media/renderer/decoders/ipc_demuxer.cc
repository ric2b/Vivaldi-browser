// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2013 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/renderer/decoders/ipc_demuxer.h"

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/strings/string_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_log.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

#include "platform_media/common/platform_logging_util.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "platform_media/renderer/decoders/ipc_demuxer_stream.h"
#include "platform_media/renderer/decoders/ipc_factory.h"
#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"

namespace {

// http://www.iana.org/assignments/media-types/media-types.xhtml#audio
static const char* const kIPCMediaPipelineSupportedMimeTypes[] = {
    "audio/3gpp",      /* 3gpp - mp4 */
    "audio/3gpp2",     /* 3gpp2 - mp4 */
    "audio/aac",       /* aac */
    "audio/aacp",      /* aac */
    "audio/mp4",       /* mp4 (aac) */
    "audio/x-m4a",     /* mp4 (aac) */
    "video/3gpp",      /**/
    "video/3gpp2",     /**/
    "video/m4v",       /**/
    "video/mp4",       /**/
    "video/mpeg",      /**/
    "video/x-m4v",     /**/
    "video/quicktime", /**/
#if defined(OS_WIN)
    "video/mpeg4", /**/
#endif
};

std::string MimeTypeFromContentTypeOrURL(const std::string& content_type,
                                         const GURL& url) {
  std::string mime_type = base::ToLowerASCII(content_type);
  if (mime_type.empty() || mime_type == "application/octet-stream") {
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
    scoped_refptr<base::SequencedTaskRunner> media_task_runner,
    MediaLog* media_log)
    : media_task_runner_(std::move(media_task_runner)), media_log_(media_log) {
  DCHECK(media_task_runner_);
}

IPCDemuxer::~IPCDemuxer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owner_sequence_checker_);
  // Ensure that Stop was called while we were on the media thread.
  DCHECK(!ipc_media_pipeline_host_);
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

// static
std::string IPCDemuxer::CanPlayType(const std::string& content_type,
                                    const GURL& url) {
  if (!IPCMediaPipelineHost::IsAvailable())
    return std::string();
  const std::string mime_type = MimeTypeFromContentTypeOrURL(content_type, url);
  if (CanPlayType(mime_type))
    return mime_type;
  return std::string();
}

// static
bool IPCDemuxer::CanPlayType(const std::string& mime_type) {
  for (const char* supported_mime_type : kIPCMediaPipelineSupportedMimeTypes)
    if (mime_type == supported_mime_type)
      return true;

  return false;
}

void IPCDemuxer::StartIPC(DataSource* data_source,
                          std::string mimetype,
                          StartIPCResult callback) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  ipc_media_pipeline_host_ = std::make_unique<IPCMediaPipelineHost>();

  // Unretained() is safe as this owns the host.
  ipc_media_pipeline_host_->Initialize(
      data_source, std::move(mimetype),
      base::BindOnce(&IPCDemuxer::OnStartIPCDone, base::Unretained(this),
                     std::move(callback)));
}

void IPCDemuxer::OnStartIPCDone(StartIPCResult callback, bool success) {
  DCHECK(!audio_stream_);
  DCHECK(!video_stream_);
  if (!success) {
    // Allow the caller to delete a failed demuxer on the owner thread without
    // extra hops to the media thread.
    ipc_media_pipeline_host_.reset();
  }
  std::move(callback).Run(success);
}

std::string IPCDemuxer::GetDisplayName() const {
  return "IPCDemuxer";
}

void IPCDemuxer::Initialize(DemuxerHost* host,
                            PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(ipc_media_pipeline_host_);

  if (ipc_media_pipeline_host_->audio_config().is_valid()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << Loggable(ipc_media_pipeline_host_->audio_config());
    audio_stream_.reset(new IPCDemuxerStream(DemuxerStream::AUDIO,
                                             ipc_media_pipeline_host_.get()));
  } else {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Audio Config is not Valid ";
  }

  if (ipc_media_pipeline_host_->video_config().is_valid()) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
            << Loggable(ipc_media_pipeline_host_->video_config());
    video_stream_.reset(new IPCDemuxerStream(DemuxerStream::VIDEO,
                                             ipc_media_pipeline_host_.get()));
    MEDIA_LOG(INFO, media_log_)
        << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " " << GetDisplayName();
  } else {
    LOG(WARNING) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                 << " Video Config is not Valid ";
  }

  host->SetDuration(ipc_media_pipeline_host_->time_info().duration);
  ipc_media_pipeline_host_->data_source()->SetBitrate(
      ipc_media_pipeline_host_->bitrate());

  // Demuxer requires that the callback runs after the method returns.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(status_cb), PIPELINE_OK));
}

void IPCDemuxer::StartWaitingForSeek(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owner_sequence_checker_);

  // We are called from the owner thread, not the media thread, so hop to it. In
  // BindOnce() we cannot use a weak pointer as it should be used only on the
  // media thread. We must not access any fields in the instance that can be
  // changed on the media thread either. But we can use Unretained(this). When
  // WebMediaPlayerImpl::~WebMediaPlayerImpl later runs on the main thread, it
  // posts the demuxer instance it owns to the media thread first. Thus this
  // instance will be deleted strictly after the posted method returns.
  media_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&IPCDemuxer::StartWaitingForSeekOnMediaThread,
                                base::Unretained(this)));
}

void IPCDemuxer::StartWaitingForSeekOnMediaThread() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (ipc_media_pipeline_host_) {
    ipc_media_pipeline_host_->StartWaitingForSeek();
  }
}

void IPCDemuxer::CancelPendingSeek(base::TimeDelta seek_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(owner_sequence_checker_);
}

void IPCDemuxer::Seek(base::TimeDelta time, PipelineStatusCallback status_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  if (!ipc_media_pipeline_host_) {
    LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
               << ": PIPELINE_ERROR_ABORT";
    std::move(status_cb).Run(PIPELINE_ERROR_ABORT);
    return;
  }

  ipc_media_pipeline_host_->Seek(time, std::move(status_cb));
}

void IPCDemuxer::Stop() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Stop streams before we destroy the host as the streams contains raw host
  // pointers.
  if (audio_stream_) {
    audio_stream_->Stop();
  }
  if (video_stream_) {
    video_stream_->Stop();
  }
  if (ipc_media_pipeline_host_) {
    // Follow FFmpegDemuxer::Stop() and stop the data source.
    ipc_media_pipeline_host_->data_source()->Stop();

    // IPCMediaPipelineHost must only live on the media thread so reset it.
    ipc_media_pipeline_host_.reset();
  }

  // We will be destroyed soon. Invalidate all weak pointers while we're still
  // on the media thread.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void IPCDemuxer::AbortPendingReads() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
}

std::vector<DemuxerStream*> IPCDemuxer::GetAllStreams() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  std::vector<DemuxerStream*> result;
  if (audio_stream_)
    result.push_back(audio_stream_.get());
  if (video_stream_)
    result.push_back(video_stream_.get());
  return result;
}

IPCDemuxerStream* IPCDemuxer::GetStream(DemuxerStream::Type type) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

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
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  if (!ipc_media_pipeline_host_)
    return base::TimeDelta();

  // Make sure we abide by the Demuxer::GetStartTime() contract.  We cannot
  // guarantee that the platform decoders return a non-negative value.
  return std::max(ipc_media_pipeline_host_->time_info().start_time,
                  base::TimeDelta());
}

base::Time IPCDemuxer::GetTimelineOffset() const {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

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
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  IPCDemuxerStream* audio_stream = GetStream(DemuxerStream::AUDIO);
  CHECK(audio_stream);
  bool enabled = !track_ids.empty();
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " : "
          << (enabled ? "enabling" : "disabling") << " audio stream";
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
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  IPCDemuxerStream* video_stream = GetStream(DemuxerStream::VIDEO);
  CHECK(video_stream);
  bool enabled = !track_ids.empty();
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << " : "
          << (enabled ? "enabling" : "disabling") << " video stream";
  video_stream->set_enabled(enabled, currTime);

  std::set<IPCDemuxerStream*> enabled_streams;
  enabled_streams.insert(video_stream);
  std::vector<DemuxerStream*> streams(enabled_streams.begin(),
                                      enabled_streams.end());
  std::move(change_completed_cb).Run(DemuxerStream::VIDEO, streams);
}

absl::optional<container_names::MediaContainerName>
IPCDemuxer::GetContainerForMetrics() const {
  return absl::nullopt;
}

void IPCDemuxer::VivaldiFinishOnMediaThread() {
  // Chromium calls Stop() only after Initialize(), but we may be waiting for
  // the IPC to start that we run before Initialize(), so force Stop() here.
  Stop();
}

}  // namespace media
