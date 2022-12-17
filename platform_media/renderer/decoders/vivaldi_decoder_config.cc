// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "platform_media/renderer/decoders/vivaldi_decoder_config.h"

#include "base/command_line.h"
#include "build/build_config.h"

#include "base/vivaldi_switches.h"
#include "platform_media/common/feature_toggles.h"

#if BUILDFLAG(IS_MAC)
#include "platform_media/renderer/decoders/mac/at_audio_decoder.h"
#include "platform_media/renderer/decoders/mac/viv_video_decoder.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "media/base/win/mf_helpers.h"
#include "platform_media/renderer/decoders/win/wmf_audio_decoder.h"
#include "platform_media/renderer/decoders/win/wmf_video_decoder.h"
#endif

#if !defined(USE_SYSTEM_PROPRIETARY_CODECS)
#error "This file should only be compiled with USE_SYSTEM_PROPRIETARY_CODECS"
#endif

namespace media {

/* static */
bool VivaldiDecoderConfig::OnlyFFmpegAudio() {
  static bool use_old = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kVivaldiOldPlatformAudio);

  return !use_old;
}

/* static */
void VivaldiDecoderConfig::AddAudioDecoders(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<AudioDecoder>>& decoders) {
  if (VivaldiDecoderConfig::OnlyFFmpegAudio())
    return;

    // The system audio decoders should be the first to take priority over
    // FFmpeg.
#if BUILDFLAG(IS_MAC)
  decoders.insert(decoders.begin(),
                  std::make_unique<ATAudioDecoder>(task_runner));
#elif BUILDFLAG(IS_WIN)
  decoders.insert(decoders.begin(),
                  std::make_unique<WMFAudioDecoder>(task_runner));
#endif
}

/* static */
void VivaldiDecoderConfig::AddVideoDecoders(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    std::vector<std::unique_ptr<VideoDecoder>>& decoders) {
#if BUILDFLAG(IS_MAC)
  decoders.push_back(VivVideoDecoder::Create(task_runner, media_log));
#endif  // IS_MAC
#if BUILDFLAG(IS_WIN)
  decoders.push_back(std::make_unique<WMFVideoDecoder>(task_runner));
#endif  // IS_WIN
}

}  // namespace media
