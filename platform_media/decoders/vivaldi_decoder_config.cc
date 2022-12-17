// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "platform_media/decoders/vivaldi_decoder_config.h"

#include "base/command_line.h"
#include "build/build_config.h"

#include "base/vivaldi_switches.h"

#if BUILDFLAG(IS_MAC)
#include "platform_media/decoders/mac/viv_video_decoder.h"
#endif
#if BUILDFLAG(IS_WIN)
#include "platform_media/decoders/win/wmf_video_decoder.h"
#endif

#if !defined(USE_SYSTEM_PROPRIETARY_CODECS)
#error "This file should only be compiled with USE_SYSTEM_PROPRIETARY_CODECS"
#endif

namespace media {

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
