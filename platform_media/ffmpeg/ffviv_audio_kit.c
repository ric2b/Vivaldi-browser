// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#include <stdint.h>

#include "platform_media/ffmpeg/ffviv_audio_kit.h"

#include "third_party/ffmpeg/libavcodec/aac_ac3_parser.h"
#include "third_party/ffmpeg/libavcodec/adts_parser.h"
#include "third_party/ffmpeg/libavcodec/get_bits.h"
#include "third_party/ffmpeg/libavcodec/mpeg4audio.h"
#include "third_party/ffmpeg/libavcodec/mpeg4audio_copy_pce.h"
#include "third_party/ffmpeg/libavcodec/put_bits.h"
#include "third_party/ffmpeg/libavutil/error.h"
#include "third_party/ffmpeg/libavutil/log.h"
#include "third_party/ffmpeg/libavutil/mem.h"

static int ffviv_ensure_size(int size, uint8_t** data, int* data_size) {
  int ret = 0;

  if (size > *data_size) {
    uint8_t* tmp = av_realloc(*data, size);
    if (!tmp) {
      ret = AVERROR(ENOMEM);
      goto cleanup;
    }
    *data = tmp;
  }
  *data_size = size;

cleanup:
  return ret;
}

static int ffviv_ensure_zeros(int size, uint8_t** data, int* data_size) {
  int ret = ffviv_ensure_size(size, data, data_size);
  if (ret < 0)
    goto cleanup;

  memset(*data, 0, size);

cleanup:
  return ret;
}

int ffviv_copy_data(const void* source,
                    int size,
                    uint8_t** data,
                    int* data_size) {
  int ret = ffviv_ensure_size(size, data, data_size);
  if (ret < 0)
    goto cleanup;

  memcpy(*data, source, size);

cleanup:
  return ret;
}

void ffviv_finish_adts_converter(FFVIV_AdtsConverter* converter) {
  if (converter->channel_pce_data) {
    av_free(converter->channel_pce_data);
    converter->channel_pce_data = NULL;
    converter->channel_pce_size = 0;
  } else {
    av_assert1(converter->channel_pce_size == 0);
  }
}

int ffviv_check_adts_header(AVCodecContext* avctx,
                            FFVIV_AdtsConverter* converter,
                            const uint8_t* data,
                            size_t size) {
  int status = 0;
  GetBitContext gbc;
  PutBitContext pbc;

  av_assert1(*header_size == 0);

  memset(&converter->header, 0, sizeof converter->header);
  converter->header_size = 0;
  converter->channel_pce_size = 0;
  if (size < AV_AAC_ADTS_HEADER_SIZE)
    goto cleanup;

  init_get_bits8(&gbc, data, AV_AAC_ADTS_HEADER_SIZE);
  status = ff_adts_header_parse(&gbc, &converter->header);
  if (status == AAC_PARSE_ERROR_SYNC) {
    // ff_adts_header_parse() did not detect ADTS header. So this is raw AAC.
    status = 0;
    goto cleanup;
  }
  if (status < 0) {
    // Bad ADTS packet.
    goto cleanup;
  }
  if (!converter->header.crc_absent && converter->header.num_aac_frames > 1)
    goto invalid_data;
  int header_size = AV_AAC_ADTS_HEADER_SIZE;
  if (!converter->header.crc_absent) {
    if (size - header_size < 2)
      goto invalid_data;
    header_size += 2;
  }

  int channel_pce_size = 0;
  if (converter->header.chan_config == 0) {
    init_get_bits8(&gbc, data + header_size, size - header_size);
    // Check for channel PCE
    if (get_bits(&gbc, 3) != 5)
      goto invalid_data;

    if (!converter->channel_pce_data) {
      converter->channel_pce_data = av_mallocz(MAX_PCE_SIZE);
      if (!converter->channel_pce_data) {
        status = AVERROR(ENOMEM);
        goto cleanup;
      }
    }

    init_put_bits(&pbc, converter->channel_pce_data, MAX_PCE_SIZE);
    channel_pce_size = ff_copy_pce_data(&pbc, &gbc) / 8;
    flush_put_bits(&pbc);
    header_size += get_bits_count(&gbc) / 8;
  }
  if (header_size == size) {
    // Packet without any raw data
    goto invalid_data;
  }

  // We got a valid ADTS header
  converter->header_size = header_size;
  converter->channel_pce_size = channel_pce_size;

  av_log(avctx, AV_LOG_DEBUG,
         "ADTS: object_type=%d sample_rate=%d(%d) samples=%d bit_rate=%d "
         "sampling_index=%d chan_config=%d num_aac_frames=%d\n",
         converter->header.object_type, converter->header.sample_rate,
         avctx->sample_rate, converter->header.samples,
         converter->header.bit_rate, converter->header.sampling_index,
         converter->header.chan_config, converter->header.num_aac_frames);

cleanup:
  return status;

invalid_data:
  av_log(avctx, AV_LOG_ERROR, "invalid ADTS header\n");
  status = AVERROR_INVALIDDATA;
  goto cleanup;
}

