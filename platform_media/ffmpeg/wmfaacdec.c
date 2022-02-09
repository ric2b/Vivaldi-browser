// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include <Windows.h>

#include <mfapi.h>

#include <VersionHelpers.h>
#include <mferror.h>
#include <mftransform.h>
#include <wmcodecdsp.h>

#include <stdbool.h>

#include "config.h"
#include "third_party/ffmpeg/libavcodec/aac_ac3_parser.h"
#include "third_party/ffmpeg/libavcodec/ac3_parser_internal.h"
#include "third_party/ffmpeg/libavcodec/adts_header.h"
#include "third_party/ffmpeg/libavcodec/avcodec.h"
#include "third_party/ffmpeg/libavcodec/bytestream.h"
#include "third_party/ffmpeg/libavcodec/decode.h"
#include "third_party/ffmpeg/libavcodec/get_bits.h"
#include "third_party/ffmpeg/libavcodec/internal.h"
#include "third_party/ffmpeg/libavcodec/mpegaudiodecheader.h"
#include "third_party/ffmpeg/libavutil/avassert.h"
#include "third_party/ffmpeg/libavutil/log.h"
#include "third_party/ffmpeg/libavutil/opt.h"

#define FFWMF_EXTRA_STDERR_LOG 0

typedef struct WMFDecodeContext {
  AVClass* av_class;

  IMFTransform* decoder;
  IMFSample* output_sample;
  uint8_t* extradata;
  int extradata_size;
  int output_samples_per_second;
  int output_channel_count;
  int output_bits_per_sample;
  uint32_t input_stream_alignment;
  bool has_more_output_samples;
  bool adts_mode;
  bool after_eof;
} WMFDecodeContext;

typedef HRESULT(STDAPICALLTYPE* ffwmf_DllGetClassObject_ptr)(REFCLSID rclsid,
                                                             REFIID riid,
                                                             LPVOID* ppv);

static ffwmf_DllGetClassObject_ptr ffwmf_get_class_object = NULL;

#define FFWMF_DEFAULT_STREAM_ID ((DWORD)0)

// IMFSample's timestamp is expressed in hundreds of nanoseconds.
#define FFWMF_SAMPLE_TIME_UNITS_PER_SECOND ((double)1.0e7)

#define FFWMF_LOG_ERROR(avctx, ...)                                           \
  do {                                                                        \
    char _buf[100];                                                           \
    snprintf(_buf, sizeof(_buf), __VA_ARGS__);                                \
    av_log(avctx, AV_LOG_ERROR, "%s:%d: %s", __FILE__, __LINE__, _buf);       \
    if (FFWMF_EXTRA_STDERR_LOG) {                                             \
      const char* _name = strrchr(__FILE__, '/');                             \
      _name = _name ? _name + 1 : __FILE__;                                   \
      fprintf(stderr, "%s:%d:%s:\tERROR %s\n", _name, __LINE__, __FUNCTION__, \
              _buf);                                                          \
      fflush(stderr);                                                         \
    }                                                                         \
  } while (0)

#define FFWMF_DEBUG_LOG(avctx, ...)                                            \
  do {                                                                         \
    if (FFWMF_EXTRA_STDERR_LOG) {                                              \
      char _buf[100];                                                          \
      snprintf(_buf, sizeof(_buf), __VA_ARGS__);                               \
      const char* _name = strrchr(__FILE__, '/');                              \
      _name = _name ? _name + 1 : __FILE__;                                    \
      fprintf(stderr, "%s:%d:%s:\t%s\n", _name, __LINE__, __FUNCTION__, _buf); \
      fflush(stderr);                                                          \
    }                                                                          \
  } while (0)

#define FFWMF_CHECK_STATUS() \
  do {                       \
    if (status < 0)          \
      goto cleanup;          \
  } while (0)

#define FFWMF_RETURN_STATUS() \
  do {                        \
    return status;            \
  } while (0)

#define FFWMF_LOG_HRESULT(hr, details) \
  FFWMF_LOG_ERROR(avctx, "failed %s, hr=0x%lX\n", details, hr)

