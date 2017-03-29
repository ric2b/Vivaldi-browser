// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_renderer_factory.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "base/single_thread_task_runner.h"
#include "media/base/media_log.h"
#include "media/filters/gpu_video_decoder.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"

#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_video_decoder.h"
#endif

#if !defined(OS_ANDROID)
#include "media/filters/opus_audio_decoder.h"
#endif

#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#include "media/base/platform_mime_util.h"
#include "media/filters/ipc_demuxer.h"
#include "media/filters/pass_through_audio_decoder.h"
#include "media/filters/pass_through_video_decoder.h"
#if defined(OS_MACOSX)
#include "media/filters/at_audio_decoder.h"
#endif
#if defined(OS_WIN)
#include "media/filters/wmf_audio_decoder.h"
#include "media/filters/wmf_video_decoder.h"
#endif
#endif

namespace media {

DefaultRendererFactory::DefaultRendererFactory(
    const scoped_refptr<MediaLog>& media_log,
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
    const AudioHardwareConfig& audio_hardware_config)
    : media_log_(media_log),
      gpu_factories_(gpu_factories),
      audio_hardware_config_(audio_hardware_config) {
}

DefaultRendererFactory::~DefaultRendererFactory() {
}

scoped_ptr<Renderer> DefaultRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    bool use_platform_media_pipeline,
    bool platform_pipeline_enlarges_buffers_on_underflow) {
  DCHECK(audio_renderer_sink);

  // Create our audio decoders and renderer.
  ScopedVector<AudioDecoder> audio_decoders;

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  if (use_platform_media_pipeline) {
    audio_decoders.push_back(new PassThroughAudioDecoder(media_task_runner));
  } else {
    if (IsPlatformAudioDecoderAvailable()) {
#if defined(OS_MACOSX)
      audio_decoders.push_back(new ATAudioDecoder(media_task_runner));
#elif defined(OS_WIN)
      audio_decoders.push_back(new WMFAudioDecoder(media_task_runner));
#endif
    }
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

#if !defined(MEDIA_DISABLE_FFMPEG)
  audio_decoders.push_back(new FFmpegAudioDecoder(
      media_task_runner, base::Bind(&MediaLog::AddLogEvent, media_log_)));
#endif

#if !defined(OS_ANDROID)
  audio_decoders.push_back(new OpusAudioDecoder(media_task_runner));
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  }
#endif

  scoped_ptr<AudioRendererImpl> audio_renderer(new AudioRendererImpl(
      media_task_runner, audio_renderer_sink, audio_decoders.Pass(),
      audio_hardware_config_, media_log_));
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  if (use_platform_media_pipeline &&
      platform_pipeline_enlarges_buffers_on_underflow) {
    // Disable the part of underflow handling in AudioRendererImpl that
    // increases the audio queue size on each underflow.  Some implementations
    // of PlatformMediaPipeline do something that achieves the same goal.
    audio_renderer->set_increase_queue_size_on_underflow(false);
  }
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

  // Create our video decoders and renderer.
  ScopedVector<VideoDecoder> video_decoders;

  // |gpu_factories_| requires that its entry points be called on its
  // |GetTaskRunner()|.  Since |pipeline_| will own decoders created from the
  // factories, require that their message loops are identical.
  DCHECK(!gpu_factories_.get() ||
         (gpu_factories_->GetTaskRunner() == media_task_runner.get()));

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  if (use_platform_media_pipeline) {
    video_decoders.push_back(new PassThroughVideoDecoder(media_task_runner));
  } else {
    if (IsPlatformVideoDecoderAvailable()) {
#if defined(OS_WIN)
      video_decoders.push_back(new WMFVideoDecoder(media_task_runner));
#endif
    }
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

  // TODO(pgraszka): When chrome fixes the dropping frames issue in the
  // GpuVideoDecoder, we should make it our first choice on the list of video
  // decoders, for more details see: DNA-36050,
  // https://code.google.com/p/chromium/issues/detail?id=470466.
  if (gpu_factories_.get()) {
    video_decoders.push_back(new GpuVideoDecoder(gpu_factories_));
  }

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(new VpxVideoDecoder(media_task_runner));
#endif

#if !defined(MEDIA_DISABLE_FFMPEG)
  video_decoders.push_back(new FFmpegVideoDecoder(media_task_runner));
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  }
#endif

  scoped_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      media_task_runner, video_renderer_sink, video_decoders.Pass(), true,
      gpu_factories_, media_log_));

  // Create renderer.
  return scoped_ptr<Renderer>(new RendererImpl(
      media_task_runner, audio_renderer.Pass(), video_renderer.Pass()));
}

}  // namespace media
