// Copyright (c) 2022 Vivaldi Technologies AS. All rights reserved

#ifndef PLATFORM_MEDIA_FFMPEG_FFVIV_AUDIO_KIT_H_
#define PLATFORM_MEDIA_FFMPEG_FFVIV_AUDIO_KIT_H_

#include <stdint.h>

#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/ffmpeg/libavcodec/adts_header.h"
#include "third_party/ffmpeg/libavcodec/avcodec.h"

// Ensure that `data` has enough capaity to hold `size` bytes reallocating if
// necessary and copy `source` there. `data_size` must point to the current size
// of `data` on entry and will be updated to `size` on success. Return a
// negative value on errors.
int ffviv_copy_data(const void* source,
                    int size,
                    uint8_t** data,
                    int* data_size);

// To process AAC sound with ADTS headers FFmpeg provides aac_adtstoasc
// bit-stream filter that converts ADTS headers to extra meta data. In platform
// decoders we cannot use that as we need to know if the original data contains
// ADTS to adjust them to make decoders to work. To facilitate that we use this
// data structure and related method. The code here roughly follows
// aac_adtstoasc_filter.c in FFmpeg.
typedef struct FFVIV_AdtsConverter {
  AACADTSHeaderInfo header;
  int header_size;

  // PCE (Program Config Element) with channel config. Typically this is zero as
  // on Internet files with custom channel configuration not covered by presets
  // in `header` are extremely rare.
  int channel_pce_size;

  // The custom channel config if any.
  // Not raw_ptr because it is allocated using a cusotm allocator.
  RAW_PTR_EXCLUSION uint8_t* channel_pce_data;

} FFVIV_AdtsConverter;

// Release `converter` data. There is no init method as zero-initializing
// `converter` is enough.
void ffviv_finish_adts_converter(FFVIV_AdtsConverter* converter);

// Check if `data` contains ADTS header. If so, on return
// `converter->header_size` will be non-zero and `converter->header` will
// contain the parsed header. If no ADTS header were detected,
// `converter->header_size` is zero. Return 0 on success or a negative value on
// errors due to invalid ADTS or failed memory allocation.
int ffviv_check_adts_header(AVCodecContext* avctx,
                            FFVIV_AdtsConverter* converter,
                            const uint8_t* data,
                            size_t size);

// Assuming `converter->header_size` is not zero, convert the header into the
// extra metadata or AudioSpecificConfig as those are known in AAC spec.
// `audio_config_data` and `audio_config_size` must point to the previous values
// of the audio config bytes and its size or to zero values if there were none.
// Return 0 on success or a negative value on errors.
int ffviv_convert_adts_to_aac_config(AVCodecContext* avctx,
                                     FFVIV_AdtsConverter* converter,
                                     uint8_t** data,
                                     int* data_size);

int ffviv_construct_simple_aac_config(AVCodecContext* avctx,
                                      int object_type,
                                      int sampling_rate,
                                      int channels,
                                      uint8_t** data,
                                      int* data_size);

#endif  // PLATFORM_MEDIA_FFMPEG_FFVIV_AUDIO_KIT_H_
