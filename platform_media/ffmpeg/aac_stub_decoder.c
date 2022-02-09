// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

/*
 * AAC decoder
 * Copyright (c) 2005-2006 Oded Shimon ( ods15 ods15 dyndns org )
 * Copyright (c) 2006-2007 Maxim Gavrilov ( maxim.gavrilov gmail com )
 * Copyright (c) 2008-2013 Alex Converse <alex.converse@gmail.com>
 *
 * AAC LATM decoder
 * Copyright (c) 2008-2010 Paul Kendall <paul@kcbbs.gen.nz>
 * Copyright (c) 2010      Janne Grunau <janne-libav@jannau.net>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "third_party/ffmpeg/libavcodec/aacdectab.h"
#include "third_party/ffmpeg/libavcodec/profiles.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

// Provide a minimal stub decoder implementation to convince the rest of FFmpeg
// that AAC is supported. The real decoding is implemented outside FFmpeg in
// ATAudioDecoder and WMFAudioDecoder classes using Chromium decoding
// infrastructure.

static int aac_stub_decode(AVCodecContext* avctx,
                           void* out,
                           int* got_frame_ptr,
                           AVPacket* avpkt) {
  return AVERROR_INVALIDDATA;
}

static void aac_stub_flush(AVCodecContext* avctx) {}

static av_cold int aac_stub_init(AVCodecContext* avctx) {
  avctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
  return 0;
}

static av_cold int aac_stub_close(AVCodecContext* avctx) {
  return 0;
}

/**
 * AVOptions for Japanese DTV specific extensions (ADTS only)
 */
#define AACDEC_FLAGS AV_OPT_FLAG_DECODING_PARAM | AV_OPT_FLAG_AUDIO_PARAM
static const AVOption options[] = {
    {"dual_mono_mode",
     "Select the channel to decode for dual mono",
     offsetof(AACContext, force_dmono_mode),
     AV_OPT_TYPE_INT,
     {.i64 = -1},
     -1,
     2,
     AACDEC_FLAGS,
     "dual_mono_mode"},

    {"auto",
     "autoselection",
     0,
     AV_OPT_TYPE_CONST,
     {.i64 = -1},
     INT_MIN,
     INT_MAX,
     AACDEC_FLAGS,
     "dual_mono_mode"},

    {"main",
     "Select Main/Left channel",
     0,
     AV_OPT_TYPE_CONST,
     {.i64 = 1},
     INT_MIN,
     INT_MAX,
     AACDEC_FLAGS,
     "dual_mono_mode"},

    {"sub",
     "Select Sub/Right channel",
     0,
     AV_OPT_TYPE_CONST,
     {.i64 = 2},
     INT_MIN,
     INT_MAX,
     AACDEC_FLAGS,
     "dual_mono_mode"},

    {"both",
     "Select both channels",
     0,
     AV_OPT_TYPE_CONST,
     {.i64 = 0},
     INT_MIN,
     INT_MAX,
     AACDEC_FLAGS,
     "dual_mono_mode"},

    {NULL},
};

static const AVClass aac_decoder_class = {
    .class_name = "AAC decoder",
    .item_name = av_default_item_name,
    .option = options,
    .version = LIBAVUTIL_VERSION_INT,
};

const AVCodec ff_aac_decoder = {
    .name = "aac",
    .long_name = NULL_IF_CONFIG_SMALL("AAC (Advanced Audio Coding)"),
    .type = AVMEDIA_TYPE_AUDIO,
    .id = AV_CODEC_ID_AAC,
    .priv_data_size = sizeof(void*),
    .init = aac_stub_init,
    .close = aac_stub_close,
    .decode = aac_stub_decode,
    .sample_fmts =
        (const enum AVSampleFormat[]){AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE},
    .capabilities = AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DR1,
    .caps_internal = FF_CODEC_CAP_INIT_THREADSAFE | FF_CODEC_CAP_INIT_CLEANUP,
    .channel_layouts = aac_channel_layout,
    .flush = aac_stub_flush,
    .priv_class = &aac_decoder_class,
    .profiles = NULL_IF_CONFIG_SMALL(ff_aac_profiles),
};