int ffviv_convert_adts_to_aac_config(AVCodecContext* avctx,
                                     FFVIV_AdtsConverter* converter,
                                     uint8_t** data,
                                     int* data_size) {
  int size = 2 + converter->channel_pce_size;
  int status = ffviv_ensure_zeros(size, data, data_size);
  if (status < 0)
    goto cleanup;

  PutBitContext pbc;
  init_put_bits(&pbc, *data, size);
  put_bits(&pbc, 5, converter->header.object_type);
  put_bits(&pbc, 4, converter->header.sampling_index);
  put_bits(&pbc, 4, converter->header.chan_config);
  put_bits(&pbc, 1, 0);  // frame length - 1024 samples
  put_bits(&pbc, 1, 0);  // does not depend on core coder
  put_bits(&pbc, 1, 0);  // is not extension
  flush_put_bits(&pbc);
  if (converter->channel_pce_size != 0) {
    memcpy(*data + 2, converter->channel_pce_data, converter->channel_pce_size);
  }

cleanup:
  return status;
}

int ffviv_construct_simple_aac_config(AVCodecContext* avctx,
                                      int object_type,
                                      int sampling_rate,
                                      int channels,
                                      uint8_t** data,
                                      int* data_size) {
  int status = 0;

  if (object_type >= 31) {
    av_log(avctx, AV_LOG_ERROR, "unsupported object type %d\n", object_type);
    status = AVERROR_PATCHWELCOME;
    goto cleanup;
  }

  int sampling_index = 0;
  for (;; ++sampling_index) {
    if (sampling_index == sizeof ff_mpeg4audio_sample_rates /
                              sizeof ff_mpeg4audio_sample_rates[0]) {
      av_log(avctx, AV_LOG_ERROR, "unsupported sampling rate %d\n",
             sampling_rate);
      status = AVERROR_PATCHWELCOME;
      goto cleanup;
    }
    if (ff_mpeg4audio_sample_rates[sampling_index] == sampling_rate)
      break;
  }

  if (channels == 0 || channels == 7 || channels > 8) {
    av_log(avctx, AV_LOG_ERROR, "unsupported chanel number %d\n", channels);
    status = AVERROR_PATCHWELCOME;
    goto cleanup;
  }
  int chan_config = channels == 8 ? channels - 1 : channels;

  int size = 2;
  status = ffviv_ensure_zeros(size, data, data_size);
  if (status < 0)
    goto cleanup;

  PutBitContext pbc;
  init_put_bits(&pbc, *data, size);
  put_bits(&pbc, 5, object_type);
  put_bits(&pbc, 4, sampling_index);
  put_bits(&pbc, 4, chan_config);
  put_bits(&pbc, 1, 0);  // frame length - 1024 samples
  put_bits(&pbc, 1, 0);  // does not depend on core coder
  put_bits(&pbc, 1, 0);  // is not extension
  flush_put_bits(&pbc);

cleanup:
  return status;
}
