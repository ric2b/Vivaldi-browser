// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/default_renderer_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/base/decoder_factory.h"
#include "media/base/media_log.h"
#include "media/filters/gpu_video_decoder.h"
#include "media/renderers/audio_renderer_impl.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "media/renderers/renderer_impl.h"
#include "media/renderers/video_renderer_impl.h"

#if !defined(MEDIA_DISABLE_FFMPEG)
#include "media/filters/ffmpeg_audio_decoder.h"
#if !defined(DISABLE_FFMPEG_VIDEO_DECODERS)
#include "media/filters/ffmpeg_video_decoder.h"
#endif
#endif

#if !defined(MEDIA_DISABLE_LIBVPX)
#include "media/filters/vpx_video_decoder.h"
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#include "media/base/pipeline_stats.h"
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
    DecoderFactory* decoder_factory,
    const GetGpuFactoriesCB& get_gpu_factories_cb)
    : media_log_(media_log),
      decoder_factory_(decoder_factory),
      get_gpu_factories_cb_(get_gpu_factories_cb) {}

DefaultRendererFactory::~DefaultRendererFactory() {
}

ScopedVector<AudioDecoder> DefaultRendererFactory::CreateAudioDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    bool use_platform_media_pipeline) {
  // Create our audio decoders and renderer.
  ScopedVector<AudioDecoder> audio_decoders;

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  if (use_platform_media_pipeline) {
    audio_decoders.push_back(new PassThroughAudioDecoder(media_task_runner));
  } else {
#if defined(OS_MACOSX)
    audio_decoders.push_back(new ATAudioDecoder(media_task_runner));
#elif defined(OS_WIN)
    audio_decoders.push_back(new WMFAudioDecoder(media_task_runner));
#endif
#endif  // defined(USE_SYSTEM_PROPRIETARY_CODECS)

#if !defined(MEDIA_DISABLE_FFMPEG)
  audio_decoders.push_back(
      new FFmpegAudioDecoder(media_task_runner, media_log_));
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  }
#endif

  // Use an external decoder only if we cannot otherwise decode in the
  // renderer.
  if (decoder_factory_)
    decoder_factory_->CreateAudioDecoders(media_task_runner, &audio_decoders);

  return audio_decoders;
}

ScopedVector<VideoDecoder> DefaultRendererFactory::CreateVideoDecoders(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const RequestSurfaceCB& request_surface_cb,
    GpuVideoAcceleratorFactories* gpu_factories,
    bool use_platform_media_pipeline) {
  // Create our video decoders and renderer.
  ScopedVector<VideoDecoder> video_decoders;

  if (gpu_factories) {
    // |gpu_factories_| requires that its entry points be called on its
    // |GetTaskRunner()|.  Since |pipeline_| will own decoders created from the
    // factories, require that their message loops are identical.
    DCHECK(gpu_factories->GetTaskRunner() == media_task_runner.get());
  }

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  if (use_platform_media_pipeline) {
    video_decoders.push_back(new PassThroughVideoDecoder(media_task_runner));
  } else {
#endif

  // TODO(pgraszka): When chrome fixes the dropping frames issue in the
  // GpuVideoDecoder, we should make it our first choice on the list of video
  // decoders, for more details see: DNA-36050,
  // https://code.google.com/p/chromium/issues/detail?id=470466.
    if (decoder_factory_) {
      decoder_factory_->CreateVideoDecoders(media_task_runner, gpu_factories,
                                            &video_decoders);
    }
    if (gpu_factories) {
      video_decoders.push_back(
        new GpuVideoDecoder(gpu_factories, request_surface_cb, media_log_));
    }
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  }
#endif

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#if defined(OS_WIN)
    video_decoders.push_back(new WMFVideoDecoder(media_task_runner));
#elif defined(OS_MACOSX)
    if (!gpu_factories)
      pipeline_stats::ReportNoGpuProcessForDecoder();
#endif
#endif

#if !defined(MEDIA_DISABLE_LIBVPX)
  video_decoders.push_back(new VpxVideoDecoder());
#endif

#if !defined(MEDIA_DISABLE_FFMPEG) && !defined(DISABLE_FFMPEG_VIDEO_DECODERS)
  video_decoders.push_back(new FFmpegVideoDecoder());
#endif

  return video_decoders;
}

std::unique_ptr<Renderer> DefaultRendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    const RequestSurfaceCB& request_surface_cb,
    bool use_platform_media_pipeline,
    bool platform_pipeline_enlarges_buffers_on_underflow) {
  DCHECK(audio_renderer_sink);

  std::unique_ptr<AudioRenderer> audio_renderer(new AudioRendererImpl(
      media_task_runner, audio_renderer_sink,
      CreateAudioDecoders(media_task_runner,
          use_platform_media_pipeline), media_log_));

  GpuVideoAcceleratorFactories* gpu_factories = nullptr;
  if (!get_gpu_factories_cb_.is_null())
    gpu_factories = get_gpu_factories_cb_.Run();

  std::unique_ptr<VideoRenderer> video_renderer(new VideoRendererImpl(
      media_task_runner, worker_task_runner, video_renderer_sink,
      CreateVideoDecoders(media_task_runner, request_surface_cb, gpu_factories,
                          use_platform_media_pipeline),
      true, gpu_factories, media_log_));

  return base::MakeUnique<RendererImpl>(
      media_task_runner, std::move(audio_renderer), std::move(video_renderer));
}

}  // namespace media