#define FFWMF_CHECK_HRESULT(hr, details) \
  do {                                   \
    av_assert1(status >= 0);             \
    if (FAILED(hr)) {                    \
      FFWMF_LOG_HRESULT(hr, details);    \
      status = AVERROR_UNKNOWN;          \
      goto cleanup;                      \
    }                                    \
  } while (0)

HMODULE ffwmf_aac_dll = NULL;

// Permanently load WMF library.
static HMODULE ffwmf_load_audio_library(int internal) {
  if (!ffwmf_aac_dll) {
    // The name of the library changed with Windows-8.
    const wchar_t* dll_name =
        IsWindows8OrGreater() ? L"msauddecmft.dll" : L"msmpeg2adec.dll";

    // Query if the library was already loaded. This is useful in case the
    // application use a sandbox with a library preload when LoadLibrary() fails
    // but gettings its handle works.
    if (!GetModuleHandleEx(0, dll_name, &ffwmf_aac_dll)) {
      ffwmf_aac_dll = LoadLibrary(dll_name);
    }
  }

  return ffwmf_aac_dll;
}

static int ffwmf_copy_extra_data(AVCodecContext* avctx,
                                 const void* extradata,
                                 int extradata_size) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;

  if (extradata_size < 0) {
    // This may never happen according to FFmpeg sources, but lets be safe.
    extradata_size = 0;
  }

  if (wmf->extradata) {
    av_free(wmf->extradata);
    wmf->extradata = NULL;
  }
  wmf->extradata_size = extradata_size;
  if (extradata_size) {
    wmf->extradata = av_malloc(extradata_size);
    if (!wmf->extradata) {
      wmf->extradata_size = 0;
      status = AVERROR(ENOMEM);
    } else {
      memcpy(wmf->extradata, extradata, extradata_size);
    }
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_create_transformer(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IClassFactory* factory = NULL;

  av_assert1(!wmf->decoder);

  HMODULE library = ffwmf_load_audio_library(1);
  if (!library) {
    FFWMF_LOG_ERROR(avctx, "failed to load WMF audio DLL\n");
    status = AVERROR_UNKNOWN;
    goto cleanup;
  }

  // CoCreateInstance() may not be available due to sandbox restrictions, so
  // lookup the factory method directly.
  static const char get_class_object_name[] = "DllGetClassObject";
  if (!ffwmf_get_class_object) {
    ffwmf_get_class_object = (ffwmf_DllGetClassObject_ptr)GetProcAddress(
        library, get_class_object_name);
    if (!ffwmf_get_class_object) {
      FFWMF_LOG_ERROR(avctx, "failed to retrive %s\n", get_class_object_name);
      status = AVERROR_UNKNOWN;
      goto cleanup;
    }
  }

  av_assert1(avctx->codec_id == AV_CODEC_ID_AAC);
  GUID guid = CLSID_CMSAACDecMFT;

  HRESULT hr =
      ffwmf_get_class_object(&guid, &IID_IClassFactory, (void**)&factory);
  FFWMF_CHECK_HRESULT(hr, get_class_object_name);

  hr = factory->lpVtbl->CreateInstance(factory, NULL, &IID_IMFTransform,
                                       (void**)&wmf->decoder);
  FFWMF_CHECK_HRESULT(hr, "IClassFactory::CreateInstance()");

cleanup:
  if (factory) {
    factory->lpVtbl->Release(factory);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_set_input_media_type(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaType* media_type = NULL;
  HEAACWAVEFORMAT* aac_wave_format = NULL;
  HRESULT hr;

  // For details how to fill HEAACWAVEFORMAT see
  // https://docs.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-heaacwaveformat
  size_t format_size = offsetof(HEAACWAVEFORMAT, pbAudioSpecificConfig);
  if (!wmf->adts_mode) {
    format_size += wmf->extradata_size;
  }
  aac_wave_format = av_malloc(format_size);
  if (!aac_wave_format) {
    status = AVERROR(ENOMEM);
    goto cleanup;
  }
  memset(aac_wave_format, 0, format_size);
  aac_wave_format->wfInfo.wfx.wFormatTag = WAVE_FORMAT_MPEG_HEAAC;

  // Both FFmpeg and WMF uses 0 channels as unknown.
  aac_wave_format->wfInfo.wfx.nChannels = avctx->channels;

  // Both FFmpeg and WMF uses 0 rate as unknown.
  aac_wave_format->wfInfo.wfx.nSamplesPerSec = avctx->sample_rate;

  aac_wave_format->wfInfo.wfx.nBlockAlign = 1;
  aac_wave_format->wfInfo.wfx.cbSize = format_size - sizeof(WAVEFORMATEX);
  if (wmf->adts_mode) {
    // Set input type to adts and do not set the extra.
    aac_wave_format->wfInfo.wPayloadType = 1;
  } else {
    // Set the input type to raw and copy the extra config.
    aac_wave_format->wfInfo.wPayloadType = 0;
    memcpy(aac_wave_format->pbAudioSpecificConfig, wmf->extradata,
           wmf->extradata_size);
  }

  hr = MFCreateMediaType(&media_type);
  FFWMF_CHECK_HRESULT(hr, "MFCreateMediaType()");

  hr = MFInitMediaTypeFromWaveFormatEx(media_type, &aac_wave_format->wfInfo.wfx,
                                       format_size);
  FFWMF_CHECK_HRESULT(hr, "MFInitMediaTypeFromWaveFormatEx()");

  hr = wmf->decoder->lpVtbl->SetInputType(wmf->decoder, FFWMF_DEFAULT_STREAM_ID,
                                          media_type, /*fwFlags=*/0);
  if (FAILED(hr)) {
    const char* error_name;
    switch (hr) {
      case S_OK:
        error_name = "S_OK";
        break;
      case MF_E_INVALIDMEDIATYPE:
        error_name = "MF_E_INVALIDMEDIATYPE";
        break;
      case MF_E_INVALIDSTREAMNUMBER:
        error_name = "MF_E_INVALIDSTREAMNUMBER";
        break;
      case MF_E_INVALIDTYPE:
        error_name = "MF_E_INVALIDTYPE";
        break;
      case MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING:
        error_name = "MF_E_TRANSFORM_CANNOT_CHANGE_MEDIATYPE_WHILE_PROCESSING";
        break;
      case MF_E_TRANSFORM_TYPE_NOT_SET:
        error_name = "MF_E_TRANSFORM_TYPE_NOT_SET";
        break;
      case MF_E_UNSUPPORTED_D3D_TYPE:
        error_name = "MF_E_UNSUPPORTED_D3D_TYPE";
        break;
      default:
        error_name = "other";
        break;
    }
    FFWMF_LOG_ERROR(avctx,
                    "failed IMFTransform::SetInputType(), error=%s(hr=0x%lX)\n",
                    error_name, hr);
    status = AVERROR_UNKNOWN;
    goto cleanup;
  }

cleanup:
  if (media_type) {
    media_type->lpVtbl->Release(media_type);
  }
  if (aac_wave_format) {
    av_free(aac_wave_format);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_set_output_media_type_for_index(AVCodecContext* avctx,
                                                 DWORD index,
                                                 bool* found) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaType* media_type = NULL;

  av_assert1(!*found);

  HRESULT hr = wmf->decoder->lpVtbl->GetOutputAvailableType(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, index, &media_type);
  if (hr == MF_E_NO_MORE_TYPES) {
    FFWMF_LOG_ERROR(avctx,
                    "failed to find PCM format among supported outputs\n");
    status = AVERROR_UNKNOWN;
    goto cleanup;
  }
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetOutputAvailableType()");

  GUID subtype = {0};
  hr = media_type->lpVtbl->GetGUID(media_type, &MF_MT_SUBTYPE, &subtype);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::GetGUID(MF_MT_SUBTYPE)");

  if (!IsEqualGUID(&subtype, &MFAudioFormat_PCM))
    goto cleanup;

  DWORD output_bits_per_sample = 0;
  DWORD output_samples_per_second = 0;
  DWORD output_channel_count = 0;

  hr = media_type->lpVtbl->GetUINT32(media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE,
                                     &output_bits_per_sample);
  FFWMF_CHECK_HRESULT(hr,
                      "IMFMediaType::GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE)");
  if (output_bits_per_sample != 16 && output_bits_per_sample != 32)
    goto cleanup;

  hr = media_type->lpVtbl->GetUINT32(
      media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &output_samples_per_second);
  FFWMF_CHECK_HRESULT(
      hr, "IMFMediaType::GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND)");

  hr = media_type->lpVtbl->GetUINT32(media_type, &MF_MT_AUDIO_NUM_CHANNELS,
                                     &output_channel_count);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaType::GetUINT32(MF_MT_AUDIO_NUM_CHANNELS)");

  hr = wmf->decoder->lpVtbl->SetOutputType(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, media_type, /*flags=*/0);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::SetOutputType()");

  wmf->output_bits_per_sample = output_bits_per_sample;
  wmf->output_channel_count = output_channel_count;
  wmf->output_samples_per_second = output_samples_per_second;

  *found = true;

cleanup:
  if (media_type) {
    media_type->lpVtbl->Release(media_type);
  }
  FFWMF_RETURN_STATUS();
}

// On entry |*sample_result| must be null. On a successfull return the caller is
// responsible for calling |Release()| on |*sample_result|. On errors
// |*sample_result| is kept at null.
static int ffwmf_create_sample(AVCodecContext* avctx,
                               DWORD buffer_size,
                               DWORD buffer_alignment_size,
                               IMFSample** sample_result) {
  IMFSample* sample = NULL;
  IMFMediaBuffer* buffer = NULL;
  int status = 0;

  av_assert1(!*sample_result);

  HRESULT hr = MFCreateSample(&sample);
  FFWMF_CHECK_HRESULT(hr, "MFCreateSample()");

  /*
   * Assume buffer_alignment_size is one of the sizes in
   * https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mfcreatealignedmemorybuffer
   */
  DWORD alignment_param = buffer_alignment_size ? buffer_alignment_size - 1 : 0;
  hr = MFCreateAlignedMemoryBuffer(buffer_size, alignment_param, &buffer);
  FFWMF_CHECK_HRESULT(hr, "MFCreateAlignedMemoryBuffer()");

  hr = sample->lpVtbl->AddBuffer(sample, buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::AddBuffer()");

  // We successfully created and initialized the sample. Transfer the ownership
  // to the result to prevent releasing it in the cleanup.
  *sample_result = sample;
  sample = NULL;

cleanup:
  if (sample) {
    sample->lpVtbl->Release(sample);
  }
  if (buffer) {
    buffer->lpVtbl->Release(buffer);
  }
  FFWMF_RETURN_STATUS();
}

static int ffwmf_set_output_media_type(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;
  int status = 0;

  bool found = false;
  for (DWORD i = 0; !found; ++i) {
    status = ffwmf_set_output_media_type_for_index(avctx, i, &found);
    FFWMF_CHECK_STATUS();
  }

  avctx->sample_fmt = (wmf->output_bits_per_sample == 32) ? AV_SAMPLE_FMT_S32
                                                          : AV_SAMPLE_FMT_S16;
  avctx->bits_per_raw_sample = wmf->output_bits_per_sample;
  avctx->sample_rate = wmf->output_samples_per_second;
  avctx->channels = wmf->output_channel_count;
  avctx->channel_layout = av_get_default_channel_layout(avctx->channels);

  MFT_OUTPUT_STREAM_INFO output_stream_info = {0};
  HRESULT hr = wmf->decoder->lpVtbl->GetOutputStreamInfo(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, &output_stream_info);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetOutputStreamInfo()");

  // Create the output sample that is used for all output calls unless MFT
  // insists that it allocates the sample.
  if (wmf->output_sample) {
    wmf->output_sample->lpVtbl->Release(wmf->output_sample);
    wmf->output_sample = NULL;
  }
  if (!(output_stream_info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
    status = ffwmf_create_sample(avctx, output_stream_info.cbSize,
                                 output_stream_info.cbAlignment,
                                 &wmf->output_sample);
    FFWMF_CHECK_STATUS();
  }

cleanup:
  FFWMF_RETURN_STATUS();
}

static int ffwmf_setup_transformer(AVCodecContext* avctx) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;

  status = ffwmf_create_transformer(avctx);
  FFWMF_CHECK_STATUS();

  status = ffwmf_set_input_media_type(avctx);
  FFWMF_CHECK_STATUS();

  status = ffwmf_set_output_media_type(avctx);
  FFWMF_CHECK_STATUS();

  /* This must be called after both input and output are configured. */
  MFT_INPUT_STREAM_INFO input_stream_info;
  HRESULT hr = wmf->decoder->lpVtbl->GetInputStreamInfo(
      wmf->decoder, FFWMF_DEFAULT_STREAM_ID, &input_stream_info);
  FFWMF_CHECK_HRESULT(hr, "IMFTransform::GetInputStreamInfo()");
  wmf->input_stream_alignment = input_stream_info.cbAlignment;

cleanup:
  FFWMF_RETURN_STATUS();
}

static int ffwmf_process_input(AVCodecContext* avctx, AVPacket* pkt) {
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFSample* input_sample = NULL;
  IMFMediaBuffer* buffer = NULL;
  int status = 0;

  av_assert1(pkt->size);

  status = ffwmf_create_sample(avctx, pkt->size, wmf->input_stream_alignment,
                               &input_sample);
  FFWMF_CHECK_STATUS();

  HRESULT hr = input_sample->lpVtbl->GetBufferByIndex(input_sample, 0, &buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::GetBufferByIndex()");

  uint8_t* buffer_ptr = NULL;
  hr = buffer->lpVtbl->Lock(buffer, &buffer_ptr, NULL, NULL);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Lock()");

  memcpy(buffer_ptr, pkt->data, pkt->size);

  hr = buffer->lpVtbl->Unlock(buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Unlock()");

  hr = buffer->lpVtbl->SetCurrentLength(buffer, pkt->size);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::SetCurrentLength()");

  if (pkt->pts != AV_NOPTS_VALUE) {
    double presentation_time_in_seconds = pkt->pts * av_q2d(avctx->time_base);

    hr = input_sample->lpVtbl->SetSampleTime(
        input_sample, (int64_t)(presentation_time_in_seconds *
                                FFWMF_SAMPLE_TIME_UNITS_PER_SECOND));
    FFWMF_CHECK_HRESULT(hr, "IMFSample::SetSampleTime()");
  }

  hr = wmf->decoder->lpVtbl->ProcessInput(wmf->decoder, FFWMF_DEFAULT_STREAM_ID,
                                          input_sample, /*dwFlags=*/0);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::SetSampleTime()");

cleanup:
  if (buffer) {
    buffer->lpVtbl->Release(buffer);
  }
  if (input_sample) {
    input_sample->lpVtbl->Release(input_sample);
  }
  FFWMF_RETURN_STATUS();
}

static int process_output_sample(AVCodecContext* avctx,
                                 IMFSample* sample,
                                 AVFrame* frame) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaBuffer* buffer = NULL;
  bool locked_buffer = false;
  HRESULT hr;

  frame->sample_rate = wmf->output_samples_per_second;

  hr = sample->lpVtbl->ConvertToContiguousBuffer(sample, &buffer);
  FFWMF_CHECK_HRESULT(hr, "IMFSample::ConvertToContiguousBuffer()");

  uint8_t* data = NULL;
  DWORD data_size = 0;
  hr = buffer->lpVtbl->Lock(buffer, &data, NULL, &data_size);
  FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::Lock()");

  locked_buffer = true;

  int sample_size =
      wmf->output_channel_count * (wmf->output_bits_per_sample / 8);
  frame->nb_samples = data_size / sample_size;
  data_size = frame->nb_samples * sample_size;

  if (frame->nb_samples == 0) {
    status = AVERROR(EAGAIN);
    goto cleanup;
  }
  if (wmf->after_eof) {
    // We are draining the transformer to get the last frame. But for some
    // reasons AAC decoder may return all-zeros useless frame. Drop it if so and
    // return EOF.
    int i;
    bool all_zeros = true;
    for (i = 0; i < data_size; ++i) {
      if (data[i] != 0) {
        all_zeros = false;
        break;
      }
    }
    if (all_zeros) {
      status = AVERROR_EOF;
      goto cleanup;
    }
  }
  status = ff_get_buffer(avctx, frame, 0);
  FFWMF_CHECK_STATUS();

  // For the interleave formats like AV_SAMPLE_FMT_S32 and AV_SAMPLE_FMT_S16
  // where each sample contains the data for all channels the pointer to the
  // first and only audio buffer is in frame->data[0]. So we just copy there
  // what WMF returned.
  memcpy(frame->data[0], data, data_size);

cleanup:
  if (buffer) {
    if (locked_buffer) {
      hr = buffer->lpVtbl->Unlock(buffer);
      if (FAILED(hr)) {
        FFWMF_LOG_HRESULT(hr, "IMFMediaBuffer::Lock()");
        status = AVERROR_UNKNOWN;
      }
    }
    buffer->lpVtbl->Release(buffer);
  }
  return status;
}

static int ffwmf_process_output(AVCodecContext* avctx, AVFrame* frame) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  IMFMediaBuffer* buffer = NULL;
  HRESULT hr = S_OK;

  wmf->has_more_output_samples = false;
  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  for (;;) {
    // Make the whole buffer available for use by |decoder_| again after it was
    // filled with data by the previous call to ProcessOutput().
    if (wmf->output_sample) {
      hr = wmf->output_sample->lpVtbl->ConvertToContiguousBuffer(
          wmf->output_sample, &buffer);
      FFWMF_CHECK_HRESULT(hr, "IMFSample::ConvertToContiguousBuffer()");

      hr = buffer->lpVtbl->SetCurrentLength(buffer, 0);
      FFWMF_CHECK_HRESULT(hr, "IMFMediaBuffer::SetCurrentLength(0)");

      buffer->lpVtbl->Release(buffer);
      buffer = NULL;
    } else if (output_data_buffer.pSample) {
      // The sample was allocated by MFT.
      output_data_buffer.pSample->lpVtbl->Release(output_data_buffer.pSample);
      output_data_buffer.pSample = NULL;
    }

    memset(&output_data_buffer, 0, sizeof output_data_buffer);
    output_data_buffer.pSample = wmf->output_sample;
    output_data_buffer.dwStreamID = FFWMF_DEFAULT_STREAM_ID;
    DWORD process_output_status = 0;
    hr = wmf->decoder->lpVtbl->ProcessOutput(
        wmf->decoder, /*dwFlags=*/0, /*cOutputBufferCount=*/1,
        &output_data_buffer, &process_output_status);
    if (output_data_buffer.pEvents) {
      // Even though we're not interested in events we have to clean them up.
      output_data_buffer.pEvents->lpVtbl->Release(output_data_buffer.pEvents);
      output_data_buffer.pEvents = NULL;
    }
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      // This is normal, return to ffmpeg without any decoded data to indicate
      // that more input is necessary.
      status = AVERROR(EAGAIN);
      goto cleanup;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      // Update the output media type.
      status = ffwmf_set_output_media_type(avctx);
      FFWMF_CHECK_STATUS();

      // Try to get more output.
      continue;
    }
    // Report any other non-successfull HRESULT.
    FFWMF_CHECK_HRESULT(hr, "IMFTransform::ProcessOutput()");

    status = process_output_sample(avctx, output_data_buffer.pSample, frame);
    if (status != AVERROR(EAGAIN)) {
      FFWMF_CHECK_STATUS();
      if (output_data_buffer.dwStatus & MFT_OUTPUT_DATA_BUFFER_INCOMPLETE) {
        // With ADTS AAC it seems the MFT sets this flag even if the following
        // |ProcessOutput()| returns MF_E_TRANSFORM_NEED_MORE_INPUT contrary to
        // MS documentation. It probably indicates that if we send
        // |MFT_MESSAGE_COMMAND_DRAIN| message then more data will be returned
        // without any extra input. But en extra call to ProcessOutput() is
        // harmless, so we do that whenever the flag is set, see
        // https://stackoverflow.com/questions/69830565/imftransfomerprocessinput-and-mf-e-transform-need-more-input/69831748
        wmf->has_more_output_samples = true;
      }
      goto cleanup;
    }
    status = 0;
  }

cleanup:
  if (!wmf->output_sample && output_data_buffer.pSample) {
    // The sample was allocated by MFT.
    output_data_buffer.pSample->lpVtbl->Release(output_data_buffer.pSample);
  }
  if (buffer) {
    buffer->lpVtbl->Release(buffer);
  }
  return status;
}

static av_cold int ffwmf_init_decoder(AVCodecContext* avctx) {
  int status;

  status =
      ffwmf_copy_extra_data(avctx, avctx->extradata, avctx->extradata_size);
  FFWMF_CHECK_STATUS();

cleanup:
  FFWMF_DEBUG_LOG(avctx, "return=%d", status);
  FFWMF_RETURN_STATUS();
}

static av_cold int ffwmf_close_decoder(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;

  if (wmf->decoder) {
    wmf->decoder->lpVtbl->Release(wmf->decoder);
    wmf->decoder = NULL;
  }
  if (wmf->output_sample) {
    wmf->output_sample->lpVtbl->Release(wmf->output_sample);
    wmf->output_sample = NULL;
  }
  if (wmf->extradata) {
    av_free(wmf->extradata);
    wmf->extradata = NULL;
  }
  FFWMF_DEBUG_LOG(avctx, "closed");
  return 0;
}

static int ffwmf_check_adts_header(AVCodecContext* avctx,
                                   const uint8_t* data,
                                   size_t size,
                                   bool* with_adts_header) {
  int status = 0;
  AACADTSHeaderInfo adts_header = {0};
  GetBitContext gbc;

  av_assert1(!*with_adts_header);
  init_get_bits(&gbc, data, size);
  status = ff_adts_header_parse(&gbc, &adts_header);
  if (status >= 0) {
    status = 0;
    *with_adts_header = true;
  } else if (status == AAC_AC3_PARSE_ERROR_SYNC) {
    // ff_adts_header_parse() did not detect ADTS header. So this is raw AAC.
    status = 0;
  } else {
    // Bad ADTS packet.
    goto cleanup;
  }

cleanup:
  FFWMF_RETURN_STATUS();
}

static int ffwmf_receive_frame(AVCodecContext* avctx, AVFrame* frame) {
  int status = 0;
  WMFDecodeContext* wmf = avctx->priv_data;
  AVPacket pkt;
  bool has_packet = false;

  if (wmf->has_more_output_samples) {
    // See if we can read more before trying to get a new packet.
    status = ffwmf_process_output(avctx, frame);
    if (status != AVERROR(EAGAIN)) {
      FFWMF_CHECK_STATUS();
      goto cleanup;
    }
  }

  if (wmf->after_eof) {
    // No more data to read.
    status = AVERROR_EOF;
    goto cleanup;
  }

  status = ff_decode_get_packet(avctx, &pkt);
  if (status == AVERROR(EAGAIN))
    goto cleanup;
  if (status == AVERROR_EOF) {
    wmf->after_eof = true;
    // Drain the transformer so it can return the last sample which can be
    // incomplete or zero. We do it only for ADTS input as according to
    // https://docs.microsoft.com/en-us/windows/win32/medfound/aac-decoder and
    // testing the raw AAC data always generate strictly one output per input
    // and there is nothing to drain.
    if (!wmf->adts_mode)
      goto cleanup;
    HRESULT hr = wmf->decoder->lpVtbl->ProcessMessage(
        wmf->decoder, MFT_MESSAGE_COMMAND_DRAIN, 0);
    if (FAILED(hr)) {
      // Just log the error, do not report it to FFmpeg.
      FFWMF_LOG_HRESULT(hr, "IMFTransform::ProcessMessage()");
    }
  } else {
    FFWMF_CHECK_STATUS();
    has_packet = true;
    if (!pkt.size) {
      FFWMF_DEBUG_LOG(avctx, "empty_packet");
    }
  }

  if (has_packet && pkt.size) {
    // Check if the data includes ADTS header.
    bool with_adts_header = false;
    status =
        ffwmf_check_adts_header(avctx, pkt.data, pkt.size, &with_adts_header);
    FFWMF_CHECK_STATUS();

    size_t new_extra_data_size = 0;
    uint8_t* new_extra_data = av_packet_get_side_data(
        &pkt, AV_PKT_DATA_NEW_EXTRADATA, &new_extra_data_size);
    if (new_extra_data) {
      status = ffwmf_copy_extra_data(wmf, new_extra_data, new_extra_data_size);
      FFWMF_CHECK_STATUS();
    }

    if (!wmf->decoder) {
      wmf->adts_mode = with_adts_header;
      status = ffwmf_setup_transformer(avctx);
      FFWMF_CHECK_STATUS();
    } else if (wmf->adts_mode != with_adts_header) {
      FFWMF_LOG_ERROR(avctx,
                      "Mix of ADTS and non-adts packets is not supported\n");
      status = AVERROR_INVALIDDATA;
      goto cleanup;
    }

    status = ffwmf_process_input(avctx, &pkt);
    if (status == AVERROR(EAGAIN))
      goto cleanup;
    FFWMF_CHECK_STATUS();

    // Release the packet now as we no longer need it.
    av_packet_unref(&pkt);
    has_packet = false;
  }

  status = ffwmf_process_output(avctx, frame);
  FFWMF_CHECK_STATUS();

cleanup:
  if (has_packet) {
    av_packet_unref(&pkt);
  }
  FFWMF_DEBUG_LOG(avctx, "return=%d", status);
  FFWMF_RETURN_STATUS();
}

static void ffwmf_flush(AVCodecContext* avctx) {
  WMFDecodeContext* wmf = avctx->priv_data;

  HRESULT hr = wmf->decoder->lpVtbl->ProcessMessage(
      wmf->decoder, MFT_MESSAGE_COMMAND_FLUSH, 0);
  if (FAILED(hr)) {
    FFWMF_LOG_HRESULT(hr, "IMFTransform::ProcessMessage()");
  }
  FFWMF_DEBUG_LOG(avctx, "flushed");
}

static const AVClass ffwmf_aac_decoder_class = {
    .class_name = "WMF AAC decoder",
    .version = LIBAVUTIL_VERSION_INT,
};

const AVCodec ffwmf_aac_decoder = {
    .name = "aac_wmf",
    .long_name = NULL_IF_CONFIG_SMALL("AAC (Windows Media Foundation)"),
    .type = AVMEDIA_TYPE_AUDIO,
    .id = AV_CODEC_ID_AAC,
    .priv_data_size = sizeof(WMFDecodeContext),
    .init = ffwmf_init_decoder,
    .close = ffwmf_close_decoder,
    .receive_frame = ffwmf_receive_frame,
    .sample_fmts =
        (const enum AVSampleFormat[]){AV_SAMPLE_FMT_S32, AV_SAMPLE_FMT_S16,
                                      AV_SAMPLE_FMT_NONE},
    .wrapper_name = "wmf",
    .capabilities =
        AV_CODEC_CAP_CHANNEL_CONF | AV_CODEC_CAP_DELAY | AV_CODEC_CAP_DR1,
    .caps_internal = FF_CODEC_CAP_INIT_THREADSAFE | FF_CODEC_CAP_INIT_CLEANUP,
    .flush = ffwmf_flush,
    .priv_class = &ffwmf_aac_decoder_class,
};
